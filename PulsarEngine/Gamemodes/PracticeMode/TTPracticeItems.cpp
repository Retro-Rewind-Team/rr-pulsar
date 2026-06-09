#include <kamek.hpp>
#include <runtimeWrite.hpp>
#include <Gamemodes/PracticeMode/TTPracticeInternal.hpp>
#include <MarioKartWii/Input/ControllerHolder.hpp>
#include <MarioKartWii/Item/ItemManager.hpp>
#include <MarioKartWii/Item/Obj/Kumo.hpp>

namespace Pulsar {
namespace TTPractice {

static Item::ObjKumo* FindActiveThunderCloud(Item::Player& player) {
    Item::Manager* manager = Item::Manager::sInstance;
    if (manager == nullptr) return nullptr;

    Item::ObjHolder& holder = manager->itemObjHolders[OBJ_THUNDER_CLOUD];
    if (holder.itemObj == nullptr) return nullptr;

    for (u32 i = 0; i < holder.capacity; ++i) {
        Item::Obj* obj = holder.itemObj[i];
        if (obj == nullptr || (obj->bitfield74 & ITEM_OBJ_KILLED) != 0) continue;
        if ((obj->bitfield78 & ITEM_OBJ_UNAVAILABLE) != 0) continue;
        if (obj->itemObjId != OBJ_THUNDER_CLOUD) continue;
        if (*reinterpret_cast<u32*>(reinterpret_cast<u8*>(obj) + TC_TIMER_OFFSET) >= TC_NATURAL_STRIKE_TIMER) continue;

        void* carrier = *reinterpret_cast<void**>(reinterpret_cast<u8*>(obj) + 0x1a0);
        if (carrier == &player) return static_cast<Item::ObjKumo*>(obj);
    }

    return nullptr;
}

static bool IsItemUsePressed(Item::Player& player) {
    Input::ControllerHolder& holder = player.GetControllerHolder();
    const u16 held = holder.inputStates[0].buttonActions;
    const u16 prevHeld = holder.inputStates[1].buttonActions;
    return (held & ~prevHeld & INPUT_ITEM_USE) != 0;
}

static bool IsItemUseHeld(Item::Player& player) {
    return (player.GetControllerHolder().inputStates[0].buttonActions & INPUT_ITEM_USE) != 0;
}

static void CycleItem(u32 hudSlotId, s32 direction) {
    u32& selected = selectedItemIndexes[hudSlotId];
    s32 next = static_cast<s32>(selected) + direction;
    if (next < 0) next = ITEM_COUNT - 1;
    if (next >= static_cast<s32>(ITEM_COUNT)) next = 0;
    selected = static_cast<u32>(next);
}

static void GiveSelectedItem(Item::Player& player, ItemId item) {
    player.inventory.SetItem(item, false);
    player.bitfield |= 0x2;
    hasGrantedItem[player.hudSlotId] = true;
}

static bool ShouldAutoRefill(ItemId item) {
    return item == STAR || item == MUSHROOM || item == MEGA_MUSHROOM || item == BULLET_BILL || item == THUNDER_CLOUD;
}

static bool ShouldAutoRefill(Item::Player& player, ItemId item) {
    if (!ShouldAutoRefill(item)) return false;
    if (item == THUNDER_CLOUD && FindActiveThunderCloud(player) != nullptr) return false;
    return true;
}

static bool ShouldRefillOnItemUse(ItemId item) {
    return item == GOLDEN_MUSHROOM || item == TRIPLE_MUSHROOM;
}

static bool CanGrantSelectedItem(Item::Player& player, ItemId item) {
    return item != THUNDER_CLOUD || FindActiveThunderCloud(player) == nullptr;
}

static void RequestThunderCloudStrike(Item::Player& player, bool hadThunderCloudBeforeUpdate) {
    if (!hadThunderCloudBeforeUpdate || !IsEnabled()) return;
    if (!player.isHuman || player.isRemote || player.hudSlotId >= 4) return;
    if (!IsItemUsePressed(player)) return;

    Item::ObjKumo* thunderCloud = FindActiveThunderCloud(player);
    if (thunderCloud == nullptr) return;

    *reinterpret_cast<u32*>(reinterpret_cast<u8*>(thunderCloud) + TC_TIMER_OFFSET) = TC_STRIKE_TIMER;
}

static s8 GetAnalogWheelDirection(u32 hudSlotId, float stickX) {
    s8 direction = 0;
    if (stickX <= -STICK_WHEEL_THRESHOLD) {
        direction = -1;
    } else if (stickX >= STICK_WHEEL_THRESHOLD) {
        direction = 1;
    }

    s8& previousDirection = stickWheelDirections[hudSlotId];
    if (direction == 0) {
        previousDirection = 0;
        return 0;
    }
    if (previousDirection == direction) return 0;

    previousDirection = direction;
    return direction;
}

static s8 GetWheelDirection(Item::Player& player) {
    Input::ControllerHolder& holder = player.GetControllerHolder();
    if (holder.curController == nullptr) return 0;

    const u32 hudSlotId = player.hudSlotId;
    const ControllerType type = holder.curController->GetType();
    switch (type) {
        case NUNCHUCK: {
            stickWheelDirections[hudSlotId] = 0;
            const u16 pressed = holder.uiinputStates[0].rawButtons & ~holder.uiinputStates[1].rawButtons;
            if ((pressed & WPAD::WPAD_BUTTON_LEFT) != 0) return -1;
            if ((pressed & WPAD::WPAD_BUTTON_RIGHT) != 0) return 1;
            return 0;
        }
        case CLASSIC: {
            Input::WiiController* controller = static_cast<Input::WiiController*>(holder.curController);
            const Vec2D& stickR = controller->kpadStatus[0].extStatus.cl.stickR;
            if (IsAnalogRespawnInputHeld(stickR.x, stickR.z)) {
                stickWheelDirections[hudSlotId] = 0;
                return 0;
            }
            return GetAnalogWheelDirection(hudSlotId, stickR.x);
        }
        case GCN: {
            Input::GCNController* controller = static_cast<Input::GCNController*>(holder.curController);
            if (IsAnalogRespawnInputHeld(controller->cStickHorizontal, controller->cStickVertical)) {
                stickWheelDirections[hudSlotId] = 0;
                return 0;
            }
            return GetAnalogWheelDirection(hudSlotId, controller->cStickHorizontal);
        }
        default:
            stickWheelDirections[hudSlotId] = 0;
            return 0;
    }
}

static void UpdatePracticeWheel(Item::Player& player) {
    if (!IsEnabled()) return;
    if (!player.isHuman || player.isRemote || player.hudSlotId >= 4) return;

    Item::PlayerInventory& inventory = player.inventory;
    if (inventory.currentItemId == ITEM_NONE) stickWheelDirections[player.hudSlotId] = 0;

    const s8 direction = GetWheelDirection(player);
    bool changed = false;

    if (direction != 0) {
        CycleItem(player.hudSlotId, direction);
        changed = true;
    }

    const ItemId selectedItem = ITEM_WHEEL_ITEMS[selectedItemIndexes[player.hudSlotId]];
    const bool inventoryEmpty = inventory.currentItemId == ITEM_NONE;
    const bool refillableOnUse = ShouldRefillOnItemUse(selectedItem);
    if (!inventoryEmpty || !refillableOnUse) {
        canRefillOnUse[player.hudSlotId] = false;
    } else if (!IsItemUseHeld(player)) {
        canRefillOnUse[player.hudSlotId] = true;
    }

    const bool refillOnUse = inventoryEmpty && refillableOnUse && canRefillOnUse[player.hudSlotId] && IsItemUsePressed(player);
    if (CanGrantSelectedItem(player, selectedItem) &&
        (changed || refillOnUse ||
         (inventory.currentItemId == ITEM_NONE && (!hasGrantedItem[player.hudSlotId] || ShouldAutoRefill(player, selectedItem))))) {
        GiveSelectedItem(player, selectedItem);
        canRefillOnUse[player.hudSlotId] = false;
    }
}

void UpdatePlayerAndPracticeWheel(Item::Player& player) {
    const bool hadThunderCloudBeforeUpdate = FindActiveThunderCloud(player) != nullptr;
    player.Update();
    RequestThunderCloudStrike(player, hadThunderCloudBeforeUpdate);
    UpdatePlayerRespawnShortcut(player);
    UpdatePracticeWheel(player);
}
kmCall(0x8079994c, UpdatePlayerAndPracticeWheel);

}  // namespace TTPractice
}  // namespace Pulsar
