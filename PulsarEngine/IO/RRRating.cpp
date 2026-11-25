#include <IO/IO.hpp>
#include <IO/RRRating.hpp>
#include <MarioKartWii/RKSYS/RKSYSMgr.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <PulsarSystem.hpp>
#include <core/nw4r/ut/Misc.hpp>

namespace Pulsar {
namespace RRRating {

IO *ratingIO;
OS::Mutex ratingMutex;
bool checkedPendingRace;
bool raceRecorded;
bool raceActive;

void Init(IO &io) {
    OS::InitMutex(&ratingMutex);
    ratingIO = &io;
}

static bool CreateRatingFile(IO &io, const char *modFolder, const RKSYS::Binary &raw) {
    if (io.OpenFile(path, FILE_MODE_READ)) {
        io.Close();
        return true;
    }

    alignas(0x20) File file = {magic, version, capacity, {}};
    char legacyPath[IOS::ipcMaxPath];
    snprintf(legacyPath, sizeof(legacyPath), "%s/RRRating.pul", modFolder);
    if (io.OpenFile(legacyPath, FILE_MODE_READ)) {
        alignas(0x20) LegacyFile legacy;
        const s32 length = io.Read(sizeof(legacy), &legacy);
        io.Close();
        if (length != sizeof(legacy) || legacy.magic != magic || legacy.version != 1 || legacy.entryCapacity != capacity)
            return false;

        for (u16 i = 0; i < capacity; ++i) {
            if (legacy.entries[i].pid == 0)
                continue;
            const float vr = legacy.entries[i].vr;
            const float br = legacy.entries[i].br;
            if (!(vr >= 0.0f && vr <= 42949672.0 && br >= 0.0f && br <= 42949672.0))
                return false;
            file.entries[i].pid = legacy.entries[i].pid;
            file.entries[i].vr = static_cast<u32>(vr * 100.0 + 0.5);
            file.entries[i].br = static_cast<u32>(br * 100.0 + 0.5);
        }
    } else {
        for (u32 i = 0; i < 4; ++i) {
            const RKSYS::RKPD &license = raw.core.licenses[i];
            if (license.magic != 0x524B5044 || license.dwcUserData.gsProfileId <= 0)
                continue;
            file.entries[i].pid = license.dwcUserData.gsProfileId;
            file.entries[i].vr = license.vr;
            file.entries[i].br = license.br;
        }
    }

    for (u16 i = 0; i < capacity; ++i) {
        Entry &entry = file.entries[i];
        if (entry.pid == 0)
            continue;
        for (u16 j = i + 1; j < capacity; ++j) {
            Entry &duplicate = file.entries[j];
            if (duplicate.pid == entry.pid) {
                if (duplicate.vr > entry.vr)
                    entry = duplicate;
                duplicate.pid = 0;
            }
        }
    }

    if (!io.FolderExists(folder) && !io.CreateFolder(folder))
        return false;
    if (!io.CreateAndOpen(path, FILE_MODE_WRITE))
        return false;

    const s32 written = io.Write(sizeof(file), &file);
    io.Close();
    return written == sizeof(file);
}

static bool ReadRatingFromFile(IO &io, u32 pid, u32 &vr, u32 &br) {
    if (pid == 0 || !io.OpenFile(path, FILE_MODE_READ))
        return false;

    alignas(0x20) File file;
    const s32 length = io.Read(sizeof(file), &file);
    io.Close();
    if (length != sizeof(file) || file.magic != magic || file.version != version || file.entryCapacity != capacity)
        return false;

    for (u16 i = 0; i < capacity; ++i) {
        const Entry &entry = file.entries[i];
        if (entry.pid == pid) {
            vr = entry.vr;
            br = entry.br;
            return true;
        }
    }
    return false;
}

static bool WriteRatingToFile(IO &io, u32 pid, u32 vr, u32 br) {
    if (pid == 0 || !io.OpenFile(path, FILE_MODE_READ_WRITE))
        return false;

    alignas(0x20) File file;
    const s32 length = io.Read(sizeof(file), &file);
    if (length != sizeof(file) || file.magic != magic || file.version != version || file.entryCapacity != capacity) {
        io.Close();
        return false;
    }

    u16 slot = 0;
    for (u16 i = 0; i < capacity; ++i) {
        const Entry &entry = file.entries[i];
        if (entry.pid == pid) {
            if (entry.vr == vr && entry.br == br) {
                io.Close();
                return true;
            }
            slot = i;
            break;
        }
        const Entry &selected = file.entries[slot];
        if (selected.pid != 0 && (entry.pid == 0 || entry.vr < selected.vr))
            slot = i;
    }

    alignas(0x20) const Entry entry = {pid, vr, br};
    io.Seek(offsetof(File, entries) + slot * sizeof(entry));
    const s32 entryWritten = io.Write(sizeof(entry), &entry);
    io.Close();
    return entryWritten == sizeof(entry);
}

static bool RecordPendingRace(u32 pid, u32 penalizedVR) {
    if (!ratingIO->OpenFile(path, FILE_MODE_READ_WRITE))
        return false;
    alignas(0x20) const PendingRace pending = {pid, penalizedVR};
    ratingIO->Seek(sizeof(File));
    const s32 written = ratingIO->Write(sizeof(pending), &pending);
    ratingIO->Close();
    return written == sizeof(pending);
}

static void ClearPendingRace() {
    raceActive = false;
    if (!raceRecorded)
        return;
    nw4r::ut::AutoMutexLock lock(ratingMutex);
    raceRecorded = !RecordPendingRace(0, 0);
}
static SectionLoadHook ClearPendingRaceOnSectionLoad(ClearPendingRace);

static void TrackPendingRace() {
    const RKNet::Controller *controller = RKNet::Controller::sInstance;
    const bool isRacing = controller != nullptr && Raceinfo::sInstance->stage == RACESTAGE_RACE && Racedata::sInstance->racesScenario.settings.gamemode == MODE_PUBLIC_VS
      && controller->connectionState == RKNet::CONNECTIONSTATE_ROOM && !controller->shutdownScheduled;
    if (!isRacing) {
        if (raceActive)
            ClearPendingRace();
        return;
    }
    if (raceActive || ratingIO == nullptr)
        return;
    raceActive = true;

    const RKSYS::Mgr &save = *RKSYS::Mgr::sInstance;
    const RKSYS::LicenseMgr &license = save.licenses[save.curLicenseId];
    const s32 pid = license.dwcAccUserData.gsProfileId;
    if (pid <= 0)
        return;
    nw4r::ut::AutoMutexLock lock(ratingMutex);
    if (CreateRatingFile(*ratingIO, System::sInstance->GetModFolder(), *save.rawRKSYS) && WriteRatingToFile(*ratingIO, pid, license.vr.points, license.br.points)) {
        const u32 vr = license.vr.points;
        raceRecorded = RecordPendingRace(pid, vr > 210 ? vr - 210 : 1);
        if (raceRecorded)
            checkedPendingRace = true;
    }
}
static RaceFrameHook TrackPendingRaceDuringRace(TrackPendingRace);

static void ReadLicenseRating(const RKSYS::Binary &raw, RKSYS::LicenseMgr &dest, u32 licenseId) {
    const RKSYS::RKPD &license = raw.core.licenses[licenseId];
    dest.vr.points = license.vr;
    dest.br.points = license.br;
    if (ratingIO == nullptr)
        return;

    nw4r::ut::AutoMutexLock lock(ratingMutex);
    if (!CreateRatingFile(*ratingIO, System::sInstance->GetModFolder(), raw))
        return;

    if (!checkedPendingRace && ratingIO->OpenFile(path, FILE_MODE_READ)) {
        alignas(0x20) PendingRace pending;
        ratingIO->Seek(sizeof(File));
        const s32 length = ratingIO->Read(sizeof(pending), &pending);
        ratingIO->Close();
        checkedPendingRace = true;
        if (length == sizeof(pending) && pending.pid != 0 && pending.penalizedVR > 0 && pending.penalizedVR < maxRating) {
            u32 vr, br;
            if (ReadRatingFromFile(*ratingIO, pending.pid, vr, br) && vr > pending.penalizedVR) {
                // The stored target makes recovery safe to retry after another power loss.
                checkedPendingRace = WriteRatingToFile(*ratingIO, pending.pid, pending.penalizedVR, br);
            }
            if (checkedPendingRace)
                checkedPendingRace = RecordPendingRace(0, 0);
        }
    }

    const s32 pid = license.dwcUserData.gsProfileId;
    if (pid > 0 && !ReadRatingFromFile(*ratingIO, pid, dest.vr.points, dest.br.points)) {
        WriteRatingToFile(*ratingIO, pid, dest.vr.points, dest.br.points);
    }
    if (dest.vr.points > maxRating)
        dest.vr.points = maxRating;
    if (dest.br.points > maxRating)
        dest.br.points = maxRating;
}
kmWrite32(0x80544f80, 0x7fe4fb78);  // mr r4, r31
kmCall(0x80544f84, ReadLicenseRating);
kmWrite32(0x80544f88, 0x48000028);  // Skip the original VR and BR reads and setters.

static void WriteLicenseRating(const RKSYS::Binary &raw, const RKSYS::LicenseMgr &src, u32 licenseId) {
    const s32 pid = raw.core.licenses[licenseId].dwcUserData.gsProfileId;
    if (ratingIO == nullptr || pid <= 0)
        return;

    nw4r::ut::AutoMutexLock lock(ratingMutex);
    if (CreateRatingFile(*ratingIO, System::sInstance->GetModFolder(), raw))
        WriteRatingToFile(*ratingIO, pid, src.vr.points, src.br.points);
}
kmWrite32(0x8054699c, 0x7fe4fb78);  // mr r4, r31
kmCall(0x805469a8, WriteLicenseRating);
kmWrite32(0x805469ac, 0x48000018);  // Skip the original BR write.

}  // namespace RRRating
}  // namespace Pulsar
