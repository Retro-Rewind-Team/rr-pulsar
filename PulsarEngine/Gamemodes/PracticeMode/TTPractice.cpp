#include <kamek.hpp>
#include <runtimeWrite.hpp>
#include <Gamemodes/PracticeMode/TTPractice.hpp>
#include <PulsarSystem.hpp>
#include <Settings/Settings.hpp>
#include <MarioKartWii/Item/ItemPlayer.hpp>
#include <MarioKartWii/Item/ItemManager.hpp>
#include <MarioKartWii/Item/Obj/Kumo.hpp>
#include <MarioKartWii/Input/ControllerHolder.hpp>
#include <MarioKartWii/Kart/KartManager.hpp>
#include <MarioKartWii/Kart/KartPhysics.hpp>
#include <MarioKartWii/File/RKG.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <MarioKartWii/Objects/Collidable/Itembox/Itembox.hpp>

namespace Pulsar {
namespace TTPractice {

static const u32 ITEM_COUNT = 7;
static const u16 INPUT_ITEM_USE = 0x4;
static const float STICK_WHEEL_THRESHOLD = 0.5f;
static const float RESPAWN_STICK_THRESHOLD = 0.5f;
static const u16 RESPAWN_HOLD_FRAMES = 30;
static const u32 TC_TIMER_OFFSET = 0x1d8;
static const u32 TC_NATURAL_STRIKE_TIMER = 600;
static const u32 TC_STRIKE_TIMER = 599;
static const u32 ITEM_OBJ_KILLED = 0x1;
static const u32 ITEM_OBJ_UNAVAILABLE = 0xc0;
static const u32 KART_STATUS_IN_BULLET = 0x8000000;
static const ItemId ITEM_WHEEL_ITEMS[ITEM_COUNT] = {
    TRIPLE_MUSHROOM, GOLDEN_MUSHROOM, MEGA_MUSHROOM, STAR, BULLET_BILL, THUNDER_CLOUD, MUSHROOM
};

struct SavedRaceProgress {
    u16 checkpoint;
    float raceCompletion;
    float raceCompletionMax;
    float firstKcpLapCompletion;
    float nextCheckpointLapCompletion;
    float nextCheckpointLapCompletionMax;
    u16 currentLap;
    u8 maxLap;
    u8 currentKCP;
    u8 maxKCP;
};

static bool isPracticeMode = false;
static u32 selectedItemIndexes[4] = {0, 0, 0, 0};
static s8 stickWheelDirections[4] = {0, 0, 0, 0};
static u16 respawnShortcutTimers[4] = {0, 0, 0, 0};
static u16 respawnSaveTimers[4] = {0, 0, 0, 0};
static Vec3 savedRespawnPositions[4];
static Quat savedRespawnRotations[4];
static SavedRaceProgress savedRespawnRaceProgress[4];
static bool hasSavedRespawn[4] = {false, false, false, false};
static bool hasGrantedItem[4] = {false, false, false, false};
static bool canRefillOnUse[4] = {false, false, false, false};

kmRuntimeUse(0x80590238);  // Kart::Link::SetKartPosition
kmRuntimeUse(0x80590288);  // Kart::Link::SetKartRotation
kmRuntimeUse(0x80590e28);  // Kart::Link::UpdateCameraOnRespawn
kmRuntimeUse(0x8059c118);  // Kart::Killer::CancelBullet
kmRuntimeUse(0x80828860);  // Objects::Itembox::Update
kmRuntimeUse(0x80819430);  // VolcanoPiece::Update timer read
kmRuntimeUse(0x80819de0);  // VolcanoPiece::IsCollidingImpl age calculation
kmRuntimeUse(0x80819e64);  // VolcanoPiece::IsCollidingImpl collision transform timer
kmRuntimeUse(0x808199dc);  // VolcanoPiece::IsCollidingNoTriangleCheckImpl timer read
kmRuntimeUse(0x808199e8);  // VolcanoPiece::IsCollidingNoTriangleCheckImpl age calculation
kmRuntimeUse(0x80819dd4);  // VolcanoPiece::IsCollidingImpl timer read
kmRuntimeUse(0x8081b020);  // ObjectExternKCL::IsCollidingNoTriangleCheckImpl collision transform timer
kmRuntimeUse(0x8081b03c);  // ObjectExternKCL::IsCollidingNoTriangleCheckImpl y-scale timer
kmRuntimeUse(0x8081b1d8);  // ObjectExternKCL::IsCollidingImpl collision transform timer
kmRuntimeUse(0x8081b1f4);  // ObjectExternKCL::IsCollidingImpl y-scale timer
kmRuntimeUse(0x80680854);  // ObjectExternKCL::IsCollidingNoTerrainInfo collision transform timer
kmRuntimeUse(0x8068086c);  // ObjectExternKCL::IsCollidingNoTerrainInfo y-scale timer
kmRuntimeUse(0x80680960);  // ObjectExternKCL::IsCollidingAddEntryNoTerrainInfo collision transform timer
kmRuntimeUse(0x80680978);  // ObjectExternKCL::IsCollidingAddEntryNoTerrainInfo y-scale timer
kmRuntimeUse(0x80680a6c);  // ObjectExternKCL::vf_0xfc collision transform timer
kmRuntimeUse(0x80680a84);  // ObjectExternKCL::vf_0xfc y-scale timer
kmRuntimeUse(0x80680e58);  // ObjectExternKCL::IsCollidingNoTerrainInfoNoTriangleCheck y-scale timer
kmRuntimeUse(0x80680e70);  // ObjectExternKCL::IsCollidingNoTerrainInfoNoTriangleCheck collision transform timer
kmRuntimeUse(0x80680f54);  // ObjectExternKCL::IsCollidingAddEntryNoTerrainInfoNoTriangleCheck y-scale timer
kmRuntimeUse(0x80680f6c);  // ObjectExternKCL::IsCollidingAddEntryNoTerrainInfoNoTriangleCheck collision transform timer
kmRuntimeUse(0x80681050);  // ObjectExternKCL::IsColliding y-scale timer
kmRuntimeUse(0x80681068);  // ObjectExternKCL::IsColliding collision transform timer
kmRuntimeUse(0x8081ad80);  // ObjectExternKCL::UpdateCollisionPosition saved timer
kmRuntimeUse(0x8081ad98);  // ObjectExternKCL::UpdateCollisionPosition cache comparison timer
kmRuntimeUse(0x8081ada4);  // ObjectExternKCL::UpdateCollisionPosition current/future matrix branch
kmRuntimeUse(0x80818358);  // VolcanoPiece::UpdateCollisionPosition saved timer
kmRuntimeUse(0x80818370);  // VolcanoPiece::UpdateCollisionPosition age calculation
kmRuntimeUse(0x80818698);  // VolcanoPiece::SetYScale saved timer
kmRuntimeUse(0x808186b0);  // VolcanoPiece::SetYScale age calculation
kmRuntimeUse(0x808187e4);  // VolcanoPiece::UpdateTransformationMtxImpl age calculation

static Item::ObjKumo* FindActiveThunderCloud(Item::Player& player);

typedef void (*SetKartPositionFn)(Kart::Link* link, const Vec3& position);
typedef void (*SetKartRotationFn)(Kart::Link* link, const Quat& rotation);
typedef void (*UpdateCameraOnRespawnFn)(const Kart::Link* link);
typedef void (*CancelBulletFn)(Kart::Killer* killer);
typedef void (*ItemBoxUpdateFn)(Objects::Itembox* itembox);

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

static ItemBoxUpdateFn GetItemBoxUpdate() {
    static const ItemBoxUpdateFn function = reinterpret_cast<ItemBoxUpdateFn>(kmRuntimeAddr(0x80828860));
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

static void ApplyTimedKCLFreeze(bool enabled) {
    kmRuntimeWrite32A(0x80819430, enabled ? 0x38000000 : 0x80050020);  // li r0, 0 / lwz r0, 0x20(r5)
    kmRuntimeWrite32A(0x80819de0, enabled ? 0x38800000 : 0x7c892050);  // li r4, 0 / subf r4, r9, r4
    kmRuntimeWrite32A(0x80819e64, enabled ? 0x38800000 : 0x7ec4b378);  // li r4, 0 / mr r4, r22
    kmRuntimeWrite32A(0x808199dc, enabled ? 0x38800000 : 0x808a0020);  // li r4, 0 / lwz r4, 0x20(r10)
    kmRuntimeWrite32A(0x808199e8, enabled ? 0x38800000 : 0x7c892050);  // li r4, 0 / subf r4, r9, r4
    kmRuntimeWrite32A(0x80819dd4, enabled ? 0x38800000 : 0x808a0020);  // li r4, 0 / lwz r4, 0x20(r10)
    kmRuntimeWrite32A(0x8081b020, enabled ? 0x38800000 : 0x7fc4f378);  // li r4, 0 / mr r4, r30
    kmRuntimeWrite32A(0x8081b03c, enabled ? 0x38800000 : 0x7fc4f378);  // li r4, 0 / mr r4, r30
    kmRuntimeWrite32A(0x8081b1d8, enabled ? 0x38800000 : 0x7fc4f378);  // li r4, 0 / mr r4, r30
    kmRuntimeWrite32A(0x8081b1f4, enabled ? 0x38800000 : 0x7fc4f378);  // li r4, 0 / mr r4, r30
    kmRuntimeWrite32A(0x80680854, enabled ? 0x38800000 : 0x7fe4fb78);  // li r4, 0 / mr r4, r31
    kmRuntimeWrite32A(0x8068086c, enabled ? 0x38800000 : 0x7fe4fb78);  // li r4, 0 / mr r4, r31
    kmRuntimeWrite32A(0x80680960, enabled ? 0x38800000 : 0x7fe4fb78);  // li r4, 0 / mr r4, r31
    kmRuntimeWrite32A(0x80680978, enabled ? 0x38800000 : 0x7fe4fb78);  // li r4, 0 / mr r4, r31
    kmRuntimeWrite32A(0x80680a6c, enabled ? 0x38800000 : 0x7fe4fb78);  // li r4, 0 / mr r4, r31
    kmRuntimeWrite32A(0x80680a84, enabled ? 0x38800000 : 0x7fe4fb78);  // li r4, 0 / mr r4, r31
    kmRuntimeWrite32A(0x80680e58, enabled ? 0x38800000 : 0x7fe4fb78);  // li r4, 0 / mr r4, r31
    kmRuntimeWrite32A(0x80680e70, enabled ? 0x38800000 : 0x7fe4fb78);  // li r4, 0 / mr r4, r31
    kmRuntimeWrite32A(0x80680f54, enabled ? 0x38800000 : 0x7fe4fb78);  // li r4, 0 / mr r4, r31
    kmRuntimeWrite32A(0x80680f6c, enabled ? 0x38800000 : 0x7fe4fb78);  // li r4, 0 / mr r4, r31
    kmRuntimeWrite32A(0x80681050, enabled ? 0x38800000 : 0x7fe4fb78);  // li r4, 0 / mr r4, r31
    kmRuntimeWrite32A(0x80681068, enabled ? 0x38800000 : 0x7fe4fb78);  // li r4, 0 / mr r4, r31
    kmRuntimeWrite32A(0x8081ad80, enabled ? 0x3be00000 : 0x7c9f2378);  // li r31, 0 / mr r31, r4
    kmRuntimeWrite32A(0x8081ad98, enabled ? 0x7c1f0050 : 0x7c040050);  // subf r0, r31, r0 / subf r0, r4, r0
    kmRuntimeWrite32A(0x8081ada4, enabled ? 0x2c1f0000 : 0x2c040000);  // cmpwi r31, 0 / cmpwi r4, 0
    kmRuntimeWrite32A(0x80818358, enabled ? 0x3ba00000 : 0x7c9d2378);  // li r29, 0 / mr r29, r4
    kmRuntimeWrite32A(0x80818370, enabled ? 0x38000000 : 0x7c040050);  // li r0, 0 / subf r0, r4, r0
    kmRuntimeWrite32A(0x80818698, enabled ? 0x3ba00000 : 0x7c9d2378);  // li r29, 0 / mr r29, r4
    kmRuntimeWrite32A(0x808186b0, enabled ? 0x38000000 : 0x7c040050);  // li r0, 0 / subf r0, r4, r0
    kmRuntimeWrite32A(0x808187e4, enabled ? 0x38000000 : 0x7c050050);  // li r0, 0 / subf r0, r5, r0
}

static void RefreshTimedKCLFreeze() {
    ApplyTimedKCLFreeze(isPracticeMode);
}

static RaceLoadHook RefreshTimedKCLFreezeOnRaceLoad(RefreshTimedKCLFreeze);
static FrameLoadHook RefreshTimedKCLFreezeOnFrameLoad(RefreshTimedKCLFreeze);

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

void SetPracticeMode(bool enabled) {
    isPracticeMode = enabled;
    ApplyTimedKCLFreeze(enabled);
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

bool AreItemBoxesEnabled() {
    if (!Settings::Mgr::IsCreated()) return true;
    return Settings::Mgr::Get().GetUserSettingValue(Settings::SETTINGSTYPE_TTPRACTICE, RADIO_TTPRACTICE_ITEMBOXES) ==
           TTPRACTICE_ITEMBOXES_ENABLED;
}

ItemId GetStartingItem(u32 hudSlotId) {
    if (hudSlotId >= 4) hudSlotId = 0;
    return ITEM_WHEEL_ITEMS[selectedItemIndexes[hudSlotId]];
}

static bool IsEnabled() {
    const Racedata* racedata = Racedata::sInstance;
    return isPracticeMode && racedata != nullptr && racedata->racesScenario.settings.gamemode == MODE_TIME_TRIAL;
}

static void UpdatePracticeItemBox(Objects::Itembox* itembox) {
    if (itembox != nullptr && IsEnabled() && !AreItemBoxesEnabled()) {
        itembox->isActive = 0;
        itembox->timer = 0;
        itembox->respawnTime = 0x7fffffff;
        itembox->DisableCollision();
        itembox->ToggleVisible(false);
        return;
    }

    GetItemBoxUpdate()(itembox);
}
kmWritePointer(0x808d7bd4, UpdatePracticeItemBox);

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

static void SaveRaceProgress(RaceinfoPlayer& player, u32 hudSlotId) {
    SavedRaceProgress& progress = savedRespawnRaceProgress[hudSlotId];
    progress.checkpoint = player.checkpoint;
    progress.raceCompletion = player.raceCompletion;
    progress.raceCompletionMax = player.raceCompletionMax;
    progress.firstKcpLapCompletion = player.firstKcpLapCompletion;
    progress.nextCheckpointLapCompletion = player.nextCheckpointLapCompletion;
    progress.nextCheckpointLapCompletionMax = player.nextCheckpointLapCompletionMax;
    progress.currentLap = player.currentLap;
    progress.maxLap = player.maxLap;
    progress.currentKCP = player.currentKCP;
    progress.maxKCP = player.maxKCP;
}

static void RestoreRaceProgress(RaceinfoPlayer& player, u32 hudSlotId) {
    const SavedRaceProgress& progress = savedRespawnRaceProgress[hudSlotId];
    player.checkpoint = progress.checkpoint;
    player.raceCompletion = progress.raceCompletion;
    player.raceCompletionMax = progress.raceCompletionMax;
    player.firstKcpLapCompletion = progress.firstKcpLapCompletion;
    player.nextCheckpointLapCompletion = progress.nextCheckpointLapCompletion;
    player.nextCheckpointLapCompletionMax = progress.nextCheckpointLapCompletionMax;
    player.currentLap = progress.currentLap;
    player.maxLap = progress.maxLap;
    player.currentKCP = progress.currentKCP;
    player.maxKCP = progress.maxKCP;
}

static void RestoreSavedKartTransform(Kart::Player& kartPlayer, Kart::Physics& physics, Kart::PhysicsHolder* physicsHolder,
                                      RaceinfoPlayer& raceinfoPlayer, u32 hudSlotId) {
    Vec3 restorePosition = savedRespawnPositions[hudSlotId];

    CancelBulletIfActive(kartPlayer);
    GetSetKartPosition()(&kartPlayer, restorePosition);
    GetSetKartRotation()(&kartPlayer, savedRespawnRotations[hudSlotId]);
    physics.position = restorePosition;
    RestoreRaceProgress(raceinfoPlayer, hudSlotId);
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
    Raceinfo* raceinfo = Raceinfo::sInstance;
    if (raceinfo == nullptr || raceinfo->players == nullptr) return;
    RaceinfoPlayer* raceinfoPlayer = raceinfo->players[playerId];
    if (raceinfoPlayer == nullptr) return;
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
        SaveRaceProgress(*raceinfoPlayer, hudSlotId);
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
    RestoreSavedKartTransform(*kartPlayer, *physics, physicsHolder, *raceinfoPlayer, hudSlotId);
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

}  // namespace TTPractice
}  // namespace Pulsar
