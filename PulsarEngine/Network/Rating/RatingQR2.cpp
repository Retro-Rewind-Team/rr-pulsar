#include <kamek.hpp>
#include <core/GS/QR2/QR2.hpp>
#include <core/rvl/DWC/DWCMatch.hpp>
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

typedef void (*KeyListCallback)(int keyType, void *buffer);
static KeyListCallback OriginalKeyListCallback = nullptr;

static bool IsStreamerModeActiveForServer() {
    const Settings::Mgr &settings = Settings::Mgr::Get();
    if (settings.GetSettingValue(Pulsar::Settings::SETTING_STREAMERMODE) == STREAMERMODE_DISABLED) {
        return false;
    }
    RKNet::Controller *controller = RKNet::Controller::sInstance;
    if (controller == nullptr)
        return false;
    if (controller->roomType == RKNet::ROOMTYPE_FROOM_HOST || controller->roomType == RKNet::ROOMTYPE_FROOM_NONHOST) {
        return false;
    }
    return true;
}

static void RatingServerKeyCallback(int key, void *buffer) {
    RKSYS::Mgr *rksys = RKSYS::Mgr::sInstance;
    u32 licenseId = 0;
    if (rksys) {
        licenseId = rksys->curLicenseId;
    }

    if (key == 0x65 || key == 0x66) {  // ev / eb
        const float rating = key == 0x65 ? GetUserVR(licenseId) : GetUserBR(licenseId);
        char bufferText[32];
        const int points = (int)rating;
        snprintf(bufferText, sizeof(bufferText), "%d", points < 1 ? 1 : points > MAX_RATING ? MAX_RATING : points);
        qr2_buffer_addA(buffer, bufferText);
        return;
    }
    DWC::iQR2ServerKeyCallback(key, buffer);
}

static void RatingKeyListCallback(int keyType, void *buffer) {
    if (OriginalKeyListCallback) {
        OriginalKeyListCallback(keyType, buffer);
    }
    if (keyType == 0) {  // QR2_SERVER_KEY
        // Add ev and eb if they aren't already in the buffer (they are missing in private rooms)
        // QR2 keeps the key count immediately after its 256-byte key array.
        u8 *keys = reinterpret_cast<u8 *>(buffer);
        int count = *reinterpret_cast<int *>(keys + 0x100);
        bool hasEv = false;
        bool hasEb = false;
        for (int i = 0; i < count; i++) {
            if (keys[i] == 0x65)
                hasEv = true;
            if (keys[i] == 0x66)
                hasEb = true;
        }
        if (!hasEv)
            qr2_keybuffer_add(buffer, 0x65);
        if (!hasEb)
            qr2_keybuffer_add(buffer, 0x66);
    }
}

static int InitRatingQR2(void *q, int socket, int boundPort, const char *gameName, const char *secretKey, int isPublic, int natNegotiate, void *serverKeyCallback, void *playerKeyCallback,
  void *teamKeyCallback, void *keyListCallback, void *countCallback, void *addErrorCallback, void *userdata) {
    OriginalKeyListCallback = reinterpret_cast<KeyListCallback>(keyListCallback);
    return qr2_init_socketA(q, socket, boundPort, gameName, secretKey, isPublic, natNegotiate, reinterpret_cast<void *>(RatingServerKeyCallback), playerKeyCallback, teamKeyCallback,
      reinterpret_cast<void *>(RatingKeyListCallback), countCallback, addErrorCallback, userdata);
}
kmCall(0x800d4f28, InitRatingQR2);

static RFL::StoreData s_originalMiis[2];
static bool s_originalMiisStored = false;

void GetOriginalMiis(RFL::StoreData *outMii0, RFL::StoreData *outMii1) {
    if (s_originalMiisStored) {
        *outMii0 = s_originalMiis[0];
        *outMii1 = s_originalMiis[1];
    }
}

bool HasOriginalMiisStored() {
    return s_originalMiisStored;
}

static void ReplaceUserPacketMiiForServer(RKNet::USERHandler *handler) {
    if (!IsStreamerModeActiveForServer()) {
        s_originalMiisStored = false;
        return;
    }

    s_originalMiis[0] = handler->toSendPacket.rflPacket.rawMiis[0];
    s_originalMiis[1] = handler->toSendPacket.rflPacket.rawMiis[1];
    s_originalMiisStored = true;

    u32 randomIdx = Network::streamerModeRandomIndex;
    RFL::StoreData *miiSlot0 = &handler->toSendPacket.rflPacket.rawMiis[0];
    RFL::StoreData *miiSlot1 = &handler->toSendPacket.rflPacket.rawMiis[1];

    RFL::GetStoreData(miiSlot0, RFL::RFLDataSource_Default, randomIdx);
    RFL::GetStoreData(miiSlot1, RFL::RFLDataSource_Default, randomIdx);
}

// clang-format off
asm void AsmHook_AfterMiiCopyLoop() {
    nofralloc
    // The caller keeps the USER handler in r31.
    stwu    r1, -0x20(r1)
    mflr    r12
    stw     r12, 0x24(r1)
    stw     r3, 0x8(r1)
    stw     r4, 0xc(r1)
    stw     r30, 0x10(r1)

    mr      r3, r31
    bl      ReplaceUserPacketMiiForServer

    lwz     r30, 0x10(r1)
    lwz     r4, 0xc(r1)
    lwz     r3, 0x8(r1)
    lwz     r12, 0x24(r1)
    addi    r1, r1, 0x20
    mtlr    r12

    // Execute the replaced instruction: lis r4, -0x7f64 (0x80a0)
    lis     r4, -0x7f64

    blr
}  // clang-format on
kmCall(0x80663088, AsmHook_AfterMiiCopyLoop);

static wchar_t *GetLoginMiiName(Mii *mii) {
    if (IsStreamerModeActiveForServer()) {
        RFL::StoreData tempMii;
        u32 randomIdx = Network::streamerModeRandomIndex;
        RFL::GetStoreData(&tempMii, RFL::RFLDataSource_Default, randomIdx);
        memcpy(fakeMiiName, tempMii.miiName, sizeof(fakeMiiName));
        return fakeMiiName;
    }
    return mii->info.name;
}

// clang-format off
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
    
    // Restore registers for DWC_LoginAsync
    lwz     r4, 0x10(r1)
    lwz     r5, 0x14(r1)
    lwz     r6, 0x18(r1)
    lwz     r0, 0x34(r1)
    mtlr    r0
    addi    r1, r1, 0x30
    
    blr
}  // clang-format on
kmCall(0x80658cd8, AsmHook_BeforeDWCLoginAsync);

}  // namespace PointRating
}  // namespace Pulsar
