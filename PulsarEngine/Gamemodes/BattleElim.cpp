#include <RetroRewind.hpp>
#include <runtimeWrite.hpp>
#include <MarioKartWii/Race/Racedata.hpp>
#include <MarioKartWii/Kart/KartManager.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <MarioKartWii/UI/Ctrl/CtrlRace/CtrlRaceTime.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/Race/RaceData.hpp>

namespace Pulsar {
namespace BattleElim {

extern "C" volatile unsigned int gBattleElimFlag = 0;
extern "C" volatile unsigned int gBattleElimMakeInvisible = 0;
extern "C" volatile unsigned int gBattleElimRemaining = 0;
extern "C" volatile void* gBattleElimStorePtr = 0;
extern "C" volatile unsigned short gBattleElimElimOrder[12] = {0};
extern "C" volatile unsigned short gBattleElimEliminations = 0;
extern "C" volatile unsigned int gBattleElimWinnersAssigned = 0;
extern "C" volatile bool gBattleElimPersistentEliminated[12] = {false};

static const u8 MAX_BATTLE_PLAYERS = 12;

static bool IsValidPlayerId(u32 pid) {
    return pid < MAX_BATTLE_PLAYERS;
}

static bool IsFriendRoomType(u32 roomType) {
    return roomType == RKNet::ROOMTYPE_FROOM_HOST || roomType == RKNet::ROOMTYPE_FROOM_NONHOST || roomType == RKNet::ROOMTYPE_NONE;
}

static bool IsVsRoomType(u32 roomType) {
    return roomType == RKNet::ROOMTYPE_VS_REGIONAL || roomType == RKNet::ROOMTYPE_VS_WW;
}

static bool ShouldForceRegionalElimination(const RKNet::Controller& controller, const System* system) {
    return controller.roomType == RKNet::ROOMTYPE_BT_REGIONAL && system && system->netMgr.region == 0x0F;
}

static bool ShouldApplyBattleElimination(const RacedataScenario& scenario, bool requireBalloon = false) {
    const System* system = System::sInstance;
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    if (!system || !controller) return false;

    const u32 roomType = controller->roomType;
    if (IsVsRoomType(roomType)) return false;
    if (requireBalloon && scenario.settings.battleType != BATTLE_BALLOON) return false;

    if (ShouldForceRegionalElimination(*controller, system)) return true;

    const GameMode mode = scenario.settings.gamemode;
    if (!IsFriendRoomType(roomType)) return false;
    if (mode != MODE_PRIVATE_BATTLE && mode != MODE_PUBLIC_BATTLE) return false;
    if (system->IsContext(PULSAR_TEAM_BATTLE) != BATTLE_TEAMS_DISABLED) return false;

    return system->IsContext(PULSAR_ELIMINATION);
}

static bool IsAnyLocalPlayerEliminated() {
    if (!Racedata::sInstance) return false;
    const u8 localCount = Racedata::sInstance->menusScenario.localPlayerCount;
    for (u8 hud = 0; hud < localCount; ++hud) {
        const u32 pid = Racedata::sInstance->GetPlayerIdOfLocalPlayer(hud);
        if (IsValidPlayerId(pid) && gBattleElimPersistentEliminated[pid]) return true;
    }
    return false;
}

static bool IsAnyLocalPlayerFinished() {
    if (!Racedata::sInstance) return false;
    const u8 localCount = Racedata::sInstance->menusScenario.localPlayerCount;
    for (u8 hud = 0; hud < localCount; ++hud) {
        const u32 pid = Racedata::sInstance->GetPlayerIdOfLocalPlayer(hud);
        if (IsValidPlayerId(pid) && CtrlRaceTime::HasPlayerFinished(pid)) return true;
    }
    return false;
}

static void ClearBattleEliminationState(bool resetStorePtr) {
    gBattleElimFlag = 0;
    gBattleElimMakeInvisible = 0;
    gBattleElimRemaining = 0;
    gBattleElimEliminations = 0;
    gBattleElimWinnersAssigned = 0;
    if (resetStorePtr) gBattleElimStorePtr = 0;
    for (u8 pid = 0; pid < MAX_BATTLE_PLAYERS; ++pid) {
        gBattleElimElimOrder[pid] = 0;
        gBattleElimPersistentEliminated[pid] = false;
    }
}

static void RegisterElimination(u8 pid, bool makePersistent) {
    if (!IsValidPlayerId(pid)) return;
    if (makePersistent) gBattleElimPersistentEliminated[pid] = true;
    if (gBattleElimElimOrder[pid] == 0) {
        unsigned short elimNum = ++gBattleElimEliminations;
        gBattleElimElimOrder[pid] = elimNum;
    }
}

static bool IsPlayerDisconnected(u8 aid, u32 availableAids) {
    return (aid >= MAX_BATTLE_PLAYERS) || ((availableAids & (1 << aid)) == 0);
}

static void ResetBattleElimState() {
    if (!Racedata::sInstance) return;
    const RacedataScenario& scenario = Racedata::sInstance->menusScenario;
    if (!ShouldApplyBattleElimination(scenario, true)) return;

    ClearBattleEliminationState(false);
    if (System::sInstance) {
        gBattleElimRemaining = System::sInstance->nonTTGhostPlayersCount;
    }
}
static RaceLoadHook sBattleElimResetHook(ResetBattleElimState);

static void ResetBattleElimOnSectionLoad() {
    ClearBattleEliminationState(true);
}
static SectionLoadHook sBattleElimSectionResetHook(ResetBattleElimOnSectionLoad);

asmFunc ForceInvisible() {
    ASM(
        nofralloc;
        mflr r11;
        mr r12, r3;
        lis r9, gBattleElimStorePtr @ha;
        lwz r9, gBattleElimStorePtr @l(r9);
        cmpwi r9, 0;
        beq do_original;
        lwz r10, 0x48(r9);
        cmpwi r10, 0;
        bne do_original;
        bl loc_after;
        cmpw cr7, r0, r0;
        cmpw cr7, r0, r0;
        cmpw cr7, r0, r0;
        loc_after : mflr r3;
        lfs f0, 0(r3);
        mtlr r11;
        blr;
        do_original : lfs f0, 0(r12);
        mtlr r11;
        blr;)
}

static void BattleElimRemainingUpdate() {
    // Per-frame update. Counts active players and sets elimination flags.
    if (!Racedata::sInstance) {
        gBattleElimFlag = 0;
        return;
    }

    const RacedataScenario& scenario = Racedata::sInstance->menusScenario;
    if (!ShouldApplyBattleElimination(scenario)) {
        gBattleElimFlag = 0;
        return;
    }

    System* system = System::sInstance;
    Raceinfo* raceinfo = Raceinfo::sInstance;
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    if (!system || !raceinfo || !controller) return;

    const u8 total = system->nonTTGhostPlayersCount;
    if (total <= 1) return;

    const RKNet::ControllerSub& sub = controller->subs[controller->currentSub];
    const u32 availableAids = sub.availableAids;

    u32 activeNotFinished = 0;
    u32 finishedCount = 0;

    for (u8 pid = 0; pid < total && pid < MAX_BATTLE_PLAYERS; ++pid) {
        if (gBattleElimPersistentEliminated[pid]) {
            RegisterElimination(pid, false);
            continue;
        }

        const bool playerFinished = CtrlRaceTime::HasPlayerFinished(pid);
        const u8 aid = controller->aidsBelongingToPlayerIds[pid];
        const bool disconnected = IsPlayerDisconnected(aid, availableAids);

        if (playerFinished) {
            ++finishedCount;
            RegisterElimination(pid, true);
        } else if (disconnected) {
            RegisterElimination(pid, true);
        } else {
            ++activeNotFinished;
        }
    }

    gBattleElimRemaining = activeNotFinished;

    if (raceinfo->players) {
        for (u8 pid = 0; pid < total && pid < MAX_BATTLE_PLAYERS; ++pid) {
            RaceinfoPlayer* player = raceinfo->players[pid];
            if (!player) continue;
            if (!Raceinfo::sInstance->IsAtLeastStage(RACESTAGE_RACE)) player->battleScore = 3;
        }
    }

    const bool isSpectator = scenario.settings.gametype == GAMETYPE_ONLINE_SPECTATOR;
    if ((activeNotFinished <= 1 && finishedCount > 0) || isSpectator) {
        gBattleElimFlag = 1;
    } else {
        gBattleElimFlag = 0;
    }
}
static RaceFrameHook sBattleElimRemainingHook(BattleElimRemainingUpdate);

asmFunc OnBattleRespawn() {
    ASM(
        lis r12, gBattleElimFlag @ha;
        lwz r11, gBattleElimFlag @l(r12);
        cmpwi r11, 0;
        bne alreadySet;
        li r11, 1;
        stw r11, gBattleElimFlag @l(r12);
        alreadySet :;
        lha r4, 0x02d6(r31);
        blr;)
}

asmFunc ForceTimerOnStore() {
    ASM(
        lis r12, gBattleElimFlag @ha;
        lwz r11, gBattleElimFlag @l(r12);
        cmpwi r11, 0;
        beq original;
        li r11, 1;
        stb r11, 0x40(r29);
        li r0, 0;
        stw r0, 0x48(r29);
        lis r12, gBattleElimStorePtr @ha;
        stw r29, gBattleElimStorePtr @l(r12);
        lis r12, gBattleElimFlag @ha;
        li r10, 0;
        stw r10, gBattleElimFlag @l(r12);
        blr;
        original : stw r0, 0x48(r29);
        lis r12, gBattleElimStorePtr @ha;
        stw r29, gBattleElimStorePtr @l(r12);
        blr;)
}

asmFunc ForceBalloonBattle() {
    ASM(
        oris r0, r0, 0x8000;
        xoris r0, r0, 0;
        stw r0, 0x8(r1);
        lwz r3, 0x0(r31);)
}

asmFunc GetFanfare() {
    ASM(
        nofralloc;
        lwzx r3, r3, r0;
        cmpwi r3, 0x6b;
        beq - UnusedFanFareID;
        li r3, 0x6D;
        b end;
        UnusedFanFareID :;
        li r3, 0x6f;
        end : blr;)
}

asmFunc GetFanfareKO() {
    ASM(
        nofralloc;
        lwzx r3, r3, r0;
        cmpwi r3, 0x68;
        beq - UnusedFanFareID;
        b end;
        UnusedFanFareID :;
        li r3, 0x6f;
        end : blr;)
}

kmRuntimeUse(0x80579C1C);  // OnBattleRespawn [ZPL]
kmRuntimeUse(0x80535C7C);  // ForceTimerOnStore [ZPL]
kmRuntimeUse(0x806619AC);  // ForceBalloonBattle [Ro]
kmRuntimeUse(0x8058CB7C);  // ForceInvisible [Xer, edited by ZPL]
kmRuntimeUse(0x805348E8);  // Drive after finish [Supastarrio]
kmRuntimeUse(0x80534880);
kmRuntimeUse(0x80799CAC);  // Drive thru items [Sponge]
kmRuntimeUse(0x807123e8);  // GetFanfare [Zeraora]
static void ApplyDefaultBattleElimPatches() {
    kmRuntimeWrite32A(0x80579C1C, 0xa89f02d6);
    kmRuntimeWrite32A(0x80535C7C, 0x901d0048);
    kmRuntimeWrite32A(0x806619AC, 0x807f0000);
    kmRuntimeWrite32A(0x8058CB7C, 0xc0030000);
    kmRuntimeWrite32A(0x805348E8, 0x2C00FFFF);
    kmRuntimeWrite32A(0x80534880, 0x2C050000);
    kmRuntimeWrite32A(0x80799CAC, 0x9421ffd0);
    kmRuntimeWrite32A(0x807123e8, 0x7c63002e);
}

void BattleElim() {
    ApplyDefaultBattleElimPatches();

    System* system = System::sInstance;
    if (!Racedata::sInstance) return;

    const RacedataScenario& scenario = Racedata::sInstance->menusScenario;
    const bool eliminationActive = ShouldApplyBattleElimination(scenario, true);

    if (eliminationActive) {
        // When elimination is active, redirect calls to our ASM stubs
        // and adjust a few behaviors (driving after finish, item behavior).
        kmRuntimeCallA(0x80579C1C, OnBattleRespawn);
        kmRuntimeCallA(0x80535C7C, ForceTimerOnStore);
        kmRuntimeCallA(0x806619AC, ForceBalloonBattle);
        kmRuntimeCallA(0x8058CB7C, ForceInvisible);
        kmRuntimeCallA(0x807123e8, GetFanfare);
        kmRuntimeWrite32A(0x805348E8, 0x2C000000);
        kmRuntimeWrite32A(0x80534880, 0x2C05FFFF);
        kmRuntimeWrite32A(0x80799CAC, 0x9421ffd0);
        if (IsAnyLocalPlayerEliminated() && IsAnyLocalPlayerFinished()) {
            // If a local player is both eliminated and finished, tweak the
            // drive-through-items behavior to disable pickups.
            kmRuntimeWrite32A(0x80799CAC, 0x4e800020);
        }
    }

    if (system && system->IsContext(PULSAR_MODE_LAPKO)) {
        kmRuntimeCallA(0x807123e8, GetFanfareKO);
    }
}
static PageLoadHook BattleElimHook(BattleElim);

// Fix Balloon Stealing [Gaberboo]
kmWrite32(0x80538a28, 0x38000002);
kmWrite32(0x8053cec8, 0x38000002);

// Convert OnMoveHit to OnRemoveHit [ZPL]
kmWrite32(0x8053b618, 0x38800002);
kmWrite32(0x80538a74, 0x60000000);

kmRuntimeUse(0x80532BCC);  // Battle Time Duration [Ro]
void BattleTimer() {
    kmRuntimeWrite32A(0x80532BCC, 0x380000B4);

    if (!Racedata::sInstance) return;
    const RacedataScenario& scenario = Racedata::sInstance->menusScenario;
    if (!ShouldApplyBattleElimination(scenario)) return;

    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    if (!controller) return;

    const RKNet::ControllerSub& sub = controller->subs[controller->currentSub];
    u32 durationInst = 0x380000B4;
    if (sub.playerCount >= 10) {
        durationInst = 0x3800012C;
    } else if (sub.playerCount >= 7) {
        durationInst = 0x380000F0;
    } else if (sub.playerCount > 0 && sub.playerCount <= 3) {
        durationInst = 0x38000078;
    }

    kmRuntimeWrite32A(0x80532BCC, durationInst);
}
static PageLoadHook BattleTimerHook(BattleTimer);

}  // namespace BattleElim
}  // namespace Pulsar