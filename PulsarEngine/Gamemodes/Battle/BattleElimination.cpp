#include <RetroRewind.hpp>
#include <Gamemodes/Battle/BattleElimination.hpp>
#include <Patching/RuntimeChoice.hpp>
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
    const System* system = System::sInstance;
    return system->IsContext(PULSAR_ELIMINATION) && system->IsContext(PULSAR_FFA);
}

static void SetInitialBattleScores(RacedataScenario& scenario, u16 startScore) {
    Raceinfo* raceinfo = Raceinfo::sInstance;
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
    Raceinfo* raceinfo = Raceinfo::sInstance;
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
        if (eliminatedCount >= (playerCount - 1)) {
            for (u8 idx = 0; idx < playerCount && idx < MAX_BATTLE_PLAYERS; ++idx) {
                raceinfo->EndPlayerRace(idx);
                raceinfo->CheckEndRaceOnline(idx);
            }
        }
    }
}
static RaceFrameHook BattleElimTimerHook(SetTimerToZeroWhenAllPlayersEliminated);

static u32 sBattleEliminationActive = 0;
static u32 sFanfareMode = 0;
static u32 sBattleTimeDuration = 180;

static u32 GetFanfareMode() {
    System* system = System::sInstance;
    if (system == nullptr) return 0;
    if (ShouldApplyBattleElimination()) return 1;
    if (system->IsContext(PULSAR_MODE_LAPKO)) return 2;
    return 0;
}

static u32 GetBattleTimeDuration() {
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    if (controller == nullptr) return 180;
    const RKNet::ControllerSub& sub = controller->subs[controller->currentSub];
    if (ShouldApplyBattleElimination()) {
        if (sub.playerCount == 12 || sub.playerCount == 11 || sub.playerCount == 10) {
            return 300;
        } else if (sub.playerCount == 9 || sub.playerCount == 8 || sub.playerCount == 7) {
            return 240;
        } else if (sub.playerCount == 6 || sub.playerCount == 5 || sub.playerCount == 4) {
            return 180;
        } else if (sub.playerCount == 3 || sub.playerCount == 2 || sub.playerCount == 1) {
            return 120;
        }
    }
    return 180;
}

static void UpdateBattleEliminationPatchState() {
    sBattleEliminationActive = ShouldApplyBattleElimination() ? 1 : 0;
    sFanfareMode = GetFanfareMode();
    sBattleTimeDuration = GetBattleTimeDuration();
}
static FrameLoadHook UpdateBattleEliminationPatchStateHook(UpdateBattleEliminationPatchState);

asmFunc ForceBalloonBattleDispatch() {
    ASM(
        nofralloc;
        stwu r1, -0x20(r1);
        stw r12, 0x8(r1);
        mfcr r12;
        stw r12, 0xC(r1);

        lis r12, sBattleEliminationActive @ha;
        lwz r12, sBattleEliminationActive @l(r12);
        cmpwi r12, 0;
        bne useElimination;

        RuntimeChoice_RestoreScratchAndCR();
        lwz r3, 0(r31);
        blr;

        useElimination:;
        RuntimeChoice_RestoreScratchAndCR();
        oris r0, r0, 0x8000;
        xoris r0, r0, 0;
        stw r0, 0x8(r1);
        lwz r3, 0x0(r31);
        blr;)
}
kmCall(0x806619AC, ForceBalloonBattleDispatch);

asmFunc GetFanfareDispatch() {
    ASM(
        nofralloc;
        stwu r1, -0x20(r1);
        stw r11, 0x8(r1);
        stw r12, 0xC(r1);
        mfcr r12;
        stw r12, 0x10(r1);

        lis r11, sFanfareMode @ha;
        lwz r11, sFanfareMode @l(r11);
        cmpwi r11, 1;
        beq useElimination;
        cmpwi r11, 2;
        beq useKo;

        lwz r12, 0x10(r1);
        mtcrf 0xff, r12;
        lwz r11, 0x8(r1);
        lwz r12, 0xC(r1);
        addi r1, r1, 0x20;
        lwzx r3, r3, r0;
        blr;

        useElimination:;
        lwz r12, 0x10(r1);
        mtcrf 0xff, r12;
        lwz r11, 0x8(r1);
        lwz r12, 0xC(r1);
        addi r1, r1, 0x20;
        lwzx r3, r3, r0;
        cmpwi r3, 0x6b;
        beq unusedElimFanfare;
        li r3, 0x6D;
        blr;
        unusedElimFanfare:;
        li r3, 0x6f;
        blr;

        useKo:;
        lwz r12, 0x10(r1);
        mtcrf 0xff, r12;
        lwz r11, 0x8(r1);
        lwz r12, 0xC(r1);
        addi r1, r1, 0x20;
        lwzx r3, r3, r0;
        cmpwi r3, 0x68;
        bne endKo;
        li r3, 0x6f;
        endKo:;
        blr;)
}
kmCall(0x807123e8, GetFanfareDispatch);

// Fix Balloon Stealing [Gaberboo]
kmWrite32(0x80538a28, 0x38000002);
kmWrite32(0x8053cec8, 0x38000002);

// Convert OnMoveHit to OnRemoveHit [ZPL]
kmWrite32(0x8053b618, 0x38800002);
kmWrite32(0x80538a74, 0x60000000);

RuntimeChoice_CachedInstruction(LoadBattleTimeDuration, 0x80532BCC, r0, sBattleTimeDuration);

}  // namespace BattleElim
}  // namespace Pulsar
