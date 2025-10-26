#include <RetroRewind.hpp>
#include <runtimeWrite.hpp>
#include <MarioKartWii/Race/Racedata.hpp>
#include <MarioKartWii/Kart/KartManager.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <MarioKartWii/UI/Ctrl/CtrlRace/CtrlRaceTime.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>

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

int IsAnyLocalPlayerEliminated() {
    // Returns true if any local split-screen player has been eliminated.
    if (!Racedata::sInstance) return false;
    const u8 localCount = Racedata::sInstance->menusScenario.localPlayerCount;
    for (u8 hud = 0; hud < localCount; ++hud) {
        const u32 pid = Racedata::sInstance->GetPlayerIdOfLocalPlayer(hud);
        if (pid < 12 && gBattleElimPersistentEliminated[pid]) return true;
    }
    return false;
};

int IsAnyLocalPlayerFinished() {
    // Returns true if any local split-screen player has finished the match.
    if (!Racedata::sInstance) return false;
    const u8 localCount = Racedata::sInstance->menusScenario.localPlayerCount;
    for (u8 hud = 0; hud < localCount; ++hud) {
        const u32 pid = Racedata::sInstance->GetPlayerIdOfLocalPlayer(hud);
        if (pid < 12 && CtrlRaceTime::HasPlayerFinished(pid)) return true;
    }
    return false;
};

static void ResetBattleElimState() {
    // Called on race load. If the current context is a Balloon Battle
    // session with elimination enabled (or RR BT regional override),
    // reset all elimination-related runtime state.
    const RacedataScenario& scenario = Racedata::sInstance->menusScenario;
    const GameMode mode = scenario.settings.gamemode;
    bool isElim = ELIMINATION_DISABLED;
    if ((RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_HOST || RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_NONHOST || RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_NONE) && (mode == MODE_PRIVATE_BATTLE || mode == MODE_PUBLIC_BATTLE) && System::sInstance->IsContext(PULSAR_TEAM_BATTLE) == BATTLE_TEAMS_DISABLED) {
        isElim = Pulsar::System::sInstance->IsContext(PULSAR_ELIMINATION) ? ELIMINATION_ENABLED : ELIMINATION_DISABLED;
    }
    if ((isElim || (RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_BT_REGIONAL && System::sInstance->netMgr.region == 0x0F)) && RKNet::Controller::sInstance->roomType != RKNet::ROOMTYPE_VS_REGIONAL && RKNet::Controller::sInstance->roomType != RKNet::ROOMTYPE_VS_WW) {
        gBattleElimFlag = 0;
        gBattleElimMakeInvisible = 0;
        u8 playerCount = 0;
        if (System::sInstance) {
            gBattleElimRemaining = System::sInstance->nonTTGhostPlayersCount;
            playerCount = System::sInstance->nonTTGhostPlayersCount;
        } else {
            gBattleElimRemaining = 0;
        }
        (void)playerCount;
        gBattleElimEliminations = 0;
        gBattleElimWinnersAssigned = 0;
        for (int i = 0; i < 12; ++i) {
            gBattleElimElimOrder[i] = 0;
            gBattleElimPersistentEliminated[i] = false;
        }
    } else {
        return;
    }
}
static RaceLoadHook sBattleElimResetHook(ResetBattleElimState);

