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
static const u32 COIN_ADD_INTRO_BRANCH_ORIGINAL = 0x4082000C;
static const u32 COIN_ADD_SKIP_INTRO_BRANCH = 0x4800000C;

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

    // The replaced instruction was lwz r4, 0xb4(r3), so restore both live registers.
    asmVolatile(mr r3, itemBoxPtr; lwz r4, 0xb4(r3););
}

// Itembox::Update checks timer against respawnTime before bringing a broken box back.
kmCall(0x808288b4, PreventMissionItemBoxRespawn);

extern "C" u32 sMissionCoinAddIntroBranch; 
extern "C" u32 sMissionCoinAddSkipIntroBranch;
static u32 AddMissionCoin(void* coinManager, const KMP::Holder<GOBJ>* object) {
    typedef u32 (*AddCoinFn)(void*, const KMP::Holder<GOBJ>*);
    static const AddCoinFn sAddCoin = reinterpret_cast<AddCoinFn>(sMissionCoinAddIntroBranch);

    if (Racedata::sInstance != nullptr && object != nullptr && object->raw != nullptr &&
        coinManager != nullptr && IsMissionCoinObjective(Racedata::sInstance->racesScenario)) {
        object->raw->settings[0] = 1;
        object->raw->settings[1] = 0;
        object->raw->settings[2] = 1;
        const bool skippedIntroGuard = (sMissionCoinAddSkipIntroBranch, COIN_ADD_INTRO_BRANCH_ORIGINAL, COIN_ADD_SKIP_INTRO_BRANCH);
        const u32 result = sAddCoin(coinManager, object);
        if (skippedIntroGuard)
            sMissionCoinAddSkipIntroBranch, COIN_ADD_INTRO_BRANCH_ORIGINAL;
        return result;
    }

    return sAddCoin(coinManager, object);
}

kmCall(0x808277a0, AddMissionCoin);

}
}
