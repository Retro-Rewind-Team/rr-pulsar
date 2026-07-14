#include <Gamemodes/MissionMode/MissionMode.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <MarioKartWii/KMP/KMPManager.hpp>
#include <Settings/SettingsParam.hpp>
#include <core/rvl/OS/OS.hpp>

namespace Pulsar {
namespace MissionMode {

static void* sMissionState = 0;
static bool sMissionTimeRankFailure = false;
static bool sMissionRankReported = false;

static const u32 MISSION_ITEM_MODE_OFFSET = 0x2E;
static const u32 MISSION_FEATURE_FLAGS_OFFSET = 0x2F;
static const u32 MISSION_OBJECTIVE_OFFSET = 0x02;
static const u32 MISSION_SCORE_REQUIRED_OFFSET = 0x08;
static const u32 MISSION_CUSTOM_ITEMS_OFFSET = 0x54;
static const u32 MISSION_ENGINE_OFFSET = 0x07;
static const u32 MISSION_RANK_TIMES_OFFSET = 0x30;
static const u32 MISSION_RANK_COUNT = 6;
static const u32 MISSION_RANK_FIELD_OFFSET = 0x10;

bool IsMissionScenario(const RacedataScenario& scenario) {
    return scenario.settings.gamemode == MODE_MISSION_TOURNAMENT;
}

bool HasMissionFeature(const RacedataScenario& scenario, MissionFeatureFlag feature) {
    return IsMissionScenario(scenario) &&
           (scenario.mission[MISSION_FEATURE_FLAGS_OFFSET] & static_cast<u8>(feature)) != 0;
}

u8 GetMissionItemMode(const RacedataScenario& scenario) {
    return scenario.mission[MISSION_ITEM_MODE_OFFSET];
}

u32 GetMissionCustomItems(const RacedataScenario& scenario) {
    const u8* bytes = &scenario.mission[MISSION_CUSTOM_ITEMS_OFFSET];
    return (static_cast<u32>(bytes[0]) << 24) |
           (static_cast<u32>(bytes[1]) << 16) |
           (static_cast<u32>(bytes[2]) << 8) |
           static_cast<u32>(bytes[3]);
}

void ApplyMissionScenarioSettings(RacedataScenario& scenario) {
    if (!IsMissionScenario(scenario)) return;

    const u8 flags = scenario.mission[MISSION_FEATURE_FLAGS_OFFSET];
    const u8 engine = scenario.mission[MISSION_ENGINE_OFFSET];
    
    if ((flags & ENGINE_500CC) != 0) {
        scenario.settings.engineClass = CC_50;
    } else if (engine == 2) {
        scenario.settings.engineClass = CC_150;
    } else {
        scenario.settings.engineClass = CC_100;
    }

    if ((flags & ITEM_MODE_OVERRIDE) == 0 || GetMissionItemMode(scenario) == GAMEMODE_DEFAULT)
        scenario.settings.itemMode = ITEMS_BALANCED;
}

static u16 GetMissionCPUCount(const RacedataScenario& scenario) {
    return static_cast<u16>((static_cast<u16>(scenario.mission[0x58]) << 8) |
                             static_cast<u16>(scenario.mission[0x59]));
}

void PopulateMissionCPUs(RacedataScenario& scenario) {
    if (scenario.settings.gamemode != MODE_MISSION_TOURNAMENT) return;

    ApplyMissionScenarioSettings(scenario);

    u16 cpuCount = GetMissionCPUCount(scenario);
    if (cpuCount > 11) cpuCount = 11;

    for (u32 i = 1; i < 12; ++i) scenario.players[i].playerType = PLAYER_NONE;
    for (u32 i = 0; i < cpuCount; ++i) {
        RacedataPlayer& player = scenario.players[i + 1];
        const u32 kmtOffset = 0x5A + i * 2;
        player.characterId = static_cast<CharacterId>(scenario.mission[kmtOffset]);
        player.kartId = static_cast<KartId>(scenario.mission[kmtOffset + 1]);
        player.playerType = PLAYER_CPU;
    }
}

typedef void (*GetInitialPhysicsValuesFn)(Raceinfo*, Vec3*, Vec3*, u8);
static const GetInitialPhysicsValuesFn sGetInitialPhysicsValues =
    reinterpret_cast<GetInitialPhysicsValuesFn>(0x805362dc);

static bool HasMissionCPUs(const RacedataScenario& scenario) {
    for (u32 i = 1; i < 12; ++i)
        if (scenario.players[i].playerType == PLAYER_CPU) return true;
    return false;
}

static void SetMissionStartPosition(Raceinfo* raceinfo, Vec3* position, Vec3* angles, u8 playerId) {
    const RacedataScenario& scenario = Racedata::sInstance->racesScenario;
    if (scenario.settings.gamemode != MODE_MISSION_TOURNAMENT || !HasMissionCPUs(scenario) ||
        playerId >= scenario.playerCount) {
        sGetInitialPhysicsValues(raceinfo, position, angles, playerId);
        return;
    }

    const KMP::Holder<KTPT>* holder = raceinfo->GetKTPTHolder(0);
    if (holder == nullptr) {
        sGetInitialPhysicsValues(raceinfo, position, angles, playerId);
        return;
    }

    const u32 playerPosition = scenario.playerCount - playerId;
    const_cast<KMP::Holder<KTPT>*>(holder)->CalcCoordinates(*position, *angles, playerPosition, scenario.playerCount);
}

kmCall(0x8058ee78, SetMissionStartPosition);
kmCall(0x805a70e8, SetMissionStartPosition);

static u32 GetMissionValue(const void* mission, u32 offset) {
    return *reinterpret_cast<const u32*>(reinterpret_cast<const u8*>(mission) + offset);
}

static void SetMissionValue(void* mission, u32 offset, u32 value) {
    *reinterpret_cast<u32*>(reinterpret_cast<u8*>(mission) + offset) = value;
}

static u16 GetMissionU16(const void* mission, u32 offset) {
    const u8* const bytes = reinterpret_cast<const u8*>(mission) + offset;
    return static_cast<u16>((static_cast<u16>(bytes[0]) << 8) | bytes[1]);
}

static bool IsMissionScoreObjective() {
    if (Racedata::sInstance == 0) return false;

    switch (GetMissionU16(Racedata::sInstance->racesScenario.mission, MISSION_OBJECTIVE_OFFSET)) {
        case 0:
        case 3:
        case 4:
        case 5:
        case 6:
        case 8:
        case 0xb:
        case 0xc:
        case 0xd:
            return true;
        default:
            return false;
    }
}

static u32 GetMissionScoreDisplayTarget(const void* raceConfig) {
    if (IsMissionScoreObjective())
        return GetMissionValue(Racedata::sInstance->racesScenario.mission,
                               MISSION_SCORE_REQUIRED_OFFSET);
    return GetMissionValue(raceConfig, 0xBCC);
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

static bool SetMissionRankFromTime(void* mission) {
    if (Racedata::sInstance == 0 ||
        Racedata::sInstance->racesScenario.settings.gamemode != MODE_MISSION_TOURNAMENT)
        return false;

    u32 finishTimeMillis = 0;
    if (!GetMissionFinishTimeMillis(finishTimeMillis)) return false;
    if (sMissionRankReported) return true;

    const u8* const missionData = Racedata::sInstance->racesScenario.mission;
    static const char* const rankNames[MISSION_RANK_COUNT] = {
        "3 Stars", "2 Stars", "1 Star", "A", "B", "C"
    };
    
    for (u32 rank = 0; rank < MISSION_RANK_COUNT; ++rank) {
        const u32 thresholdSeconds = GetMissionValue(missionData,
                MISSION_RANK_TIMES_OFFSET + rank * sizeof(u32));
        if (thresholdSeconds != 0 && finishTimeMillis < thresholdSeconds * 1000) {
            sMissionTimeRankFailure = false;
            sMissionRankReported = true;
            SetMissionValue(mission, MISSION_RANK_FIELD_OFFSET, rank);
            OS::Report("[Pulsar] Mission rank: %u (%s), finish=%u:%02u.%03u, cutoff=%u s\n",
                       rank + 1, rankNames[rank], finishTimeMillis / 60000,
                       (finishTimeMillis / 1000) % 60, finishTimeMillis % 1000, thresholdSeconds);
            return true;
        }
    }

    const u32 cCutoffSeconds = GetMissionValue(missionData,
            MISSION_RANK_TIMES_OFFSET + (MISSION_RANK_COUNT - 1) * sizeof(u32));
    sMissionTimeRankFailure = true;
    sMissionRankReported = true;
    SetMissionValue(mission, 0xc, 2);
    OS::Report("[Pulsar] Mission rank: FAILED, finish=%u:%02u.%03u, C cutoff=%u s\n",
               finishTimeMillis / 60000, (finishTimeMillis / 1000) % 60,
               finishTimeMillis % 1000, cCutoffSeconds);
    return false;
}

typedef void (*SetMissionObjectiveCompleteFn)(void*, u32, u32);
static const SetMissionObjectiveCompleteFn sSetMissionObjectiveComplete =
    reinterpret_cast<SetMissionObjectiveCompleteFn>(0x8053e194);

static void FixMissionScoreCalcRank(void* mission) {
    if (Racedata::sInstance == 0 ||
        Racedata::sInstance->racesScenario.settings.gamemode != MODE_MISSION_TOURNAMENT)
        return;

    const u32 score = GetMissionValue(mission, 8);
    const u32 requiredScore = GetMissionValue(Racedata::sInstance->racesScenario.mission,
                                              MISSION_SCORE_REQUIRED_OFFSET);
    if (score < requiredScore) return;

    const u32 raceManager = GetMissionValue(mission, 4);
    const u32 objective = GetMissionU16(Racedata::sInstance->racesScenario.mission,
                                        MISSION_OBJECTIVE_OFFSET);
    sSetMissionObjectiveComplete(reinterpret_cast<void*>(raceManager), objective, 0);
}

static void SetMissionState(void* mission) {
    if (sMissionState != mission || GetMissionValue(mission, 0xc) == 0) {
        sMissionTimeRankFailure = false;
        sMissionRankReported = false;
    }
    sMissionState = mission;
}

static bool HasMissionScoreRequirement(void* mission) {
    if (!IsMissionScoreObjective()) {
        return GetMissionValue(mission, 8) >=
               GetMissionValue(Racedata::sInstance->racesScenario.mission, MISSION_RANK_TIMES_OFFSET);
    }
    return GetMissionValue(mission, 8) >=
           GetMissionValue(Racedata::sInstance->racesScenario.mission, MISSION_SCORE_REQUIRED_OFFSET);
}

static bool IsMissionPresentationFailure() {
    if (Racedata::sInstance == 0 || sMissionState == 0 ||
        Racedata::sInstance->racesScenario.settings.gamemode != MODE_MISSION_TOURNAMENT) {
        return false;
    }

    return GetMissionValue(sMissionState, 0xc) == 2 &&
           (sMissionTimeRankFailure || !HasMissionScoreRequirement(sMissionState));
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
        SetMissionValue(mission, 0xc, 2);
    }
}

static void FixMissionTimeoutEnd(void* mission) {
    SetMissionState(mission);
    CallMissionFunction(mission, 0xc);
    if (!HasMissionScoreRequirement(mission) && Raceinfo::sInstance != 0 &&
        Raceinfo::sInstance->stage == RACESTAGE_IS_FINISHING) {
        Raceinfo::sInstance->stage = RACESTAGE_FINISHED;
    }
}

static u32 FixMissionCanEnd(void* mission) {
    SetMissionState(mission);
    const bool timerExpired = Raceinfo::sInstance != 0 && Raceinfo::sInstance->timerMgr != 0 &&
                              Raceinfo::sInstance->timerMgr->hasRaceTimeRanOut;
    const u32 status = GetMissionValue(mission, 0xc);
    const bool timeoutFailure = timerExpired && status == 1 &&
                                !HasMissionScoreRequirement(mission);
    if (timeoutFailure) {
        SetMissionValue(mission, 0xc, 2);
        return 1;
    }
    if (status == 1) SetMissionRankFromTime(mission);
    return status != 0;
}

static u32 GetMissionPresentationStatus(u32 playerId) {
    typedef u32 (*GetStatusFn)(u32);
    const GetStatusFn getStatus = reinterpret_cast<GetStatusFn>(0x8078cfa4);
    const u32 status = getStatus(playerId);

    if (sMissionState != 0 && GetMissionValue(sMissionState, 0xc) == 1)
        SetMissionRankFromTime(sMissionState);
    return IsMissionPresentationFailure() ? 2 : status;
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
kmCall(0x8053dacc, FixMissionTimeout);
kmCall(0x8053dae0, FixMissionTimeoutEnd);

kmCall(0x807f773c, GetMissionScoreDisplayTarget);
kmWrite32(0x807f7740, 0x907f01a0);  // stw r3, 0x1a0(r31)
kmWrite32(0x807f7744, 0x2c030000);  // cmpwi r3, 0

kmBranch(0x8053dff8, FixMissionScoreCalcRank);
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
    for (u32 i = 0; i < 4; ++i) scenario.settings.hudPlayerIds[i] = i ? 0xff : 0;

    scenario.players[0].playerType = PLAYER_REAL_LOCAL;
    scenario.players[0].hudSlotId = 0;
    for (u32 i = 1; i < 12; ++i) {
        scenario.players[i].playerType = PLAYER_NONE;
        scenario.players[i].hudSlotId = -1;
    }

    memset(scenario.mission, 0, sizeof(scenario.mission));
}

}  // namespace MissionMode
}  // namespace Pulsar