static void ResetBattleElimOnSectionLoad() {
    gBattleElimFlag = 0;
    gBattleElimMakeInvisible = 0;
    gBattleElimRemaining = 0;
    gBattleElimEliminations = 0;
    gBattleElimWinnersAssigned = 0;
    gBattleElimStorePtr = 0;
    for (int i = 0; i < 12; ++i) {
        gBattleElimElimOrder[i] = 0;
        gBattleElimPersistentEliminated[i] = false;
    }
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
    const RacedataScenario& scenario = Racedata::sInstance->menusScenario;
    const GameMode mode = scenario.settings.gamemode;
    Racedata* racedata = Racedata::sInstance;
    bool isElim = ELIMINATION_DISABLED;
    if ((RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_HOST || RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_NONHOST || RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_NONE) && (mode == MODE_PRIVATE_BATTLE || mode == MODE_PUBLIC_BATTLE) && System::sInstance->IsContext(PULSAR_TEAM_BATTLE) == BATTLE_TEAMS_DISABLED) {
        isElim = Pulsar::System::sInstance->IsContext(PULSAR_ELIMINATION) ? ELIMINATION_ENABLED : ELIMINATION_DISABLED;
    }
    if ((isElim || (RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_BT_REGIONAL && System::sInstance->netMgr.region == 0x0F)) && RKNet::Controller::sInstance->roomType != RKNet::ROOMTYPE_VS_REGIONAL && RKNet::Controller::sInstance->roomType != RKNet::ROOMTYPE_VS_WW) {
        const System* sys = System::sInstance;
        Raceinfo* ri = Raceinfo::sInstance;
        if (!sys || !ri) return;
        const u8 total = sys->nonTTGhostPlayersCount;
        if (total <= 1) return;

        const RKNet::Controller* controller = RKNet::Controller::sInstance;
        const RKNet::ControllerSub& sub = controller->subs[controller->currentSub];
        const u32 availableAids = sub.availableAids;
        u32 activeNotFinished = 0;  // Still battling.
        u32 actualFinished = 0;  // Reached finish (treated as eliminated here).
        u32 disconnectedCount = 0;  // Offline/DC (also eliminated).

        for (u8 pid = 0; pid < total; ++pid) {
            const bool playerFinished = CtrlRaceTime::HasPlayerFinished(pid);
            const u8 aid = controller->aidsBelongingToPlayerIds[pid];
            const bool disconnected = (aid >= 12) || ((availableAids & (1 << aid)) == 0);
            if (gBattleElimPersistentEliminated[pid]) {
                // Already marked eliminated earlier.
                ++disconnectedCount;
                if (gBattleElimElimOrder[pid] == 0) {
                    unsigned short elimNum = ++gBattleElimEliminations;
                    gBattleElimElimOrder[pid] = elimNum;
                }
            } else if (playerFinished) {
                // Consider finishing as being out for Elimination.
                ++actualFinished;
                if (gBattleElimElimOrder[pid] == 0) {
                    unsigned short elimNum = ++gBattleElimEliminations;
                    gBattleElimElimOrder[pid] = elimNum;
                    gBattleElimPersistentEliminated[pid] = true;
                }
            } else if (disconnected) {
                // Disconnected players are eliminated and assigned an order.
                ++disconnectedCount;
                if (gBattleElimElimOrder[pid] == 0) {
                    unsigned short elimNum = ++gBattleElimEliminations;
                    gBattleElimElimOrder[pid] = elimNum;
                    gBattleElimPersistentEliminated[pid] = true;
                }
            } else {
                // Still in the match.
                ++activeNotFinished;
            }
        }

        gBattleElimRemaining = activeNotFinished;

        if (ri && ri->players) {
            for (u8 pid = 0; pid < total; ++pid) {
                RaceinfoPlayer* pl = ri->players[pid];
                if (!pl) continue;
                unsigned short elimOrd = gBattleElimElimOrder[pid];
                // Before the race starts, initialize balloon count to 3.
                if (!Raceinfo::sInstance->IsAtLeastStage(RACESTAGE_RACE)) pl->battleScore = 3;
            }
        }

        // Raise the flag once only one active player remains and at least one
        // player has actually finished (or if spectating).
        if (activeNotFinished <= 1 && actualFinished > 0 || racedata->menusScenario.settings.gametype == GAMETYPE_ONLINE_SPECTATOR) {
            gBattleElimFlag = 1;
        } else {
            gBattleElimFlag = 0;
        }
    } else {
        gBattleElimFlag = 0;
        return;
    }
}
static RaceFrameHook sBattleElimRemainingHook(BattleElimRemainingUpdate);

