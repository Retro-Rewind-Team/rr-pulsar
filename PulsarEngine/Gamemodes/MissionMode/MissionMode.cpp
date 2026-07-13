#include <Gamemodes/MissionMode/MissionMode.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>

namespace Pulsar {
namespace MissionMode {

static void* sMissionState = 0;

static u32 GetMissionScore(void* mission) {
    return *reinterpret_cast<const u32*>(reinterpret_cast<const u8*>(mission) + 8);
}

static u32 GetMissionRequiredScore() {
    return *reinterpret_cast<const u32*>(reinterpret_cast<const u8*>(Racedata::sInstance) + 0xbcc);
}

static bool HasMissionScoreRequirement(void* mission) {
    return GetMissionScore(mission) >= GetMissionRequiredScore();
}

static bool IsMissionPresentationFailure() {
    if (Racedata::sInstance == 0 || sMissionState == 0 ||
        Racedata::sInstance->racesScenario.settings.gamemode != MODE_MISSION_TOURNAMENT) {
        return false;
    }

    return !HasMissionScoreRequirement(sMissionState) &&
           *reinterpret_cast<const u32*>(reinterpret_cast<const u8*>(sMissionState) + 0xc) == 2;
}

static void FixMissionTimeout(void* mission) {
    sMissionState = mission;
    typedef void (*MissionFinishFn)(void*);
    const u32* const vtable = *reinterpret_cast<const u32* const*>(mission);
    const MissionFinishFn missionFinish = reinterpret_cast<MissionFinishFn>(vtable[0x34 / 4]);

    // RaceModeMission::calcTimeOut calls vtable + 0x34 here, then continues
    // with its untouched generic end call after this hook returns. Calling the
    // timeout entry itself would re-enter this patched instruction.
    missionFinish(mission);
    if (!HasMissionScoreRequirement(mission)) {
        // setFinalScore can select a lower rank and leave status at 1 even
        // though the mission target was not reached. Timeout must be a loss.
        *reinterpret_cast<u32*>(reinterpret_cast<u8*>(mission) + 0xc) = 2;
    }
}

static void FixMissionTimeoutEnd(void* mission) {
    sMissionState = mission;
    typedef void (*MissionEndFn)(void*);
    const u32* const vtable = *reinterpret_cast<const u32* const*>(mission);
    const MissionEndFn missionEnd = reinterpret_cast<MissionEndFn>(vtable[0xc / 4]);

    // This is the generic end call immediately after the timeout score path.
    // Invoke its virtual target directly so the original epilogue can resume.
    missionEnd(mission);
    if (!HasMissionScoreRequirement(mission) && Raceinfo::sInstance != 0 &&
        Raceinfo::sInstance->stage == RACESTAGE_IS_FINISHING) {
        // The normal timeout path leaves a failed mission in the one-frame
        // finishing stage because its status was previously reported as a
        // success. Complete the stage transition after the normal end call so
        // the results section can be opened.
        Raceinfo::sInstance->stage = RACESTAGE_FINISHED;
    }
}

static u32 FixMissionCanEnd(void* mission) {
    sMissionState = mission;
    const bool timerExpired = Raceinfo::sInstance != 0 && Raceinfo::sInstance->timerMgr != 0 &&
                              Raceinfo::sInstance->timerMgr->hasRaceTimeRanOut;
    const u32 status = *reinterpret_cast<const u32*>(reinterpret_cast<const u8*>(mission) + 0xc);
    const bool timeoutFailure = timerExpired && status == 1 &&
                                !HasMissionScoreRequirement(mission);
    if (timeoutFailure) {
        // The race-state machine must still see an endable mission. Convert
        // the premature success into a failure before returning, rather than
        // returning false and leaving the race in its finishing state.
        *reinterpret_cast<u32*>(reinterpret_cast<u8*>(mission) + 0xc) = 2;
        return 1;
    }
    return status != 0;
}

// FUN_8078cfa4 is the game's shared result-status query. Ghidra shows that
// its Mission Mode branch always returns 0, which is the first-place status.
// The game calls it from several result-animation and audio paths, so wrap
// those call sites and correct the returned status after the original query.
static u32 GetMissionPresentationStatus(u32 playerId) {
    typedef u32 (*GetStatusFn)(u32);
    const GetStatusFn getStatus = reinterpret_cast<GetStatusFn>(0x8078cfa4);
    const u32 status = getStatus(playerId);

    if (IsMissionPresentationFailure()) {
        return 2;
    }

    return status;
}

kmCall(0x807121fc, GetMissionPresentationStatus);
kmCall(0x8071223c, GetMissionPresentationStatus);
kmCall(0x80712250, GetMissionPresentationStatus);
kmCall(0x80712270, GetMissionPresentationStatus);
kmCall(0x807122c4, GetMissionPresentationStatus);
kmCall(0x80712364, GetMissionPresentationStatus);
kmCall(0x80712390, GetMissionPresentationStatus);
kmCall(0x807123b0, GetMissionPresentationStatus);
kmCall(0x807cc7f0, GetMissionPresentationStatus);
kmCall(0x807cc880, GetMissionPresentationStatus);
kmCall(0x808644b0, GetMissionPresentationStatus);

// 0x8053dacc is the virtual timeout call made by RaceModeMission::calc.
kmCall(0x8053dacc, FixMissionTimeout);
kmCall(0x8053dae0, FixMissionTimeoutEnd);

// This small leaf function only returns whether mission status is set.
kmBranch(0x8053dafc, FixMissionCanEnd);

void PrepareMenuScenario() {
    RacedataScenario& scenario = Racedata::sInstance->menusScenario;

    scenario.playerCount = 1;
    scenario.screenCount = 1;
    scenario.localPlayerCount = 1;

    scenario.settings.gamemode = MODE_MISSION_TOURNAMENT;
    scenario.settings.gametype = GAMETYPE_DEFAULT;
    scenario.settings.engineClass = CC_150;
    scenario.settings.courseId = LUIGI_CIRCUIT;
    scenario.settings.cupId = 0;
    scenario.settings.raceNumber = 0;
    scenario.settings.modeFlags = 0;
    scenario.settings.selectId = 0;
    scenario.settings.hudPlayerIds[0] = 0;
    scenario.settings.hudPlayerIds[1] = 0xff;
    scenario.settings.hudPlayerIds[2] = 0xff;
    scenario.settings.hudPlayerIds[3] = 0xff;

    scenario.players[0].playerType = PLAYER_REAL_LOCAL;
    scenario.players[0].hudSlotId = 0;
    for (u32 i = 1; i < 12; ++i) {
        scenario.players[i].playerType = PLAYER_NONE;
        scenario.players[i].hudSlotId = -1;
    }

    for (u32 i = 0; i < sizeof(scenario.mission); ++i) {
        scenario.mission[i] = 0;
    }
}

}  // namespace MissionMode
}  // namespace Pulsar
