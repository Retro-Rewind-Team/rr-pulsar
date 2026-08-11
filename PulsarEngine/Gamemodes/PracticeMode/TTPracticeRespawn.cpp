#include <kamek.hpp>
#include <runtimeWrite.hpp>
#include <Gamemodes/PracticeMode/TTPracticeInternal.hpp>
#include <MarioKartWii/Input/ControllerHolder.hpp>
#include <MarioKartWii/Kart/KartPhysics.hpp>
#include <MarioKartWii/Race/RaceData.hpp>

namespace Pulsar {
namespace TTPractice {

kmRuntimeUse(0x80590238);  // Kart::Link::SetKartPosition
kmRuntimeUse(0x80590288);  // Kart::Link::SetKartRotation
kmRuntimeUse(0x80590e28);  // Kart::Link::UpdateCameraOnRespawn
kmRuntimeUse(0x8059c118);  // Kart::Killer::CancelBullet

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

static bool IsAnalogRespawnShortcutHeld(float stickX, float stickY) {
    return stickY >= RESPAWN_STICK_THRESHOLD && stickY >= stickX && stickY >= -stickX;
}

static bool IsAnalogRespawnSaveHeld(float stickX, float stickY) {
    const float down = -stickY;
    return down >= RESPAWN_STICK_THRESHOLD && down >= stickX && down >= -stickX;
}

bool IsAnalogRespawnInputHeld(float stickX, float stickY) {
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

void UpdatePlayerAndPracticeWheel(Item::Player& player);

void UpdatePlayerRespawnShortcut(Item::Player& player) {
    UpdateRespawnShortcut(player);
}

}  // namespace TTPractice
}  // namespace Pulsar
