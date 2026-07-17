#include <Gamemodes/MissionMode/MissionMode.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/KMP/KMPManager.hpp>
#include <runtimeWrite.hpp>

namespace Pulsar {
namespace MissionMode {

static const u32 MISSION_OBJECTIVE_OFFSET = 0x02;
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

kmRuntimeUse(0x8087bacc);
kmRuntimeUse(0x8087baf4);
static u32 AddMissionCoin(void* coinManager, const KMP::Holder<GOBJ>* object) {
    typedef u32 (*AddCoinFn)(void*, const KMP::Holder<GOBJ>*);
    static const AddCoinFn sAddCoin = reinterpret_cast<AddCoinFn>(kmRuntimeAddr(0x8087bacc));

    if (Racedata::sInstance != nullptr && object != nullptr && object->raw != nullptr &&
        coinManager != nullptr && IsMissionCoinObjective(Racedata::sInstance->racesScenario)) {
        object->raw->settings[0] = 1;
        object->raw->settings[1] = 0;
        object->raw->settings[2] = 1;
        const bool skippedIntroGuard = KamekRuntimeWrite::CondWrite32(kmRuntimeAddr(0x8087baf4),
                                                                       COIN_ADD_INTRO_BRANCH_ORIGINAL,
                                                                       COIN_ADD_SKIP_INTRO_BRANCH);
        const u32 result = sAddCoin(coinManager, object);
        if (skippedIntroGuard)
            KamekRuntimeWrite::Write32(kmRuntimeAddr(0x8087baf4), COIN_ADD_INTRO_BRANCH_ORIGINAL);
        return result;
    }

    return sAddCoin(coinManager, object);
}

kmCall(0x808277a0, AddMissionCoin);

}
}
