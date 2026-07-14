#include <kamek.hpp>
#include <runtimeWrite.hpp>
#include <Race/CustomItems.hpp>
#include <MarioKartWii/Item/ItemManager.hpp>
#include <MarioKartWii/Item/ItemSlot.hpp>
#include <MarioKartWii/Item/ItemBehaviour.hpp>
#include <MarioKartWii/Item/ItemPlayer.hpp>
#include <Gamemodes/MissionMode/MissionMode.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <core/rvl/OS/OS.hpp>
#include <Settings/Settings.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <PulsarSystem.hpp>

namespace Pulsar {
namespace Race {

static const u32 ITEM_COUNT = 19;
static const u32 ALL_ITEMS_BITFIELD = 0x7FFFF;
static const u32 PLAYER_OBJ_SLOT_COUNT = 3;

u32 GetEffectiveCustomItemsBitfield() {
    if (Racedata::sInstance != nullptr) {
        const RacedataScenario &raceScenario = Racedata::sInstance->racesScenario;
        const RacedataScenario &menuScenario = Racedata::sInstance->menusScenario;
        const RacedataScenario *mission = nullptr;
        if (MissionMode::IsMissionScenario(raceScenario)) mission = &raceScenario;
        else if (MissionMode::IsMissionScenario(menuScenario)) mission = &menuScenario;
        if (mission != nullptr) {
            if (MissionMode::HasMissionFeature(*mission, MissionMode::CUSTOM_ITEMS_OVERRIDE))
                return MissionMode::GetMissionCustomItems(*mission) & ALL_ITEMS_BITFIELD;
            return ALL_ITEMS_BITFIELD;
        }
    }
    const RKNet::Controller *controller = RKNet::Controller::sInstance;
    if (controller == nullptr) return Settings::Mgr::Get().GetCustomItems();

    const RKNet::RoomType roomType = controller->roomType;
    const bool isFriendRoom = roomType == RKNet::ROOMTYPE_FROOM_HOST || roomType == RKNet::ROOMTYPE_FROOM_NONHOST;
    if (!isFriendRoom) {
        if (roomType != RKNet::ROOMTYPE_NONE) return ALL_ITEMS_BITFIELD;
        return Settings::Mgr::Get().GetCustomItems();
    }

    if (System::sInstance == nullptr) return Settings::Mgr::Get().GetCustomItems();
    if (System::sInstance->IsVanillaMode()) return ALL_ITEMS_BITFIELD;
    return System::sInstance->netMgr.customItemsBitfield;
}

static bool sFallbackItemDropFix[12];

extern "C" void __ptmf_scall(Item::PlayerObj *playerObj, const Ptmf_0A<Item::PlayerObj, void> *ptmf);
extern "C" Item::ItemSlotData *itemSlotData;

static void ClearPlayerObjUse(Item::PlayerObj &playerObj) {
    playerObj.itemObjId = OBJ_NONE;
    playerObj.itemId = ITEM_NONE;
    playerObj.useType = Item::PlayerObj::NO_ITEM;
    for (u32 i = 0; i < PLAYER_OBJ_SLOT_COUNT; ++i) playerObj.usedObjs[i] = nullptr;
    playerObj.activeItemCount = 0;
    playerObj.unknown_0x54 = static_cast<ItemObjId>(0);
}

static bool HasSpawnedPlayerObjs(const Item::PlayerObj &playerObj) {
    if (playerObj.useType == Item::PlayerObj::ONLY_USE) return true;
    if (playerObj.activeItemCount == 0 || playerObj.activeItemCount > PLAYER_OBJ_SLOT_COUNT) return false;

    for (u32 i = 0; i < playerObj.activeItemCount; ++i) {
        if (playerObj.usedObjs[i] == nullptr) return false;
    }
    return true;
}

static void CallPlayerObjPtmfIfValid(Item::PlayerObj *playerObj, const Ptmf_0A<Item::PlayerObj, void> *ptmf) {
    if (playerObj == nullptr || ptmf == nullptr) return;

    if (!HasSpawnedPlayerObjs(*playerObj)) {
        ClearPlayerObjUse(*playerObj);
        return;
    }

    __ptmf_scall(playerObj, ptmf);
}

static void RotateSpawnedObjQueue(Item::ObjHolder &holder) {
    if (holder.spawnedCount <= 1) return;

    Item::Obj *first = holder.itemObj[0];
    for (u32 i = 0; i < holder.spawnedCount - 1; ++i) {
        holder.itemObj[i] = holder.itemObj[i + 1];
    }
    holder.itemObj[holder.spawnedCount - 1] = first;
}

static bool FreeOneSpawnedObj(Item::ObjHolder &holder) {
    if (holder.itemObj == nullptr || holder.spawnedCount == 0) return false;

    const u32 prevBodyCount = holder.bodyCount;
    const u32 prevSpawnedCount = holder.spawnedCount;
    Item::Obj *oldestObj = holder.itemObj[0];
    if (oldestObj == nullptr) return false;

    holder.OnObjKillFinish(oldestObj);
    RotateSpawnedObjQueue(holder);
    return holder.bodyCount < prevBodyCount || holder.spawnedCount < prevSpawnedCount;
}

static Item::PlayerObj *GetPlayerObjFromUsedObjs(Item::Obj **usedObjs) {
    // ObjHolder::Spawn receives &PlayerObj::usedObjs[0]; usedObjs starts at 0x20.
    return reinterpret_cast<Item::PlayerObj *>(reinterpret_cast<u8 *>(usedObjs) - 0x20);
}

static void SafePlayerObjSpawn(Item::ObjHolder *holder, u32 quantity, Item::Obj **usedObjs, u8 playerId, const Vec3 &playerPos, bool r8) {
    if (usedObjs != nullptr) {
        for (u32 i = 0; i < PLAYER_OBJ_SLOT_COUNT; ++i) usedObjs[i] = nullptr;
    }

    Item::PlayerObj *playerObj = nullptr;
    if (usedObjs != nullptr) playerObj = GetPlayerObjFromUsedObjs(usedObjs);
    if (holder == nullptr || holder->itemObj == nullptr || usedObjs == nullptr || playerObj == nullptr) {
        if (playerObj != nullptr) ClearPlayerObjUse(*playerObj);
        return;
    }

    const u32 requestedCount = quantity > PLAYER_OBJ_SLOT_COUNT ? PLAYER_OBJ_SLOT_COUNT : quantity;
    u32 spawnedCount = 0;
    while (spawnedCount < requestedCount) {
        while (holder->bodyCount >= holder->capacity) {
            if (!FreeOneSpawnedObj(*holder)) break;
        }
        if (holder->bodyCount >= holder->capacity) break;

        Item::Obj *spawnedObj = holder->itemObj[holder->bodyCount];
        if (spawnedObj == nullptr) break;

        usedObjs[spawnedCount] = spawnedObj;
        ++holder->bodyCount;
        spawnedObj->Spawn(holder->itemObjId, playerId, playerPos, r8);
        ++spawnedCount;
    }

    playerObj->activeItemCount = spawnedCount;
    if (spawnedCount == 0) ClearPlayerObjUse(*playerObj);
}

static bool IsItemAvailable(ItemId id, const Item::ItemSlotData *slotData) {
    if (id >= ITEM_COUNT) return false;

    const u32 bitfield = GetEffectiveCustomItemsBitfield();
    if (bitfield != 0 && bitfield != ALL_ITEMS_BITFIELD && ((bitfield >> id) & 1)) return true;

    return Item::Manager::IsThereCapacityForItem(id);
}

static bool IsRestrictedFallbackItem(ItemId id) {
    return id == LIGHTNING || id == BULLET_BILL || id == POW_BLOCK || id == BLOOPER || id == BLUE_SHELL;
}

static ItemId GetRandomItemFromRow(u32 rowIndex, const Item::ItemSlotData::Probabilities &itemProbabilities, u32 bitfield,
                                   Item::ItemSlotData *slotData, bool excludeRestrictedItems) {
    if (itemProbabilities.probabilities == nullptr || itemProbabilities.rowCount == 0) return MUSHROOM;
    if (rowIndex >= itemProbabilities.rowCount) rowIndex = itemProbabilities.rowCount - 1;

    const u16 *row = &itemProbabilities.probabilities[rowIndex * ITEM_COUNT];

    ItemId candidates[ITEM_COUNT];
    u16 weights[ITEM_COUNT];
    u32 candidateCount = 0;
    u32 totalProbability = 0;
    for (u32 item = 0; item < ITEM_COUNT; ++item) {
        const ItemId id = static_cast<ItemId>(item);
        if (((bitfield >> item) & 1) && row[item] > 0 &&
            (!excludeRestrictedItems || !IsRestrictedFallbackItem(id)) &&
            IsItemAvailable(id, slotData)) {
            candidates[candidateCount] = id;
            weights[candidateCount] = row[item];
            totalProbability += weights[candidateCount];
            ++candidateCount;
        }
    }

    if (totalProbability == 0) return MUSHROOM;

    static u32 randomSeed = 0;
    if (randomSeed == 0) randomSeed = OS::GetTick();
    randomSeed = randomSeed * 1103515245 + 12345;
    u32 roll = (randomSeed >> 16) % totalProbability;

    for (u32 candidate = 0; candidate < candidateCount; ++candidate) {
        if (roll < weights[candidate]) return candidates[candidate];
        roll -= weights[candidate];
    }

    return MUSHROOM;
}

static u32 GetItemTableRow(u32 position, u16 setting) {
    if (setting != 0) return setting - 1;
    return position > 0 ? position - 1 : 0;
}

static const Item::ItemSlotData::Probabilities *GetItemProbabilities(const Item::ItemSlotData &slotData,
                                                                     bool isHuman, u16 setting) {
    if (setting != 0) return &slotData.specialChances;
    return isHuman ? &slotData.playerChances : &slotData.cpuChances;
}

static bool ShouldExcludeRestrictedFallbackItems(u32 bitfield) {
    if (bitfield == ALL_ITEMS_BITFIELD) return true;

    u32 enabledCount = 0;
    for (u32 item = 0; item < ITEM_COUNT; ++item) {
        if ((bitfield >> item) & 1) ++enabledCount;
    }
    return enabledCount > 6;
}

static ItemId GetItemFromTable(u32 position, bool isHuman, u16 setting, bool isFallback) {
    u32 bitfield = GetEffectiveCustomItemsBitfield();
    if (bitfield == 0) bitfield = ALL_ITEMS_BITFIELD;

    Item::ItemSlotData *slotData = itemSlotData;
    if (slotData == nullptr) return MUSHROOM;

    const Item::ItemSlotData::Probabilities *probabilities = GetItemProbabilities(*slotData, isHuman, setting);
    const bool excludeRestrictedItems = isFallback && ShouldExcludeRestrictedFallbackItems(bitfield);
    return GetRandomItemFromRow(GetItemTableRow(position, setting), *probabilities, bitfield, slotData,
                                excludeRestrictedItems);
}

static u8 GetPlayerPosition(Item::Player *itemPlayer, u8 fallbackPosition) {
    if (itemPlayer == nullptr || itemPlayer->id >= 12 || Raceinfo::sInstance == nullptr) return fallbackPosition;

    const RaceinfoPlayer *racePlayer = Raceinfo::sInstance->players[itemPlayer->id];
    return racePlayer != nullptr ? racePlayer->position : fallbackPosition;
}

static ItemId GetFallbackItem(Item::Player *itemPlayer, u8 fallbackPosition, bool isHuman, u16 setting) {
    const u8 position = GetPlayerPosition(itemPlayer, fallbackPosition);
    return GetItemFromTable(position, isHuman, setting, true);
}

static ItemId GetFallbackItem(Item::PlayerRoulette *roulette) {
    return GetFallbackItem(roulette->itemPlayer, roulette->position,
                           roulette->itemPlayer != nullptr && roulette->itemPlayer->isHuman, roulette->setting);
}

static u32 GetBestPlacement(const Item::ItemSlotData::Probabilities *probabilities, u32 currentPlacement) {
    if (probabilities == nullptr || probabilities->probabilities == nullptr || probabilities->rowCount == 0) {
        return currentPlacement;
    }

    const u32 bitfield = GetEffectiveCustomItemsBitfield();
    if (bitfield == ALL_ITEMS_BITFIELD || bitfield == 0) return currentPlacement;

    const u32 rowCount = probabilities->rowCount;
    if (currentPlacement >= rowCount) currentPlacement = rowCount - 1;

    const u16 *probabilityTable = probabilities->probabilities;
    Item::ItemSlotData *slotData = itemSlotData;

    for (s32 distance = 0; distance < static_cast<s32>(rowCount); ++distance) {
        const s32 placements[2] = {static_cast<s32>(currentPlacement) - distance,
                                   static_cast<s32>(currentPlacement) + distance};
        for (u32 side = 0; side < 2; ++side) {
            const s32 placement = placements[side];
            if (side == 1 && placements[0] == placements[1]) continue;
            if (placement < 0 || placement >= static_cast<s32>(rowCount)) continue;

            for (u32 item = 0; item < ITEM_COUNT; ++item) {
                if (((bitfield >> item) & 1) && probabilityTable[placement * ITEM_COUNT + item] > 0 &&
                    IsItemAvailable(static_cast<ItemId>(item), slotData)) {
                    return static_cast<u32>(placement);
                }
            }
        }
    }

    return currentPlacement;
}

static asmFunc AdjustPlacement() {
    ASM(
        nofralloc;
        stwu r1, -0x20(r1);
        mflr r0;
        stw r0, 0x24(r1);
        stw r5, 0x8(r1);

        mr r3, r5;
        mr r4, r21;
        bl GetBestPlacement;
        mr r21, r3;

        lwz r5, 0x8(r1);
        lwz r0, 0x24(r1);
        mtlr r0;
        addi r1, r1, 0x20;

        // Replaced instruction
        lis r3, -0x7f64;
        blr;)
}
kmCall(0x807bb614, AdjustPlacement);

static void CustomLimitCheck() {
    register int itemCount;
    register int itemLimit;
    register u32 itemId;
    asm {
        mr itemCount, r3
        mr itemLimit, r0
        mr itemId, r21
    }

    u32 bitfield = GetEffectiveCustomItemsBitfield();
    if (bitfield == 0) bitfield = ALL_ITEMS_BITFIELD;

    if (itemId < ITEM_COUNT) {
        if (!((bitfield >> itemId) & 1)) {
            itemLimit = 0;
        } else if (bitfield != ALL_ITEMS_BITFIELD) {
            itemLimit = 100;
        }
    }

    asm {
        mr r3, itemCount
        mr r0, itemLimit
        subf. r0, r3, r0
    }
}
kmBranch(0x807bb7d8, CustomLimitCheck);
kmPatchExitPoint(CustomLimitCheck, 0x807bb7dc);  // Return to the ble instruction

static void CalcItemFallback() {
    register Item::PlayerRoulette *roulette;
    asm(mr roulette, r31);
    roulette->nextItemId = GetFallbackItem(roulette);
}
kmBranch(0x807ba48c, CalcItemFallback);
kmPatchExitPoint(CalcItemFallback, 0x807ba494);

static ItemId DecideItemFallback() {
    register ItemId itemId;
    asm(mr itemId, r24);
    if (itemId != ITEM_NONE) return itemId;

    register u32 position;
    register bool isHuman;
    register u32 itemBoxType;
    register Item::Player *itemPlayer;
    asm {
        mr position, r21
        mr isHuman, r20
        mr itemBoxType, r22
        mr itemPlayer, r18
    }
    const ItemId fallbackItem = GetFallbackItem(itemPlayer, static_cast<u8>(position), isHuman,
                                                static_cast<u16>(itemBoxType));
    if (itemPlayer != nullptr && itemPlayer->id < 12) sFallbackItemDropFix[itemPlayer->id] = true;
    return fallbackItem;
}
kmWrite32(0x807bb8b4, 0x3B000014);  // li r24, 0x14
kmCall(0x807bb8b8, DecideItemFallback);

// Restore original probability sum logic (removes legacy partial filtering)
kmWrite32(0x807bb83c, 0x7ED60214);

// Infinite loop fix for ItemHolderItem_spawn
kmWrite32(0x80795e4c, 0x408100C8);
kmCall(0x80791a48, SafePlayerObjSpawn);
kmCall(0x80791b28, CallPlayerObjPtmfIfValid);
kmCall(0x807923ac, CallPlayerObjPtmfIfValid);

static void InitItemFallback1() {
    register Item::PlayerRoulette *roulette;
    asm(mr roulette, r23);
    roulette->nextItemId = GetFallbackItem(roulette);
}
kmBranch(0x807ba138, InitItemFallback1);
kmPatchExitPoint(InitItemFallback1, 0x807ba140);

static void InitItemFallback2() {
    register Item::PlayerRoulette *roulette;
    asm(mr roulette, r23);
    roulette->nextItemId = GetFallbackItem(roulette);
}
kmBranch(0x807ba194, InitItemFallback2);
kmPatchExitPoint(InitItemFallback2, 0x807ba19c);

static ItemId DecideRouletteItemFiltered(Item::ItemSlotData *slotData, u16 itemBoxType, u8 position, ItemId prevRandomItem, bool r7) {
    const u32 bitfield = GetEffectiveCustomItemsBitfield();
    if (bitfield == ALL_ITEMS_BITFIELD) {
        return slotData->DecideRouletteItem(itemBoxType, position, prevRandomItem, r7);
    }

    return GetItemFromTable(position, true, itemBoxType, false);
}
kmCall(0x807ba428, DecideRouletteItemFiltered);

static void SetItemFix(Item::PlayerInventory &inventory, ItemId id, bool isItemForcedDueToCapacity) {
    Item::Player *itemPlayer = inventory.itemPlayer;
    if (itemPlayer) {
        const u8 playerId = itemPlayer->id;
        if (playerId < 12 && sFallbackItemDropFix[playerId]) {
            isItemForcedDueToCapacity = false;
            sFallbackItemDropFix[playerId] = false;
        }
    }

    inventory.currentItemId = id;
    inventory.currentItemCount = Item::Behavior::behaviourTable[id].numberOfItems;
    inventory.loseDelayDueToDmg = 0;
    inventory.isItemForcedDueToCapacity = isItemForcedDueToCapacity;
    inventory.hasGolden = Item::Behavior::behaviourTable[id].unknown_0x10;
    for (u32 i = 0; i < sizeof(inventory.unknown_0x1D); ++i) inventory.unknown_0x1D[i] = 0;
    inventory.goldenTimer = 0;
    for (u32 i = 0; i < sizeof(inventory.unknown_0x24); ++i) inventory.unknown_0x24[i] = 0;
}
kmBranch(0x807bc940, SetItemFix);

static void EjectItemsFromDamageSafely(Item::PlayerInventory &inventory) {
    if (inventory.currentItemId >= ITEM_COUNT || inventory.currentItemCount == 0 ||
        !Item::Manager::IsThereCapacityForItem(inventory.currentItemId)) {
        inventory.ClearAll();
        return;
    }
    inventory.EjectItems();
}
kmCall(0x807bc6c4, EjectItemsFromDamageSafely);

}  // namespace Race
}  // namespace Pulsar
