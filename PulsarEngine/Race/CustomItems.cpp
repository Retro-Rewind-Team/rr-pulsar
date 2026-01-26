#include <kamek.hpp>
#include <runtimeWrite.hpp>
#include <Race/CustomItems.hpp>
#include <MarioKartWii/Item/ItemManager.hpp>
#include <MarioKartWii/Item/ItemSlot.hpp>
#include <MarioKartWii/Item/ItemBehaviour.hpp>
#include <core/rvl/OS/OS.hpp>
#include <Settings/Settings.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <PulsarSystem.hpp>

namespace Pulsar {
namespace Race {

u32 Pulsar::Race::GetEffectiveCustomItemsBitfield() {
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    if (controller) {
        const RKNet::RoomType roomType = controller->roomType;
        if (roomType == RKNet::ROOMTYPE_FROOM_HOST || roomType == RKNet::ROOMTYPE_FROOM_NONHOST) {
            if (System::sInstance) {
                return System::sInstance->netMgr.customItemsBitfield;
            }
        } else if (roomType != RKNet::ROOMTYPE_NONE) {
            // Public lobby - disable everything (force vanilla)
            return 0x7FFFF;
        }
    }
    // Offline or unknown
    Settings::Mgr* settings = &Settings::Mgr::Get();
    if (settings) {
        return settings->GetCustomItems();
    }
    return 0x7FFFF;
}

kmRuntimeUse(0x809c3670);  // Item::ItemSlotData
kmRuntimeUse(0x809c36a0);  // Item::Behavior::behaviourTable
kmRuntimeUse(0x80799be8);  // Item::ItemSlotData::itemSpawnTimers
static bool IsItemAvailable(ItemId id, const Item::ItemSlotData* slotData) {
    if (id >= 19) return false;

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
    if (bitfield != 0 && bitfield != 0x7FFFF) {
        if ((bitfield >> id) & 1) return true;
    }

    typedef bool (*IsThereCapacityForItem)(ItemId id);
    return reinterpret_cast<IsThereCapacityForItem>(kmRuntimeAddr(0x80799be8))(id);
}

static ItemId GetRandomEnabledItem(u32 position, bool isHuman, bool isSpecial) {
    u32 bitfield = Pulsar::Race::GetEffectiveCustomItemsBitfield();
    if (bitfield == 0 || bitfield == 0x7FFFF) return MUSHROOM;  // Safety or Vanilla Fallback

    Item::ItemSlotData* slotData = *reinterpret_cast<Item::ItemSlotData**>(kmRuntimeAddr(0x809c3670));
    if (!slotData) return MUSHROOM;

    const Item::ItemSlotData::Probabilities* probs;
    if (isSpecial)
        probs = &slotData->specialChances;
    else if (isHuman)
        probs = &slotData->playerChances;
    else
        probs = &slotData->cpuChances;

    if (!probs || !probs->probabilities) return MUSHROOM;

    u32 rowCount = probs->rowCount;
    if (position >= rowCount) position = rowCount - 1;

    const u16* data = probs->probabilities;

    // Search outward for the closest position with at least one enabled item that has capacity
    for (int dist = 0; dist < static_cast<int>(rowCount); ++dist) {
        int low = static_cast<int>(position) - dist;
        int high = static_cast<int>(position) + dist;
        int checks[2] = {low, high};
        for (int i = 0; i < 2; ++i) {
            int p = checks[i];
            if (i == 1 && high == low) continue;  // Skip redundant check for dist 0
            if (p >= 0 && p < static_cast<int>(rowCount)) {
                ItemId rowEnabled[19];
                int count = 0;
                for (int item = 0; item < 19; ++item) {
                    if (((bitfield >> item) & 1) && data[p * 19 + item] > 0) {
                        if (IsItemAvailable(static_cast<ItemId>(item), slotData)) {
                            rowEnabled[count++] = static_cast<ItemId>(item);
                        }
                    }
                }
                if (count > 0) {
                    static u32 lcgSeed = 0;
                    if (lcgSeed == 0) lcgSeed = OS::GetTick();

                    lcgSeed = lcgSeed * 1103515245 + 12345;
                    ItemId ret = rowEnabled[(lcgSeed >> 16) % count];
                    if (ret > 18) ret = MUSHROOM;  // UI Safety
                    return ret;
                }
            }
        }
    }

    // Absolute fallback
    ItemId anyEnabled[19];
    int anyCount = 0;
    for (int i = 0; i < 19; i++) {
        if (((bitfield >> i) & 1) && IsItemAvailable(static_cast<ItemId>(i), slotData)) {
            anyEnabled[anyCount++] = static_cast<ItemId>(i);
        }
    }

    if (anyCount == 0) {
        for (int i = 0; i < 19; i++) {
            if ((bitfield >> i) & 1) {
                anyEnabled[anyCount++] = static_cast<ItemId>(i);
            }
        }
    }

    if (anyCount == 0) return MUSHROOM;

    static u32 fallbackSeed = 0;
    if (fallbackSeed == 0) fallbackSeed = OS::GetTick();
    fallbackSeed = fallbackSeed * 1103515245 + 12345;
    ItemId ret = anyEnabled[(fallbackSeed >> 16) % anyCount];
    if (ret > 18) ret = MUSHROOM;
    return ret;
}

static u32 GetBestPlacement(const Item::ItemSlotData::Probabilities* probs, u32 currentPlacement) {
    if (probs == nullptr || probs->probabilities == nullptr) return currentPlacement;

    u32 bitfield = Pulsar::Race::GetEffectiveCustomItemsBitfield();
    if (bitfield == 0x7FFFF || bitfield == 0) return currentPlacement;

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
                for (int item = 0; item < 19; ++item) {
                    if (((bitfield >> item) & 1) && data[p * 19 + item] > 0) {
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
    if (bitfield == 0) bitfield = 0x7FFFF;

    if (itemIdx < 19) {
        if (!((bitfield >> itemIdx) & 1)) {
            limit = 0;
        } else if (bitfield != 0x7FFFF) {
            limit = 100;  // Ignore limit for custom items
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
    roulette->nextItemId = GetRandomEnabledItem(roulette->position, roulette->itemPlayer->isHuman, roulette->setting != 0);
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
        asm {
            mr row, r21
            mr isHuman, r20
            mr boxType, r22
        }
        return GetRandomEnabledItem(row, isHuman, boxType != 0);
    }
    return res;
}
kmWrite32(0x807bb8b4, 0x3B000014);  // li r24, 0x14
kmCall(0x807bb8b8, DecideItemFallback);

// Restore original probability sum logic (removes legacy partial filtering)
kmWrite32(0x807bb83c, 0x7ED60214);

// Infinite loop fix for ItemHolderItem_spawn
kmWrite32(0x80795e4c, 0x408100C8);

static void InitItemFallback1() {
    register Item::PlayerRoulette* roulette;
    asm(mr roulette, r23);
    roulette->nextItemId = GetRandomEnabledItem(roulette->position, roulette->itemPlayer->isHuman, roulette->setting != 0);
}
kmBranch(0x807ba138, InitItemFallback1);
kmPatchExitPoint(InitItemFallback1, 0x807ba140);

static void InitItemFallback2() {
    register Item::PlayerRoulette* roulette;
    asm(mr roulette, r23);
    roulette->nextItemId = GetRandomEnabledItem(roulette->position, roulette->itemPlayer->isHuman, roulette->setting != 0);
}
kmBranch(0x807ba194, InitItemFallback2);
kmPatchExitPoint(InitItemFallback2, 0x807ba19c);

static ItemId DecideRouletteItemFiltered(Item::ItemSlotData* slotData, u16 itemBoxType, u8 position, ItemId prevRandomItem, bool r7) {
    u32 bitfield = Pulsar::Race::GetEffectiveCustomItemsBitfield();
    if (bitfield == 0x7FFFF) {
        return slotData->DecideRouletteItem(itemBoxType, position, prevRandomItem, r7);
    }

    // Use closest logic for visual items too if custom items are enabled
    return GetRandomEnabledItem(position, true, itemBoxType != 0);
}
kmCall(0x807ba428, DecideRouletteItemFiltered);

}  // namespace Race
}  // namespace Pulsar