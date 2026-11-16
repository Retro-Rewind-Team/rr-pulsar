#include <kamek.hpp>
#include <Gamemodes/ItemRain/ItemRain.hpp>
#include <Gamemodes/BattleRoyale/BattleRoyale.hpp>
#include <MarioKartWii/3D/Model/ModelDirector.hpp>
#include <MarioKartWii/Item/ItemManager.hpp>
#include <MarioKartWii/Item/ItemSlot.hpp>
#include <MarioKartWii/Item/Obj/Bomb.hpp>
#include <MarioKartWii/Item/Obj/ObjProperties.hpp>
#include <MarioKartWii/Kart/KartBody.hpp>
#include <MarioKartWii/Kart/KartManager.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <PulsarSystem.hpp>

namespace Pulsar {
namespace ItemRain {

static void SpawnItemRain() {
    Item::Manager *manager = Item::Manager::sInstance;
    const Raceinfo *race = Raceinfo::sInstance;
    const System *system = System::sInstance;
    if (race->stage < RACESTAGE_RACE || !race->timerMgr->hasRaceStarted) {
        memset(spawnedItems, 0, sizeof(spawnedItems));
        nextSpawnedItem = 0;
        lastRaceFrame = 0;
        return;
    }
    const u32 frame = race->timerMgr->raceFrameCounter;
    if (frame < lastRaceFrame) {
        memset(spawnedItems, 0, sizeof(spawnedItems));
        nextSpawnedItem = 0;
    }
    lastRaceFrame = frame;
    const bool storm = system->IsContext(PULSAR_ITEMMODESTORM);
    if (!IsItemRainEnabled())
        return;

    const u32 maxFrames = storm ? 220 : 300;
    for (u32 type = 0; type < 15; ++type) {
        Item::ObjHolder &holder = manager->itemObjHolders[type];
        for (u32 i = 0; i < holder.spawnedCount;) {
            Item::Obj *obj = holder.itemObj[i];
            bool removed = false;
            for (u32 j = 0; j < sizeof(spawnedItems) / sizeof(spawnedItems[0]); ++j) {
                SpawnedItem &spawned = spawnedItems[j];
                if (spawned.obj == obj && spawned.eventBitfield == obj->eventBitfield && frame - spawned.frame >= maxFrames && (obj->bitfield78 & 0xc0) == 0) {
                    obj->DisappearDueToExcess(false);
                    holder.OnObjKillFinish(obj);
                    spawned.obj = nullptr;
                    removed = true;
                    break;
                }
            }
            if (!removed)
                ++i;
        }
    }
    if (storm ? (frame * 9) % 16 >= 9 : frame % 4 != 0)
        return;

    const RacedataScenario &scenario = Racedata::sInstance->racesScenario;
    const u8 playerId = scenario.settings.hudPlayerIds[0];
    if (playerId >= scenario.playerCount || scenario.players[playerId].playerType != PLAYER_REAL_LOCAL)
        return;
    Kart::Player *kart = Kart::Manager::sInstance->GetKartPlayer(playerId);
    const Mtx34 &matrix = kart->pointers.kartBody->kartPhysicsHolder->transforMtx;
    Vec3 facing(matrix.n02, 0.0f, matrix.n22);
    if (facing.x * facing.x + facing.z * facing.z < 0.000001f)
        return;
    facing.Normalize();
    const float x = facing.x;
    const float z = facing.z;

    Random &random = race->timerMgr->random;
    const Timer &raceTime = race->timerMgr->timers[0];
    const bool beforeThirtySeconds = frame < 1800 || (raceTime.minutes == 0 && raceTime.seconds < 30);
    u32 availableWeight = 0;
    for (u32 i = 0; i < sizeof(itemOdds) / sizeof(itemOdds[0]); ++i) {
        const ItemObjId item = itemOdds[i].item == OBJ_LIGHTNING && beforeThirtySeconds ? OBJ_MUSHROOM : itemOdds[i].item;
        const Item::ObjHolder &holder = manager->itemObjHolders[item];
        if (holder.GetTotalItemCount() < static_cast<int>(holder.capacity2) && holder.bodyCount < holder.capacity)
            availableWeight += itemOdds[i].weight;
    }
    while (availableWeight == 0) {
        Item::Obj *oldest = nullptr;
        SpawnedItem *oldestSpawn = nullptr;
        for (u32 type = 0; type < 15; ++type) {
            if (type == OBJ_LIGHTNING && beforeThirtySeconds)
                continue;
            Item::ObjHolder &holder = manager->itemObjHolders[type];
            for (u32 i = 0; i < holder.spawnedCount; ++i) {
                Item::Obj *obj = holder.itemObj[i];
                if ((obj->bitfield78 & 0xc0) != 0)
                    continue;
                for (u32 j = 0; j < sizeof(spawnedItems) / sizeof(spawnedItems[0]); ++j) {
                    SpawnedItem &spawned = spawnedItems[j];
                    if (spawned.obj == obj && spawned.eventBitfield == obj->eventBitfield && (oldestSpawn == nullptr || spawned.frame < oldestSpawn->frame)) {
                        oldest = obj;
                        oldestSpawn = &spawned;
                    }
                }
            }
        }
        if (oldest == nullptr)
            return;
        Item::ObjHolder &holder = manager->itemObjHolders[oldest->itemObjId];
        oldest->DisappearDueToExcess(false);
        holder.OnObjKillFinish(oldest);
        oldestSpawn->obj = nullptr;
        for (u32 i = 0; i < sizeof(itemOdds) / sizeof(itemOdds[0]); ++i) {
            const ItemObjId item = itemOdds[i].item == OBJ_LIGHTNING && beforeThirtySeconds ? OBJ_MUSHROOM : itemOdds[i].item;
            const Item::ObjHolder &candidate = manager->itemObjHolders[item];
            if (candidate.GetTotalItemCount() < static_cast<int>(candidate.capacity2) && candidate.bodyCount < candidate.capacity)
                availableWeight += itemOdds[i].weight;
        }
    }

    u32 roll = random.NextLimited(availableWeight);
    ItemObjId item = OBJ_MUSHROOM;
    for (u32 i = 0; i < sizeof(itemOdds) / sizeof(itemOdds[0]); ++i) {
        const ItemObjId candidate = itemOdds[i].item == OBJ_LIGHTNING && beforeThirtySeconds ? OBJ_MUSHROOM : itemOdds[i].item;
        const Item::ObjHolder &holder = manager->itemObjHolders[candidate];
        if (holder.GetTotalItemCount() >= static_cast<int>(holder.capacity2) || holder.bodyCount >= holder.capacity)
            continue;
        if (roll < itemOdds[i].weight) {
            item = candidate;
            break;
        }
        roll -= itemOdds[i].weight;
    }
    Item::ObjHolder &holder = manager->itemObjHolders[item];

    const Vec3 &playerPos = kart->GetPosition();
    const float forward = 3000.0f + random.NextFloatLimited(8500.0f);
    const float sideways = random.NextFloatLimited(16000.0f) - 8000.0f;
    Vec3 pos(playerPos.x + x * forward + z * sideways, playerPos.y + 2500.0f, playerPos.z + z * forward - x * sideways);
    Vec3 direction(0.0f, 0.0f, 0.0f);
    const u32 previousCount = holder.spawnedCount;
    spawningRain = true;
    manager->CreateItemDirect(item, &pos, &direction, playerId);
    spawningRain = false;
    Item::Obj *obj = holder.itemObj[previousCount];
    // Direct spawns are local: do not send an EVENT for an object other consoles do not have.
    obj->bitfield7c &= ~0x20;
    if (BattleRoyale::ShouldApplyBattleRoyale() && obj->item_light != nullptr)
        obj->item_light->ToggleVisible(true);
    spawnedItems[nextSpawnedItem].obj = obj;
    spawnedItems[nextSpawnedItem].frame = frame;
    spawnedItems[nextSpawnedItem].eventBitfield = obj->eventBitfield;
    nextSpawnedItem = (nextSpawnedItem + 1) % (sizeof(spawnedItems) / sizeof(spawnedItems[0]));
}
static RaceFrameHook spawnItemRainHook(SpawnItemRain);

static void ActivateBobombs(Item::Manager *manager, ItemObjId item, Matrix34f *matrix, Vec3 *direction, u32 playerId) {
    Item::ObjHolder &holder = manager->itemObjHolders[item];
    const u32 previousCount = holder.spawnedCount;
    // Choose the owner before bubble and FIB initialization, past CreateItemDirect's remote-player check.
    if (spawningRain)
        playerId = Raceinfo::sInstance->timerMgr->random.NextLimited(Racedata::sInstance->racesScenario.playerCount);
    manager->GenerateItemDirect(item, matrix, direction, playerId);
    if (spawningRain && item == OBJ_BOBOMB) {
        Item::ObjBomb *bomb = static_cast<Item::ObjBomb *>(holder.itemObj[previousCount]);
        bomb->ObjExplosion::state = Item::ObjBomb::STATE_MOVING;
        bomb->ObjExplosion::nextState = Item::ObjBomb::STATE_MOVING;
        bomb->ObjExplosion::timer = 0;
        bomb->timer = 240;
        bomb->bitfield78 |= 0x2000000;
        bomb->updatePtmf = Item::ObjProperties::objProperties[OBJ_BOBOMB].updateObjThunkPtmf;
    }
}
kmCall(0x8079a110, ActivateBobombs);

static ItemId ReplaceItemBoxResult(Item::ItemSlotData *slotData, u16 itemBoxType, u8 position, ItemId previousItem, bool isSpecialBox) {
    register Item::PlayerRoulette *roulette;
    asm(mr roulette, r23);

    if (IsItemRainEnabled()) {
        switch (roulette->nextItemId) {
            case GREEN_SHELL:
            case RED_SHELL:
            case TRIPLE_GREEN_SHELL:
            case TRIPLE_RED_SHELL:
            case BANANA:
            case TRIPLE_BANANA:
            case BOBOMB:
            case BLUE_SHELL:
            case FAKE_ITEM_BOX:
                roulette->nextItemId = MUSHROOM;
                break;
        }
    }
    return slotData->DecideRouletteItem(itemBoxType, position, previousItem, isSpecialBox);
}
kmCall(0x807ba1e4, ReplaceItemBoxResult);

}  // namespace ItemRain
}  // namespace Pulsar
