#include <kamek.hpp>
#include <MarioKartWii/KMP/KMPManager.hpp>
#include <MarioKartWii/Kart/KartPhysics.hpp>
#include <MarioKartWii/Kart/KartMovement.hpp>
#include <MarioKartWii/Item/Obj/ItemObj.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <core/egg/Math/Math.hpp>
#include <Race/GravityFields.hpp>
#include <Race/GravityFieldsTempDebug.hpp>

namespace Pulsar {
namespace Race {
namespace GravityFields {
namespace {

// KMP AREA contract:
// - type 11: gravity field
// - setting1: gravity strength in milli-units (1300 = 1.3)
// - setting2: blend duration in frames (0 = default)
const u8 kGravityAreaType = 11;
const float kDefaultGravityStrength = 1.3f;
const float kMinGravityStrength = 0.05f;
const float kMaxGravityStrength = 8.0f;
const u16 kDefaultBlendFrames = 8;
const float kVectorEpsilon = 0.0001f;
const u8 kMaxTrackedPlayers = 12;
const u8 kGravityVolumeShapeBox = 0;
const u8 kGravityVolumeShapeCylinder = 1;
const u8 kGravityVolumeShapeSphere = 2;

struct GravityState {
    bool isInitialized;
    s16 areaId;
    Vec3 gravityDown;
    Quat gravityRotation;
    float gravityStrength;
    u16 blendFrames;
    u16 exitGraceCounter;
    u32 lastResolvedRaceFrame;
    u32 lastCorrectionFrame;
};

GravityState sGravityStates[kMaxTrackedPlayers];

typedef void (*ItemVelocityInitFunc)(Item::Obj* obj, const Vec3* sourcePosition, const Vec3* sourceSpeed, const Vec3* direction, bool useRandomness);
ItemVelocityInitFunc sItemVelocityInit = reinterpret_cast<ItemVelocityInitFunc>(0x807a6738);

Vec3 MakeVec(float x, float y, float z) {
    Vec3 vec;
    vec.x = x;
    vec.y = y;
    vec.z = z;
    return vec;
}

Vec3 WorldDown() {
    return MakeVec(0.0f, -1.0f, 0.0f);
}

float ClampFloat(float value, float min, float max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

float MinFloat(float lhs, float rhs) {
    return lhs < rhs ? lhs : rhs;
}

float DotVec(const Vec3& lhs, const Vec3& rhs) {
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

Vec3 CrossVec(const Vec3& lhs, const Vec3& rhs) {
    return MakeVec(
        lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.z * rhs.x - lhs.x * rhs.z,
        lhs.x * rhs.y - lhs.y * rhs.x);
}

Vec3 SubVec(const Vec3& lhs, const Vec3& rhs) {
    return MakeVec(lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z);
}

Vec3 ScaleVec(const Vec3& vec, float scale) {
    return MakeVec(vec.x * scale, vec.y * scale, vec.z * scale);
}

float VecLength(const Vec3& vec) {
    const float squaredLength = vec.x * vec.x + vec.y * vec.y + vec.z * vec.z;
    return EGG::Math::Sqrt(squaredLength);
}

bool NormalizeSafe(Vec3& vec) {
    const float length = VecLength(vec);
    if (length <= kVectorEpsilon) return false;

    const float invLength = 1.0f / length;
    vec.x *= invLength;
    vec.y *= invLength;
    vec.z *= invLength;
    return true;
}

Quat IdentityQuat() {
    Quat quat;
    // EGG::Quatf::Set argument order is (w, x, y, z).
    quat.Set(1.0f, 0.0f, 0.0f, 0.0f);
    return quat;
}

Quat MakeRotationQuat(const Vec3& from, const Vec3& to) {
    Vec3 normalizedFrom = from;
    Vec3 normalizedTo = to;
    NormalizeSafe(normalizedFrom);
    NormalizeSafe(normalizedTo);

    const float dot = ClampFloat(DotVec(normalizedFrom, normalizedTo), -1.0f, 1.0f);
    Quat quat = IdentityQuat();
    if (dot > 0.9999f) return quat;

    Vec3 axis;
    if (dot < -0.9999f) {
        Vec3 helper = (normalizedFrom.x > -0.9f && normalizedFrom.x < 0.9f)
                          ? MakeVec(1.0f, 0.0f, 0.0f)
                          : MakeVec(0.0f, 0.0f, 1.0f);
        axis = CrossVec(normalizedFrom, helper);
        if (!NormalizeSafe(axis)) axis = MakeVec(0.0f, 0.0f, 1.0f);
        quat.SetAxisRotation(axis, PI);
        return quat;
    }

    axis = CrossVec(normalizedFrom, normalizedTo);
    if (!NormalizeSafe(axis)) return quat;

    quat.SetAxisRotation(axis, EGG::Math::Acos(dot));
    return quat;
}

void ResetGravityState(GravityState& state) {
    state.isInitialized = false;
    state.areaId = -1;
    state.gravityDown = WorldDown();
    state.gravityRotation = IdentityQuat();
    state.gravityStrength = kDefaultGravityStrength;
    state.blendFrames = kDefaultBlendFrames;
    state.exitGraceCounter = 0;
    state.lastResolvedRaceFrame = 0xFFFFFFFF;
    state.lastCorrectionFrame = 0xFFFFFFFF;
}

void ResetAllGravityStates() {
    for (u8 i = 0; i < kMaxTrackedPlayers; ++i) {
        ResetGravityState(sGravityStates[i]);
    }
}

u8 GetGravityVolumeShape(const AREA& area) {
    if (area.enemyRouteId <= kGravityVolumeShapeSphere) return area.enemyRouteId;
    if (area.shape == 1) return kGravityVolumeShapeCylinder;
    return kGravityVolumeShapeBox;
}

bool IsPointInGravitySphere(const Vec3& position, KMP::Holder<AREA>& holder) {
    const AREA* area = holder.raw;
    if (area == nullptr) return false;

    Vec3 up = holder.yVector;
    if (!NormalizeSafe(up)) up = MakeVec(0.0f, 1.0f, 0.0f);

    const float halfHeight = holder.height * 0.5f;
    const float radius = MinFloat(MinFloat(holder.halfWidth, holder.halfLength), halfHeight);
    if (radius <= kVectorEpsilon) return false;

    const Vec3 center = SubVec(area->position, ScaleVec(up, halfHeight));
    const Vec3 diff = SubVec(position, center);
    const float distanceSq = DotVec(diff, diff);
    return distanceSq <= radius * radius;
}

bool IsPointInGravityArea(const Vec3& position, KMP::Holder<AREA>& holder) {
    if (holder.raw == nullptr || holder.raw->type != kGravityAreaType) return false;

    const u8 volumeShape = GetGravityVolumeShape(*holder.raw);
    if (volumeShape == kGravityVolumeShapeSphere) return IsPointInGravitySphere(position, holder);
    return holder.IsPointInAREA(position);
}

bool TryGetGravityAreaById(const KMP::Section<AREA>& areaSection, s16 areaId, KMP::Holder<AREA>*& outHolder) {
    outHolder = nullptr;
    if (areaId < 0 || areaId >= areaSection.pointCount) return false;
    KMP::Holder<AREA>* holder = areaSection.holdersArray[areaId];
    if (holder == nullptr || holder->raw == nullptr) return false;
    if (holder->raw->type != kGravityAreaType) return false;
    outHolder = holder;
    return true;
}

void ResolveGravityParamsFromHolder(const KMP::Holder<AREA>& holder, Vec3& outDown, float& outStrength, u16& outBlendFrames) {
    Vec3 gravityDown = MakeVec(-holder.yVector.x, -holder.yVector.y, -holder.yVector.z);
    if (!NormalizeSafe(gravityDown)) gravityDown = WorldDown();

    const AREA* area = holder.raw;
    float gravityStrength = kDefaultGravityStrength;
    if (area != nullptr && area->setting1 != 0) gravityStrength = static_cast<float>(area->setting1) * 0.001f;
    gravityStrength = ClampFloat(gravityStrength, kMinGravityStrength, kMaxGravityStrength);

    u16 blendFrames = kDefaultBlendFrames;
    if (area != nullptr && area->setting2 != 0) blendFrames = area->setting2;

    outDown = gravityDown;
    outStrength = gravityStrength;
    outBlendFrames = blendFrames;
}

bool ResolveGravityFieldByAreaId(const KMP::Section<AREA>& areaSection, s16 areaId, Vec3& outDown, float& outStrength, u16& outBlendFrames) {
    KMP::Holder<AREA>* holder = nullptr;
    if (!TryGetGravityAreaById(areaSection, areaId, holder) || holder == nullptr) return false;
    ResolveGravityParamsFromHolder(*holder, outDown, outStrength, outBlendFrames);
    return true;
}

bool ResolveGravityField(const Vec3& position, s16 preferredAreaId, s16& outAreaId, Vec3& outDown, float& outStrength, u16& outBlendFrames) {
    KMP::Manager* kmpManager = KMP::Manager::sInstance;
    if (kmpManager == nullptr || kmpManager->areaSection == nullptr || kmpManager->areaSection->pointCount == 0) return false;
    KMP::Section<AREA>& areaSection = *kmpManager->areaSection;

    s16 bestAreaId = -1;
    u8 bestPriority = 0xFF;
    KMP::Holder<AREA>* bestHolder = nullptr;

    // Preserve continuity when possible, unless another overlapping field has a higher priority.
    KMP::Holder<AREA>* preferredHolder = nullptr;
    if (TryGetGravityAreaById(areaSection, preferredAreaId, preferredHolder) && IsPointInGravityArea(position, *preferredHolder)) {
        bestAreaId = preferredAreaId;
        bestPriority = preferredHolder->raw->priority;
        bestHolder = preferredHolder;
    }

    for (s16 areaId = 0; areaId < areaSection.pointCount; ++areaId) {
        if (areaId == preferredAreaId) continue;

        KMP::Holder<AREA>* holder = nullptr;
        if (!TryGetGravityAreaById(areaSection, areaId, holder)) continue;
        if (!IsPointInGravityArea(position, *holder)) continue;

        const u8 priority = holder->raw->priority;
        if (bestHolder == nullptr || priority < bestPriority) {
            bestAreaId = areaId;
            bestPriority = priority;
            bestHolder = holder;
        }
    }

    if (bestHolder == nullptr || bestHolder->raw == nullptr) return false;
    KMP::Holder<AREA>* holder = bestHolder;

    Vec3 gravityDown = WorldDown();
    float gravityStrength = kDefaultGravityStrength;
    u16 blendFrames = kDefaultBlendFrames;
    ResolveGravityParamsFromHolder(*holder, gravityDown, gravityStrength, blendFrames);

    outAreaId = bestAreaId;
    outDown = gravityDown;
    outStrength = gravityStrength;
    outBlendFrames = blendFrames;
    return true;
}

void BlendGravityState(GravityState& state, const Vec3& targetDown, float targetStrength, u16 blendFrames, bool snap) {
    Quat targetRotation = MakeRotationQuat(WorldDown(), targetDown);
    if (!state.isInitialized || snap || blendFrames <= 1) {
        state.gravityRotation = targetRotation;
        state.gravityDown = targetDown;
        state.gravityStrength = targetStrength;
        state.isInitialized = true;
        return;
    }

    float blendFactor = 1.0f / static_cast<float>(blendFrames);
    blendFactor = ClampFloat(blendFactor, 0.0f, 1.0f);

    Quat blendedRotation;
    state.gravityRotation.SlerpTo(targetRotation, blendedRotation, blendFactor);
    blendedRotation.Normalise();
    state.gravityRotation = blendedRotation;

    state.gravityDown = Vec3::RotateQuaternion(state.gravityRotation, WorldDown());
    if (!NormalizeSafe(state.gravityDown)) state.gravityDown = targetDown;

    state.gravityStrength += (targetStrength - state.gravityStrength) * blendFactor;
}

void ComputeKartGravity(const Kart::Link& link, bool snapTransition, Vec3& gravityVector, float& gravityStrength) {
    const Vec3& position = link.GetPosition();
    Raceinfo* raceinfo = Raceinfo::sInstance;
    const bool hasRaceFrame = raceinfo != nullptr;
    const u32 raceFrame = hasRaceFrame ? raceinfo->raceFrames : 0;

    const u8 playerIdx = link.GetPlayerIdx();
    if (playerIdx >= kMaxTrackedPlayers) {
        s16 areaId = -1;
        Vec3 gravityDown = WorldDown();
        float strength = kDefaultGravityStrength;
        u16 blendFrames = kDefaultBlendFrames;
        ResolveGravityField(position, -1, areaId, gravityDown, strength, blendFrames);
        gravityVector = ScaleVec(gravityDown, strength);
        gravityStrength = strength;
        return;
    }

    GravityState& state = sGravityStates[playerIdx];
    if (!state.isInitialized) ResetGravityState(state);

    // Gravity is queried by multiple gameplay paths in the same frame (body + wheels).
    // Resolve once per player/frame and reuse to avoid blending/hysteresis running multiple times.
    if (!snapTransition && hasRaceFrame && state.lastResolvedRaceFrame == raceFrame) {
        gravityVector = ScaleVec(state.gravityDown, state.gravityStrength);
        gravityStrength = state.gravityStrength;
        return;
    }

    s16 areaId = -1;
    Vec3 gravityDown = WorldDown();
    float strength = kDefaultGravityStrength;
    u16 blendFrames = kDefaultBlendFrames;
    const s16 previousAreaId = state.areaId;
    const s16 preferredAreaId = state.areaId;
    const bool resolvedFromOverlap = ResolveGravityField(position, preferredAreaId, areaId, gravityDown, strength, blendFrames);
    if (resolvedFromOverlap) {
        state.exitGraceCounter = 0;
    } else {
        areaId = -1;

        // Border hysteresis: keep the previous area for a short window to avoid rapid
        // enter/exit oscillation caused by tiny boundary jitters.
        KMP::Manager* kmpManager = KMP::Manager::sInstance;
        if (previousAreaId >= 0 && kmpManager != nullptr && kmpManager->areaSection != nullptr) {
            const KMP::Section<AREA>& areaSection = *kmpManager->areaSection;
            const u16 graceFrames = state.blendFrames > 0 ? state.blendFrames : kDefaultBlendFrames;
            if (state.exitGraceCounter < graceFrames &&
                ResolveGravityFieldByAreaId(areaSection, previousAreaId, gravityDown, strength, blendFrames)) {
                areaId = previousAreaId;
                ++state.exitGraceCounter;
            } else {
                state.exitGraceCounter = graceFrames;
            }
        }
    }

    state.areaId = areaId;
    state.blendFrames = blendFrames;
    if (hasRaceFrame) state.lastResolvedRaceFrame = raceFrame;
    BlendGravityState(state, gravityDown, strength, blendFrames, snapTransition);
    TempDebug::OnPlayerGravityResolved(playerIdx, position, previousAreaId, areaId, state.gravityDown, state.gravityStrength, blendFrames, snapTransition);

    gravityVector = ScaleVec(state.gravityDown, state.gravityStrength);
    gravityStrength = state.gravityStrength;
}

bool ShouldOverrideKartUpFromGravity(const GravityState& state) {
    return state.isInitialized && state.areaId >= 0;
}

typedef void (*MovementUpdateUpsFunc)(Kart::Movement* movement);
extern "C" void GravityFieldsMovementUpdateUps(Kart::Movement* movement);
MovementUpdateUpsFunc sMovementUpdateUps = GravityFieldsMovementUpdateUps;

void UpdateUpsWithGravityField(Kart::Movement& movement) {
    sMovementUpdateUps(&movement);

    Kart::Link& link = movement;
    const u8 playerIdx = link.GetPlayerIdx();
    if (playerIdx >= kMaxTrackedPlayers) return;

    GravityState& state = sGravityStates[playerIdx];
    if (!ShouldOverrideKartUpFromGravity(state)) return;

    Vec3 localUp = ScaleVec(state.gravityDown, -1.0f);
    if (!NormalizeSafe(localUp)) return;

    // === Body gravity force correction ===
    // The game only applies physics->gravity (a scalar) to normalAcceleration.y.
    // GetBodyGravityScalar correctly extracts the Y component of our custom gravity.
    // But the X and Z components are lost — we inject them into speed0 directly.
    // This must happen exactly once per player per frame.
    Raceinfo* raceinfo = Raceinfo::sInstance;
    const u32 currentFrame = (raceinfo != nullptr) ? raceinfo->raceFrames : 0xFFFFFFFF;
    if (state.lastCorrectionFrame != currentFrame) {
        state.lastCorrectionFrame = currentFrame;

        Kart::PhysicsHolder& physicsHolder = link.GetPhysicsHolder();
        Kart::Physics* physics = physicsHolder.physics;
        if (physics != nullptr) {
            const float extraX = state.gravityDown.x * state.gravityStrength;
            const float extraZ = state.gravityDown.z * state.gravityStrength;
            if (extraX != 0.0f || extraZ != 0.0f) {
                // Inject the missing gravity components into body velocity.
                physics->speed0.x += extraX;
                physics->speed0.z += extraZ;
                // Also update total speed so position integration picks it up immediately.
                physics->speed.x += extraX;
                physics->speed.z += extraZ;
                physicsHolder.speed.x += extraX;
                physicsHolder.speed.z += extraZ;
            }
        }
    }

    // === Force ground state when touching any surface ===
    // The game classifies floor vs wall by comparing collision normals against world-up (0,1,0).
    // In a custom gravity field, wall/ceiling surfaces aligned with our gravity-up ARE floors.
    // Detect any surface contact and force ground flags so the kart can drive, accelerate, steer.
    Kart::Status* status = static_cast<Kart::Status*>(0);
    if (movement.pointers != 0) status = movement.pointers->kartStatus;
    if (status != nullptr) {
        const u32 collisionMask = 0x20 | 0x40 | 0x400 | 0x800;  // wall3, wall, body floor, wheel floor
        const bool touchingSurface = (status->bitfield0 & collisionMask) != 0;
        if (touchingSurface) {
            status->bitfield0 |= 0x40000;   // bit 18: ground
            status->bitfield0 |= 0x1000;    // bit 12: floor collision with all wheels
            status->bitfield0 |= 0x800;     // bit 11: floor collision with any wheel
            status->bitfield0 |= 0x400;     // bit 10: floor collision with kart body
            status->bitfield0 &= ~0x8000;   // clear bit 15: airtime > 20
            const u16 wheelCount = link.GetWheelCount0();
            movement.flooorCollisionCount = static_cast<s16>(wheelCount);
            status->airtime = 0;
        }
    }

    // === Override up vectors and floor normals ===
    // Align movement-space up vectors to local gravity so steering, camera, and
    // ground-contour following work relative to the custom gravity direction.
    movement.up = localUp;
    movement.smoothedUp = localUp;

    if (status != nullptr) {
        status->floorNor = localUp;
        status->unknown_0x34 = localUp;
    }
}
kmCall(0x80578a34, UpdateUpsWithGravityField);
kmCall(0x80578f30, UpdateUpsWithGravityField);

void ApplyItemGravityField(Item::Obj& itemObj) {
    s16 areaId = -1;
    Vec3 gravityDown = WorldDown();
    float gravityStrength = kDefaultGravityStrength;
    u16 blendFrames = kDefaultBlendFrames;
    if (!ResolveGravityField(itemObj.position, -1, areaId, gravityDown, gravityStrength, blendFrames)) return;

    u8* itemObjBytes = reinterpret_cast<u8*>(&itemObj);
    // Item::ObjMiddle stores the per-frame acceleration vector at 0x18c.
    float* itemAcceleration = reinterpret_cast<float*>(itemObjBytes + 0x18c);

    Vec3 oldAcceleration = MakeVec(itemAcceleration[0], itemAcceleration[1], itemAcceleration[2]);
    float accelMagnitude = VecLength(oldAcceleration);
    if (accelMagnitude <= kVectorEpsilon) return;

    const float gravityScale = gravityStrength / kDefaultGravityStrength;
    accelMagnitude *= gravityScale;

    const Vec3 newAcceleration = ScaleVec(gravityDown, accelMagnitude);
    const Vec3 accelerationDelta = SubVec(newAcceleration, oldAcceleration);
    itemObj.speed.x += accelerationDelta.x;
    itemObj.speed.y += accelerationDelta.y;
    itemObj.speed.z += accelerationDelta.z;

    itemAcceleration[0] = newAcceleration.x;
    itemAcceleration[1] = newAcceleration.y;
    itemAcceleration[2] = newAcceleration.z;

    TempDebug::OnItemGravityApplied(itemObj, areaId, gravityDown, gravityStrength);
}

void InitItemVelocityWithGravityField(Item::Obj& itemObj, const Vec3& sourcePosition, const Vec3& sourceSpeed, const Vec3& direction, bool useRandomness) {
    sItemVelocityInit(&itemObj, &sourcePosition, &sourceSpeed, &direction, useRandomness);
    ApplyItemGravityField(itemObj);
}
kmCall(0x8079fc1c, InitItemVelocityWithGravityField);
kmCall(0x807a1bc8, InitItemVelocityWithGravityField);
kmCall(0x807a1c5c, InitItemVelocityWithGravityField);
kmCall(0x807a381c, InitItemVelocityWithGravityField);

void SyncRespawnGravity(Kart::Link& link) {
    link.UpdateCameraOnRespawn();

    Vec3 gravityVector;
    float gravityStrength;
    ComputeKartGravity(link, true, gravityVector, gravityStrength);

    Kart::PhysicsHolder& physicsHolder = link.GetPhysicsHolder();
    if (physicsHolder.physics != nullptr) {
        physicsHolder.physics->gravity = GetBodyGravityScalar(gravityVector, gravityStrength, physicsHolder.physics->gravity);
    }

    Vec3 gravityDown = WorldDown();
    if (gravityStrength > kVectorEpsilon) gravityDown = ScaleVec(gravityVector, 1.0f / gravityStrength);

    s16 areaId = -1;
    const u8 playerIdx = link.GetPlayerIdx();
    if (playerIdx < kMaxTrackedPlayers) areaId = sGravityStates[playerIdx].areaId;
    TempDebug::OnRespawn(link, areaId, gravityDown, gravityStrength);
}
kmCall(0x80584418, SyncRespawnGravity);

void ResetGravityStateOnRaceLoad() {
    ResetAllGravityStates();
    TempDebug::OnRaceLoad();
}
static RaceLoadHook resetGravityStateOnRaceLoadHook(ResetGravityStateOnRaceLoad);

}  // namespace

u8 GetGravityAreaType() {
    return kGravityAreaType;
}

float GetDefaultGravityStrength() {
    return kDefaultGravityStrength;
}

bool TryGetGravityAtPosition(const Vec3& position, Vec3& gravityDown, float& gravityStrength) {
    s16 areaId = -1;
    u16 blendFrames = kDefaultBlendFrames;
    if (!ResolveGravityField(position, -1, areaId, gravityDown, gravityStrength, blendFrames)) {
        gravityDown = WorldDown();
        gravityStrength = kDefaultGravityStrength;
        return false;
    }
    return true;
}

void UpdateKartGravity(const Kart::Link& link, Vec3& gravityVector, float& gravityStrength) {
    ComputeKartGravity(link, false, gravityVector, gravityStrength);
}

void UpdateKartGravity(const Kart::Sub& sub, Vec3& gravityVector, float& gravityStrength) {
    ComputeKartGravity(sub, false, gravityVector, gravityStrength);
}

void ForceKartGravityRefresh(const Kart::Link& link) {
    Vec3 gravityVector;
    float gravityStrength;
    ComputeKartGravity(link, true, gravityVector, gravityStrength);
    (void)gravityVector;
    (void)gravityStrength;
}

Vec3 GetGravityDownAtPosition(const Vec3& position) {
    Vec3 gravityDown;
    float gravityStrength;
    TryGetGravityAtPosition(position, gravityDown, gravityStrength);
    return gravityDown;
}

float GetBodyGravityScalar(const Vec3& gravityVector, float gravityStrength, float previousScalar) {
    (void)gravityStrength;
    (void)previousScalar;
    return gravityVector.y;
}

}  // namespace GravityFields
}  // namespace Race
}  // namespace Pulsar
