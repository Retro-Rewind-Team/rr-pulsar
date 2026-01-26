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

static const u32 MAGIC = 'RRRT';
static const u16 VERSION = 1;
static const u32 MAX_LICENSES = 4;
static const u32 MAX_PROFILES = 100;

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

static ProfileEntry sProfiles[MAX_PROFILES] = {};
static LicenseBackup sBackups[MAX_LICENSES] = {};
static u32 sReplaceIdx = 0;
static bool sLoaded = false;
static char sPath[IOS::ipcMaxPath] __attribute__((aligned(32))) = {};

static const char* GetPath() {
    if (sPath[0] == '\0') {
        const System* sys = System::sInstance;
        if (!sys) return nullptr;
        snprintf(sPath, IOS::ipcMaxPath, "%s/RRRating.pul", sys->GetModFolder());
    }
    return sPath;
}

static ProfileEntry* FindProfile(s32 id) {
    if (id <= 0) return nullptr;
    for (u32 i = 0; i < MAX_PROFILES; ++i) {
        if (sProfiles[i].profileId == id) return &sProfiles[i];
    }
    return nullptr;
}

static ProfileEntry* AllocProfile(s32 id) {
    for (u32 i = 0; i < MAX_PROFILES; ++i) {
        if (!sProfiles[i].hasData) {
            sProfiles[i].profileId = id;
            return &sProfiles[i];
        }
    }
    ProfileEntry& rep = sProfiles[sReplaceIdx];
    sReplaceIdx = (sReplaceIdx + 1) % MAX_PROFILES;
    rep.hasData = false;
    rep.vr = 0.0f;
    rep.br = 0.0f;
    rep.profileId = id;
    return &rep;
}

static ProfileEntry* GetProfile(s32 id, bool create) {
    if (id <= 0) return nullptr;
    ProfileEntry* e = FindProfile(id);
    if (e) return e;
    if (create) return AllocProfile(id);
    return nullptr;
}

static ProfileEntry* GetProfileForLicense(u32 licenseId, bool create) {
    RKSYS::Mgr* mgr = RKSYS::Mgr::sInstance;
    if (!mgr || licenseId >= MAX_LICENSES) return nullptr;
    return GetProfile(mgr->licenses[licenseId].dwcAccUserData.gsProfileId, create);
}

