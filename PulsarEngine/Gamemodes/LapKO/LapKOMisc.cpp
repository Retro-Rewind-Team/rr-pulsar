#include <Gamemodes/LapKO/LapKOMgr.hpp>
#include <MarioKartWii/Item/ItemSlot.hpp>
#include <MarioKartWii/Item/ItemManager.hpp>
#include <Gamemodes/ItemRain/ItemRain.hpp>
#include <Patching/RuntimeChoice.hpp>

namespace Pulsar {
namespace LapKO {

static void FrameUpdate() {
    System* system = System::sInstance;
    if (!system->IsContext(PULSAR_MODE_LAPKO)) return;
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    if (controller->roomType != RKNet::ROOMTYPE_NONE && controller->roomType != RKNet::ROOMTYPE_FROOM_NONHOST && controller->roomType != RKNet::ROOMTYPE_FROOM_HOST) return;
    system->lapKoMgr->UpdateFrame();
}
static RaceFrameHook lapKoFrameHook(FrameUpdate);

static bool ShouldUseExtendedWifiRules() {
    System* system = System::sInstance;
    if (system == nullptr) return false;
    if (!system->IsContext(PULSAR_MODE_LAPKO) && !system->IsContext(PULSAR_MODE_BATTLEROYALE)) return false;
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    if (controller == nullptr) return false;
    return controller->roomType == RKNet::ROOMTYPE_NONE ||
           controller->roomType == RKNet::ROOMTYPE_FROOM_NONHOST ||
           controller->roomType == RKNet::ROOMTYPE_FROOM_HOST;
}

static bool ShouldDisableIdleDisconnect() {
#ifdef RR_TESTS
    return true;
#else
    return ShouldUseExtendedWifiRules();
#endif
}

static u32 sWifiTimeLimitHigh = 0x00050000;
static u32 sWifiTimeLimitMs = 300000;
static u32 sDisableIdleDisconnect = 0;
static u32 sUseAttachedCameraHud = 0;

static void UpdateRuntimePatchState() {
    if (ShouldUseExtendedWifiRules()) {
        sWifiTimeLimitHigh = 0x000D0000;
        sWifiTimeLimitMs = 900000;
    } else {
        sWifiTimeLimitHigh = 0x00050000;
        sWifiTimeLimitMs = 300000;
    }

    sDisableIdleDisconnect = ShouldDisableIdleDisconnect() ? 1 : 0;

    if (Racedata::sInstance == nullptr) {
        sUseAttachedCameraHud = 0;
    } else {
        const RacedataScenario& scenario = Racedata::sInstance->menusScenario;
        sUseAttachedCameraHud = scenario.localPlayerCount <= 1 ? 1 : 0;
    }
}
static SectionLoadHook UpdateRuntimePatchStateHook(UpdateRuntimePatchState);

RuntimeChoice_CachedInstruction2(LoadWifiTimeLimit, 0x8053F3B8, r3, sWifiTimeLimitHigh, r4, sWifiTimeLimitMs);
kmWriteNop(0x8053F3BC);

RuntimeChoice_ConditionalAddOrZero(LoadIdleDisconnectResultA, 0x80521408, sDisableIdleDisconnect, r0, r3, 1);
RuntimeChoice_ConditionalAddOrZero(LoadIdleDisconnectResultB, 0x8053EF6C, sDisableIdleDisconnect, r0, r3, 1);
RuntimeChoice_ConditionalAddOrZero(LoadIdleDisconnectResultC, 0x8053F0B4, sDisableIdleDisconnect, r0, r3, 1);
RuntimeChoice_ConditionalAddOrZero(LoadIdleDisconnectResultD, 0x8053F124, sDisableIdleDisconnect, r0, r3, 1);

// Change HUD Elements to Attached PlayerID [Ro]
kmWrite32(0x807EB500, 0x3800006A);
kmWrite32(0x807EB550, 0x38000001);
kmWrite32(0x807E20B4, 0x38000001);

extern "C" void exhaustPipeboost(void*);
asmFunc cameraIDHUDLocal() {
    ASM(
        nofralloc;
        stwu sp, -0x20(sp);
        stw r12, 0x8(sp);
        mfcr r12;
        stw r12, 0xC(sp);

        lis r12, sUseAttachedCameraHud @ha;
        lwz r12, sUseAttachedCameraHud @l(r12);
        cmpwi r12, 0;
        bne useAttached;

        lwz r12, 0xC(sp);
        mtcrf 0xff, r12;
        lwz r12, 0x8(sp);
        addi sp, sp, 0x20;
        lwz r0, 0x14(sp);
        blr;

        useAttached:;
        lwz r12, 0xC(sp);
        mtcrf 0xff, r12;
        lwz r12, 0x8(sp);
        addi sp, sp, 0x20;
        lis r3, exhaustPipeboost @h;
        lwz r3, exhaustPipeboost @l(r3);
        lwz r3, 0x9D8(r3);
        lwz r3, 0(r3);
        lwz r3, 4(r3);
        lbz r3, 0(r3);
        lwz r0, 0x14(sp);
        blr;)
}
kmCall(0x807EC8D4, cameraIDHUDLocal);

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

static ItemId DecideItemHook(Item::ItemSlotData* slotData, u16 setting, u8 position, bool isHuman, bool disableTripleShellsAndBananas, Item::Player* player) {
    ItemId item = slotData->DecideItem(setting, position, isHuman, disableTripleShellsAndBananas, player);

    System* system = System::sInstance;
    if (ItemRain::IsItemRainEnabled()) {
        if (item == GREEN_SHELL || item == RED_SHELL || item == BLUE_SHELL || item == BANANA || item == BOBOMB || item == TRIPLE_BANANA || item == TRIPLE_GREEN_SHELL || item == TRIPLE_RED_SHELL || item == FAKE_ITEM_BOX) {
            return MUSHROOM;
        }
    }

    if (system == nullptr || !system->IsContext(PULSAR_MODE_LAPKO)) return item;

    if (item == BLUE_SHELL) {
        LapKO::Mgr* lapKoMgr = system->lapKoMgr;
        if (lapKoMgr->roundIndex >= lapKoMgr->totalRounds) {
            return MEGA_MUSHROOM;
        }

        const Raceinfo* ri = Raceinfo::sInstance;
        if (ri == nullptr) return item;

        u8 playerCount = Item::Manager::sInstance->playerCount;
        if (playerCount < 6) {
            float threshold = 0.08f * (6 - playerCount);

            u8 firstId = ri->playerIdInEachPosition[0];
            u8 secondId = ri->playerIdInEachPosition[1];

            if (firstId >= 12 || secondId >= 12) return item;

            RaceinfoPlayer* first = ri->players[firstId];
            RaceinfoPlayer* second = ri->players[secondId];

            if (first == nullptr || second == nullptr) return item;

            float diff = first->raceCompletion - second->raceCompletion;

            if (diff < threshold) {
                return MEGA_MUSHROOM;
            }
        }
    }
    return item;
}
kmCall(0x807ba160, DecideItemHook);

// Fix Lap Counter Color in LapKO [Saucy]
extern "C" void LapCounterColorFixHelper(CtrlRaceBase* self) {
    System* system = System::sInstance;
    if (self == nullptr) return;
    if (system == nullptr || (!system->IsContext(PULSAR_MODE_LAPKO) && !system->IsContext(PULSAR_MODE_BATTLEROYALE))) return;

    const char* leftPane = nullptr;
    if (self->layout.GetPaneByName("lap_lefft") != nullptr) {
        leftPane = "lap_lefft";
    } else if (self->layout.GetPaneByName("lap_left") != nullptr) {
        leftPane = "lap_left";
    }

    const char* rightPane = nullptr;
    if (self->layout.GetPaneByName("lap_riighter") != nullptr) {
        rightPane = "lap_riighter";
    } else if (self->layout.GetPaneByName("lap_right") != nullptr) {
        rightPane = "lap_right";
    }

    if (leftPane != nullptr) self->HudSlotColorEnable(leftPane, true);
    if (rightPane != nullptr) self->HudSlotColorEnable(rightPane, true);
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
