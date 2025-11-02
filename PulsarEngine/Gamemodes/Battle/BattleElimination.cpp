#include <RetroRewind.hpp>
#include <Gamemodes/Battle/BattleElimination.hpp>
#include <runtimeWrite.hpp>
#include <MarioKartWii/Race/Racedata.hpp>
#include <MarioKartWii/Kart/KartManager.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <MarioKartWii/UI/Ctrl/CtrlRace/CtrlRaceTime.hpp>
#include <MarioKartWii/System/Timer.hpp>
#include <Gamemodes/LapKO/LapKOMgr.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/Race/RaceData.hpp>

namespace Pulsar {
namespace BattleElim {

static const u8 MAX_BATTLE_PLAYERS = 12;
static const u16 kEliminationDisplayDuration = 180;

struct EliminationDisplayState {
    u8 recentEliminations[4];
    u8 recentCount;
    u16 timer;
};

static EliminationDisplayState sEliminationDisplay = {{0xFF, 0xFF, 0xFF, 0xFF}, 0, 0};
static bool sEliminationRecorded[MAX_BATTLE_PLAYERS];

static void ResetDisplayState() {
    sEliminationDisplay.recentCount = 0;
    sEliminationDisplay.timer = 0;
    sEliminationDisplay.recentEliminations[0] = 0xFF;
    sEliminationDisplay.recentEliminations[1] = 0xFF;
    sEliminationDisplay.recentEliminations[2] = 0xFF;
    sEliminationDisplay.recentEliminations[3] = 0xFF;
}

static void ResetEliminationTracking() {
    ResetDisplayState();
    for (u8 idx = 0; idx < MAX_BATTLE_PLAYERS; ++idx) {
        sEliminationRecorded[idx] = false;
    }
}

static void AppendElimination(u8 playerId) {
    if (sEliminationDisplay.recentCount >= 4) {
        for (u8 i = 1; i < 4; ++i) {
            sEliminationDisplay.recentEliminations[i - 1] = sEliminationDisplay.recentEliminations[i];
        }
        sEliminationDisplay.recentCount = 3;
    }
    sEliminationDisplay.recentEliminations[sEliminationDisplay.recentCount++] = playerId;
    sEliminationDisplay.timer = kEliminationDisplayDuration;
}

static void TickEliminationDisplayInternal() {
    if (sEliminationDisplay.timer == 0) return;
    --sEliminationDisplay.timer;
    if (sEliminationDisplay.timer == 0) {
        ResetDisplayState();
    }
}

static bool IsValidPlayerId(u32 pid) {
    return pid < MAX_BATTLE_PLAYERS;
}

bool ShouldApplyBattleElimination() {
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    const System* system = System::sInstance;
    const Racedata* racedata = Racedata::sInstance;
    const RacedataScenario& scenario = racedata->menusScenario;
    const GameMode mode = scenario.settings.gamemode;
    bool isElim = ELIMINATION_DISABLED;
    if ((controller->roomType == RKNet::ROOMTYPE_FROOM_HOST || controller->roomType == RKNet::ROOMTYPE_FROOM_NONHOST || controller->roomType == RKNet::ROOMTYPE_NONE) && (mode == MODE_PRIVATE_BATTLE || mode == MODE_PUBLIC_BATTLE || mode == MODE_BATTLE) && system->IsContext(PULSAR_TEAM_BATTLE) == BATTLE_TEAMS_DISABLED) {
        isElim = system->IsContext(PULSAR_ELIMINATION) ? ELIMINATION_ENABLED : ELIMINATION_DISABLED;
    }
    if ((isElim || (controller->roomType == RKNet::ROOMTYPE_BT_REGIONAL && system->netMgr.region == 0x0F)) && controller->roomType != RKNet::ROOMTYPE_VS_REGIONAL && controller->roomType != RKNet::ROOMTYPE_VS_WW && scenario.settings.battleType == BATTLE_BALLOON) {
        return true;
    }
    return false;
}

static void SetInitialBattleScores(RacedataScenario& scenario, u16 startScore) {
    Raceinfo* raceinfo = Raceinfo::sInstance;
    Racedata& racedata = *Racedata::sInstance;
    const u8 playerCount = Pulsar::System::sInstance->nonTTGhostPlayersCount;
    if (!ShouldApplyBattleElimination()) {
        ResetEliminationTracking();
        return;
    }
    const bool atRaceStage = raceinfo->IsAtLeastStage(RACESTAGE_RACE);
    if (!atRaceStage) {
        ResetEliminationTracking();
    }
    for (u8 idx = 0; idx < playerCount && idx < MAX_BATTLE_PLAYERS; ++idx) {
        RaceinfoPlayer* player = raceinfo->players[idx];
        if (!atRaceStage) player->battleScore = 3;
    }
}
static RaceFrameHook BattleElimInitScoresHook(SetInitialBattleScores);

static void SetVanishOnElim(u8 playerIdx) {
    Raceinfo* raceinfo = Raceinfo::sInstance;
    const u8 playerCount = Pulsar::System::sInstance->nonTTGhostPlayersCount;
    if (!ShouldApplyBattleElimination() || !raceinfo->IsAtLeastStage(RACESTAGE_RACE)) {
        ResetEliminationTracking();
        TickEliminationDisplayInternal();
        return;
    }
    for (u8 idx = 0; idx < playerCount && idx < MAX_BATTLE_PLAYERS; ++idx) {
        RaceinfoPlayer* player = raceinfo->players[idx];
        if (player->battleScore == 0) {
            if (!sEliminationRecorded[idx]) {
                AppendElimination(idx);
                sEliminationRecorded[idx] = true;
            }
            player->Vanish();
            player->stateFlags &= ~0x20;
            player->stateFlags |= 0x10;
        } else {
            sEliminationRecorded[idx] = false;
        }
    }
    TickEliminationDisplayInternal();
}
static RaceFrameHook BattleElimVanishHook(SetVanishOnElim);

u16 GetEliminationDisplayTimer() {
    return sEliminationDisplay.timer;
}

u8 GetRecentEliminationCount() {
    return sEliminationDisplay.recentCount;
}

u8 GetRecentEliminationId(u8 index) {
    if (index >= sEliminationDisplay.recentCount || index >= 4) return 0xFF;
    return sEliminationDisplay.recentEliminations[index];
}

static void ApplySpectatorToEliminatedPlayersOnly(LapKO::Mgr* lapKOMgr) {
    System* system = System::sInstance;
    Raceinfo* raceinfo = Raceinfo::sInstance;
    Racedata& racedata = *Racedata::sInstance;
    const u8 playerCount = Pulsar::System::sInstance->nonTTGhostPlayersCount;
    const u8 localPlayerCount = Racedata::sInstance->menusScenario.localPlayerCount;
    const RacedataScenario& scenario = Racedata::sInstance->menusScenario;
    const GameMode mode = scenario.settings.gamemode;
    if (!raceinfo->IsAtLeastStage(RACESTAGE_RACE)) return;
    if (!ShouldApplyBattleElimination()) return;
    if (lapKOMgr == nullptr) return;
    for (u8 localIdx = 0; localIdx < localPlayerCount; ++localIdx) {
        const u32 pid = Racedata::sInstance->GetPlayerIdOfLocalPlayer(localIdx);
        if (!IsValidPlayerId(pid)) continue;
        if (pid >= playerCount) continue;
        RaceinfoPlayer* localPlayer = raceinfo->players[pid];
        if (localPlayer->battleScore == 0) {
            lapKOMgr->isSpectating = true;
            if (mode != MODE_BATTLE) {
                lapKOMgr->UpdateSpectatorInputs(*raceinfo);
                lapKOMgr->MaintainSpectatorView(*raceinfo);
            }
        }
    }
}
static RaceFrameHook BattleElimSpectateHook(ApplySpectatorToEliminatedPlayersOnly);

static void SetTimerToZeroWhenAllPlayersEliminated() {
    Raceinfo* raceinfo = Raceinfo::sInstance;
    Racedata& racedata = *Racedata::sInstance;
    const RacedataScenario& scenario = racedata.menusScenario;
    const GameMode mode = scenario.settings.gamemode;
    const u8 playerCount = Pulsar::System::sInstance->nonTTGhostPlayersCount;
    const u8 localPlayerCount = scenario.localPlayerCount;
    if (!raceinfo->IsAtLeastStage(RACESTAGE_RACE)) return;
    if (!ShouldApplyBattleElimination()) return;
    u32 eliminatedCount = 0;
    for (u8 playerIdx = 0; playerIdx < playerCount && playerIdx < MAX_BATTLE_PLAYERS; ++playerIdx) {
        RaceinfoPlayer* player = raceinfo->players[playerIdx];
        if (!player) continue;
        if (player->battleScore == 0) ++eliminatedCount;
    }
    if (mode == MODE_BATTLE) {
        bool allLocalEliminated = true;
        if (localPlayerCount == 0) {
            allLocalEliminated = false;
        } else {
            for (u8 localIdx = 0; localIdx < playerCount && localIdx < localPlayerCount; ++localIdx) {
                const u32 pid = Racedata::sInstance->GetPlayerIdOfLocalPlayer(localIdx);
                if (!IsValidPlayerId(pid)) {
                    allLocalEliminated = false;
                    break;
                }
                RaceinfoPlayer* localPlayer = raceinfo->players[pid];
                if (!localPlayer || localPlayer->battleScore != 0) {
                    allLocalEliminated = false;
                    break;
                }
            }
        }
        if (allLocalEliminated || eliminatedCount >= (playerCount - 1)) {
            RaceTimerMgr* timerMgr = raceinfo->timerMgr;
            for (u8 idx = 0; idx < playerCount && idx < MAX_BATTLE_PLAYERS; ++idx) {
                Timer& timer = timerMgr->timers[idx];
                timer.minutes = 0;
                timer.seconds = 0;
                timer.milliseconds = 0;
                raceinfo->EndPlayerRace(idx);
                raceinfo->CheckEndRaceOnline(idx);
            }
        }
    } else {
        RaceTimerMgr* timerMgr = raceinfo->timerMgr;
        if (eliminatedCount >= (playerCount - 1)) {
            for (u8 idx = 0; idx < playerCount && idx < MAX_BATTLE_PLAYERS; ++idx) {
                raceinfo->EndPlayerRace(idx);
                raceinfo->CheckEndRaceOnline(idx);
            }
        }
    }
}
static RaceFrameHook BattleElimTimerHook(SetTimerToZeroWhenAllPlayersEliminated);

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

kmRuntimeUse(0x806619AC);  // ForceBalloonBattle [Ro]
kmRuntimeUse(0x807123e8);  // GetFanfare [Zeraora]
void BattleElim() {
    kmRuntimeWrite32A(0x806619AC, 0x807f0000);
    kmRuntimeWrite32A(0x807123e8, 0x7c63002e);
    System* system = System::sInstance;
    if (!Racedata::sInstance) return;
    RacedataScenario& scenario = Racedata::sInstance->menusScenario;
    const bool eliminationActive = ShouldApplyBattleElimination();
    if (eliminationActive) {
        kmRuntimeCallA(0x806619AC, ForceBalloonBattle);
        kmRuntimeCallA(0x807123e8, GetFanfare);
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
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    const RKNet::ControllerSub& sub = controller->subs[controller->currentSub];
    kmRuntimeWrite32A(0x80532BCC, 0x380000B4);
    const RacedataScenario& scenario = Racedata::sInstance->menusScenario;
    const GameMode mode = scenario.settings.gamemode;
    bool isElim = ELIMINATION_DISABLED;
    if ((RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_HOST || RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_NONHOST || RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_NONE) && (mode == MODE_PRIVATE_BATTLE || mode == MODE_PUBLIC_BATTLE) && System::sInstance->IsContext(PULSAR_TEAM_BATTLE) == BATTLE_TEAMS_DISABLED) {
        isElim = Pulsar::System::sInstance->IsContext(PULSAR_ELIMINATION) ? ELIMINATION_ENABLED : ELIMINATION_DISABLED;
    }
    if ((isElim || (RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_BT_REGIONAL && System::sInstance->netMgr.region == 0x0F)) && RKNet::Controller::sInstance->roomType != RKNet::ROOMTYPE_VS_REGIONAL && RKNet::Controller::sInstance->roomType != RKNet::ROOMTYPE_VS_WW && scenario.settings.battleType == BATTLE_BALLOON) {
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