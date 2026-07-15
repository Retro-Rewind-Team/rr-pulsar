#include <Gamemodes/MissionMode/MissionMode.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <MarioKartWii/KMP/KMPManager.hpp>
#include <Settings/SettingsParam.hpp>
<<<<<<< HEAD
#include <MarioKartWii/UI/Ctrl/CtrlRace/CtrlRaceScore.hpp>
#include <core/rvl/OS/OS.hpp>
#include <runtimeWrite.hpp>
=======
>>>>>>> 583bc8f6 (Add ranking display to mission end screen)

namespace Pulsar {
namespace MissionMode {

static const u32 MISSION_ITEM_MODE_OFFSET = 0x2E;
static const u32 MISSION_FEATURE_FLAGS_OFFSET = 0x2F;
static const u32 MISSION_OBJECTIVE_OFFSET = 0x02;
static const u32 MISSION_SCORE_REQUIRED_OFFSET = 0x08;
static const u32 MISSION_LAP_COUNT_MIN = 1;
static const u32 MISSION_LAP_COUNT_MAX = 9;
static const u32 MISSION_COMPETITION_MODE_FLAG = 1 << 2;
static const u32 MISSION_CUSTOM_ITEMS_OFFSET = 0x54;
static const u32 MISSION_ENGINE_OFFSET = 0x07;
<<<<<<< HEAD
static const u16 MISSION_OBJECTIVE_ENEMY_DOWN_02 = 0x06;
=======
>>>>>>> 583bc8f6 (Add ranking display to mission end screen)
bool IsMissionScenario(const RacedataScenario& scenario) {
    return scenario.settings.gamemode == MODE_MISSION_TOURNAMENT;
}

static u16 GetMissionCPUCount(const RacedataScenario& scenario) {
    return static_cast<u16>((static_cast<u16>(scenario.mission[0x58]) << 8) |
                             static_cast<u16>(scenario.mission[0x59]));
}

void PopulateMissionCPUs(RacedataScenario& scenario) {
    if (scenario.settings.gamemode != MODE_MISSION_TOURNAMENT) return;

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

static u32 GetMissionValue(const void* mission, u32 offset) {
    return *reinterpret_cast<const u32*>(reinterpret_cast<const u8*>(mission) + offset);
}

static u16 GetMissionU16(const void* mission, u32 offset) {
    const u8* const bytes = reinterpret_cast<const u8*>(mission) + offset;
    return static_cast<u16>((static_cast<u16>(bytes[0]) << 8) | bytes[1]);
}

bool IsMissionToGateObjective(const RacedataScenario& scenario) {
    return IsMissionScenario(scenario) && GetMissionU16(scenario.mission, MISSION_OBJECTIVE_OFFSET) == 0x09;
}

u8 GetMissionLapCount(const RacedataScenario& scenario) {
    if (!IsMissionScenario(scenario)) return 0;

    const u16 objective = GetMissionU16(scenario.mission, MISSION_OBJECTIVE_OFFSET);
    if (objective != 1 && objective != 2) return 0;

    const u32 requestedLaps = GetMissionValue(scenario.mission, MISSION_SCORE_REQUIRED_OFFSET);
    if (requestedLaps < MISSION_LAP_COUNT_MIN || requestedLaps > MISSION_LAP_COUNT_MAX)
        return 0;
    return static_cast<u8>(requestedLaps);
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

    scenario.settings.modeFlags &= ~MISSION_COMPETITION_MODE_FLAG;
    scenario.settings.lapCount = GetMissionLapCount(scenario);

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

typedef void (*RaceManagerPlayerEndLapFn)(void*);
kmRuntimeUse(0x805349b8);

static void MissionRaceManagerPlayerEndLap(void* player) {
    RacedataScenario* scenario = nullptr;
    if (Racedata::sInstance != nullptr)
        scenario = &Racedata::sInstance->racesScenario;

    const u8 requestedLaps = scenario != nullptr ? GetMissionLapCount(*scenario) : 0;
    const bool enabledCompetitionPath = scenario != nullptr && requestedLaps != 0 &&
                                        (scenario->settings.modeFlags & MISSION_COMPETITION_MODE_FLAG) == 0;
    const u32 oldModeFlags = enabledCompetitionPath ? scenario->settings.modeFlags : 0;
    if (enabledCompetitionPath) scenario->settings.modeFlags |= MISSION_COMPETITION_MODE_FLAG;

    static const RaceManagerPlayerEndLapFn sRaceManagerPlayerEndLap =
        reinterpret_cast<RaceManagerPlayerEndLapFn>(kmRuntimeAddr(0x805349b8));
    sRaceManagerPlayerEndLap(player);

    if (enabledCompetitionPath) scenario->settings.modeFlags = oldModeFlags;
}

kmCall(0x80534fbc, MissionRaceManagerPlayerEndLap);
kmCall(0x805350d4, MissionRaceManagerPlayerEndLap);

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

kmRuntimeUse(0x805362dc);
typedef void (*GetInitialPhysicsValuesFn)(Raceinfo*, Vec3*, Vec3*, u8);
static const GetInitialPhysicsValuesFn sGetInitialPhysicsValues =
    reinterpret_cast<GetInitialPhysicsValuesFn>(kmRuntimeAddr(0x805362dc));

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

bool IsMissionScoreObjective(const RacedataScenario& scenario) {
    if (!IsMissionScenario(scenario)) return false;

<<<<<<< HEAD
    switch (GetMissionU16(scenario.mission, MISSION_OBJECTIVE_OFFSET)) {
=======
static u16 GetMissionU16(const void* mission, u32 offset) {
    const u8* const bytes = reinterpret_cast<const u8*>(mission) + offset;
    return static_cast<u16>((static_cast<u16>(bytes[0]) << 8) | bytes[1]);
}

static bool IsMissionScoreObjective() {
    if (Racedata::sInstance == 0) return false;

    switch (GetMissionU16(Racedata::sInstance->racesScenario.mission, MISSION_OBJECTIVE_OFFSET)) {
>>>>>>> 583bc8f6 (Add ranking display to mission end screen)
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

bool IsMissionBossObjective(const RacedataScenario& scenario) {
    return IsMissionScenario(scenario) &&
           GetMissionU16(scenario.mission, MISSION_OBJECTIVE_OFFSET) == MISSION_OBJECTIVE_ENEMY_DOWN_02;
}

static u32 GetMissionScoreDisplayTarget(const void* raceConfig) {
    if (Racedata::sInstance != 0 && IsMissionScoreObjective(Racedata::sInstance->racesScenario))
        return GetMissionValue(Racedata::sInstance->racesScenario.mission,
                               MISSION_SCORE_REQUIRED_OFFSET);
    return GetMissionValue(raceConfig, 0xBCC);
}

<<<<<<< HEAD
kmRuntimeUse(0x807f784c);
static void FixMissionScoreLayout(CtrlRaceScore* self) {
    typedef void (*CtrlRaceScoreOnUpdateFn)(CtrlRaceScore*);
    static const CtrlRaceScoreOnUpdateFn sCtrlRaceScoreOnUpdate =
        reinterpret_cast<CtrlRaceScoreOnUpdateFn>(kmRuntimeAddr(0x807f784c));
    sCtrlRaceScoreOnUpdate(self);

    if (Racedata::sInstance != 0 && IsMissionToGateObjective(Racedata::sInstance->racesScenario))
        self->isHidden = true;

    if (Racedata::sInstance == 0 ||
        !IsMissionScoreObjective(Racedata::sInstance->racesScenario))
        return;

    nw4r::lyt::Pane* slash = self->layout.GetPaneByName("slash");
    if (slash == nullptr) return;

    static const float SCORE_SLASH_ONE_DIGIT_OFFSET = 22.0f;
    const u32 target = GetMissionValue(Racedata::sInstance->racesScenario.mission,
                                       MISSION_SCORE_REQUIRED_OFFSET);
    static CtrlRaceScore* sAdjustedControl = nullptr;
    static float sSlashBaseX = 0.0f;

    if (target >= 10) {
        if (sAdjustedControl == self) sAdjustedControl = nullptr;
        return;
    }

    if (sAdjustedControl != self) {
        sAdjustedControl = self;
        sSlashBaseX = slash->trans.x;
    }
    slash->trans.x = sSlashBaseX + SCORE_SLASH_ONE_DIGIT_OFFSET;
}

kmWritePointer(0x808d3fdc, FixMissionScoreLayout);

kmCall(0x807f773c, GetMissionScoreDisplayTarget);
kmWrite32(0x807f7740, 0x907f01a0);
kmWrite32(0x807f7744, 0x2c030000);

static void* sMissionState = nullptr;
static bool sMissionTimeRankFailure = false;
static bool sMissionRankReported = false;
static const u32 MISSION_RANK_TIMES_OFFSET = 0x30;
static const u32 MISSION_RANK_COUNT = 6;
static const u32 MISSION_RANK_FIELD_OFFSET = 0x10;

static u32 GetMissionScore(void* mission) {
    return *reinterpret_cast<const u32*>(reinterpret_cast<const u8*>(mission) + 8);
}

static u32 GetMissionRequiredScore() {
    return *reinterpret_cast<const u32*>(reinterpret_cast<const u8*>(Racedata::sInstance) + 0xbcc);
}

static void SetMissionValue(void* mission, u32 offset, u32 value) {
    *reinterpret_cast<u32*>(reinterpret_cast<u8*>(mission) + offset) = value;
}

static bool GetMissionFinishTimeMillis(u32& finishTimeMillis) {
    if (Raceinfo::sInstance == nullptr) return false;
    Timer* finishTime = nullptr;
    if (Raceinfo::sInstance->players != nullptr && Raceinfo::sInstance->players[0] != nullptr)
        finishTime = Raceinfo::sInstance->players[0]->raceFinishTime;
    if (finishTime == nullptr || !finishTime->isActive) {
        if (Raceinfo::sInstance->timerMgr == nullptr || !Raceinfo::sInstance->timerMgr->timers[0].isActive)
            return false;
        finishTime = &Raceinfo::sInstance->timerMgr->timers[0];
    }
    finishTimeMillis = (static_cast<u32>(finishTime->minutes) * 60 + finishTime->seconds) * 1000 +
                       finishTime->milliseconds;
    return true;
}

static bool SetMissionRankFromTime(void* mission) {
    if (Racedata::sInstance == nullptr ||
        Racedata::sInstance->racesScenario.settings.gamemode != MODE_MISSION_TOURNAMENT ||
        sMissionRankReported)
        return false;
    u32 finishTimeMillis = 0;
    if (!GetMissionFinishTimeMillis(finishTimeMillis)) return false;
    const u8* const missionData = Racedata::sInstance->racesScenario.mission;
    for (u32 rank = 0; rank < MISSION_RANK_COUNT; ++rank) {
        const u32 threshold = GetMissionValue(missionData, MISSION_RANK_TIMES_OFFSET + rank * 4);
        if (threshold != 0 && finishTimeMillis < threshold * 1000) {
            sMissionTimeRankFailure = false;
            sMissionRankReported = true;
            SetMissionValue(mission, MISSION_RANK_FIELD_OFFSET, rank);
            return true;
        }
    }
    sMissionTimeRankFailure = true;
    sMissionRankReported = true;
    SetMissionValue(mission, 0xc, 2);
    return false;
}

typedef void (*SetMissionObjectiveCompleteFn)(void*, u32, u32);
static void FixMissionScoreCalcRank(void* mission) {
    if (Racedata::sInstance == nullptr ||
        Racedata::sInstance->racesScenario.settings.gamemode != MODE_MISSION_TOURNAMENT ||
        GetMissionScore(mission) < GetMissionValue(Racedata::sInstance->racesScenario.mission,
                                                    MISSION_SCORE_REQUIRED_OFFSET))
        return;
    const u32 raceManager = GetMissionValue(mission, 4);
    const u32 objective = GetMissionU16(Racedata::sInstance->racesScenario.mission, MISSION_OBJECTIVE_OFFSET);
    reinterpret_cast<SetMissionObjectiveCompleteFn>(0x8053e194)(reinterpret_cast<void*>(raceManager), objective, 0);
}

static bool HasMissionScoreRequirement(void* mission) {
    return GetMissionScore(mission) >= GetMissionRequiredScore();
}

static bool IsMissionPresentationFailure() {
    if (Racedata::sInstance == nullptr || sMissionState == nullptr ||
        Racedata::sInstance->racesScenario.settings.gamemode != MODE_MISSION_TOURNAMENT)
        return false;
    return (sMissionTimeRankFailure || !HasMissionScoreRequirement(sMissionState)) &&
           *reinterpret_cast<const u32*>(reinterpret_cast<const u8*>(sMissionState) + 0xc) == 2;
}

static void FixMissionTimeout(void* mission) {
    if (sMissionState != mission) {
        sMissionTimeRankFailure = false;
        sMissionRankReported = false;
    }
    sMissionState = mission;
    typedef void (*MissionFinishFn)(void*);
    const u32* const vtable = *reinterpret_cast<const u32* const*>(mission);
    reinterpret_cast<MissionFinishFn>(vtable[0x34 / 4])(mission);
    if (!HasMissionScoreRequirement(mission))
        *reinterpret_cast<u32*>(reinterpret_cast<u8*>(mission) + 0xc) = 2;
}

static void FixMissionTimeoutEnd(void* mission) {
    sMissionState = mission;
    typedef void (*MissionEndFn)(void*);
    const u32* const vtable = *reinterpret_cast<const u32* const*>(mission);
    reinterpret_cast<MissionEndFn>(vtable[0xc / 4])(mission);
    if (!HasMissionScoreRequirement(mission) && Raceinfo::sInstance != nullptr &&
        Raceinfo::sInstance->stage == RACESTAGE_IS_FINISHING)
        Raceinfo::sInstance->stage = RACESTAGE_FINISHED;
}

static u32 FixMissionCanEnd(void* mission) {
    sMissionState = mission;
    const bool timerExpired = Raceinfo::sInstance != nullptr && Raceinfo::sInstance->timerMgr != nullptr &&
                               Raceinfo::sInstance->timerMgr->hasRaceTimeRanOut;
    const u32 status = *reinterpret_cast<const u32*>(reinterpret_cast<const u8*>(mission) + 0xc);
    if (timerExpired && status == 1 && !HasMissionScoreRequirement(mission)) {
        *reinterpret_cast<u32*>(reinterpret_cast<u8*>(mission) + 0xc) = 2;
        return 1;
    }
    if (status == 1) SetMissionRankFromTime(mission);
    return status != 0;
}

static u32 GetMissionPresentationStatus(u32 playerId) {
    typedef u32 (*GetStatusFn)(u32);
    const u32 status = reinterpret_cast<GetStatusFn>(0x8078cfa4)(playerId);
    if (sMissionState != nullptr && *reinterpret_cast<const u32*>(reinterpret_cast<const u8*>(sMissionState) + 0xc) == 1)
        SetMissionRankFromTime(sMissionState);
    return IsMissionPresentationFailure() ? 2 : status;
}

// Ranking hooks are implemented by MissionModeRanking.cpp.

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

}
}
