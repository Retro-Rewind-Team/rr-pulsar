#include <Gamemodes/MissionMode/MissionModeSave.hpp>
#include <IO/IO.hpp>
#include <PulsarSystem.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/RKSYS/RKSYSMgr.hpp>

namespace Pulsar {
namespace MissionMode {

namespace {

static const u32 MAGIC = 'RRMS';
static const u16 VERSION = 1;
static const u32 MAX_LICENSES = 4;
static const u32 MAX_MISSIONS = 256;
static const u32 MAX_RATING = 6;

struct MissionEntry {
    u32 finishTimeMillis;
    u8 rating;
    u8 hasData;
    u16 reserved;
};

struct PackedHeader {
    u32 magic;
    u16 version;
    u16 count;
};

struct PackedEntry {
    u32 finishTimeMillis;
    u8 rating;
    u8 flags;
    u16 reserved;
};

static MissionEntry sMissions[MAX_LICENSES][MAX_MISSIONS] = {};
static bool sLoaded = false;
static char sPath[IOS::ipcMaxPath] __attribute__((aligned(32))) = {};

static const char* GetPath() {
    if (sPath[0] == '\0') {
        const System* sys = System::sInstance;
        if (!sys) return nullptr;
        snprintf(sPath, IOS::ipcMaxPath, "%s/RRMission.pul", sys->GetModFolder());
    }
    return sPath;
}

static bool GetCurrentLicenseId(u32& licenseId) {
    RKSYS::Mgr* mgr = RKSYS::Mgr::sInstance;
    if (mgr == nullptr || mgr->curLicenseId >= MAX_LICENSES) return false;
    licenseId = mgr->curLicenseId;
    return true;
}

static void Load() {
    if (sLoaded) return;
    sLoaded = true;

    IO* io = IO::sInstance;
    const char* path = GetPath();
    if (!io || !path) return;
    if (!io->OpenFile(path, FILE_MODE_READ)) return;

    union {
        PackedHeader h;
        u8 pad[32];
    } hBuf __attribute__((aligned(32))) = {};
    if (io->Read(sizeof(PackedHeader), &hBuf.h) != sizeof(PackedHeader)) {
        io->Close();
        return;
    }
    if (hBuf.h.magic != MAGIC || hBuf.h.version != VERSION) {
        io->Close();
        return;
    }

    const u32 maxEntries = MAX_LICENSES * MAX_MISSIONS;
    const u32 count = hBuf.h.count < maxEntries ? hBuf.h.count : maxEntries;
    for (u32 i = 0; i < count; ++i) {
        union {
            PackedEntry e;
            u8 pad[32];
        } eBuf __attribute__((aligned(32))) = {};
        if (io->Read(sizeof(PackedEntry), &eBuf.e) != (s32)sizeof(PackedEntry)) {
            break;
        }

        const u32 licenseId = i / MAX_MISSIONS;
        const u32 missionId = i % MAX_MISSIONS;
        if ((eBuf.e.flags & 1) == 0) continue;
        if (eBuf.e.rating > MAX_RATING) continue;

        MissionEntry& entry = sMissions[licenseId][missionId];
        entry.finishTimeMillis = eBuf.e.finishTimeMillis;
        entry.rating = eBuf.e.rating;
        entry.hasData = 1;
    }
    io->Close();
}

static void Save() {
    IO* io = IO::sInstance;
    const char* path = GetPath();
    if (!io || !path) return;

    struct {
        PackedHeader h;
        PackedEntry e[MAX_LICENSES][MAX_MISSIONS];
        u8 pad[32];
    } file __attribute__((aligned(32))) = {};
    file.h.magic = MAGIC;
    file.h.version = VERSION;
    file.h.count = MAX_LICENSES * MAX_MISSIONS;

    for (u32 licenseId = 0; licenseId < MAX_LICENSES; ++licenseId) {
        for (u32 missionId = 0; missionId < MAX_MISSIONS; ++missionId) {
            const MissionEntry& entry = sMissions[licenseId][missionId];
            PackedEntry& packed = file.e[licenseId][missionId];
            packed.finishTimeMillis = entry.finishTimeMillis;
            packed.rating = entry.rating;
            packed.flags = entry.hasData ? 1 : 0;
            packed.reserved = 0;
        }
    }

    if (!io->OpenFile(path, FILE_MODE_WRITE) && !io->CreateAndOpen(path, FILE_MODE_WRITE)) {
        return;
    }
    io->Overwrite(sizeof(file), &file);
    io->Close();
}

static u8 ConvertMissionRankToRating(u32 missionRank) {
    // Mission ranks are ordered from best to worst (3 stars through C).
    // Keep 0 available for no rank while making 6 the best saved rating.
    if (missionRank > 5) return 0;
    return static_cast<u8>(6 - missionRank);
}

}  // namespace

void SaveMissionResult(u32 finishTimeMillis, u32 missionRank) {
    Load();

    u32 licenseId = 0;
    if (!GetCurrentLicenseId(licenseId)) return;
    if (RKSYS::Mgr::sInstance == nullptr || RKSYS::Mgr::sInstance->curLicenseId >= MAX_LICENSES ||
        Racedata::sInstance == nullptr ||
        Racedata::sInstance->racesScenario.settings.gamemode != MODE_MISSION_TOURNAMENT) {
        return;
    }

    const u32 missionId = Racedata::sInstance->racesScenario.settings.raceNumber;
    const u8 rating = ConvertMissionRankToRating(missionRank);
    if (missionId >= MAX_MISSIONS || rating == 0) return;

    MissionEntry& entry = sMissions[licenseId][missionId];
    if (entry.hasData && finishTimeMillis > entry.finishTimeMillis) return;
    if (entry.hasData && finishTimeMillis == entry.finishTimeMillis && rating <= entry.rating) return;

    entry.finishTimeMillis = finishTimeMillis;
    entry.rating = rating;
    entry.hasData = 1;
    Save();
}

bool GetMissionRecord(u32 missionId, u32& finishTimeMillis, u8& rating) {
    finishTimeMillis = 0;
    rating = 0;
    Load();

    u32 licenseId = 0;
    if (missionId >= MAX_MISSIONS || !GetCurrentLicenseId(licenseId)) return false;

    const MissionEntry& entry = sMissions[licenseId][missionId];
    if (!entry.hasData) return false;
    finishTimeMillis = entry.finishTimeMillis;
    rating = entry.rating;
    return true;
}

}  // namespace MissionMode
}  // namespace Pulsar
