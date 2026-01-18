#include <kamek.hpp>
#include <runtimeWrite.hpp>
#include <MarioKartWii/Item/ItemManager.hpp>
#include <MarioKartWii/Item/ItemSlot.hpp>
#include <core/rvl/OS/OS.hpp>
#include <Settings/Settings.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <PulsarSystem.hpp>

namespace Pulsar {
namespace Race {

static u32 GetEffectiveCustomItemsBitfield() {
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

static ItemId GetRandomEnabledItem() {
    u32 bitfield = GetEffectiveCustomItemsBitfield();
    if (bitfield == 0) return MUSHROOM;  // Safety

    ItemId enabled[19];
    int count = 0;
    for (int i = 0; i < 19; i++) {
        if ((bitfield >> i) & 1) {
            enabled[count++] = static_cast<ItemId>(i);
        }
    }

    if (count == 0) return MUSHROOM;

    static u32 lcgSeed = 0;
    if (lcgSeed == 0) lcgSeed = OS::GetTick();

    lcgSeed = lcgSeed * 1103515245 + 12345;
    u32 idx = (lcgSeed >> 16) % count;

    ItemId ret = enabled[idx];
    if (ret > 18) ret = MUSHROOM;  // UI Safety
    return ret;
}

static u32 GetBestPlacement(const Item::ItemSlotData::Probabilities* probs, u32 currentPlacement) {
    if (probs == nullptr || probs->probabilities == nullptr) return currentPlacement;

    u32 bitfield = GetEffectiveCustomItemsBitfield();
    if (bitfield == 0x7FFFF || bitfield == 0) return currentPlacement;

    u32 rowCount = probs->rowCount;
    if (currentPlacement >= rowCount) currentPlacement = rowCount - 1;

    // Check if current placement has any enabled item with prob > 0
    const u16* data = probs->probabilities;
    for (int i = 0; i < 19; i++) {
        if (((bitfield >> i) & 1) && data[currentPlacement * 19 + i] > 0) {
            return currentPlacement;
        }
    }

    // Search outward for the closest placement with at least one enabled item
    for (int dist = 1; dist < static_cast<int>(rowCount); ++dist) {
        int low = static_cast<int>(currentPlacement) - dist;
        int high = static_cast<int>(currentPlacement) + dist;
        int checks[2] = {low, high};
        for (int i = 0; i < 2; ++i) {
            int p = checks[i];
            if (p >= 0 && p < static_cast<int>(rowCount)) {
                for (int item = 0; item < 19; ++item) {
                    if (((bitfield >> item) & 1) && data[p * 19 + item] > 0) {
                        return static_cast<u32>(p);
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

    u32 bitfield = GetEffectiveCustomItemsBitfield();
    if (bitfield == 0) bitfield = 0x7FFFF;

    if (itemIdx < 19 && !((bitfield >> itemIdx) & 1)) {
        limit = 0;
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
    roulette->nextItemId = GetRandomEnabledItem();
}
kmBranch(0x807ba48c, CalcItemFallback);
kmPatchExitPoint(CalcItemFallback, 0x807ba494);

static ItemId DecideItemFallback() {
    register ItemId res;
    asm(mr res, r24);
    if (res == 0x14) {  // ITEM_NONE
        return GetRandomEnabledItem();
    }
    return res;
}
kmWrite32(0x807bb8b4, 0x3B000014);  // li r24, 0x14
kmCall(0x807bb8b8, DecideItemFallback);

// Restore original probability sum logic (removes legacy partial filtering)
kmWrite32(0x807bb83c, 0x7ED60214);

struct ItemObjProperties {
    void* makeArray;  // 0x0
    u32 limit;  // 0x4
    u32 unk_0x8;
    u32 capacity;  // 0xC
    u32 capacity2;  // 0x10
    u8 padding[0x74 - 0x14];
};

kmRuntimeUse(0x80790fb8);
kmRuntimeUse(0x809C2F48);
static void UniversalLimitLock() {
    reinterpret_cast<void (*)()> kmRuntimeAddr(0x80790fb8)();

    u32 bitfield = GetEffectiveCustomItemsBitfield();
    if (bitfield != 0 && bitfield != 0x7FFFF) {
        ItemObjProperties* properties = reinterpret_cast<ItemObjProperties*> kmRuntimeAddr(0x809C2F48);
        if (properties) {
            for (int i = 0; i < 15; i++) {
                if (properties[i].limit < 12) properties[i].limit = 12;
                if (properties[i].capacity < 12) properties[i].capacity = 12;
                if (properties[i].capacity2 < 12) properties[i].capacity2 = 12;
            }
        }
    }
}
kmCall(0x80790ae8, UniversalLimitLock);

}  // namespace Race
}  // namespace Pulsar