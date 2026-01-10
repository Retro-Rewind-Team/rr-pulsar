/*
    RatingQR2.cpp
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
#include <runtimeWrite.hpp>
#include <core/rvl/os/OS.hpp>
#include <core/rvl/RFL/RFL.hpp>
#include <include/c_stdio.h>
#include <include/c_string.h>
#include <MarioKartWii/RKSYS/RKSYSMgr.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/RKNet/USER.hpp>
#include <MarioKartWii/Mii/Mii.hpp>
#include <Network/Rating/PlayerRating.hpp>
#include <Settings/Settings.hpp>

namespace Pulsar {
namespace Network {
extern u32 streamerModeRandomIndex;
}

static wchar_t fakeMiiName[11];

namespace PointRating {

kmRuntimeUse(0x8010f434);
kmRuntimeUse(0x800e4c88);
typedef void (*qr2_buffer_addA_t)(void* buffer, const char* value);
static const qr2_buffer_addA_t qr2_buffer_addA = (qr2_buffer_addA_t)kmRuntimeAddr(0x8010f434);
typedef void (*ServerKeyCallback)(int key, void* buffer);
static const ServerKeyCallback OriginalServerKeyCallback = (ServerKeyCallback)kmRuntimeAddr(0x800e4c88);

static bool IsStreamerModeActiveForServer() {
    const Settings::Mgr& settings = Settings::Mgr::Get();
    if (settings.GetUserSettingValue(Settings::SETTINGSTYPE_ONLINE, RADIO_STREAMERMODE) == STREAMERMODE_DISABLED) {
        return false;
    }
    RKNet::Controller* controller = RKNet::Controller::sInstance;
    if (controller == nullptr) return false;
    if (controller->roomType == RKNet::ROOMTYPE_FROOM_HOST || controller->roomType == RKNet::ROOMTYPE_FROOM_NONHOST) {
        return false;
    }
    return true;
}

static int ClampRatingForQr2(float vr) {
    int scaled = (int)(vr * 100.0f);
    if (scaled < 1) {
        scaled = 1;
    } else if (scaled > 1000000) {
        scaled = 1000000;
    }
    return scaled;
}

static void MyServerKeyCallback(int key, void* buffer) {
    RKSYS::Mgr* rksys = RKSYS::Mgr::sInstance;
    u32 licenseId = 0;
    if (rksys) {
        licenseId = rksys->curLicenseId;
    }

    if (key == 0x65) {  // ev
        float vr = PointRating::GetUserVR(licenseId);
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", ClampRatingForQr2(vr));
        qr2_buffer_addA(buffer, buf);
        return;
    }
    if (key == 0x66) {  // eb
        float br = PointRating::GetUserBR(licenseId);
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", ClampRatingForQr2(br));
        qr2_buffer_addA(buffer, buf);
        return;
    }
    OriginalServerKeyCallback(key, buffer);
}

kmRuntimeUse(0x8010ecac);
typedef int (*qr2_init_socketA_t)(void* q, int s, int bound_port, const char* gamename, const char* secret_key, int is_public, int nat_negotiate, void* server_key_callback, void* player_key_callback, void* team_key_callback, void* key_list_callback, void* count_callback, void* adderror_callback, void* userdata);
static const qr2_init_socketA_t qr2_init_socketA_Real = (qr2_init_socketA_t)kmRuntimeAddr(0x8010ecac);

static int Hook_qr2_init_socketA(void* q, int s, int bound_port, const char* gamename, const char* secret_key, int is_public, int nat_negotiate, void* server_key_callback, void* player_key_callback, void* team_key_callback, void* key_list_callback, void* count_callback, void* adderror_callback, void* userdata) {
    return qr2_init_socketA_Real(q, s, bound_port, gamename, secret_key, is_public, nat_negotiate, (void*)MyServerKeyCallback, player_key_callback, team_key_callback, key_list_callback, count_callback, adderror_callback, userdata);
}
kmCall(0x800d4f28, Hook_qr2_init_socketA);

static RFL::StoreData s_originalMiis[2];
static bool s_originalMiisStored = false;

void GetOriginalMiis(RFL::StoreData* outMii0, RFL::StoreData* outMii1) {
    if (s_originalMiisStored) {
        *outMii0 = s_originalMiis[0];
        *outMii1 = s_originalMiis[1];
    }
}

bool HasOriginalMiisStored() {
    return s_originalMiisStored;
}

static void ReplaceUserPacketMiiForServer(RKNet::USERHandler* handler) {
    if (!IsStreamerModeActiveForServer()) {
        s_originalMiisStored = false;
        return;
    }

    s_originalMiis[0] = handler->toSendPacket.rflPacket.rawMiis[0];
    s_originalMiis[1] = handler->toSendPacket.rflPacket.rawMiis[1];
    s_originalMiisStored = true;

    u32 randomIdx = Network::streamerModeRandomIndex;
    RFL::StoreData* miiSlot0 = &handler->toSendPacket.rflPacket.rawMiis[0];
    RFL::StoreData* miiSlot1 = &handler->toSendPacket.rflPacket.rawMiis[1];

    RFL::GetStoreData(miiSlot0, RFL::RFLDataSource_Default, randomIdx);
    RFL::GetStoreData(miiSlot1, RFL::RFLDataSource_Default, randomIdx);
}

asm void AsmHook_AfterMiiCopyLoop() {
    nofralloc
    // Save volatile registers used by the caller
    stwu    r1, -0x20(r1)
    mflr    r12
    stw     r12, 0x24(r1)
    stw     r3, 0x8(r1)
    stw     r4, 0xc(r1)
    stw     r30, 0x10(r1)

    // Call our C++ function with r31 (USERHandler*) as argument
    mr      r3, r31
    bl      ReplaceUserPacketMiiForServer

    // Restore registers
    lwz     r30, 0x10(r1)
    lwz     r4, 0xc(r1)
    lwz     r3, 0x8(r1)
    lwz     r12, 0x24(r1)
    addi    r1, r1, 0x20
    mtlr    r12

    // Execute the replaced instruction: lis r4, -0x7f64 (0x80a0)
    lis     r4, -0x7f64

    // Return
    blr
}
kmCall(0x80663088, AsmHook_AfterMiiCopyLoop);

static wchar_t* GetLoginMiiName(Mii* mii) {
    if (IsStreamerModeActiveForServer()) {
        RFL::StoreData tempMii;
        u32 randomIdx = Network::streamerModeRandomIndex;
        RFL::GetStoreData(&tempMii, RFL::RFLDataSource_Default, randomIdx);
        memcpy(fakeMiiName, tempMii.miiName, sizeof(fakeMiiName));
        return fakeMiiName;
    }
    return mii->info.name;
}

asm void AsmHook_BeforeDWCLoginAsync() {
    nofralloc
    
    stwu    r1, -0x30(r1)
    mflr    r0
    stw     r0, 0x34(r1)
    // Save registers that DWC_LoginAsync needs
    stw     r4, 0x10(r1)
    stw     r5, 0x14(r1)
    stw     r6, 0x18(r1)
    
    // r3 already has the Mii pointer
    bl      GetLoginMiiName
    
    // r3 now has the name pointer to use
    // Restore registers for DWC_LoginAsync
    lwz     r4, 0x10(r1)
    lwz     r5, 0x14(r1)
    lwz     r6, 0x18(r1)
    lwz     r0, 0x34(r1)
    mtlr    r0
    addi    r1, r1, 0x30
    
    // Return with r3 = name pointer
    blr
}
kmCall(0x80658cd8, AsmHook_BeforeDWCLoginAsync);

}  // namespace PointRating
}  // namespace Pulsar
