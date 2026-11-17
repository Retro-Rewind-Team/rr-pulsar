#include <Gamemodes/LapKO/LapKOMgr.hpp>
#include <MarioKartWii/Item/ItemSlot.hpp>
#include <MarioKartWii/Item/ItemManager.hpp>
#include <GlobalFunction.hpp>

namespace Pulsar {
namespace LapKO {

static void FrameUpdate() {
    System *system = System::sInstance;
    if (!system->IsContext(PULSAR_MODE_LAPKO))
        return;
    if (IsPublicRoom())
        return;
    system->lapKoMgr->UpdateFrame();
}
static RaceFrameHook lapKoFrameHook(FrameUpdate);

extern "C" u32 sWifiRaceTimeLimit = 300000;
extern "C" u8 sDisableIdleDisconnect = false;
static void WifiEdits() {
    sWifiRaceTimeLimit = 300000;
    sDisableIdleDisconnect = false;

#ifdef RR_TESTS
    sDisableIdleDisconnect = true;
#endif

    System *system = System::sInstance;
    if (system == nullptr)
        return;
    if (!system->IsContext(PULSAR_MODE_LAPKO) && !system->IsContext(PULSAR_MODE_BATTLEROYALE))
        return;
    if (IsPublicRoom())
        return;

    sWifiRaceTimeLimit = 900000;
    sDisableIdleDisconnect = true;
}
static SectionLoadHook WifiEditsHook(WifiEdits);

asmFunc LoadWifiRaceTimeLimit() {
    ASM(
        nofralloc;
        lis r4, sWifiRaceTimeLimit @ha;
        lwz r4, sWifiRaceTimeLimit @l(r4);
        blr;
    )
}
kmCall(0x8053F3BC, LoadWifiRaceTimeLimit);

asmFunc UpdateIdleDisconnectCounter() {
    ASM(
        nofralloc;
        stwu r1, -0x10(r1);
        stw r12, 0x8(r1);
        lis r12, sDisableIdleDisconnect @ha;
        lbz r0, sDisableIdleDisconnect @l(r12);
        cmpwi r0, 0;
        lwz r12, 0x8(r1);
        addi r1, r1, 0x10;
        beq increment;
        li r0, 0;
        blr;
    increment:
        addi r0, r3, 1;
        blr;
    )
}
kmCall(0x80521408, UpdateIdleDisconnectCounter);
kmCall(0x8053EF6C, UpdateIdleDisconnectCounter);
kmCall(0x8053F0B4, UpdateIdleDisconnectCounter);
kmCall(0x8053F124, UpdateIdleDisconnectCounter);

// Change HUD Elements to Attached PlayerID [Ro]
kmWrite32(0x807EB500, 0x3800006A);
kmWrite32(0x807E20B4, 0x38000001);

extern "C" void exhaustPipeboost(void *);
extern "C" u8 sUseLocalCameraHudId = false;
asmFunc cameraIDHUD() {
    ASM(
        nofralloc;
        lis r12, sUseLocalCameraHudId @ha;
        lbz r12, sUseLocalCameraHudId @l(r12);
        cmpwi r12, 0;
        beq original;
        lis r3, exhaustPipeboost @h;
        lwz r3, exhaustPipeboost @l(r3);
        lwz r3, 0x9D8(r3);
        lwz r3, 0(r3);
        lwz r3, 4(r3);
        lbz r3, 0(r3);
    original:
        ;
        lwz r0, 0x14(sp);
        blr;
    )
}

static void camerIDHUDLocal() {
    const RacedataScenario &scenario = Racedata::sInstance->menusScenario;
    sUseLocalCameraHudId = scenario.localPlayerCount <= 1;
}
static SectionLoadHook cameraIDHUDHook(camerIDHUDLocal);
kmCall(0x807EC8D4, cameraIDHUD);

extern "C" void ptr_playerBase(void *);
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

    end:
        blr;
    )
}
kmCall(0x807EB290, HideMapIcon);

asmFunc HideNametag() {
    ASM(
        lwz r0, 4(r3);
        lwz r12, 0xC(r3);
        andis.r12, r12, 0xC;
        beq end;
        ori r0, r0, 0x10;
    end:
        blr;
    )
}
kmCall(0x807F09A4, HideNametag);

// Fix Lap Counter Color in LapKO [Saucy]
extern "C" void LapCounterColorFixHelper(CtrlRaceBase *self) {
    System *system = System::sInstance;
    if (self == nullptr)
        return;
    if (system == nullptr || (!system->IsContext(PULSAR_MODE_LAPKO) && !system->IsContext(PULSAR_MODE_BATTLEROYALE)))
        return;

    const char *leftPane = nullptr;
    if (self->layout.GetPaneByName("lap_lefft") != nullptr) {
        leftPane = "lap_lefft";
    } else if (self->layout.GetPaneByName("lap_left") != nullptr) {
        leftPane = "lap_left";
    }

    const char *rightPane = nullptr;
    if (self->layout.GetPaneByName("lap_riighter") != nullptr) {
        rightPane = "lap_riighter";
    } else if (self->layout.GetPaneByName("lap_right") != nullptr) {
        rightPane = "lap_right";
    }

    if (leftPane != nullptr)
        self->HudSlotColorEnable(leftPane, true);
    if (rightPane != nullptr)
        self->HudSlotColorEnable(rightPane, true);
}

asmFunc LapCounterColorFix() {
    ASM(
        nofralloc;
        stwu sp, -0x10(sp);
        mflr r0;
        stw r0, 0x14(sp);

        mr r3, r28;
        bl LapCounterColorFixHelper;

        lwz r0, 0x14(sp);
        mtlr r0;
        addi sp, sp, 0x10;

        mr r3, r28;
        blr;
    )
}
kmCall(0x807EF7E8, LapCounterColorFix);

}  // namespace LapKO
}  // namespace Pulsar