static void Load() {
    if (sLoaded) return;
    sLoaded = true;

    IO* io = IO::sInstance;
    const char* path = GetPath();
    if (!io || !path || !io->OpenFile(path, FILE_MODE_READ)) return;

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

    u16 count = (hBuf.h.count < MAX_PROFILES) ? hBuf.h.count : MAX_PROFILES;
    for (u16 i = 0; i < count; ++i) {
        union {
            PackedEntry e;
            u8 pad[32];
        } eBuf __attribute__((aligned(32))) = {};
        if (io->Read(sizeof(PackedEntry), &eBuf.e) != (s32)sizeof(PackedEntry)) break;
        if ((eBuf.e.flags & 1) && eBuf.e.profileId > 0) {
            sProfiles[i].profileId = eBuf.e.profileId;
            sProfiles[i].vr = eBuf.e.vr;
            sProfiles[i].br = eBuf.e.br;
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
        PackedHeader h;
        PackedEntry e[MAX_PROFILES];
        u8 pad[32];
    } file __attribute__((aligned(32))) = {};
    file.h.magic = MAGIC;
    file.h.version = VERSION;
    file.h.count = MAX_PROFILES;

    for (u32 i = 0; i < MAX_PROFILES; ++i) {
        file.e[i].profileId = sProfiles[i].profileId;
        file.e[i].vr = sProfiles[i].vr;
        file.e[i].br = sProfiles[i].br;
        file.e[i].flags = sProfiles[i].hasData ? 1u : 0u;
    }

    if (!io->OpenFile(path, FILE_MODE_WRITE)) io->CreateAndOpen(path, FILE_MODE_WRITE);
    io->Overwrite(sizeof(file), &file);
    io->Close();
}

static u16 ClampU16(float v) {
    return (v < (float)MIN_RATING) ? MIN_RATING : (v > (float)MAX_RATING) ? MAX_RATING
                                                                          : (u16)v;
}

static float ClampF(float v) {
    return (v < (float)MIN_RATING) ? (float)MIN_RATING : (v > (float)MAX_RATING) ? (float)MAX_RATING
                                                                                 : v;
}

float GetUserVR(u32 licenseId) {
    Load();
    ProfileEntry* e = GetProfileForLicense(licenseId, false);
    return (e && e->hasData) ? e->vr : DEFAULT_RATING;
}

float GetUserBR(u32 licenseId) {
    Load();
    ProfileEntry* e = GetProfileForLicense(licenseId, false);
    return (e && e->hasData) ? e->br : DEFAULT_RATING;
}

void SetUserVR(u32 licenseId, float vr) {
    Load();
    ProfileEntry* e = GetProfileForLicense(licenseId, true);
    if (e) {
        e->vr = ClampF(vr);
        e->hasData = true;
        Save();
    }
}

void SetUserBR(u32 licenseId, float br) {
    Load();
    ProfileEntry* e = GetProfileForLicense(licenseId, true);
    if (e) {
        e->br = ClampF(br);
        e->hasData = true;
        Save();
    }
}

static ProfileEntry* GetProfileByLicense(const RKSYS::LicenseMgr& lic, bool create) {
    return GetProfile(lic.dwcAccUserData.gsProfileId, create);
}

static void ApplyToLicense(u32 idx, RKSYS::LicenseMgr& lic) {
    Load();
    if (idx >= MAX_LICENSES) return;

    sBackups[idx].originalVr = ClampU16((float)lic.vr.points);
    sBackups[idx].originalBr = ClampU16((float)lic.br.points);
    sBackups[idx].hasOriginal = true;

    ProfileEntry* e = GetProfileByLicense(lic, true);
    if (!e) return;

    if (!e->hasData) {
        e->vr = (float)lic.vr.points / 100.0f;
        e->br = (float)lic.br.points / 100.0f;
        e->hasData = true;
        Save();
    }
    lic.vr.points = ClampU16(e->vr);
    lic.br.points = ClampU16(e->br);
}

static void StoreFromLicense(u32 idx, RKSYS::LicenseMgr& lic) {
    Load();
    if (idx >= MAX_LICENSES) return;

    if (!sBackups[idx].hasOriginal) {
        sBackups[idx].originalVr = ClampU16((float)lic.vr.points);
        sBackups[idx].originalBr = ClampU16((float)lic.br.points);
        sBackups[idx].hasOriginal = true;
    }

    ProfileEntry* e = GetProfileByLicense(lic, true);
    if (!e) return;

    if (!e->hasData) {
        e->vr = (float)lic.vr.points / 100.0f;
        e->br = (float)lic.br.points / 100.0f;
        e->hasData = true;
        Save();
    }
    lic.vr.points = ClampU16(e->vr);
    lic.br.points = ClampU16(e->br);
}

extern "C" int SaveManager_ReadLicenseHook() {
    RKSYS::Mgr* mgr = RKSYS::Mgr::sInstance;
    if (!mgr) return 1;

    RKSYS::LicenseMgr* lic = nullptr;
    asm("mr %0, r31" : "=r"(lic));
    if (!lic) return 1;

    u32 base = reinterpret_cast<u32>(&mgr->licenses[0]);
    u32 addr = reinterpret_cast<u32>(lic);
    if (addr >= base) {
        u32 idx = (addr - base) / sizeof(RKSYS::LicenseMgr);
        if (idx < MAX_LICENSES) ApplyToLicense(idx, *lic);
    }
    return 1;
}

extern "C" void SaveManager_WriteLicenseHook(RKSYS::Binary* raw, u32 idx) {
    RKSYS::Mgr* mgr = RKSYS::Mgr::sInstance;
    if (!mgr || idx >= MAX_LICENSES) return;

    StoreFromLicense(idx, mgr->licenses[idx]);

    if (raw && sBackups[idx].hasOriginal) {
        raw->core.licenses[idx].magic = 'RKPD';
        raw->core.licenses[idx].vr = sBackups[idx].originalVr;
        raw->core.licenses[idx].br = sBackups[idx].originalBr;
    }
}
kmCall(0x805455a8, SaveManager_ReadLicenseHook);
kmCall(0x80546f9c, SaveManager_WriteLicenseHook);

}  // namespace PointRating
}  // namespace Pulsar