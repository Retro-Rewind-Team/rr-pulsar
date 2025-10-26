#include <Gamemodes/LapKO/LapKOMgr.hpp>
#include <runtimeWrite.hpp>

namespace Pulsar {
namespace LapKO {

static void FrameUpdate() {
    System* system = System::sInstance;
    if (system == nullptr) return;
    if (!system->IsContext(PULSAR_MODE_LAPKO)) return;
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    if (controller->roomType != RKNet::ROOMTYPE_NONE && controller->roomType != RKNet::ROOMTYPE_FROOM_NONHOST && controller->roomType != RKNet::ROOMTYPE_FROOM_HOST) return;
    system->lapKoMgr->UpdateFrame();
}
static RaceFrameHook lapKoFrameHook(FrameUpdate);

kmRuntimeUse(0x8053F3B8);  // Wifi Time Limit Expansion [Chadderz]
kmRuntimeUse(0x8053F3BC);
kmRuntimeUse(0x80521408);  // No Disconnect [Bully]
kmRuntimeUse(0x8053EC94);
kmRuntimeUse(0x8053EF6C);
kmRuntimeUse(0x8053F0B4);
kmRuntimeUse(0x8053F124);
static void WifiEdits() {
    // Default is 5 minutes (300k Milliseconds)
    kmRuntimeWrite32A(0x8053F3B8, 0x3C600005);
    kmRuntimeWrite32A(0x8053F3BC, 0x388393E0);

    // Default disconnect behavior
    kmRuntimeWrite32A(0x80521408, 0x38030001);
    kmRuntimeWrite32A(0x8053EC94, 0x38040001);
    kmRuntimeWrite32A(0x8053EF6C, 0x38030001);
    kmRuntimeWrite32A(0x8053F0B4, 0x38030001);
    kmRuntimeWrite32A(0x8053F124, 0x38030001);

    System* system = System::sInstance;
    if (system == nullptr) return;
    if (!system->IsContext(PULSAR_MODE_LAPKO)) return;
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    if (controller->roomType != RKNet::ROOMTYPE_NONE && controller->roomType != RKNet::ROOMTYPE_FROOM_NONHOST && controller->roomType != RKNet::ROOMTYPE_FROOM_HOST) return;

    // 15 minutes if LapKO is enabled (900k Milliseconds)
    kmRuntimeWrite32A(0x8053F3B8, 0x3C60000D);
    kmRuntimeWrite32A(0x8053F3BC, 0x6064BBA0);

    // Disable disconnects from being idle
    kmRuntimeWrite32A(0x80521408, 0x38000000);
    kmRuntimeWrite32A(0x8053EC94, 0x38000000);
    kmRuntimeWrite32A(0x8053EF6C, 0x38000000);
    kmRuntimeWrite32A(0x8053F0B4, 0x38000000);
    kmRuntimeWrite32A(0x8053F124, 0x38000000);
}
static SectionLoadHook WifiEditsHook(WifiEdits);

// Change HUD Elements to Attached PlayerID [Ro]
kmWrite32(0x807EB500, 0x3800006A);
kmWrite32(0x807EB550, 0x38000001);
kmWrite32(0x807E20B4, 0x38000001);

extern "C" void exhaustPipeboost(void*);
asmFunc cameraIDHUD() {
    ASM(
        nofralloc;
        lis r3, exhaustPipeboost @h;
        lwz r3, exhaustPipeboost @l(r3);
        lwz r3, 0x9D8(r3);
        lwz r3, 0(r3);
        lwz r3, 4(r3);
        lbz r3, 0(r3);
        lwz r0, 0x14(sp);
        blr;)
}
kmCall(0x807EC8D4, cameraIDHUD);

extern "C" void ptr_playerBase(void*);
asmFunc HideMapIcon() {
    ASM(
        lwz r5, 0x38(r3);
        lis r12, ptr_playerBase @ha;
        lwz r12, ptr_playerBase @l(r12);
        lwz r12, 0x20(r12);
        mulli r11, r4, 4;
        lwzx r12, r12, r11;
        lwz r12, 0(r12);
        lwz r12, 4(r12);
        lwz r12, 0xC(r12);
        andis.r12, r12, 0xC;
        beq end;
        ori r5, r5, 0x10;

        end : blr;)
}
kmCall(0x807EB290, HideMapIcon);

asmFunc HideNametag() {
    ASM(
        lwz r0, 4(r3);
        lwz r12, 0xC(r3);
        andis.r12, r12, 0xC;
        beq end;
        ori r0, r0, 0x10;
        end : blr;)
}
kmCall(0x807F09A4, HideNametag);

}  // namespace LapKO
}  // namespace Pulsar