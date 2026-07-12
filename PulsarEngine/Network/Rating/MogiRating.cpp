#include <kamek.hpp>
#include <IO/IO.hpp>
#include <include/c_stdio.h>
#include <PulsarSystem.hpp>
#include <MarioKartWii/RKSYS/RKSYSMgr.hpp>
#include <Network/GPReport.hpp>
#include <Network/Rating/MogiRating.hpp>

namespace Pulsar {
namespace MogiRating {

static const u32 MAGIC = 'RRMR';
static const u16 VERSION = 1;
static const u32 MAX_LICENSES = 4;
static const u32 MAX_PROFILES = 100;
#ifdef TEST
static const s32 RESERVED_PROFILE_ID_BASE = 2000000000;
#else
static const s32 RESERVED_PROFILE_ID_BASE = 1000000000;
#endif

struct ProfileEntry {
    s32 profileId;
    float mmr;
    bool hasData;
};

struct PackedHeader {
    u32 magic;
    u16 version;
    u16 count;
};

struct PackedEntry {
    s32 profileId;
    float mmr;
    u32 flags;
};

static ProfileEntry sProfiles[MAX_PROFILES] = {};
static s32 sBoundProfileIds[MAX_LICENSES] = {};
static u32 sReplaceIdx = 0;
static bool sLoaded = false;
static char sPath[IOS::ipcMaxPath] __attribute__((aligned(32))) = {};

static bool IsUsableProfileId(s32 id) {
    return id > 0 && id < RESERVED_PROFILE_ID_BASE;
}

static const char* GetPath() {
    if (sPath[0] == '\0') {
        const System* system = System::sInstance;
        if (!system) return nullptr;
        snprintf(sPath, IOS::ipcMaxPath, "%s/RRMogi.pul", system->GetModFolder());
    }
    return sPath;
}

static ProfileEntry* FindProfile(s32 profileId) {
    if (!IsUsableProfileId(profileId)) return nullptr;
    for (u32 i = 0; i < MAX_PROFILES; ++i) {
        if (sProfiles[i].profileId == profileId) return &sProfiles[i];
    }
    return nullptr;
}

static ProfileEntry* GetProfile(s32 profileId, bool create) {
    if (!IsUsableProfileId(profileId)) return nullptr;

    ProfileEntry* profile = FindProfile(profileId);
    if (profile || !create) return profile;

    for (u32 i = 0; i < MAX_PROFILES; ++i) {
        if (!sProfiles[i].hasData) {
            sProfiles[i].profileId = profileId;
            return &sProfiles[i];
        }
    }

    ProfileEntry& replacement = sProfiles[sReplaceIdx];
    sReplaceIdx = (sReplaceIdx + 1) % MAX_PROFILES;
    replacement.profileId = profileId;
    replacement.mmr = DEFAULT_MMR;
    replacement.hasData = false;
    return &replacement;
}

static s32 ResolveProfileId(u32 licenseId) {
    if (licenseId >= MAX_LICENSES) return 0;

    RKSYS::Mgr* manager = RKSYS::Mgr::sInstance;
    if (manager != nullptr) {
        const s32 profileId = manager->licenses[licenseId].dwcAccUserData.gsProfileId;
        if (IsUsableProfileId(profileId)) {
            sBoundProfileIds[licenseId] = profileId;
            return profileId;
        }
    }

    return IsUsableProfileId(sBoundProfileIds[licenseId]) ? sBoundProfileIds[licenseId] : 0;
}

static float ClampMMR(float mmr) {
    if (mmr < MIN_MMR) return MIN_MMR;
    if (mmr > MAX_MMR) return MAX_MMR;
    return mmr;
}

static void Load() {
    if (sLoaded) return;
    sLoaded = true;

    IO* io = IO::sInstance;
    const char* path = GetPath();
    if (!io || !path || !io->OpenFile(path, FILE_MODE_READ)) return;

    PackedHeader header = {};
    if (io->Read(sizeof(header), &header) != sizeof(header) || header.magic != MAGIC || header.version != VERSION) {
        io->Close();
        return;
    }

    const u16 count = header.count < MAX_PROFILES ? header.count : MAX_PROFILES;
    for (u16 i = 0; i < count; ++i) {
        PackedEntry entry = {};
        if (io->Read(sizeof(entry), &entry) != (s32)sizeof(entry)) break;
        if ((entry.flags & 1) && IsUsableProfileId(entry.profileId)) {
            sProfiles[i].profileId = entry.profileId;
            sProfiles[i].mmr = ClampMMR(entry.mmr);
            sProfiles[i].hasData = true;
        }
    }
    io->Close();
}

static void Save() {
    IO* io = IO::sInstance;
    const char* path = GetPath();
    if (!io || !path) return;

    struct {
        PackedHeader header;
        PackedEntry entries[MAX_PROFILES];
    } file __attribute__((aligned(32))) = {};
    file.header.magic = MAGIC;
    file.header.version = VERSION;
    file.header.count = MAX_PROFILES;

    for (u32 i = 0; i < MAX_PROFILES; ++i) {
        file.entries[i].profileId = sProfiles[i].profileId;
        file.entries[i].mmr = sProfiles[i].mmr;
        file.entries[i].flags = sProfiles[i].hasData ? 1u : 0u;
    }

    if (!io->OpenFile(path, FILE_MODE_WRITE) && !io->CreateAndOpen(path, FILE_MODE_WRITE)) return;
    io->Overwrite(sizeof(file), &file);
    io->Close();
}

void SaveProfileMMR(s32 profileId, float mmr) {
    Load();
    ProfileEntry* profile = GetProfile(profileId, true);
    if (!profile) return;

    profile->mmr = ClampMMR(mmr);
    profile->hasData = true;
    Save();
}

float GetUserMMR(u32 licenseId) {
    Load();
    ProfileEntry* profile = GetProfile(ResolveProfileId(licenseId), false);
    return profile && profile->hasData ? ClampMMR(profile->mmr) : DEFAULT_MMR;
}

void SetUserMMR(u32 licenseId, float mmr) {
    Load();
    ProfileEntry* profile = GetProfile(ResolveProfileId(licenseId), true);
    if (!profile) return;

    profile->mmr = ClampMMR(mmr);
    profile->hasData = true;
    Save();
    ReportCurrentMMR(licenseId);
}

void ReportCurrentMMR(u32 licenseId) {
    const int scaled = (int)(GetUserMMR(licenseId) * 100.0f + 0.5f);
    char buffer[24];
    if (snprintf(buffer, sizeof(buffer), "mmr=%d", scaled) >= 0) {
        Network::Report("wl:mkw_mmr", buffer);
    }
}

void BindLicenseProfileId(u32 licenseId, s32 profileId) {
    if (licenseId < MAX_LICENSES && IsUsableProfileId(profileId)) sBoundProfileIds[licenseId] = profileId;
}

}  // namespace MogiRating
}  // namespace Pulsar
