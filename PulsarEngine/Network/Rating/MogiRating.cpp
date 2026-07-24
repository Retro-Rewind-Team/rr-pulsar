#include <kamek.hpp>
#include <IO/IO.hpp>
#include <include/c_stdio.h>
#include <PulsarSystem.hpp>
#include <MarioKartWii/RKSYS/RKSYSMgr.hpp>
#include <Network/Mogi.hpp>
#include <Network/GPReport.hpp>
#include <Network/Rating/MogiRating.hpp>

namespace Pulsar {
namespace MogiRating {

static const u32 MAGIC = 'RRMR';
static const u16 VERSION = 2;
static const u16 LEGACY_VERSION = 1;
static const u32 MAX_LICENSES = 4;
static const u32 MAX_PROFILES = 100;
#ifdef TEST
static const s32 RESERVED_PROFILE_ID_BASE = 2000000000;
#else
static const s32 RESERVED_PROFILE_ID_BASE = 1000000000;
#endif

struct ProfileEntry {
    s32 profileId;
    float mmr[MMR_MODE_COUNT];
    u32 dataFlags;
    float storedMMR[MMR_MODE_COUNT];
    u32 storedFlags;
};

struct PackedHeader {
    u32 magic;
    u16 version;
    u16 count;
};

struct PackedEntry {
    s32 profileId;
    float mmr[MMR_MODE_COUNT];
    u32 flags;
};

struct LegacyPackedEntry {
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

static bool IsValidMode(MMRMode mode) {
    return static_cast<u32>(mode) < MMR_MODE_COUNT;
}

static u32 ModeFlag(MMRMode mode) {
    return 1u << static_cast<u32>(mode);
}

MMRMode GetCurrentMode() {
    const System* system = System::sInstance;
    if (system != nullptr) {
        if (system->netMgr.region == Mogi::REGION_CT) return MMR_MODE_CT;
        if (system->netMgr.region == Mogi::REGION_REG) return MMR_MODE_REGULAR;
    }
    return MMR_MODE_RETRO;
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
        if (sProfiles[i].dataFlags == 0 && sProfiles[i].storedFlags == 0) {
            sProfiles[i].profileId = profileId;
            for (u32 mode = 0; mode < MMR_MODE_COUNT; ++mode) {
                sProfiles[i].mmr[mode] = DEFAULT_MMR;
                sProfiles[i].storedMMR[mode] = DEFAULT_MMR;
            }
            sProfiles[i].dataFlags = 0;
            sProfiles[i].storedFlags = 0;
            return &sProfiles[i];
        }
    }

    ProfileEntry& replacement = sProfiles[sReplaceIdx];
    sReplaceIdx = (sReplaceIdx + 1) % MAX_PROFILES;
    replacement.profileId = profileId;
    for (u32 mode = 0; mode < MMR_MODE_COUNT; ++mode) {
        replacement.mmr[mode] = DEFAULT_MMR;
        replacement.storedMMR[mode] = DEFAULT_MMR;
    }
    replacement.dataFlags = 0;
    replacement.storedFlags = 0;
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
    if (io->Read(sizeof(header), &header) != sizeof(header) || header.magic != MAGIC ||
        (header.version != LEGACY_VERSION && header.version != VERSION)) {
        io->Close();
        return;
    }

