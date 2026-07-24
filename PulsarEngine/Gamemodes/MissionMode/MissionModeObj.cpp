#include <Gamemodes/MissionMode/MissionMode.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/KMP/KMPManager.hpp>
#include <MarioKartWii/System/Random.hpp>
#include <MarioKartWii/Objects/Collidable/Itembox/Itembox.hpp>

namespace Pulsar {
namespace MissionMode {

static const u32 MISSION_OBJECTIVE_OFFSET = 0x02;
static const u16 MISSION_OBJECTIVE_BREAK_ITEM_BOXES = 4;
static const u32 ITEMBOX_NO_RESPAWN_TIME = 0x7fffffff;
static const u32 COIN_ADD_INTRO_GUARD_VALUE = 5;

static u16 GetMissionU16(const void* mission, u32 offset) {
    const u8* bytes = reinterpret_cast<const u8*>(mission) + offset;
    return static_cast<u16>((bytes[0] << 8) | bytes[1]);
}

static bool IsMissionCoinObjective(const RacedataScenario& scenario) {
    return IsMissionScenario(scenario) &&
           GetMissionU16(scenario.mission, MISSION_OBJECTIVE_OFFSET) == 8;
}

static bool IsMissionBreakItemBoxObjective(const RacedataScenario& scenario) {
    return IsMissionScenario(scenario) &&
           GetMissionU16(scenario.mission, MISSION_OBJECTIVE_OFFSET) == MISSION_OBJECTIVE_BREAK_ITEM_BOXES;
}

static void PreventMissionItemBoxRespawn(Objects::Itembox* itembox) {
    register u32 itemBoxPtr;
    asm(mr itemBoxPtr, r3;);

    if (itembox != nullptr && Racedata::sInstance != nullptr &&
        IsMissionBreakItemBoxObjective(Racedata::sInstance->racesScenario)) {
        itembox->respawnTime = ITEMBOX_NO_RESPAWN_TIME;
    }

    asmVolatile(mr r3, itemBoxPtr; lwz r4, 0xb4(r3););
}

kmCall(0x808288b4, PreventMissionItemBoxRespawn);

extern "C" u32 sMissionCoinAddIntroBranch;
static u32 AddMissionCoin(void* coinManager, const KMP::Holder<GOBJ>* object) {
    typedef u32 (*AddCoinFn)(void*, const KMP::Holder<GOBJ>*);
    static const AddCoinFn sAddCoin = reinterpret_cast<AddCoinFn>(&sMissionCoinAddIntroBranch);

    if (Racedata::sInstance != nullptr && object != nullptr && object->raw != nullptr &&
        coinManager != nullptr && IsMissionCoinObjective(Racedata::sInstance->racesScenario)) {
        RacedataSettings& settings = Racedata::sInstance->racesScenario.settings;
        const GameType oldGameType = settings.gametype;
        object->raw->settings[0] = 1;
        object->raw->settings[1] = 0;
        object->raw->settings[2] = 1;
        if (static_cast<u32>(oldGameType) == COIN_ADD_INTRO_GUARD_VALUE)
            settings.gametype = GAMETYPE_DEFAULT;
        const u32 result = sAddCoin(coinManager, object);
        settings.gametype = oldGameType;
        return result;
    }

    return sAddCoin(coinManager, object);
}

kmCall(0x808277a0, AddMissionCoin);

}
}
