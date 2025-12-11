/*
    RatingSave.cpp
    Copyright (C) 2025 ZPL

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <kamek.hpp>
#include <IO/IO.hpp>
#include <PulsarSystem.hpp>
#include <Network/Rating/PlayerRating.hpp>
#include <MarioKartWii/RKSYS/RKSYSMgr.hpp>
#include <MarioKartWii/System/Rating.hpp>

namespace Pulsar {
namespace PointRating {

struct ProfileEntry {
    s32 profileId;
    float vr;
    float br;
    bool hasData;
};

struct LicenseBackup {
    u16 originalVr;
    u16 originalBr;
    bool hasOriginal;
};

struct PackedHeader {
    u32 magic;
    u16 version;
    u16 count;
};

struct PackedEntry {
    s32 profileId;
    float vr;
    float br;
    u32 flags;
};

static const u32 kMagic = 'RRRT';
static const u16 kVersion = 1;
static const u32 kMaxLicenses = 4;
static const u32 kMaxProfiles = 100;
static ProfileEntry sProfileEntries[kMaxProfiles] = {};
static LicenseBackup sLicenseBackups[kMaxLicenses] = {};
static u32 sNextReplacementIdx = 0;
static bool sLoaded = false;
static char sFilePath[IOS::ipcMaxPath] __attribute__((aligned(32))) = {0};

static const char* GetFilePath() {
    if (sFilePath[0] == '\0') {
        const System* system = System::sInstance;
        if (system == nullptr) return nullptr;
        snprintf(sFilePath, IOS::ipcMaxPath, "%s/%s", system->GetModFolder(), "RRRating.pul");
        sFilePath[IOS::ipcMaxPath - 1] = '\0';
    }
    return sFilePath;
}

static bool IsValidProfileId(s32 profileId) {
    return profileId > 0;
}

static void ClearProfileEntry(ProfileEntry& entry) {
    entry.profileId = 0;
    entry.vr = 0.0f;
    entry.br = 0.0f;
    entry.hasData = false;
}

static ProfileEntry* FindProfileEntry(s32 profileId) {
    if (!IsValidProfileId(profileId)) return nullptr;
    for (u32 idx = 0; idx < kMaxProfiles; ++idx) {
        ProfileEntry& entry = sProfileEntries[idx];
        if (entry.profileId == profileId) {
            return &entry;
        }
    }
    return nullptr;
}

static ProfileEntry* AllocateProfileEntry(s32 profileId) {
    for (u32 idx = 0; idx < kMaxProfiles; ++idx) {
        ProfileEntry& entry = sProfileEntries[idx];
        if (!entry.hasData) {
            entry.profileId = profileId;
            return &entry;
        }
    }

    ProfileEntry& replacement = sProfileEntries[sNextReplacementIdx];
    sNextReplacementIdx = (sNextReplacementIdx + 1) % kMaxProfiles;
    ClearProfileEntry(replacement);
    replacement.profileId = profileId;
    return &replacement;
}

static ProfileEntry* ResolveEntryForProfile(s32 profileId, bool create) {
    if (!IsValidProfileId(profileId)) return nullptr;
    ProfileEntry* entry = FindProfileEntry(profileId);
    if (entry == nullptr && create) {
        entry = AllocateProfileEntry(profileId);
    }
    return entry;
}

static ProfileEntry* ResolveEntryForLicense(const RKSYS::LicenseMgr& license, bool create) {
    return ResolveEntryForProfile(license.dwcAccUserData.gsProfileId, create);
}

static ProfileEntry* ResolveEntryForLicenseId(u32 licenseId, bool create) {
    RKSYS::Mgr* mgr = RKSYS::Mgr::sInstance;
    if (mgr == nullptr || licenseId >= kMaxLicenses) return nullptr;
    return ResolveEntryForLicense(mgr->licenses[licenseId], create);
}

static void EnsureLoaded() {
    if (sLoaded) return;
    IO* io = IO::sInstance;
    const char* path = GetFilePath();
    if (io == nullptr || path == nullptr) return;

    if (io->OpenFile(path, FILE_MODE_READ)) {
        union {
            PackedHeader header;
            u8 padding[32];
        } headerBuf __attribute__((aligned(32)));
        memset(&headerBuf, 0, sizeof(headerBuf));

        const s32 readHeader = io->Read(sizeof(PackedHeader), &headerBuf.header);
        const PackedHeader& header = headerBuf.header;

        if (readHeader == sizeof(header) && header.magic == kMagic && header.version == kVersion) {
            const u16 count = header.count <= kMaxProfiles ? header.count : kMaxProfiles;
            for (u16 idx = 0; idx < count; ++idx) {
                union {
                    PackedEntry entry;
                    u8 padding[32];
                } entryBuf __attribute__((aligned(32)));
                memset(&entryBuf, 0, sizeof(entryBuf));

                if (io->Read(sizeof(PackedEntry), &entryBuf.entry) != static_cast<s32>(sizeof(PackedEntry))) {
                    break;
                }
                const PackedEntry& entry = entryBuf.entry;

                if ((entry.flags & 0x1) != 0 && IsValidProfileId(entry.profileId)) {
                    ProfileEntry& dest = sProfileEntries[idx];
                    dest.profileId = entry.profileId;
                    dest.vr = entry.vr;
                    dest.br = entry.br;
                    dest.hasData = true;
                }
            }
        }
        io->Close();
    }
    sLoaded = true;
}

static void Persist() {
    IO* io = IO::sInstance;
    const char* path = GetFilePath();
    if (io == nullptr || path == nullptr) return;

    struct PackedFile {
        PackedHeader header;
        PackedEntry entries[kMaxProfiles];
        u8 padding[32];
    } file __attribute__((aligned(32)));
    memset(&file, 0, sizeof(file));

    file.header.magic = kMagic;
    file.header.version = kVersion;
    file.header.count = kMaxProfiles;

    for (u32 idx = 0; idx < kMaxProfiles; ++idx) {
        const ProfileEntry& entry = sProfileEntries[idx];
        file.entries[idx].profileId = entry.profileId;
        file.entries[idx].vr = entry.vr;
        file.entries[idx].br = entry.br;
        file.entries[idx].flags = entry.hasData ? 0x1 : 0x0;
    }

    if (!io->OpenFile(path, FILE_MODE_WRITE)) {
        io->CreateAndOpen(path, FILE_MODE_WRITE);
    }
    io->Overwrite(sizeof(file), &file);
    io->Close();
}

static u16 ClampRating(float value) {
    if (value < (float)MinRating) return MinRating;
    if (value > (float)MaxRating) return MaxRating;
    return static_cast<u16>(value);
}

static float ClampRatingF(float value) {
    if (value < (float)MinRating) return (float)MinRating;
    if (value > (float)MaxRating) return (float)MaxRating;
    return value;
}

float GetUserVR(u32 licenseId) {
    EnsureLoaded();
    ProfileEntry* entry = ResolveEntryForLicenseId(licenseId, false);
    if (entry != nullptr && entry->hasData) {
        return entry->vr;
    }
    return 50.0f;
}

float GetUserBR(u32 licenseId) {
    EnsureLoaded();
    ProfileEntry* entry = ResolveEntryForLicenseId(licenseId, false);
    if (entry != nullptr && entry->hasData) {
        return entry->br;
    }
    return 50.0f;
}

void SetUserVR(u32 licenseId, float vr) {
    EnsureLoaded();
    ProfileEntry* entry = ResolveEntryForLicenseId(licenseId, true);
    if (entry != nullptr) {
        entry->vr = ClampRatingF(vr);
        entry->hasData = true;
        Persist();
    }
}

void SetUserBR(u32 licenseId, float br) {
    EnsureLoaded();
    ProfileEntry* entry = ResolveEntryForLicenseId(licenseId, true);
    if (entry != nullptr) {
        entry->br = ClampRatingF(br);
        entry->hasData = true;
        Persist();
    }
}

static void ApplyToLicense(u32 licenseIdx, RKSYS::LicenseMgr& license) {
    EnsureLoaded();
    if (licenseIdx >= kMaxLicenses) return;

    LicenseBackup& backup = sLicenseBackups[licenseIdx];
    backup.originalVr = ClampRating((float)license.vr.points);
    backup.originalBr = ClampRating((float)license.br.points);
    backup.hasOriginal = true;

    ProfileEntry* entry = ResolveEntryForLicense(license, true);
    if (entry == nullptr) {
        return;
    }

    if (!entry->hasData) {
        entry->vr = (float)license.vr.points / 100.0f;
        entry->br = (float)license.br.points / 100.0f;
        entry->hasData = true;
        Persist();
    }

    license.vr.points = ClampRating(entry->vr);
    license.br.points = ClampRating(entry->br);
}

static void StoreFromLicense(u32 licenseIdx, RKSYS::LicenseMgr& license) {
    EnsureLoaded();
    if (licenseIdx >= kMaxLicenses) return;

    LicenseBackup& backup = sLicenseBackups[licenseIdx];
    if (!backup.hasOriginal) {
        backup.originalVr = ClampRating((float)license.vr.points);
        backup.originalBr = ClampRating((float)license.br.points);
        backup.hasOriginal = true;
    }

    ProfileEntry* entry = ResolveEntryForLicense(license, true);
    if (entry != nullptr) {
        if (!entry->hasData) {
            entry->vr = (float)license.vr.points / 100.0f;
            entry->br = (float)license.br.points / 100.0f;
            entry->hasData = true;
            Persist();
        }

        license.vr.points = ClampRating(entry->vr);
        license.br.points = ClampRating(entry->br);
    }
}

extern "C" int SaveManager_ReadLicenseHook() {
    RKSYS::Mgr* mgr = RKSYS::Mgr::sInstance;
    if (mgr != nullptr) {
        RKSYS::LicenseMgr* license = nullptr;
        asm("mr %0, r31" : "=r"(license));
        if (license != nullptr) {
            const u32 base = reinterpret_cast<u32>(&mgr->licenses[0]);
            const u32 addr = reinterpret_cast<u32>(license);
            if (addr >= base) {
                const u32 diff = (addr - base) / sizeof(RKSYS::LicenseMgr);
                if (diff < kMaxLicenses) {
                    ApplyToLicense(diff, *license);
                }
            }
        }
    }
    return 1;
}

extern "C" void SaveManager_WriteLicenseHook(RKSYS::Binary* raw, u32 licenseIdx) {
    RKSYS::Mgr* mgr = RKSYS::Mgr::sInstance;
    if (mgr != nullptr && licenseIdx < kMaxLicenses) {
        StoreFromLicense(licenseIdx, mgr->licenses[licenseIdx]);

        if (raw != nullptr) {
            RKSYS::RKPD& rawLicense = raw->core.licenses[licenseIdx];
            rawLicense.magic = 'RKPD';

            LicenseBackup& backup = sLicenseBackups[licenseIdx];
            if (backup.hasOriginal) {
                rawLicense.vr = backup.originalVr;
                rawLicense.br = backup.originalBr;
            }
        }
    }
}
kmCall(0x805455a8, SaveManager_ReadLicenseHook);
kmCall(0x80546f9c, SaveManager_WriteLicenseHook);

}  // namespace PointRating
}  // namespace Pulsar