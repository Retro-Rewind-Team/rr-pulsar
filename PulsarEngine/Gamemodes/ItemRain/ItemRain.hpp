#include <MarioKartWii/System/Identifiers.hpp>
#include <PulsarSystem.hpp>
#include <GlobalFunction.hpp>

namespace Item {
class Obj;
}

namespace Pulsar {
namespace ItemRain {

inline bool IsItemRainEnabled() {
    const System *system = System::sInstance;
    const GameMode mode = Racedata::sInstance->racesScenario.settings.gamemode;
    return !IsTimeTrialOrGhostMode(mode) && !system->IsContext(PULSAR_MODE_OTT) && (system->IsContext(PULSAR_ITEMMODERAIN) || system->IsContext(PULSAR_ITEMMODESTORM));
}

struct SpawnedItem {
    Item::Obj *obj;
    u32 frame;
    u16 eventBitfield;
};

static SpawnedItem spawnedItems[256];
static u32 nextSpawnedItem;
static u32 lastRaceFrame;
static bool spawningRain;

struct ItemOdds {
    u8 weight;
    ItemObjId item;
};

static const ItemOdds itemOdds[] = {
    {20, OBJ_MUSHROOM},
    {14, OBJ_BANANA},
    {12, OBJ_MEGA_MUSHROOM},
    {10, OBJ_GREEN_SHELL},
    {9, OBJ_STAR},
    {8, OBJ_FAKE_ITEM_BOX},
    {7, OBJ_GOLDEN_MUSHROOM},
    {7, OBJ_RED_SHELL},
    {3, OBJ_BULLET_BILL},
    {3, OBJ_BOBOMB},
    {3, OBJ_POW_BLOCK},
    {3, OBJ_BLUE_SHELL},
    {1, OBJ_LIGHTNING},
};

}  // namespace ItemRain
}  // namespace Pulsar
