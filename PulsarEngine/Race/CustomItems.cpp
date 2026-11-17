#include <kamek.hpp>
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
#include <GlobalFunction.hpp>

namespace Pulsar {
namespace Race {

static const u32 PLAYER_OBJ_SLOT_COUNT = 3;

u32 GetEffectiveCustomItemsBitfield() {
    const RKNet::Controller *controller = RKNet::Controller::sInstance;
    if (controller == nullptr)
        return Settings::Mgr::Get().GetCustomItems();

    const bool isFriendRoom = IsFriendRoom();
    if (!isFriendRoom) {
        if (IsOnlineRoom())
            return ALL_CUSTOM_ITEMS;
        return Settings::Mgr::Get().GetCustomItems();
    }

    if (System::sInstance == nullptr)
        return Settings::Mgr::Get().GetCustomItems();
    if (System::sInstance->IsVanillaMode())
        return ALL_CUSTOM_ITEMS;
    return System::sInstance->netMgr.customItemsBitfield;
}

static bool sUsedFallbackItem[12];

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

static void CallPlayerObjPtmfIfValid(Item::PlayerObj *playerObj, const Ptmf_0A<Item::PlayerObj, void> *ptmf) {
    if (playerObj == nullptr || ptmf == nullptr)
        return;

    if (playerObj->useType != Item::PlayerObj::ONLY_USE) {
        if (playerObj->activeItemCount == 0 || playerObj->activeItemCount > PLAYER_OBJ_SLOT_COUNT) {
            ClearPlayerObjUse(*playerObj);
            return;
        }
        for (u32 i = 0; i < playerObj->activeItemCount; ++i) {
            if (playerObj->usedObjs[i] == nullptr) {
                ClearPlayerObjUse(*playerObj);
                return;
            }
        }
    }

    __ptmf_scall(playerObj, ptmf);
}

static void SafePlayerObjSpawn(Item::ObjHolder *holder, u32 quantity, Item::Obj **usedObjs, u8 playerId, const Vec3 &playerPos, bool r8) {
    if (usedObjs == nullptr)
        return;

    for (u32 i = 0; i < PLAYER_OBJ_SLOT_COUNT; ++i) usedObjs[i] = nullptr;
    Item::PlayerObj *playerObj = reinterpret_cast<Item::PlayerObj *>(reinterpret_cast<u8 *>(usedObjs) - offsetof(Item::PlayerObj, usedObjs));
    if (playerObj == nullptr)
        return;
    if (holder == nullptr || holder->itemObj == nullptr) {
        ClearPlayerObjUse(*playerObj);
        return;
    }

    const u32 requestedCount = quantity > PLAYER_OBJ_SLOT_COUNT ? PLAYER_OBJ_SLOT_COUNT : quantity;
    u32 spawnedCount = 0;
    while (spawnedCount < requestedCount) {
        while (holder->bodyCount >= holder->capacity) {
            if (holder->itemObj == nullptr || holder->spawnedCount == 0 || holder->itemObj[0] == nullptr)
                break;

            const u32 prevBodyCount = holder->bodyCount;
            const u32 prevSpawnedCount = holder->spawnedCount;
            holder->OnObjKillFinish(holder->itemObj[0]);
            if (holder->spawnedCount > 1) {
                Item::Obj *first = holder->itemObj[0];
                for (u32 i = 1; i < holder->spawnedCount; ++i) holder->itemObj[i - 1] = holder->itemObj[i];
                holder->itemObj[holder->spawnedCount - 1] = first;
            }
            if (holder->bodyCount >= prevBodyCount && holder->spawnedCount >= prevSpawnedCount)
                break;
        }
        if (holder->bodyCount >= holder->capacity)
            break;

        Item::Obj *spawnedObj = holder->itemObj[holder->bodyCount];
        if (spawnedObj == nullptr)
            break;

        usedObjs[spawnedCount] = spawnedObj;
        ++holder->bodyCount;
        spawnedObj->Spawn(holder->itemObjId, playerId, playerPos, r8);
        ++spawnedCount;
    }

    playerObj->activeItemCount = spawnedCount;
    if (spawnedCount == 0)
        ClearPlayerObjUse(*playerObj);
}

static bool IsItemAvailable(ItemId id, const Item::ItemSlotData *slotData, u32 bitfield) {
    if (id == LIGHTNING && slotData->itemSpawnTimers[0] > 0)
        return false;

    return bitfield != ALL_CUSTOM_ITEMS || Item::Manager::IsThereCapacityForItem(id);
}

static ItemId GetItemFromTable(Item::Player *itemPlayer, u8 position, bool isHuman, u16 setting, bool isFallback) {
    if (itemPlayer != nullptr && itemPlayer->id < 12 && Raceinfo::sInstance != nullptr) {
        const RaceinfoPlayer *racePlayer = Raceinfo::sInstance->players[itemPlayer->id];
        if (racePlayer != nullptr)
            position = racePlayer->position;
    }

    const u32 bitfield = GetEffectiveCustomItemsBitfield();
    if (bitfield == 0)
        return ITEM_NONE;
    const Item::ItemSlotData *slotData = itemSlotData;
    if (slotData == nullptr)
        return MUSHROOM;

    const Item::ItemSlotData::Probabilities &probabilities = setting != 0 ? slotData->specialChances : (isHuman ? slotData->playerChances : slotData->cpuChances);
    if (probabilities.probabilities == nullptr || probabilities.rowCount == 0)
        return MUSHROOM;

    u32 rowIndex = setting != 0 ? setting - 1 : (position > 0 ? position - 1 : 0);
    if (rowIndex >= probabilities.rowCount)
        rowIndex = probabilities.rowCount - 1;
    const u16 *row = &probabilities.probabilities[rowIndex * CUSTOM_ITEM_COUNT];

    u32 weightedItems = bitfield;
    if (isFallback) {
        u32 enabledCount = 0;
        for (u32 items = bitfield & ALL_CUSTOM_ITEMS; items != 0; items >>= 1) enabledCount += items & 1;
        if (enabledCount > 6)
            weightedItems &= ~((1 << LIGHTNING) | (1 << BULLET_BILL) | (1 << POW_BLOCK) | (1 << BLOOPER) | (1 << BLUE_SHELL));
    }

    u16 weights[CUSTOM_ITEM_COUNT] = {};
    u32 totalProbability = 0;
    for (u32 item = 0; item < CUSTOM_ITEM_COUNT; ++item) {
        const ItemId id = static_cast<ItemId>(item);
        if (((weightedItems >> item) & 1) && row[item] > 0 && IsItemAvailable(id, slotData, bitfield)) {
            weights[item] = row[item];
            totalProbability += weights[item];
        }
    }

    if (totalProbability != 0) {
        static u32 randomSeed = 0;
        if (randomSeed == 0)
            randomSeed = OS::GetTick();
        randomSeed = randomSeed * 1103515245 + 12345;
        u32 roll = (randomSeed >> 16) % totalProbability;
        for (u32 item = 0; item < CUSTOM_ITEM_COUNT; ++item) {
            if (roll < weights[item])
                return static_cast<ItemId>(item);
            roll -= weights[item];
        }
    }

    for (u32 item = 0; item < CUSTOM_ITEM_COUNT; ++item) {
        const ItemId id = static_cast<ItemId>(item);
        if (((bitfield >> item) & 1) && IsItemAvailable(id, slotData, bitfield))
            return id;
    }
    return MUSHROOM;
}

static ItemId GetFallbackItem(Item::PlayerRoulette *roulette) {
    return GetItemFromTable(roulette->itemPlayer, roulette->position, roulette->itemPlayer != nullptr && roulette->itemPlayer->isHuman, roulette->setting, true);
}

static u32 GetBestPlacement(const Item::ItemSlotData::Probabilities *probabilities, u32 currentPlacement) {
    if (probabilities == nullptr || probabilities->probabilities == nullptr || probabilities->rowCount == 0) {
        return currentPlacement;
    }

    const u32 bitfield = GetEffectiveCustomItemsBitfield();
    if (bitfield == ALL_CUSTOM_ITEMS || bitfield == 0)
        return currentPlacement;

    const u32 rowCount = probabilities->rowCount;
    if (currentPlacement >= rowCount)
        currentPlacement = rowCount - 1;

    const u16 *probabilityTable = probabilities->probabilities;
    Item::ItemSlotData *slotData = itemSlotData;

    for (s32 distance = 0; distance < static_cast<s32>(rowCount); ++distance) {
        const s32 placements[2] = {static_cast<s32>(currentPlacement) - distance, static_cast<s32>(currentPlacement) + distance};
        for (u32 side = 0; side < 2; ++side) {
            const s32 placement = placements[side];
            if (side == 1 && placements[0] == placements[1])
                continue;
            if (placement < 0 || placement >= static_cast<s32>(rowCount))
                continue;

            for (u32 item = 0; item < CUSTOM_ITEM_COUNT; ++item) {
                if (((bitfield >> item) & 1) && probabilityTable[placement * CUSTOM_ITEM_COUNT + item] > 0 && IsItemAvailable(static_cast<ItemId>(item), slotData, bitfield)) {
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
        blr;
    )
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

    const u32 bitfield = GetEffectiveCustomItemsBitfield();
    if (itemId < CUSTOM_ITEM_COUNT) {
        if (!((bitfield >> itemId) & 1)) {
            itemLimit = 0;
        } else if (bitfield != ALL_CUSTOM_ITEMS) {
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
    if (itemId != ITEM_NONE)
        return itemId;

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
    const ItemId fallbackItem = GetItemFromTable(itemPlayer, static_cast<u8>(position), isHuman, static_cast<u16>(itemBoxType), true);
    if (itemPlayer != nullptr && itemPlayer->id < 12)
        sUsedFallbackItem[itemPlayer->id] = true;
    return fallbackItem;
}
kmWrite32(0x807bb8b4, 0x3B000014);  // li r24, 0x14
kmCall(0x807bb8b8, DecideItemFallback);

// Restore the original probability sum.
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

static ItemId DecideRouletteItemFiltered(Item::ItemSlotData *slotData, u16 itemBoxType, u8 position, ItemId prevRandomItem, bool isSpecialBox) {
    if (GetEffectiveCustomItemsBitfield() == ALL_CUSTOM_ITEMS)
        return slotData->DecideRouletteItem(itemBoxType, position, prevRandomItem, isSpecialBox);

    return GetItemFromTable(nullptr, position, true, itemBoxType, false);
}
kmCall(0x807ba428, DecideRouletteItemFiltered);

static void SetItemFix(Item::PlayerInventory &inventory, ItemId id, bool isItemForcedDueToCapacity) {
    Item::Player *itemPlayer = inventory.itemPlayer;
    if (itemPlayer) {
        const u8 playerId = itemPlayer->id;
        if (playerId < 12 && sUsedFallbackItem[playerId]) {
            isItemForcedDueToCapacity = false;
            sUsedFallbackItem[playerId] = false;
        }
    }

    inventory.currentItemId = id;
    inventory.currentItemCount = Item::Behavior::behaviourTable[id].numberOfItems;
    inventory.loseDelayDueToDmg = 0;
    inventory.isItemForcedDueToCapacity = isItemForcedDueToCapacity;
    inventory.hasGolden = Item::Behavior::behaviourTable[id].unknown_0x10;
    memset(inventory.unknown_0x1D, 0, sizeof(inventory.unknown_0x1D));
    inventory.goldenTimer = 0;
    memset(inventory.unknown_0x24, 0, sizeof(inventory.unknown_0x24));
}
kmBranch(0x807bc940, SetItemFix);

static void EjectItemsFromDamageSafely(Item::PlayerInventory &inventory) {
    if (inventory.currentItemId >= CUSTOM_ITEM_COUNT || inventory.currentItemCount == 0 || !Item::Manager::IsThereCapacityForItem(inventory.currentItemId)) {
        inventory.ClearAll();
        return;
    }
    inventory.EjectItems();
}
kmCall(0x807bc6c4, EjectItemsFromDamageSafely);

}  // namespace Race
}  // namespace Pulsar
