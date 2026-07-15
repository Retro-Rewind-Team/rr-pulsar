#include <Gamemodes/MissionMode/MissionMode.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <MarioKartWii/KMP/KMPManager.hpp>
#include <Settings/SettingsParam.hpp>

namespace Pulsar {
namespace MissionMode {

static const u32 MISSION_ITEM_MODE_OFFSET = 0x2E;
static const u32 MISSION_FEATURE_FLAGS_OFFSET = 0x2F;
static const u32 MISSION_OBJECTIVE_OFFSET = 0x02;
static const u32 MISSION_SCORE_REQUIRED_OFFSET = 0x08;
static const u32 MISSION_CUSTOM_ITEMS_OFFSET = 0x54;
static const u32 MISSION_ENGINE_OFFSET = 0x07;
static const u16 MISSION_OBJECTIVE_ENEMY_DOWN_02 = 0x06;
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

static u16 GetMissionU16(const void* mission, u32 offset) {
    const u8* const bytes = reinterpret_cast<const u8*>(mission) + offset;
    return static_cast<u16>((static_cast<u16>(bytes[0]) << 8) | bytes[1]);
}

bool IsMissionScoreObjective(const RacedataScenario& scenario) {
    if (!IsMissionScenario(scenario)) return false;

    switch (GetMissionU16(scenario.mission, MISSION_OBJECTIVE_OFFSET)) {
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

kmCall(0x807f773c, GetMissionScoreDisplayTarget);
kmWrite32(0x807f7740, 0x907f01a0);  // stw r3, 0x1a0(r31)
kmWrite32(0x807f7744, 0x2c030000);  // cmpwi r3, 0

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
