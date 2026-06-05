#include <kamek.hpp>
#include <runtimeWrite.hpp>
#include <Gamemodes/TTPractice.hpp>
#include <PulsarSystem.hpp>
#include <MarioKartWii/Item/ItemPlayer.hpp>
#include <MarioKartWii/Item/ItemManager.hpp>
#include <MarioKartWii/Item/Obj/Kumo.hpp>
#include <MarioKartWii/Input/ControllerHolder.hpp>
#include <MarioKartWii/Kart/KartManager.hpp>
#include <MarioKartWii/Kart/KartPhysics.hpp>
#include <MarioKartWii/File/RKG.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/RKSYS/RKSYSMgr.hpp>
#include <MarioKartWii/UI/Ctrl/CtrlRace/CtrlRaceItemWindow.hpp>
#include <MarioKartWii/UI/Page/Menu/CourseSelect.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>
#include <core/rvl/gx/GX.hpp>

namespace Pulsar {
namespace TTPractice {

static const u32 ITEM_COUNT = 7;
static const u16 GOLDEN_MUSHROOM_TIMER_FRAMES = 480;
static const u16 GOLDEN_MUSHROOM_WARNING_FRAMES = 120;
static const u16 INPUT_ITEM_USE = 0x4;
static const float MODE_BUTTON_X_OFFSET = -110.0f;
static const float STICK_WHEEL_THRESHOLD = 0.5f;
static const float RESPAWN_STICK_THRESHOLD = 0.5f;
static const u16 RESPAWN_HOLD_FRAMES = 30;

// Golden mushroom timer bar tuning. X/Y offsets are relative to the item window's HUD position.
static const float GOLDEN_TIMER_BAR_EDGE_EXTENSION = 4.0f;
static const float GOLDEN_TIMER_BAR_HEIGHT = 3.0f;
static const float GOLDEN_TIMER_BAR_Y_OFFSET = -20.5f;
static const u32 GOLDEN_TIMER_BAR_POSITION_INDEX = 1;
static const u8 GOLDEN_TIMER_BAR_YELLOW_GREEN = 220;
static const u8 GOLDEN_TIMER_BAR_ALPHA = 220;
static const u32 TC_TIMER_OFFSET = 0x1d8;
static const u32 TC_NATURAL_STRIKE_TIMER = 600;
static const u32 TC_STRIKE_TIMER = 599;
static const u32 ITEM_OBJ_KILLED = 0x1;
static const u32 ITEM_OBJ_UNAVAILABLE = 0xc0;
static const u32 KART_STATUS_IN_BULLET = 0x8000000;
static const ItemId ITEM_WHEEL_ITEMS[ITEM_COUNT] = {
    TRIPLE_MUSHROOM, GOLDEN_MUSHROOM, MEGA_MUSHROOM, STAR, BULLET_BILL, THUNDER_CLOUD, MUSHROOM
};
static const char* const DRIFT_BUTTON_VARIANTS[2] = {"ButtonNormal", "ButtonManual"};

static bool isPracticeMode = false;
static u32 selectedItemIndexes[4] = {0, 0, 0, 0};
static s8 stickWheelDirections[4] = {0, 0, 0, 0};
static u16 respawnShortcutTimers[4] = {0, 0, 0, 0};
static u16 respawnSaveTimers[4] = {0, 0, 0, 0};
static Vec3 savedRespawnPositions[4];
static Quat savedRespawnRotations[4];
static bool hasSavedRespawn[4] = {false, false, false, false};
static bool hasGrantedItem[4] = {false, false, false, false};
static bool canRefillOnUse[4] = {false, false, false, false};

kmRuntimeUse(0x80590238);  // Kart::Link::SetKartPosition
kmRuntimeUse(0x80590288);  // Kart::Link::SetKartRotation
kmRuntimeUse(0x80590e28);  // Kart::Link::UpdateCameraOnRespawn
kmRuntimeUse(0x8059c118);  // Kart::Killer::CancelBullet

static Item::ObjKumo* FindActiveThunderCloud(Item::Player& player);

typedef void (*SetKartPositionFn)(Kart::Link* link, const Vec3& position);
typedef void (*SetKartRotationFn)(Kart::Link* link, const Quat& rotation);
typedef void (*UpdateCameraOnRespawnFn)(const Kart::Link* link);
typedef void (*CancelBulletFn)(Kart::Killer* killer);

static SetKartPositionFn GetSetKartPosition() {
    static const SetKartPositionFn function = reinterpret_cast<SetKartPositionFn>(kmRuntimeAddr(0x80590238));
    return function;
}

static SetKartRotationFn GetSetKartRotation() {
    static const SetKartRotationFn function = reinterpret_cast<SetKartRotationFn>(kmRuntimeAddr(0x80590288));
    return function;
}

static UpdateCameraOnRespawnFn GetUpdateCameraOnRespawn() {
    static const UpdateCameraOnRespawnFn function = reinterpret_cast<UpdateCameraOnRespawnFn>(kmRuntimeAddr(0x80590e28));
    return function;
}

static CancelBulletFn GetCancelBullet() {
    static const CancelBulletFn function = reinterpret_cast<CancelBulletFn>(kmRuntimeAddr(0x8059c118));
    return function;
}

static void ClearSavedRespawns() {
    for (u32 i = 0; i < 4; ++i) {
        respawnShortcutTimers[i] = 0;
        respawnSaveTimers[i] = 0;
        hasSavedRespawn[i] = false;
    }
}

static SectionLoadHook ClearSavedRespawnsOnSectionLoad(ClearSavedRespawns);

extern "C" void fun_playSound(void*);
extern "C" void ptr_menuPageOrSomething(void*);
asmFunc PlayRespawnSaveSound() {
    ASM(
        nofralloc;
        mflr r11;
        stwu sp, -0x80(sp);
        stmw r3, 0x8(sp);
        lis r11, ptr_menuPageOrSomething @ha;
        lwz r3, ptr_menuPageOrSomething @l(r11);
        li r4, 0xDD;
        lis r12, fun_playSound @h;
        ori r12, r12, fun_playSound @l;
        mtctr r12;
        bctrl;
        lmw r3, 0x8(sp);
        addi sp, sp, 0x80;
        mtlr r11;
        blr;)
}

struct GoldenTimerBar {
    float x;
    float y;
    float width;
    float height;
    u8 red;
    u8 green;
    u8 blue;
    u8 alpha;
};

void SetPracticeMode(bool enabled) {
    isPracticeMode = enabled;
    ClearSavedRespawns();
    for (u32 i = 0; i < 4; ++i) {
        selectedItemIndexes[i] = 0;
        stickWheelDirections[i] = 0;
        hasGrantedItem[i] = false;
        canRefillOnUse[i] = false;
    }
}

bool IsPracticeMode() {
    return isPracticeMode;
}

ItemId GetStartingItem(u32 hudSlotId) {
    if (hudSlotId >= 4) hudSlotId = 0;
    return ITEM_WHEEL_ITEMS[selectedItemIndexes[hudSlotId]];
}

static bool IsEnabled() {
    const Racedata* racedata = Racedata::sInstance;
    return isPracticeMode && racedata != nullptr && racedata->racesScenario.settings.gamemode == MODE_TIME_TRIAL;
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

static bool IsAnalogRespawnShortcutHeld(float stickX, float stickY) {
    return stickY >= RESPAWN_STICK_THRESHOLD && stickY >= stickX && stickY >= -stickX;
}

static bool IsAnalogRespawnSaveHeld(float stickX, float stickY) {
    const float down = -stickY;
    return down >= RESPAWN_STICK_THRESHOLD && down >= stickX && down >= -stickX;
}

static bool IsAnalogRespawnInputHeld(float stickX, float stickY) {
    return IsAnalogRespawnShortcutHeld(stickX, stickY) || IsAnalogRespawnSaveHeld(stickX, stickY);
}

static bool IsRespawnShortcutHeld(Item::Player& player) {
    Input::ControllerHolder& holder = player.GetControllerHolder();
    if (holder.curController == nullptr) return false;

    const ControllerType type = holder.curController->GetType();
    switch (type) {
        case NUNCHUCK:
            return (holder.uiinputStates[0].rawButtons & WPAD::WPAD_BUTTON_UP) != 0;
        case CLASSIC: {
            Input::WiiController* controller = static_cast<Input::WiiController*>(holder.curController);
            const Vec2D& stickR = controller->kpadStatus[0].extStatus.cl.stickR;
            return IsAnalogRespawnShortcutHeld(stickR.x, stickR.z);
        }
        case GCN: {
            Input::GCNController* controller = static_cast<Input::GCNController*>(holder.curController);
            return IsAnalogRespawnShortcutHeld(controller->cStickHorizontal, controller->cStickVertical);
        }
        default:
            return false;
    }
}

static bool IsRespawnSaveHeld(Item::Player& player) {
    Input::ControllerHolder& holder = player.GetControllerHolder();
    if (holder.curController == nullptr) return false;

    const ControllerType type = holder.curController->GetType();
    switch (type) {
        case NUNCHUCK:
            return (holder.uiinputStates[0].rawButtons & WPAD::WPAD_BUTTON_DOWN) != 0;
        case CLASSIC: {
            Input::WiiController* controller = static_cast<Input::WiiController*>(holder.curController);
            const Vec2D& stickR = controller->kpadStatus[0].extStatus.cl.stickR;
            return IsAnalogRespawnSaveHeld(stickR.x, stickR.z);
        }
        case GCN: {
            Input::GCNController* controller = static_cast<Input::GCNController*>(holder.curController);
            return IsAnalogRespawnSaveHeld(controller->cStickHorizontal, controller->cStickVertical);
        }
        default:
            return false;
    }
}

static Kart::PhysicsHolder* GetKartPhysicsHolder(Kart::Player& kartPlayer) {
    if (kartPlayer.pointers.kartBody == nullptr) return nullptr;
    return kartPlayer.pointers.kartBody->kartPhysicsHolder;
}

static void ClearHitboxGroupMotion(Kart::HitboxGroup* hitboxGroup) {
    const Vec3 zero(0.0f, 0.0f, 0.0f);
    if (hitboxGroup == nullptr) return;

    hitboxGroup->collisionData.vel = zero;
    hitboxGroup->collisionData.movement = zero;
    hitboxGroup->collisionData.unknown_0x58 = zero;
    hitboxGroup->unknown_0x90 = 0;
    hitboxGroup->unknown_0x94 = 0.0f;
    hitboxGroup->unknown_0x98 = 0.0f;

    Kart::Hitbox* hitboxes = hitboxGroup->hitboxes;
    if (hitboxes == nullptr) return;

    for (u16 i = 0; i < hitboxGroup->hitboxCount; ++i) {
        Kart::Hitbox& hitbox = hitboxes[i];
        hitbox.lastPosition = hitbox.position;
        hitbox.unknown_0x24 = zero;
    }
}

static void ClearKartMotionHistory(Kart::Player& kartPlayer, Kart::PhysicsHolder* physicsHolder, const Vec3& position) {
    const Vec3 zero(0.0f, 0.0f, 0.0f);

    if (physicsHolder != nullptr) {
        physicsHolder->position = position;
        physicsHolder->unknown_0xcc[0] = zero;
        physicsHolder->unknown_0xcc[1] = zero;
        physicsHolder->unknown_0xcc[2] = zero;
        physicsHolder->speed = zero;
        physicsHolder->unknown_0xfc = 0.0f;
        ClearHitboxGroupMotion(physicsHolder->hitboxGroup);
    }

    Kart::Pointers& pointers = kartPlayer.pointers;
    if (pointers.values == nullptr || pointers.wheels == nullptr) return;

    for (u16 i = 0; i < pointers.values->wheelCount0; ++i) {
        Kart::Wheel* wheel = pointers.wheels[i];
        if (wheel == nullptr || wheel->wheelPhysics == nullptr) continue;

        Kart::WheelPhysics& wheelPhysics = *wheel->wheelPhysics;
        wheelPhysics.unknown_0x2c = zero;
        wheelPhysics.lastPosDiff = zero;
        wheelPhysics.unknown_0x48 = zero;
        wheelPhysics.speed = zero;
        wheelPhysics.unknown_0x6c = zero;
        ClearHitboxGroupMotion(wheelPhysics.hitboxGroup);
    }
}

static void ClearKartMomentum(Kart::Player& kartPlayer, Kart::Physics& physics, Kart::PhysicsHolder* physicsHolder) {
    const Vec3 zero(0.0f, 0.0f, 0.0f);

    physics.ResetSpeed();
    physics.speed0 = zero;
    physics.acceleration0 = zero;
    physics.unknown_0x8c = zero;
    physics.unknown_0x98 = zero;
    physics.rotVec0 = zero;
    physics.speed2 = zero;
    physics.rotVec1 = zero;
    physics.speed3 = zero;
    physics.speed = zero;
    physics.speedNorm = 0.0f;
    physics.rotVec2 = zero;
    physics.normalAcceleration = zero;
    physics.normalRotVec = zero;
    physics.engineSpeed = zero;
    physics.speed1Adj = zero;

    ClearKartMotionHistory(kartPlayer, physicsHolder, physics.position);

    Kart::Status* status = kartPlayer.pointers.kartStatus;
    if (status != nullptr) {
        status->bitfield0 &= ~(0x8000u | 0x800000u | 0x100000u | 0x2000000u | 0x8000000u | 0x40000000u |
                               0x80000000u);
        status->bitfield1 &= ~(0x200u | 0x800u | 0x2000u | 0x100000u);
        status->bitfield2 &= ~(0x1u | 0x2u | 0x400000u | KART_STATUS_IN_BULLET);
        status->airtime = 0;
        status->boostRampType = 0;
        status->jumpPadType = 0;
        status->bool_0x96 = false;
        status->bool_0x97 = false;
    }

    Kart::Movement* movement = kartPlayer.pointers.kartMovement;
    if (movement == nullptr) return;

    movement->engineSpeed = 0.0f;
    movement->lastSpeed = 0.0f;
    movement->unknown_0x28 = 0.0f;
    movement->acceleration = 0.0f;
    movement->speedRatio = 0.0f;
    movement->speedRatioCapped = 0.0f;
    movement->dirDiff = zero;
    movement->outsideDriftLastDir = zero;
    movement->slipstreamCharge = 0;
    movement->unknown_0xf0 = 0.0f;
    movement->divingRot = 0.0f;
    movement->boostRot = 0.0f;
    movement->driftState = 0;
    movement->mtCharge = 0;
    movement->smtCharge = 0;
    movement->mtBoost = 0;
    movement->outsideDriftBonus = 0.0f;
    movement->boost.mtFrames = 0;
    movement->boost.mushroomBoostPanelFrames = 0;
    movement->boost.bulletFrames = 0;
    movement->boost.trickZipperFrames = 0;
    movement->boost.megaFrames = 0;
    movement->boost.types = 0;
    movement->boost.multiplier = 1.0f;
    movement->boost.acceleration = 0.0f;
    movement->boost.speedLimit = 0.0f;
    movement->zipperBoost = 0;
    movement->zipperBoostMax = 0;
    movement->mushroomBoost2 = 0;
    movement->jumpPadMinSpeed = 0.0f;
    movement->jumpPadMaxSpeed = 0.0f;
    movement->jumpPadProperties.minSpeed = 0.0f;
    movement->jumpPadProperties.maxSpeed = 0.0f;
    movement->jumpPadProperties.velY = 0.0f;
    movement->rampBoost = 0;
    movement->hopVelY = 0.0f;
    movement->hopPosY = 0.0f;
    movement->hopGravity = 0.0f;
    movement->drivingDirection = 0;
    movement->backwardsAllowCounter = 0;
    movement->specialFloor = 0;
    movement->rawTurn = 0.0f;
}

static void CancelBulletIfActive(Kart::Player& kartPlayer) {
    Kart::Status* status = kartPlayer.pointers.kartStatus;
    Kart::Killer* killer = kartPlayer.pointers.kartKiller;
    if (status == nullptr || killer == nullptr || (status->bitfield2 & KART_STATUS_IN_BULLET) == 0) return;

    GetCancelBullet()(killer);
}

static void RestoreSavedKartTransform(Kart::Player& kartPlayer, Kart::Physics& physics, Kart::PhysicsHolder* physicsHolder, u32 hudSlotId) {
    Vec3 restorePosition = savedRespawnPositions[hudSlotId];

    CancelBulletIfActive(kartPlayer);
    GetSetKartPosition()(&kartPlayer, restorePosition);
    GetSetKartRotation()(&kartPlayer, savedRespawnRotations[hudSlotId]);
    physics.position = restorePosition;
    ClearKartMomentum(kartPlayer, physics, physicsHolder);
    GetUpdateCameraOnRespawn()(&kartPlayer);
}

static void UpdateRespawnShortcut(Item::Player& player) {
    if (!IsEnabled()) return;
    if (!player.isHuman || player.isRemote || player.hudSlotId >= 4) return;

    const Racedata* racedata = Racedata::sInstance;
    Kart::Manager* kartManager = Kart::Manager::sInstance;
    if (racedata == nullptr || kartManager == nullptr) return;

    const u8 playerId = racedata->racesScenario.settings.hudPlayerIds[player.hudSlotId];
    Kart::Player* kartPlayer = kartManager->GetKartPlayer(playerId);
    if (kartPlayer == nullptr) return;
    Kart::PhysicsHolder* physicsHolder = GetKartPhysicsHolder(*kartPlayer);
    if (physicsHolder == nullptr) return;
    Kart::Physics* physics = physicsHolder->physics;
    if (physics == nullptr) return;

    if (kartPlayer->IsRespawning()) {
        respawnShortcutTimers[player.hudSlotId] = 0;
        respawnSaveTimers[player.hudSlotId] = 0;
        return;
    }

    const u32 hudSlotId = player.hudSlotId;
    if (IsRespawnSaveHeld(player)) {
        respawnShortcutTimers[hudSlotId] = 0;
        u16& timer = respawnSaveTimers[hudSlotId];
        if (timer == RESPAWN_HOLD_FRAMES) return;
        if (timer < RESPAWN_HOLD_FRAMES) ++timer;
        if (timer < RESPAWN_HOLD_FRAMES) return;

        savedRespawnPositions[hudSlotId] = physics->position;
        savedRespawnRotations[hudSlotId] = physics->mainRot;
        hasSavedRespawn[hudSlotId] = true;
        PlayRespawnSaveSound();
        return;
    } else {
        respawnSaveTimers[hudSlotId] = 0;
    }

    if (!IsRespawnShortcutHeld(player)) {
        respawnShortcutTimers[hudSlotId] = 0;
        return;
    }

    u16& timer = respawnShortcutTimers[hudSlotId];
    if (timer == RESPAWN_HOLD_FRAMES) return;
    if (timer < RESPAWN_HOLD_FRAMES) ++timer;
    if (timer < RESPAWN_HOLD_FRAMES) return;

    if (!hasSavedRespawn[hudSlotId]) return;
    RestoreSavedKartTransform(*kartPlayer, *physics, physicsHolder, hudSlotId);
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

static void UpdatePlayerAndPracticeWheel(Item::Player& player) {
    const bool hadThunderCloudBeforeUpdate = FindActiveThunderCloud(player) != nullptr;
    player.Update();
    RequestThunderCloudStrike(player, hadThunderCloudBeforeUpdate);
    UpdateRespawnShortcut(player);
    UpdatePracticeWheel(player);
}
kmCall(0x8079994c, UpdatePlayerAndPracticeWheel);

static void SetupGoldenTimerGX() {
    GX::ClearVtxDesc();
    GX::SetVtxDesc(GX::GX_VA_POS, GX::GX_DIRECT);
    GX::SetVtxDesc(GX::GX_VA_CLR0, GX::GX_DIRECT);
    GX::SetVtxAttrFmt(GX::GX_VTXFMT0, GX::GX_VA_POS, GX::GX_POS_XYZ, GX::GX_F32, 0);
    GX::SetVtxAttrFmt(GX::GX_VTXFMT0, GX::GX_VA_CLR0, GX::GX_CLR_RGBA, GX::GX_RGBA8, 0);
    GX::SetNumTexGens(0);
    GX::SetNumChans(1);
    GX::SetChanCtrl(GX::GX_COLOR0A0, false, GX::GX_SRC_REG, GX::GX_SRC_VTX, GX::GX_LIGHT_NULL, GX::GX_DF_NONE, GX::GX_AF_NONE);
    GX::SetTevOrder(GX::GX_TEVSTAGE0, GX::GX_TEXCOORD_NULL, GX::GX_TEXMAP_NULL, GX::GX_COLOR0A0);
    GX::SetTevOp(GX::GX_TEVSTAGE0, GX::GX_PASSCLR);
    GX::SetNumTevStages(1);
    GX::SetBlendMode(GX::GX_BM_BLEND, GX::GX_BL_SRCALPHA, GX::GX_BL_INVSRCALPHA, GX::GX_LO_CLEAR);
    GX::SetZMode(false, GX::GX_ALWAYS, false);

    Mtx identity = {
        {1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f, 0.0f},
    };
    GX::LoadPosMtxImm(identity, 0);
    GX::SetCurrentMtx(0);
}

static void DrawGoldenTimerQuad(const GoldenTimerBar& bar) {
    SetupGoldenTimerGX();

    GX::Begin(GX::GX_QUADS, GX::GX_VTXFMT0, 4);
    GX_Position3f32(bar.x, bar.y, 0.0f);
    GX_Color4u8(bar.red, bar.green, bar.blue, bar.alpha);
    GX_Position3f32(bar.x + bar.width, bar.y, 0.0f);
    GX_Color4u8(bar.red, bar.green, bar.blue, bar.alpha);
    GX_Position3f32(bar.x + bar.width, bar.y - bar.height, 0.0f);
    GX_Color4u8(bar.red, bar.green, bar.blue, bar.alpha);
    GX_Position3f32(bar.x, bar.y - bar.height, 0.0f);
    GX_Color4u8(bar.red, bar.green, bar.blue, bar.alpha);
    GXEnd();
}

static bool TryBuildGoldenTimerBar(CtrlRaceItemWindow& itemWindow, GoldenTimerBar& bar) {
    if (!IsEnabled()) return false;

    Item::Manager* manager = Item::Manager::sInstance;
    if (manager == nullptr) return false;

    const u8 playerId = itemWindow.GetPlayerId();
    if (playerId >= 12) return false;

    Item::Player& player = manager->players[playerId];
    const Item::PlayerInventory& inventory = player.inventory;
    if (inventory.currentItemId != GOLDEN_MUSHROOM || !inventory.hasGolden || inventory.goldenTimer == 0) return false;

    const float remaining = inventory.goldenTimer > GOLDEN_MUSHROOM_TIMER_FRAMES
                                ? 1.0f
                                : static_cast<float>(inventory.goldenTimer) / static_cast<float>(GOLDEN_MUSHROOM_TIMER_FRAMES);

    nw4r::lyt::Pane* itemWindowPane = itemWindow.GetPane();
    if (itemWindowPane == nullptr) return false;

    const PositionAndScale& itemWindowPosition = itemWindow.positionAndscale[GOLDEN_TIMER_BAR_POSITION_INDEX];
    const float barScaleX = itemWindowPosition.scale.x;
    const float barScaleY = itemWindowPosition.scale.z;
    const float fullWidth = (itemWindowPane->size.x + GOLDEN_TIMER_BAR_EDGE_EXTENSION * 2.0f) * barScaleX;
    if (fullWidth <= 0.0f) return false;

    bar.x = itemWindowPosition.position.x - fullWidth * 0.5f;
    bar.y = itemWindowPosition.position.y + GOLDEN_TIMER_BAR_Y_OFFSET * barScaleY;
    bar.width = fullWidth * remaining;
    bar.height = GOLDEN_TIMER_BAR_HEIGHT * barScaleY;
    bar.red = 255;
    bar.green = inventory.goldenTimer <= GOLDEN_MUSHROOM_WARNING_FRAMES ? 0 : GOLDEN_TIMER_BAR_YELLOW_GREEN;
    bar.blue = 0;
    bar.alpha = GOLDEN_TIMER_BAR_ALPHA;
    return true;
}

static void DrawGoldenMushroomTimer(CtrlRaceItemWindow& itemWindow) {
    GoldenTimerBar bar;
    if (TryBuildGoldenTimerBar(itemWindow, bar)) DrawGoldenTimerQuad(bar);
}

static void DrawItemWindowWithGoldenTimer(CtrlRaceItemWindow& itemWindow, u32 curZIdx) {
    if (!itemWindow.IsInactive()) DrawGoldenMushroomTimer(itemWindow);
    itemWindow.LayoutUIControl::Draw(curZIdx);
}
kmWritePointer(0x808d3cdc, DrawItemWindowWithGoldenTimer);

static void LoadGhostSelectOrPracticeRace(Pages::Menu& page, PageId id, PushButton& button) {
    Racedata* racedata = Racedata::sInstance;
    if (!isPracticeMode || racedata == nullptr || racedata->menusScenario.settings.gamemode != MODE_TIME_TRIAL) {
        page.LoadNextPageById(id, button);
        return;
    }

    SectionMgr* sectionMgr = SectionMgr::sInstance;
    if (sectionMgr == nullptr || sectionMgr->sectionParams == nullptr || RKSYS::Mgr::sInstance == nullptr) {
        page.LoadNextPageById(id, button);
        return;
    }

    SectionParams* params = sectionMgr->sectionParams;
    const CourseId courseId = racedata->menusScenario.settings.courseId;
    params->ghostType = BEST_TIME;
    params->courseId = courseId;
    params->licenseId = RKSYS::Mgr::sInstance->curLicenseId;
    params->lastSelectedCourse = courseId;

    racedata->menusScenario.players[1].playerType = PLAYER_NONE;
    racedata->menusScenario.players[2].playerType = PLAYER_NONE;
    racedata->menusScenario.players[3].playerType = PLAYER_NONE;
    page.ChangeSectionById(SECTION_TT, button);
}
kmCall(0x80840a00, LoadGhostSelectOrPracticeRace);

SelectPage::SelectPage() {
    this->onButtonClickHandler.subject = this;
    this->onButtonClickHandler.ptmf = &SelectPage::OnButtonClick;
    this->onButtonSelectHandler.subject = this;
    this->onButtonSelectHandler.ptmf = &SelectPage::OnButtonSelect;
    this->onBackPressHandler.subject = this;
    this->onBackPressHandler.ptmf = &SelectPage::OnBackPress;

    this->externControlCount = 2;
    this->internControlCount = 0;
    this->hasBackButton = true;
    this->activePlayerBitfield = 1;
    this->playerBitfield = 1;
    this->controlSources = 2;
    this->prevPageId = PAGE_SINGLE_PLAYER_MENU;
    this->titleBmg = UI::BMG_TIME_TRIALS;

    this->controlsManipulatorManager.Init(1, false);
    this->SetManipulatorManager(this->controlsManipulatorManager);
    this->controlsManipulatorManager.SetGlobalHandler(BACK_PRESS, this->onBackPressHandler, false, false);
}

SelectPage::~SelectPage() {}

void SelectPage::OnInit() {
    Pages::Menu::OnInit();
    this->Pages::Menu::titleText = &this->title;
    this->Pages::Menu::bottomText = &this->bottom;
    this->AddControl(2, this->title, 0);
    this->AddControl(3, this->bottom, 0);
    this->title.Load(0);
    this->bottom.Load();
}

UIControl* SelectPage::CreateExternalControl(u32 controlId) {
    if (controlId >= 2) return nullptr;

    PushButton& button = this->buttons[controlId];
    this->AddControl(this->controlCount++, button, 0);
    const u32 layoutId = 1 - controlId;
    button.Load(UI::buttonFolder, "GlobePadEasy", DRIFT_BUTTON_VARIANTS[layoutId], this->activePlayerBitfield, 0, false);
    for (int i = 0; i < 4; ++i) {
        button.positionAndscale[i].position.x += MODE_BUTTON_X_OFFSET;
    }
    button.SetPosition(0.0f);
    button.buttonId = controlId;
    button.SetOnClickHandler(this->onButtonClickHandler, 0);
    button.SetOnSelectHandler(this->onButtonSelectHandler);

    button.SetMessage(controlId == 0 ? UI::BMG_TT_NORMAL_BUTTON : UI::BMG_TT_PRACTICE_BUTTON);
    return &button;
}

UIControl* SelectPage::CreateControl(u32 controlId) {
    return nullptr;
}

void SelectPage::OnActivate() {
    Pages::Menu::OnActivate();
    this->title.SetMessage(this->titleBmg);
    this->SelectButton(this->buttons[0]);
    this->OnButtonSelect(this->buttons[0], 0);
}

void SelectPage::BeforeEntranceAnimations() {
    Pages::Menu::BeforeEntranceAnimations();
    this->OnButtonSelect(this->buttons[0], 0);
}

void SelectPage::OnButtonClick(PushButton& button, u32 hudSlotId) {
    SetPracticeMode(button.buttonId == 1);
    this->LoadNextPageById(PAGE_CHARACTER_SELECT, button);
}

void SelectPage::OnButtonSelect(PushButton& button, u32 hudSlotId) {
    this->bottom.SetMessage(button.buttonId == 0 ? UI::BMG_TT_NORMAL_BOTTOM : UI::BMG_TT_PRACTICE_BOTTOM);
}

void SelectPage::OnBackPress(u32 hudSlotId) {
    SetPracticeMode(false);
    this->LoadPrevPageWithDelayById(PAGE_SINGLE_PLAYER_MENU, 0.0f);
}

}  // namespace TTPractice
}  // namespace Pulsar
