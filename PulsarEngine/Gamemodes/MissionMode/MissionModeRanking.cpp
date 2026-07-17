#include <Gamemodes/MissionMode/MissionModeRanking.hpp>
#include <Gamemodes/MissionMode/MissionModeSave.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>

namespace Pulsar {
namespace MissionMode {
namespace Ranking {

static void* sMissionState = 0;
static bool sMissionTimeRankFailure = false;
static bool sMissionRankReported = false;

static const u32 MISSION_SCORE_REQUIRED_OFFSET = 0x08;
static const u32 MISSION_STATUS_OFFSET = 0x0c;
static const u32 MISSION_OBJECTIVE_OFFSET = 0x02;
static const u32 MISSION_RANK_THRESHOLDS_OFFSET = 0x30;
static const u32 MISSION_RANK_COUNT = 6;
static const u32 MISSION_RANK_FIELD_OFFSET = 0x10;

static u32 GetMissionValue(const void* mission, u32 offset) {
    return *reinterpret_cast<const u32*>(reinterpret_cast<const u8*>(mission) + offset);
}

static u16 GetMissionU16(const void* mission, u32 offset) {
    const u8* const bytes = reinterpret_cast<const u8*>(mission) + offset;
    return static_cast<u16>((static_cast<u16>(bytes[0]) << 8) | bytes[1]);
}

static void SetMissionValue(void* mission, u32 offset, u32 value) {
    *reinterpret_cast<u32*>(reinterpret_cast<u8*>(mission) + offset) = value;
}

typedef void (*SetMissionObjectiveCompleteFn)(void*, u32, u32);
static const SetMissionObjectiveCompleteFn sSetMissionObjectiveComplete =
    reinterpret_cast<SetMissionObjectiveCompleteFn>(0x8053e194);

static void SetMissionState(void* mission);
static bool IsRankReported();
static u32 GetRank(const void* mission);
static bool SetRankFromTime(void* mission);

static bool HasMissionScoreRequirement(void* mission) {
    return GetMissionValue(mission, MISSION_SCORE_REQUIRED_OFFSET) >=
           GetMissionValue(Racedata::sInstance->racesScenario.mission,
                           MISSION_SCORE_REQUIRED_OFFSET);
}

static void FixMissionScoreCalcRank(void* mission) {
    if (Racedata::sInstance == 0 ||
        Racedata::sInstance->racesScenario.settings.gamemode != MODE_MISSION_TOURNAMENT)
        return;

    SetMissionState(mission);
    const u32 score = GetMissionValue(mission, MISSION_SCORE_REQUIRED_OFFSET);
    const u32 requiredScore = GetMissionValue(Racedata::sInstance->racesScenario.mission,
                                              MISSION_SCORE_REQUIRED_OFFSET);
    if (score < requiredScore) return;

    const u32 raceManager = GetMissionValue(mission, 4);
    const u32 objective = GetMissionU16(Racedata::sInstance->racesScenario.mission,
                                        MISSION_OBJECTIVE_OFFSET);
    const u32 rank = IsRankReported() ? GetRank(mission) : 0;
    sSetMissionObjectiveComplete(reinterpret_cast<void*>(raceManager), objective, rank);
    if (!IsRankReported()) SetRankFromTime(mission);
}

static bool GetMissionFinishTimeMillis(u32& finishTimeMillis) {
    if (Raceinfo::sInstance == 0) return false;

    Timer* finishTime = 0;
    if (Raceinfo::sInstance->players != 0 && Raceinfo::sInstance->players[0] != 0)
        finishTime = Raceinfo::sInstance->players[0]->raceFinishTime;

    if (finishTime == 0 || !finishTime->isActive) {
        if (Raceinfo::sInstance->timerMgr == 0 || !Raceinfo::sInstance->timerMgr->timers[0].isActive)
            return false;
        finishTime = &Raceinfo::sInstance->timerMgr->timers[0];
    }

    finishTimeMillis = (static_cast<u32>(finishTime->minutes) * 60 + finishTime->seconds) * 1000 +
                       finishTime->milliseconds;
    return true;
}

static void SetMissionState(void* mission) {
    if (sMissionState != mission || GetMissionValue(mission, MISSION_STATUS_OFFSET) == 0) {
        sMissionTimeRankFailure = false;
        sMissionRankReported = false;
    }
    sMissionState = mission;
}

static bool IsRankReported() {
    return sMissionRankReported;
}

static u32 GetRank(const void* mission) {
    return GetMissionValue(mission, MISSION_RANK_FIELD_OFFSET);
}

static bool SetRankFromTime(void* mission) {
    if (Racedata::sInstance == 0 ||
        Racedata::sInstance->racesScenario.settings.gamemode != MODE_MISSION_TOURNAMENT)
        return false;

    u32 finishTimeMillis = 0;
    if (!GetMissionFinishTimeMillis(finishTimeMillis)) return false;
    if (sMissionRankReported) return true;

    const u8* const missionData = Racedata::sInstance->racesScenario.mission;
    for (u32 rank = 0; rank < MISSION_RANK_COUNT; ++rank) {
        const u32 thresholdSeconds = GetMissionValue(missionData,
                MISSION_RANK_THRESHOLDS_OFFSET + rank * sizeof(u32));
        if (thresholdSeconds != 0 && finishTimeMillis < thresholdSeconds * 1000) {
            sMissionTimeRankFailure = false;
            sMissionRankReported = true;
            SetMissionValue(mission, MISSION_RANK_FIELD_OFFSET, rank);
            SaveMissionResult(finishTimeMillis, rank);
            return true;
        }
    }

    sMissionTimeRankFailure = true;
    sMissionRankReported = true;
    SetMissionValue(mission, MISSION_STATUS_OFFSET, 2);
    return false;
}

static void UpdateRankFromCurrentMission() {
    if (sMissionState != 0 && GetMissionValue(sMissionState, MISSION_STATUS_OFFSET) == 1 &&
        !sMissionRankReported) {
        SetRankFromTime(sMissionState);
    }
}

static bool IsPresentationFailure() {
    if (Racedata::sInstance == 0 || sMissionState == 0 ||
        Racedata::sInstance->racesScenario.settings.gamemode != MODE_MISSION_TOURNAMENT) {
        return false;
    }

    return GetMissionValue(sMissionState, MISSION_STATUS_OFFSET) == 2 &&
           (sMissionTimeRankFailure || !HasMissionScoreRequirement(sMissionState));
}

static bool GetResultRank(u32& rank) {
    if (Racedata::sInstance == 0 || sMissionState == 0 ||
        Racedata::sInstance->racesScenario.settings.gamemode != MODE_MISSION_TOURNAMENT) {
        return false;
    }

    const u32 status = GetMissionValue(sMissionState, MISSION_STATUS_OFFSET);
    if (status == 1 && !sMissionRankReported)
        SetRankFromTime(sMissionState);

    if (IsPresentationFailure() || GetMissionValue(sMissionState, MISSION_STATUS_OFFSET) != 1)
        return false;

    rank = GetRank(sMissionState);
    return rank < MISSION_RANK_COUNT;
}

typedef void (*MissionCallFn)(void*);
static void CallMissionFunction(void* mission, u32 offset) {
    const u32* const vtable = *reinterpret_cast<const u32* const*>(mission);
    reinterpret_cast<MissionCallFn>(vtable[offset / 4])(mission);
}

static void FixMissionTimeout(void* mission) {
    SetMissionState(mission);
    CallMissionFunction(mission, 0x34);
    if (!HasMissionScoreRequirement(mission)) {
        SetMissionValue(mission, MISSION_STATUS_OFFSET, 2);
    }
    if (GetMissionValue(mission, MISSION_STATUS_OFFSET) == 1 && !IsRankReported())
        SetRankFromTime(mission);
}

static void FixMissionTimeoutEnd(void* mission) {
    SetMissionState(mission);
    CallMissionFunction(mission, 0xc);
    if (!HasMissionScoreRequirement(mission) && Raceinfo::sInstance != 0 &&
        Raceinfo::sInstance->stage == RACESTAGE_IS_FINISHING) {
        Raceinfo::sInstance->stage = RACESTAGE_FINISHED;
    }
    if (GetMissionValue(mission, MISSION_STATUS_OFFSET) == 1 && !IsRankReported())
        SetRankFromTime(mission);
}

static u32 FixMissionCanEnd(void* mission) {
    SetMissionState(mission);
    const bool timerExpired = Raceinfo::sInstance != 0 && Raceinfo::sInstance->timerMgr != 0 &&
                              Raceinfo::sInstance->timerMgr->hasRaceTimeRanOut;
    const u32 status = GetMissionValue(mission, MISSION_STATUS_OFFSET);
    const bool timeoutFailure = timerExpired && status == 1 &&
                                !HasMissionScoreRequirement(mission);
    if (timeoutFailure) {
        SetMissionValue(mission, MISSION_STATUS_OFFSET, 2);
        return 1;
    }
    if (status == 1 && !IsRankReported()) SetRankFromTime(mission);
    return status != 0;
}

static u32 GetMissionPresentationStatus(u32 playerId) {
    typedef u32 (*GetStatusFn)(u32);
    const GetStatusFn getStatus = reinterpret_cast<GetStatusFn>(0x8078cfa4);
    const u32 status = getStatus(playerId);

    UpdateRankFromCurrentMission();
    if (IsPresentationFailure()) return 2;
    return status;
}

}

kmCall(0x807121fc, Ranking::GetMissionPresentationStatus);
kmCall(0x8071223c, Ranking::GetMissionPresentationStatus);
kmCall(0x80712250, Ranking::GetMissionPresentationStatus);
kmCall(0x80712270, Ranking::GetMissionPresentationStatus);
kmCall(0x807122c4, Ranking::GetMissionPresentationStatus);
kmCall(0x80712364, Ranking::GetMissionPresentationStatus);
kmCall(0x80712390, Ranking::GetMissionPresentationStatus);
kmCall(0x807123b0, Ranking::GetMissionPresentationStatus);
kmCall(0x807cc7f0, Ranking::GetMissionPresentationStatus);
kmCall(0x807cc880, Ranking::GetMissionPresentationStatus);
kmCall(0x808644b0, Ranking::GetMissionPresentationStatus);
kmCall(0x8053dacc, Ranking::FixMissionTimeout);
kmCall(0x8053dae0, Ranking::FixMissionTimeoutEnd);
kmBranch(0x8053dafc, Ranking::FixMissionCanEnd);
kmBranch(0x8053dff8, Ranking::FixMissionScoreCalcRank);

bool GetMissionResultRank(u32& rank) {
    return Ranking::GetResultRank(rank);
}

}
}
