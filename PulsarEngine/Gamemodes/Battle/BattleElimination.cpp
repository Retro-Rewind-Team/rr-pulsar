#include <RetroRewind.hpp>
#include <Gamemodes/Battle/BattleElimination.hpp>
#include <Gamemodes/EliminationDisplay.hpp>
#include <Gamemodes/Spectating.hpp>
#include <MarioKartWii/Race/Racedata.hpp>
#include <MarioKartWii/Kart/KartManager.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <MarioKartWii/UI/Ctrl/CtrlRace/CtrlRaceTime.hpp>
#include <MarioKartWii/System/Timer.hpp>
#include <Gamemodes/LapKO/LapKOMgr.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>

namespace Pulsar {
namespace BattleElim {

static const u8 MAX_BATTLE_PLAYERS = 12;

static bool IsValidPlayerId(u32 pid) {
    return pid < MAX_BATTLE_PLAYERS;
}

bool ShouldApplyBattleElimination() {
    const System *system = System::sInstance;
    return system->IsContext(PULSAR_ELIMINATION) && system->IsContext(PULSAR_FFA);
}

static void SetInitialBattleScores(RacedataScenario &scenario, u16 startScore) {
    Raceinfo *raceinfo = Raceinfo::sInstance;
    const u8 playerCount = Pulsar::System::sInstance->nonTTGhostPlayersCount;
    if (!ShouldApplyBattleElimination()) {
        EliminationDisplay::ResetBattleTracking();
        return;
    }
    const bool atRaceStage = raceinfo->IsAtLeastStage(RACESTAGE_RACE);
    if (!atRaceStage) {
        EliminationDisplay::Reset();
    }
    for (u8 idx = 0; idx < playerCount && idx < MAX_BATTLE_PLAYERS; ++idx) {
        RaceinfoPlayer *player = raceinfo->players[idx];
        if (!atRaceStage) player->battleScore = 3;
    }
}
static RaceFrameHook BattleElimInitScoresHook(SetInitialBattleScores);

static void SetVanishOnElim(u8 playerIdx) {
    Raceinfo *raceinfo = Raceinfo::sInstance;
    const u8 playerCount = Pulsar::System::sInstance->nonTTGhostPlayersCount;
    if (!ShouldApplyBattleElimination()) {
        EliminationDisplay::ResetBattleTracking();
        return;
    }
    if (!raceinfo->IsAtLeastStage(RACESTAGE_RACE)) {
        EliminationDisplay::Reset();
        return;
    }
    for (u8 idx = 0; idx < playerCount && idx < MAX_BATTLE_PLAYERS; ++idx) {
        RaceinfoPlayer *player = raceinfo->players[idx];
        if (player->battleScore == 0) {
            EliminationDisplay::TrackBattleElimination(idx, true);
            player->Vanish();
            player->stateFlags &= ~0x20;
            player->stateFlags |= 0x10;
        } else {
            EliminationDisplay::TrackBattleElimination(idx, false);
        }
    }
    EliminationDisplay::Tick();
}
static RaceFrameHook BattleElimVanishHook(SetVanishOnElim);

static void UpdateSpectating(LapKO::Mgr *lapKOMgr) {
    (void)lapKOMgr;
    if (!ShouldApplyBattleElimination()) return;
    Spectating::Update(*Raceinfo::sInstance);
}
static RaceFrameHook BattleElimSpectateHook(UpdateSpectating);

static void SetTimerToZeroWhenAllPlayersEliminated() {
    Raceinfo *raceinfo = Raceinfo::sInstance;
    Racedata &racedata = *Racedata::sInstance;
    const RacedataScenario &scenario = racedata.menusScenario;
    const GameMode mode = scenario.settings.gamemode;
    const u8 playerCount = Pulsar::System::sInstance->nonTTGhostPlayersCount;
    const u8 localPlayerCount = scenario.localPlayerCount;
    if (!raceinfo->IsAtLeastStage(RACESTAGE_RACE)) return;
    if (!ShouldApplyBattleElimination()) return;
    u32 eliminatedCount = 0;
    for (u8 playerIdx = 0; playerIdx < playerCount && playerIdx < MAX_BATTLE_PLAYERS; ++playerIdx) {
        RaceinfoPlayer *player = raceinfo->players[playerIdx];
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
                RaceinfoPlayer *localPlayer = raceinfo->players[pid];
                if (!localPlayer || localPlayer->battleScore != 0) {
                    allLocalEliminated = false;
                    break;
                }
            }
        }
        if (allLocalEliminated || eliminatedCount >= (playerCount - 1)) {
            RaceTimerMgr *timerMgr = raceinfo->timerMgr;
            for (u8 idx = 0; idx < playerCount && idx < MAX_BATTLE_PLAYERS; ++idx) {
                Timer &timer = timerMgr->timers[idx];
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

extern "C" u8 sForceBalloonBattle = false;
extern "C" u8 sBattleFanfareMode = 0;
extern "C" u16 sBattleDuration = 180;

asmFunc ForceBalloonBattle() {
    ASM(
        lis r3, sForceBalloonBattle @ha;
        lbz r3, sForceBalloonBattle @l(r3);
        cmpwi r3, 0;
        beq original;
        oris r0, r0, 0x8000;
        xoris r0, r0, 0;
        stw r0, 0x8(r1);
        original :;
        lwz r3, 0x0(r31);)
}
kmCall(0x806619AC, ForceBalloonBattle);

static bool IsGhostReplay() {
    const Racedata *racedata = Racedata::sInstance;
    if (racedata == nullptr || racedata->racesScenario.players[0].playerType != PLAYER_GHOST) return false;

    const SectionMgr *sectionMgr = SectionMgr::sInstance;
    if (sectionMgr == nullptr || sectionMgr->curSection == nullptr) return false;

    const SectionId sectionId = sectionMgr->curSection->sectionId;
    return sectionId == SECTION_TT_REPLAY ||
           (sectionId >= SECTION_WATCH_GHOST_FROM_CHANNEL && sectionId <= SECTION_WATCH_GHOST_FROM_MENU);
}

extern "C" u32 SelectRaceFanfare(u32 soundId) {
    // Check the active race here: a replay section alone does not prove the driver is a ghost.
    if (IsGhostReplay()) return SOUND_ID_BATTLE_WIN_RESULTS;
    if (sBattleFanfareMode == 1) return soundId == 0x6b ? 0x6f : 0x6d;
    if (sBattleFanfareMode == 2 && soundId == 0x68) return 0x6f;
    return soundId;
}

asmFunc LoadBattleFanfare() {
    ASM(
        nofralloc;
        lwzx r3, r3, r0;
        b SelectRaceFanfare;)
}
kmCall(0x807123e8, LoadBattleFanfare);

void BattleElim() {
    System *system = System::sInstance;
    Racedata *racedata = Racedata::sInstance;
    if (!racedata) return;

    const bool eliminationActive = ShouldApplyBattleElimination();
    sForceBalloonBattle = eliminationActive;
    sBattleFanfareMode = eliminationActive ? 1 : system->IsContext(PULSAR_MODE_LAPKO) ? 2
                                                                                      : 0;
}
static FrameLoadHook BattleElimHook(BattleElim);

// Fix Balloon Stealing [Gaberboo]
kmWrite32(0x80538a28, 0x38000002);
kmWrite32(0x8053cec8, 0x38000002);

// Convert OnMoveHit to OnRemoveHit [ZPL]
kmWrite32(0x8053b618, 0x38800002);
kmWrite32(0x80538a74, 0x60000000);

void BattleTimer() {
    const RKNet::Controller *controller = RKNet::Controller::sInstance;
    const RKNet::ControllerSub &sub = controller->subs[controller->currentSub];
    sBattleDuration = 180;
    if (ShouldApplyBattleElimination()) {
        if (sub.playerCount == 12 || sub.playerCount == 11 || sub.playerCount == 10) {
            sBattleDuration = 300;
        } else if (sub.playerCount == 9 || sub.playerCount == 8 || sub.playerCount == 7) {
            sBattleDuration = 240;
        } else if (sub.playerCount == 6 || sub.playerCount == 5 || sub.playerCount == 4) {
            sBattleDuration = 180;
        } else if (sub.playerCount == 3 || sub.playerCount == 2 || sub.playerCount == 1) {
            sBattleDuration = 120;
        }
    }
}
static FrameLoadHook BattleTimerHook(BattleTimer);

asmFunc LoadBattleDuration() {
    ASM(
        nofralloc;
        lis r7, sBattleDuration @ha;
        lhz r0, sBattleDuration @l(r7);
        blr;)
}
kmCall(0x80532BCC, LoadBattleDuration);

}  // namespace BattleElim
}  // namespace Pulsar