    const u16 count = header.count < MAX_PROFILES ? header.count : MAX_PROFILES;
    for (u16 i = 0; i < count; ++i) {
        if (header.version == LEGACY_VERSION) {
            LegacyPackedEntry entry = {};
            if (io->Read(sizeof(entry), &entry) != (s32)sizeof(entry)) break;
            if ((entry.flags & 1) && IsUsableProfileId(entry.profileId)) {
                sProfiles[i].profileId = entry.profileId;
                for (u32 mode = 0; mode < MMR_MODE_COUNT; ++mode) {
                    sProfiles[i].mmr[mode] = ClampMMR(entry.mmr);
                    sProfiles[i].storedMMR[mode] = sProfiles[i].mmr[mode];
                }
                sProfiles[i].dataFlags = ModeFlag(MMR_MODE_RETRO) | ModeFlag(MMR_MODE_CT) |
                                          ModeFlag(MMR_MODE_REGULAR);
                sProfiles[i].storedFlags = sProfiles[i].dataFlags;
            }
            continue;
        }

        PackedEntry entry = {};
        if (io->Read(sizeof(entry), &entry) != (s32)sizeof(entry)) break;
        if ((entry.flags & ((1u << MMR_MODE_COUNT) - 1)) && IsUsableProfileId(entry.profileId)) {
            sProfiles[i].profileId = entry.profileId;
            for (u32 mode = 0; mode < MMR_MODE_COUNT; ++mode) {
                sProfiles[i].mmr[mode] = ClampMMR(entry.mmr[mode]);
                sProfiles[i].storedMMR[mode] = sProfiles[i].mmr[mode];
            }
            sProfiles[i].dataFlags = entry.flags & ((1u << MMR_MODE_COUNT) - 1);
            sProfiles[i].storedFlags = sProfiles[i].dataFlags;
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
        for (u32 mode = 0; mode < MMR_MODE_COUNT; ++mode) {
            file.entries[i].mmr[mode] = sProfiles[i].storedMMR[mode];
        }
        file.entries[i].flags = sProfiles[i].storedFlags;
    }

    if (!io->OpenFile(path, FILE_MODE_WRITE) && !io->CreateAndOpen(path, FILE_MODE_WRITE)) return;
    io->Overwrite(sizeof(file), &file);
    io->Close();
}

void SetProfileMMR(s32 profileId, MMRMode mode, float mmr) {
    if (!IsValidMode(mode)) return;

    Load();
    ProfileEntry* profile = GetProfile(profileId, true);
    if (!profile) return;

    profile->mmr[mode] = ClampMMR(mmr);
    profile->dataFlags |= ModeFlag(mode);
    profile->storedMMR[mode] = profile->mmr[mode];
    profile->storedFlags |= ModeFlag(mode);
    Save();
}

float GetUserMMRForMode(u32 licenseId, MMRMode mode) {
    if (!IsValidMode(mode)) return DEFAULT_MMR;

    Load();
    ProfileEntry* profile = GetProfile(ResolveProfileId(licenseId), false);
    return profile && (profile->dataFlags & ModeFlag(mode)) ? ClampMMR(profile->mmr[mode]) : DEFAULT_MMR;
}

float GetUserMMR(u32 licenseId) {
    return GetUserMMRForMode(licenseId, GetCurrentMode());
}

float GetStoredMMRForMode(s32 profileId, MMRMode mode) {
    if (!IsValidMode(mode)) return DEFAULT_MMR;

    Load();
    ProfileEntry* profile = GetProfile(profileId, false);
    return profile && (profile->storedFlags & ModeFlag(mode)) ? ClampMMR(profile->storedMMR[mode]) : DEFAULT_MMR;
}

float GetStoredMMR(s32 profileId) {
    return GetStoredMMRForMode(profileId, GetCurrentMode());
}

void SetUserMMR(u32 licenseId, float mmr) {
    const MMRMode mode = GetCurrentMode();
    Load();
    ProfileEntry* profile = GetProfile(ResolveProfileId(licenseId), true);
    if (!profile) return;

    profile->mmr[mode] = ClampMMR(mmr);
    profile->dataFlags |= ModeFlag(mode);
    ReportCurrentMMR(licenseId);
}

void ReportCurrentMMR(u32 licenseId) {
    if (!Mogi::IsEnabled()) return;

    const MMRMode mode = GetCurrentMode();
    const char* modeName = mode == MMR_MODE_CT ? "ct" :
                           mode == MMR_MODE_REGULAR ? "regular" : "retro";
    const int scaled = (int)(GetUserMMRForMode(licenseId, mode) * 100.0f + 0.5f);
    char buffer[48];
    if (snprintf(buffer, sizeof(buffer), "mode=%s|mmr=%d", modeName, scaled) >= 0) {
        Network::Report("wl:mkw_mmr", buffer);
    }
}

void BindLicenseProfileId(u32 licenseId, s32 profileId) {
    if (licenseId < MAX_LICENSES && IsUsableProfileId(profileId)) sBoundProfileIds[licenseId] = profileId;
}

}  // namespace MogiRating
}  // namespace Pulsar
