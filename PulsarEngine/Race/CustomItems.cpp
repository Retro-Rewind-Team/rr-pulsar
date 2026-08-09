#include <kamek.hpp>
#include <runtimeWrite.hpp>
#include <Race/CustomItems.hpp>
#include <MarioKartWii/Item/ItemManager.hpp>
#include <MarioKartWii/Item/ItemSlot.hpp>
#include <MarioKartWii/Item/ItemBehaviour.hpp>
#include <MarioKartWii/Item/ItemPlayer.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <core/rvl/OS/OS.hpp>
#include <Settings/Settings.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <PulsarSystem.hpp>

namespace Pulsar {
namespace Race {

static const u32 ITEM_COUNT = 19;
static const u32 VANILLA_ITEM_BITFIELD = 0x7FFFF;

u32 Pulsar::Race::GetEffectiveCustomItemsBitfield() {
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    if (controller != nullptr) {
        const RKNet::RoomType roomType = controller->roomType;
        if (roomType == RKNet::ROOMTYPE_FROOM_HOST || roomType == RKNet::ROOMTYPE_FROOM_NONHOST) {
            if (System::sInstance != nullptr) {
                if (System::sInstance->IsVanillaMode()) return VANILLA_ITEM_BITFIELD;
                return System::sInstance->netMgr.customItemsBitfield;
            }
        } else if (roomType != RKNet::ROOMTYPE_NONE) {
            return VANILLA_ITEM_BITFIELD;
        }
    }
    return Settings::Mgr::Get().GetCustomItems();
}

static bool sFallbackItemDropFix[12];
static const u32 PLAYER_OBJ_SLOT_COUNT = 3;

extern "C" void __ptmf_scall(Item::PlayerObj* playerObj, const Ptmf_0A<Item::PlayerObj, void>* ptmf);

static void ClearPlayerObjUse(Item::PlayerObj& playerObj) {
    playerObj.itemObjId = OBJ_NONE;
    playerObj.itemId = ITEM_NONE;
    playerObj.useType = Item::PlayerObj::NO_ITEM;
    for (u32 i = 0; i < PLAYER_OBJ_SLOT_COUNT; ++i) playerObj.usedObjs[i] = nullptr;
    playerObj.activeItemCount = 0;
    playerObj.unknown_0x54 = static_cast<ItemObjId>(0);
}

static bool HasSpawnedPlayerObjs(const Item::PlayerObj& playerObj) {
    if (playerObj.useType == Item::PlayerObj::ONLY_USE) return true;
    if (playerObj.activeItemCount == 0 || playerObj.activeItemCount > PLAYER_OBJ_SLOT_COUNT) return false;

    for (u32 i = 0; i < playerObj.activeItemCount; ++i) {
        if (playerObj.usedObjs[i] == nullptr) return false;
    }
    return true;
}

static void CallPlayerObjPtmfIfValid(Item::PlayerObj* playerObj, const Ptmf_0A<Item::PlayerObj, void>* ptmf) {
    if (playerObj == nullptr || ptmf == nullptr) return;

    if (!HasSpawnedPlayerObjs(*playerObj)) {
        ClearPlayerObjUse(*playerObj);
        return;
    }

    __ptmf_scall(playerObj, ptmf);
}

static void RotateSpawnedObjQueue(Item::ObjHolder& holder) {
    if (holder.spawnedCount <= 1) return;

    Item::Obj* first = holder.itemObj[0];
    for (u32 i = 0; i < holder.spawnedCount - 1; ++i) {
        holder.itemObj[i] = holder.itemObj[i + 1];
    }
    holder.itemObj[holder.spawnedCount - 1] = first;
}

static bool FreeOneSpawnedObj(Item::ObjHolder& holder) {
    if (holder.itemObj == nullptr || holder.spawnedCount == 0) return false;

    const u32 prevBodyCount = holder.bodyCount;
    const u32 prevSpawnedCount = holder.spawnedCount;
    Item::Obj* oldestObj = holder.itemObj[0];
    if (oldestObj == nullptr) return false;

    holder.OnObjKillFinish(oldestObj);
    RotateSpawnedObjQueue(holder);
    return holder.bodyCount < prevBodyCount || holder.spawnedCount < prevSpawnedCount;
}

static Item::PlayerObj* GetPlayerObjFromUsedObjs(Item::Obj** usedObjs) {
    // ObjHolder::Spawn receives &PlayerObj::usedObjs[0]; usedObjs starts at 0x20.
    return reinterpret_cast<Item::PlayerObj*>(reinterpret_cast<u8*>(usedObjs) - 0x20);
}

static void SafePlayerObjSpawn(Item::ObjHolder* holder, u32 quantity, Item::Obj** usedObjs, u8 playerId, const Vec3& playerPos, bool r8) {
    if (usedObjs != nullptr) {
        for (u32 i = 0; i < PLAYER_OBJ_SLOT_COUNT; ++i) usedObjs[i] = nullptr;
    }

    Item::PlayerObj* playerObj = nullptr;
    if (usedObjs != nullptr) playerObj = GetPlayerObjFromUsedObjs(usedObjs);
    if (holder == nullptr || holder->itemObj == nullptr || usedObjs == nullptr || playerObj == nullptr) {
        if (playerObj != nullptr) ClearPlayerObjUse(*playerObj);
        return;
    }

    const u32 requested = quantity > PLAYER_OBJ_SLOT_COUNT ? PLAYER_OBJ_SLOT_COUNT : quantity;
    u32 spawned = 0;
    while (spawned < requested) {
        while (holder->bodyCount >= holder->capacity) {
            if (!FreeOneSpawnedObj(*holder)) break;
        }
        if (holder->bodyCount >= holder->capacity) break;

        Item::Obj* obj = holder->itemObj[holder->bodyCount];
        if (obj == nullptr) break;

        usedObjs[spawned] = obj;
        ++holder->bodyCount;
        obj->Spawn(holder->itemObjId, playerId, playerPos, r8);
        ++spawned;
    }

    playerObj->activeItemCount = spawned;
    if (spawned == 0) ClearPlayerObjUse(*playerObj);
}

kmRuntimeUse(0x809c3670);  // Item::ItemSlotData
kmRuntimeUse(0x809c36a0);  // Item::Behavior::behaviourTable
kmRuntimeUse(0x80799be8);  // Item::ItemSlotData::itemSpawnTimers
static bool IsItemAvailable(ItemId id, const Item::ItemSlotData* slotData) {
    if (id >= ITEM_COUNT) return false;

    // Timer checks
    Item::Behavior* behaviourTable = reinterpret_cast<Item::Behavior*>(kmRuntimeAddr(0x809c36a0));
    ItemObjId objId = behaviourTable[id].objId;
    if (slotData) {
        if (objId == 6 && slotData->itemSpawnTimers[0] != 0) return false;  // Lightning
        if (objId == 5 && slotData->itemSpawnTimers[1] != 0) return false;  // Blue Shell
        if (objId == 10 && slotData->itemSpawnTimers[2] != 0) return false;  // Blooper
        if (objId == 11 && slotData->itemSpawnTimers[3] != 0) return false;  // POW
    }

    // Capacity check bypass for custom items
    u32 bitfield = Pulsar::Race::GetEffectiveCustomItemsBitfield();
    if (bitfield != 0 && bitfield != VANILLA_ITEM_BITFIELD) {
        if ((bitfield >> id) & 1) return true;
    }

    typedef bool (*IsThereCapacityForItem)(ItemId id);
    return reinterpret_cast<IsThereCapacityForItem>(kmRuntimeAddr(0x80799be8))(id);
}

static bool IsRestrictedFallbackItem(ItemId id) {
    return id == LIGHTNING || id == BULLET_BILL || id == POW_BLOCK || id == BLOOPER || id == BLUE_SHELL;
}

static ItemId GetRandomItemFromRow(u32 rowIndex, const Item::ItemSlotData::Probabilities& probs, u32 bitfield,
                                   Item::ItemSlotData* slotData, bool excludeRestrictedItems) {
    if (probs.probabilities == nullptr || probs.rowCount == 0) return MUSHROOM;
    if (rowIndex >= probs.rowCount) rowIndex = probs.rowCount - 1;

    const u16* row = &probs.probabilities[rowIndex * ITEM_COUNT];

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

static const Item::ItemSlotData::Probabilities* GetItemProbabilities(const Item::ItemSlotData& slotData,
                                                                      bool isHuman, u16 setting) {
    if (setting != 0) return &slotData.specialChances;
    return isHuman ? &slotData.playerChances : &slotData.cpuChances;
}

static bool ShouldExcludeRestrictedFallbackItems(u32 bitfield) {
    if (bitfield == VANILLA_ITEM_BITFIELD) return true;

    u32 enabledCount = 0;
    for (u32 item = 0; item < ITEM_COUNT; ++item) {
        if ((bitfield >> item) & 1) ++enabledCount;
    }
    return enabledCount > 6;
}

static ItemId GetItemFromTable(u32 position, bool isHuman, u16 setting, bool isFallback) {
    u32 bitfield = Pulsar::Race::GetEffectiveCustomItemsBitfield();
    if (bitfield == 0) bitfield = VANILLA_ITEM_BITFIELD;

    Item::ItemSlotData* slotData = *reinterpret_cast<Item::ItemSlotData**>(kmRuntimeAddr(0x809c3670));
    if (slotData == nullptr) return MUSHROOM;

    const Item::ItemSlotData::Probabilities* probs = GetItemProbabilities(*slotData, isHuman, setting);
    const bool excludeRestrictedItems = isFallback && ShouldExcludeRestrictedFallbackItems(bitfield);
    return GetRandomItemFromRow(GetItemTableRow(position, setting), *probs, bitfield, slotData,
                                excludeRestrictedItems);
}

static u8 GetPlayerPosition(Item::Player* itemPlayer, u8 fallbackPosition) {
    if (itemPlayer != nullptr && itemPlayer->id < 12 && Raceinfo::sInstance != nullptr &&
        Raceinfo::sInstance->players[itemPlayer->id] != nullptr) {
        return Raceinfo::sInstance->players[itemPlayer->id]->position;
    }
    return fallbackPosition;
}

static ItemId GetFallbackItem(Item::Player* itemPlayer, u8 fallbackPosition, bool isHuman, u16 setting) {
    const u8 position = GetPlayerPosition(itemPlayer, fallbackPosition);
    return GetItemFromTable(position, isHuman, setting, true);
}

static ItemId GetFallbackItem(Item::PlayerRoulette* roulette) {
    return GetFallbackItem(roulette->itemPlayer, roulette->position,
                           roulette->itemPlayer != nullptr && roulette->itemPlayer->isHuman, roulette->setting);
}

static u32 GetBestPlacement(const Item::ItemSlotData::Probabilities* probs, u32 currentPlacement) {
    if (probs == nullptr || probs->probabilities == nullptr) return currentPlacement;

    u32 bitfield = Pulsar::Race::GetEffectiveCustomItemsBitfield();
    if (bitfield == VANILLA_ITEM_BITFIELD || bitfield == 0) return currentPlacement;

    u32 rowCount = probs->rowCount;
    if (currentPlacement >= rowCount) currentPlacement = rowCount - 1;

    const u16* data = probs->probabilities;
    Item::ItemSlotData* slotData = *reinterpret_cast<Item::ItemSlotData**>(kmRuntimeAddr(0x809c3670));

    for (int dist = 0; dist < static_cast<int>(rowCount); ++dist) {
        int low = static_cast<int>(currentPlacement) - dist;
        int high = static_cast<int>(currentPlacement) + dist;
        int checks[2] = {low, high};
        for (int i = 0; i < 2; ++i) {
            int p = checks[i];
            if (i == 1 && high == low) continue;
            if (p >= 0 && p < static_cast<int>(rowCount)) {
                for (int item = 0; item < ITEM_COUNT; ++item) {
                    if (((bitfield >> item) & 1) && data[p * ITEM_COUNT + item] > 0) {
                        if (IsItemAvailable(static_cast<ItemId>(item), slotData)) {
                            return static_cast<u32>(p);
                        }
                    }
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
        mr r21, r3;  // Update placement with result

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
    register int count;
    register int limit;
    register u32 itemIdx;
    asm {
        mr count, r3
        mr limit, r0
        mr itemIdx, r21
    }

    u32 bitfield = Pulsar::Race::GetEffectiveCustomItemsBitfield();
    if (bitfield == 0) bitfield = VANILLA_ITEM_BITFIELD;

    if (itemIdx < ITEM_COUNT) {
        if (!((bitfield >> itemIdx) & 1)) {
            limit = 0;
        } else if (bitfield != VANILLA_ITEM_BITFIELD) {
            limit = 100;
        }
    }

    asm {
        mr r3, count
        mr r0, limit
        subf. r0, r3, r0
    }
}
kmBranch(0x807bb7d8, CustomLimitCheck);
kmPatchExitPoint(CustomLimitCheck, 0x807bb7dc);  // Return to the ble instruction

static void CalcItemFallback() {
    register Item::PlayerRoulette* roulette;
    asm(mr roulette, r31);
    roulette->nextItemId = GetFallbackItem(roulette);
}
kmBranch(0x807ba48c, CalcItemFallback);
kmPatchExitPoint(CalcItemFallback, 0x807ba494);

static ItemId DecideItemFallback() {
    register ItemId res;
    asm(mr res, r24);
    if (res == 0x14) {  // ITEM_NONE
        register u32 row;
        register bool isHuman;
        register u32 boxType;
        register Item::Player* itemPlayer;
        asm {
            mr row, r21
            mr isHuman, r20
            mr boxType, r22
            mr itemPlayer, r18
        }
        ItemId ret = GetFallbackItem(itemPlayer, static_cast<u8>(row), isHuman, static_cast<u16>(boxType));
        if (itemPlayer) {
            const u8 playerId = itemPlayer->id;
            if (playerId < 12) sFallbackItemDropFix[playerId] = true;
        }
        return ret;
    }
    return res;
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
    register Item::PlayerRoulette* roulette;
    asm(mr roulette, r23);
    roulette->nextItemId = GetFallbackItem(roulette);
}
kmBranch(0x807ba138, InitItemFallback1);
kmPatchExitPoint(InitItemFallback1, 0x807ba140);

static void InitItemFallback2() {
    register Item::PlayerRoulette* roulette;
    asm(mr roulette, r23);
    roulette->nextItemId = GetFallbackItem(roulette);
}
kmBranch(0x807ba194, InitItemFallback2);
kmPatchExitPoint(InitItemFallback2, 0x807ba19c);

static ItemId DecideRouletteItemFiltered(Item::ItemSlotData* slotData, u16 itemBoxType, u8 position, ItemId prevRandomItem, bool r7) {
    u32 bitfield = Pulsar::Race::GetEffectiveCustomItemsBitfield();
    if (bitfield == VANILLA_ITEM_BITFIELD) {
        return slotData->DecideRouletteItem(itemBoxType, position, prevRandomItem, r7);
    }

    return GetItemFromTable(position, true, itemBoxType, false);
}
kmCall(0x807ba428, DecideRouletteItemFiltered);

static void SetItemFix(Item::PlayerInventory& inventory, ItemId id, bool isItemForcedDueToCapacity) {
    Item::Player* itemPlayer = inventory.itemPlayer;
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

static void EjectItemsFromDamageSafely(Item::PlayerInventory& inventory) {
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