asmFunc OnBattleRespawn() {
    // When a player respawns in battle, ensure the elimination flag is set
    // at least once so downstream logic can react.
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
    // If elimination is active, force the store/timer state and capture the
    // store pointer for the invisible logic; then clear the flag.
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
    // Ensure the balloon battle path is taken in code that branches on mode.
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
void BattleElim() {
    // First, set default patches (no-op behavior) at known addresses.
    kmRuntimeWrite32A(0x80579C1C, 0xa89f02d6);
    kmRuntimeWrite32A(0x80535C7C, 0x901d0048);
    kmRuntimeWrite32A(0x806619AC, 0x807f0000);
    kmRuntimeWrite32A(0x8058CB7C, 0xc0030000);
    kmRuntimeWrite32A(0x805348E8, 0x2C00FFFF);
    kmRuntimeWrite32A(0x80534880, 0x2C050000);
    kmRuntimeWrite32A(0x80799CAC, 0x9421ffd0);
    kmRuntimeWrite32A(0x807123e8, 0x7c63002e);
    const RacedataScenario& scenario = Racedata::sInstance->menusScenario;
    const GameMode mode = scenario.settings.gamemode;
    System* system = System::sInstance;
    bool isElim = ELIMINATION_DISABLED;
    if ((RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_HOST || RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_NONHOST || RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_NONE) && (mode == MODE_PRIVATE_BATTLE || mode == MODE_PUBLIC_BATTLE) && System::sInstance->IsContext(PULSAR_TEAM_BATTLE) == BATTLE_TEAMS_DISABLED && scenario.settings.battleType == BATTLE_BALLOON) {
        isElim = Pulsar::System::sInstance->IsContext(PULSAR_ELIMINATION) ? ELIMINATION_ENABLED : ELIMINATION_DISABLED;
    }
    if ((isElim || (RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_BT_REGIONAL && System::sInstance->netMgr.region == 0x0F)) && RKNet::Controller::sInstance->roomType != RKNet::ROOMTYPE_VS_REGIONAL && RKNet::Controller::sInstance->roomType != RKNet::ROOMTYPE_VS_WW) {
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
    if (system->IsContext(PULSAR_MODE_LAPKO)) {
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
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    const RKNet::ControllerSub& sub = controller->subs[controller->currentSub];
    kmRuntimeWrite32A(0x80532BCC, 0x380000B4);
    const RacedataScenario& scenario = Racedata::sInstance->menusScenario;
    const GameMode mode = scenario.settings.gamemode;
    bool isElim = ELIMINATION_DISABLED;
    if ((RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_HOST || RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_NONHOST || RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_NONE) && (mode == MODE_PRIVATE_BATTLE || mode == MODE_PUBLIC_BATTLE) && System::sInstance->IsContext(PULSAR_TEAM_BATTLE) == BATTLE_TEAMS_DISABLED) {
        isElim = Pulsar::System::sInstance->IsContext(PULSAR_ELIMINATION) ? ELIMINATION_ENABLED : ELIMINATION_DISABLED;
    }
    if ((isElim || (RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_BT_REGIONAL && System::sInstance->netMgr.region == 0x0F)) && RKNet::Controller::sInstance->roomType != RKNet::ROOMTYPE_VS_REGIONAL && RKNet::Controller::sInstance->roomType != RKNet::ROOMTYPE_VS_WW) {
        if (sub.playerCount == 12 || sub.playerCount == 11 || sub.playerCount == 10) {
            kmRuntimeWrite32A(0x80532BCC, 0x3800012C);
        } else if (sub.playerCount == 9 || sub.playerCount == 8 || sub.playerCount == 7) {
            kmRuntimeWrite32A(0x80532BCC, 0x380000F0);
        } else if (sub.playerCount == 6 || sub.playerCount == 5 || sub.playerCount == 4) {
            kmRuntimeWrite32A(0x80532BCC, 0x380000B4);
        } else if (sub.playerCount == 3 || sub.playerCount == 2 || sub.playerCount == 1) {
            kmRuntimeWrite32A(0x80532BCC, 0x38000078);
        }
    }
}
static PageLoadHook BattleTimerHook(BattleTimer);

}  // namespace BattleElim
}  // namespace Pulsar