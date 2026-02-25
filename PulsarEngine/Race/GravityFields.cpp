#include <kamek.hpp>
#include <runtimeWrite.hpp>
#include <MarioKartWii/KMP/KMPManager.hpp>
#include <MarioKartWii/Kart/KartPhysics.hpp>
#include <MarioKartWii/Kart/KartMovement.hpp>
#include <MarioKartWii/Kart/KartPointers.hpp>
#include <MarioKartWii/Item/Obj/ItemObj.hpp>
#include <MarioKartWii/Item/PlayerObj.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <core/egg/Math/Math.hpp>
#include <Race/GravityFields.hpp>
#include <Race/GravityFieldsTempDebug.hpp>

namespace Pulsar {
namespace Race {
namespace GravityFields {
namespace {

kmRuntimeUse(0x807a6738);  // Item init velocity
kmRuntimeUse(0x807b5178);  // Shell/targeting item initial placement helper
kmRuntimeUse(0x8079efec);  // Item update
kmRuntimeUse(0x807a0cd4);  // Item model update (quat path)
kmRuntimeUse(0x807a07b8);  // Item model update (vec path)

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
// Mirrors Item::Obj::UpdateModelPositionNoClip (0x807a05f4) position selection.
const u32 kItemUseRelativePositionBit = 0x8;

struct GravityState {
    bool isInitialized;
    s16 areaId;
    Vec3 gravityDown;
    Quat gravityRotation;
    float gravityStrength;
    u16 blendFrames;
    u16 exitGraceCounter;
    u32 lastResolvedRaceFrame;
};

GravityState sGravityStates[kMaxTrackedPlayers];

typedef void (*ItemVelocityInitFunc)(Item::Obj* obj, const Vec3* sourcePosition, const Vec3* sourceSpeed, const Vec3* direction, bool useRandomness);
typedef void (*ItemShellInitFunc)(Item::Obj* obj, Item::PlayerObj* playerObj, s32 param5, bool isThrow, bool unkFlag, float speed);
typedef bool (*ItemUpdateFunc)(Item::Obj* obj, int updateMode);
typedef void (*ItemModelFromQuatFunc)(Item::Obj* obj, Mtx34* transMtxCopy);
typedef void (*ItemModelFromVecsFunc)(Item::Obj* obj, float spinBlend, Mtx34* transMtxCopy);

ItemVelocityInitFunc GetItemVelocityInitFunc() {
    return reinterpret_cast<ItemVelocityInitFunc>(kmRuntimeAddr(0x807a6738));
}

ItemShellInitFunc GetItemShellInitFunc() {
    return reinterpret_cast<ItemShellInitFunc>(kmRuntimeAddr(0x807b5178));
}

ItemUpdateFunc GetItemUpdateFunc() {
    return reinterpret_cast<ItemUpdateFunc>(kmRuntimeAddr(0x8079efec));
}

ItemModelFromQuatFunc GetItemModelFromQuatFunc() {
    return reinterpret_cast<ItemModelFromQuatFunc>(kmRuntimeAddr(0x807a0cd4));
}

ItemModelFromVecsFunc GetItemModelFromVecsFunc() {
    return reinterpret_cast<ItemModelFromVecsFunc>(kmRuntimeAddr(0x807a07b8));
}

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

float AbsFloat(float value) {
    return value < 0.0f ? -value : value;
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

Vec3 AddVec(const Vec3& lhs, const Vec3& rhs) {
    return MakeVec(lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z);
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

bool TryGetStateGravityUp(u8 playerIdx, Vec3& gravityUp, s16* outAreaId) {
    if (playerIdx >= kMaxTrackedPlayers) return false;

    GravityState& state = sGravityStates[playerIdx];
    if (!ShouldOverrideKartUpFromGravity(state)) return false;

    gravityUp = ScaleVec(state.gravityDown, -1.0f);
    if (!NormalizeSafe(gravityUp)) return false;

    if (outAreaId != nullptr) *outAreaId = state.areaId;
    return true;
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

    Kart::Status* status = static_cast<Kart::Status*>(0);
    if (movement.pointers != 0) status = movement.pointers->kartStatus;

    Vec3 localUp = ScaleVec(state.gravityDown, -1.0f);
    if (!NormalizeSafe(localUp)) return;

    movement.up = localUp;
    movement.smoothedUp = localUp;

    bool overrideApplied = true;
    if (status != nullptr) {
        const bool grounded = (status->bitfield0 & 0x40000) != 0;
        if (!grounded) {
            status->floorNor = localUp;
        }
        status->unknown_0x34 = localUp;
    }

    TempDebug::OnMovementUpApplied(playerIdx, state.areaId, status, movement, localUp, overrideApplied);
}
kmCall(0x80578a34, UpdateUpsWithGravityField);
kmCall(0x80578f30, UpdateUpsWithGravityField);

float ComputeHopWorldYImpulse(const Kart::Movement& movement) {
    const float baseHopVelY = movement.hopVelY;

    if (movement.pointers == nullptr || movement.pointers->values == nullptr) return baseHopVelY;
    const u8 playerIdx = movement.pointers->values->playerIdx;

    Vec3 localUp;
    if (!TryGetPlayerGravityUp(playerIdx, localUp)) return baseHopVelY;

    // Hop code writes this directly into world-Y velocity.
    // Project to world-Y so sideways fields stop adding world-up impulse.
    return baseHopVelY * localUp.y;
}

extern "C" float GravityFieldsResolveHopWorldYImpulse(const Kart::Movement* movement) {
    if (movement == nullptr) return 0.0f;
    return ComputeHopWorldYImpulse(*movement);
}

static asm void GetHopWorldYImpulseWithGravityFieldAsm() {
    nofralloc

    // Hook point is an inlined load, not a function call site.
    // Preserve live registers expected by the surrounding vanilla code.
    stwu r1, -0x70(r1)
    stw r0, 0x08(r1)
    mfcr r0
    stw r0, 0x0c(r1)
    mflr r0
    stw r0, 0x10(r1)
    stw r3, 0x14(r1)
    stw r4, 0x18(r1)
    stw r5, 0x1c(r1)
    stw r6, 0x20(r1)
    stw r7, 0x24(r1)
    stw r8, 0x28(r1)
    stw r9, 0x2c(r1)
    stw r10, 0x30(r1)
    stw r11, 0x34(r1)
    stw r12, 0x38(r1)
    stfd f0, 0x40(r1)

    bl GravityFieldsResolveHopWorldYImpulse

    lwz r3, 0x14(r1)
    lwz r4, 0x18(r1)
    lwz r5, 0x1c(r1)
    lwz r6, 0x20(r1)
    lwz r7, 0x24(r1)
    lwz r8, 0x28(r1)
    lwz r9, 0x2c(r1)
    lwz r10, 0x30(r1)
    lwz r11, 0x34(r1)
    lwz r12, 0x38(r1)
    lfd f0, 0x40(r1)
    lwz r0, 0x0c(r1)
    mtcrf 0xFF, r0
    lwz r0, 0x10(r1)
    mtlr r0
    lwz r0, 0x08(r1)
    addi r1, r1, 0x70
    blr
}
kmCall(0x8057db7c, GetHopWorldYImpulseWithGravityFieldAsm);

struct ItemMotionSnapshot {
    Vec3 position;
    Vec3 speed;
};

struct ItemOrientationSnapshot {
    Quat quaternion;
    Vec3 basis[3];
};

const Vec3* ResolveItemPositionForGravity(const Item::Obj& itemObj);

void CaptureItemMotionSnapshot(const Item::Obj& itemObj, ItemMotionSnapshot& snapshot) {
    const Vec3* activePosition = ResolveItemPositionForGravity(itemObj);
    snapshot.position = *activePosition;
    snapshot.speed = itemObj.speed;
}

Vec3* ResolveItemPositionForGravity(Item::Obj& itemObj) {
    if ((itemObj.bitfield7c & kItemUseRelativePositionBit) != 0) {
        return &itemObj.positionRelativeToPlayer;
    }

    if (itemObj.curPosition != nullptr) {
        return itemObj.curPosition;
    }

    return &itemObj.position;
}

const Vec3* ResolveItemPositionForGravity(const Item::Obj& itemObj) {
    if ((itemObj.bitfield7c & kItemUseRelativePositionBit) != 0) {
        return &itemObj.positionRelativeToPlayer;
    }

    if (itemObj.curPosition != nullptr) {
        return itemObj.curPosition;
    }

    return &itemObj.position;
}

bool IsItemUsingRelativePosition(const Item::Obj& itemObj) {
    return (itemObj.bitfield7c & kItemUseRelativePositionBit) != 0;
}

Vec3* TryGetShellMotionDelta(Item::Obj& itemObj) {
    u8* rawObj = reinterpret_cast<u8*>(&itemObj);
    if (itemObj.itemObjId == OBJ_GREEN_SHELL) {
        return reinterpret_cast<Vec3*>(rawObj + 0x1e0);
    }
    if (itemObj.itemObjId == OBJ_RED_SHELL) {
        return reinterpret_cast<Vec3*>(rawObj + 0x2d4);
    }
    return nullptr;
}

void CaptureItemOrientationSnapshot(const Item::Obj& itemObj, ItemOrientationSnapshot& snapshot) {
    snapshot.quaternion = itemObj.quaternion;
    snapshot.basis[0] = itemObj.unknown_0x20[0];
    snapshot.basis[1] = itemObj.unknown_0x20[1];
    snapshot.basis[2] = itemObj.unknown_0x20[2];
}

void RestoreItemOrientationSnapshot(Item::Obj& itemObj, const ItemOrientationSnapshot& snapshot) {
    itemObj.quaternion = snapshot.quaternion;
    itemObj.unknown_0x20[0] = snapshot.basis[0];
    itemObj.unknown_0x20[1] = snapshot.basis[1];
    itemObj.unknown_0x20[2] = snapshot.basis[2];
}

bool ResolveItemGravity(const Item::Obj& itemObj, s16& areaId, Vec3& gravityDown, float& gravityStrength) {
    // Tethered items should keep the same gravity frame as their owning player.
    if (IsItemUsingRelativePosition(itemObj)) {
        const u8 ownerPlayerIdx = itemObj.playerUsedItemId;
        if (ownerPlayerIdx < kMaxTrackedPlayers) {
            const GravityState& ownerState = sGravityStates[ownerPlayerIdx];
            if (ownerState.isInitialized && ownerState.areaId >= 0) {
                areaId = ownerState.areaId;
                gravityDown = ownerState.gravityDown;
                gravityStrength = ownerState.gravityStrength;
                return true;
            }
        }
    }

    u16 blendFrames = kDefaultBlendFrames;
    const Vec3* position = ResolveItemPositionForGravity(itemObj);
    return ResolveGravityField(*position, -1, areaId, gravityDown, gravityStrength, blendFrames);
}

Vec3 RemapWorldDownComponent(const Vec3& sourceVector, const Vec3& targetDown, float downScale) {
    const Vec3 worldDown = WorldDown();
    const float worldDownAmount = DotVec(sourceVector, worldDown);
    const Vec3 worldDownComponent = ScaleVec(worldDown, worldDownAmount);
    const Vec3 targetDownComponent = ScaleVec(targetDown, worldDownAmount * downScale);
    return AddVec(SubVec(sourceVector, worldDownComponent), targetDownComponent);
}

Quat ConcatQuat(const Quat& lhs, const Quat& rhs) {
    Quat outQuat = IdentityQuat();
    outQuat.x = lhs.w * rhs.x + lhs.x * rhs.w + lhs.y * rhs.z - lhs.z * rhs.y;
    outQuat.y = lhs.w * rhs.y - lhs.x * rhs.z + lhs.y * rhs.w + lhs.z * rhs.x;
    outQuat.z = lhs.w * rhs.z + lhs.x * rhs.y - lhs.y * rhs.x + lhs.z * rhs.w;
    outQuat.w = lhs.w * rhs.w - lhs.x * rhs.x - lhs.y * rhs.y - lhs.z * rhs.z;
    outQuat.Normalise();
    return outQuat;
}

void RotateItemOrientation(Item::Obj& itemObj, const Quat& rotation) {
    itemObj.quaternion = ConcatQuat(rotation, itemObj.quaternion);
    itemObj.unknown_0x20[0] = Vec3::RotateQuaternion(rotation, itemObj.unknown_0x20[0]);
    itemObj.unknown_0x20[1] = Vec3::RotateQuaternion(rotation, itemObj.unknown_0x20[1]);
    itemObj.unknown_0x20[2] = Vec3::RotateQuaternion(rotation, itemObj.unknown_0x20[2]);
}

Quat MakeItemGravityRotation(const Item::Obj& itemObj, const Vec3& targetDown) {
    Vec3 normalizedDown = targetDown;
    if (!NormalizeSafe(normalizedDown)) return IdentityQuat();

    const Vec3 worldDown = WorldDown();
    const float dot = ClampFloat(DotVec(worldDown, normalizedDown), -1.0f, 1.0f);
    if (dot > 0.9999f) return IdentityQuat();

    if (dot < -0.9999f) {
        // 180-degree flips are underconstrained; use item basis so shells/bananas
        // keep a stable relation to the player when driving on ceilings.
        Vec3 helper = itemObj.unknown_0x20[0];
        if (!NormalizeSafe(helper) || AbsFloat(DotVec(helper, worldDown)) > 0.95f) {
            helper = itemObj.unknown_0x20[2];
            if (!NormalizeSafe(helper) || AbsFloat(DotVec(helper, worldDown)) > 0.95f) {
                helper = MakeVec(1.0f, 0.0f, 0.0f);
            }
        }

        // Rotate around the item's own lateral basis to preserve relative roll.
        // Using Cross(worldDown, helper) over-rotates shells on 180-degree flips.
        Vec3 axis = helper;
        if (!NormalizeSafe(axis)) axis = MakeVec(1.0f, 0.0f, 0.0f);

        Quat rotation = IdentityQuat();
        rotation.SetAxisRotation(axis, PI);
        return rotation;
    }

    return MakeRotationQuat(worldDown, normalizedDown);
}

bool TryResolveItemGravityRotation(const Item::Obj& itemObj, s16& outAreaId, Vec3& outGravityDown, float& outGravityStrength, Quat& outRotation) {
    // Bananas already orient well through vanilla collision/model code; forcing an
    // additional gravity quaternion causes inversion on 180-degree zones.
    if (itemObj.itemObjId == OBJ_BANANA) return false;

    if (!ResolveItemGravity(itemObj, outAreaId, outGravityDown, outGravityStrength)) return false;

    Vec3 normalizedDown = outGravityDown;
    if (!NormalizeSafe(normalizedDown)) return false;
    if (DotVec(normalizedDown, WorldDown()) > 0.9999f) return false;

    outRotation = MakeItemGravityRotation(itemObj, normalizedDown);
    return true;
}

void ApplyItemGravityField(Item::Obj& itemObj, const ItemMotionSnapshot* preUpdateSnapshot, bool forceOnRelativePosition = false) {
    if (!forceOnRelativePosition && IsItemUsingRelativePosition(itemObj)) return;

    s16 areaId = -1;
    Vec3 gravityDown = WorldDown();
    float gravityStrength = kDefaultGravityStrength;
    if (!ResolveItemGravity(itemObj, areaId, gravityDown, gravityStrength)) return;

    Vec3* activePosition = ResolveItemPositionForGravity(itemObj);
    if (activePosition == nullptr) return;

    Vec3 normalizedDown = gravityDown;
    if (!NormalizeSafe(normalizedDown)) return;

    if (preUpdateSnapshot != nullptr) {
        const Vec3 speedDelta = SubVec(itemObj.speed, preUpdateSnapshot->speed);

        const float speedDownScale = gravityStrength / kDefaultGravityStrength;
        const Vec3 remappedSpeedDelta = RemapWorldDownComponent(speedDelta, normalizedDown, speedDownScale);

        itemObj.speed = AddVec(preUpdateSnapshot->speed, remappedSpeedDelta);
        itemObj.lastPosition = *activePosition;
    } else {
        // Keep item memory access on known Obj fields only.
        // Vanilla applies world-down acceleration each frame, so inject the delta needed
        // to turn that into the local gravity direction for this field.
        const Vec3 desiredAcceleration = ScaleVec(normalizedDown, gravityStrength);
        const Vec3 defaultAcceleration = ScaleVec(WorldDown(), kDefaultGravityStrength);
        const Vec3 accelerationDelta = SubVec(desiredAcceleration, defaultAcceleration);
        if (VecLength(accelerationDelta) > kVectorEpsilon) {
            itemObj.speed.x += accelerationDelta.x;
            itemObj.speed.y += accelerationDelta.y;
            itemObj.speed.z += accelerationDelta.z;
        }
    }

    TempDebug::OnItemGravityApplied(itemObj, areaId, gravityDown, gravityStrength);
}

extern "C" void InitItemVelocityWithGravityField(Item::Obj* itemObj, const Vec3* sourcePosition, const Vec3* sourceSpeed, const Vec3* direction, bool useRandomness) {
    if (itemObj == nullptr || sourcePosition == nullptr || sourceSpeed == nullptr || direction == nullptr) return;
    ItemVelocityInitFunc itemVelocityInit = GetItemVelocityInitFunc();
    if (itemVelocityInit == nullptr) return;
    itemVelocityInit(itemObj, sourcePosition, sourceSpeed, direction, useRandomness);
    ItemMotionSnapshot sourceSnapshot;
    sourceSnapshot.position = *sourcePosition;
    sourceSnapshot.speed = *sourceSpeed;
    ApplyItemGravityField(*itemObj, &sourceSnapshot, true);
}
kmCall(0x8079fc1c, InitItemVelocityWithGravityField);
kmCall(0x807a1bc8, InitItemVelocityWithGravityField);
kmCall(0x807a1c5c, InitItemVelocityWithGravityField);
kmCall(0x807a381c, InitItemVelocityWithGravityField);

extern "C" void InitShellVelocityWithGravityField(Item::Obj* itemObj, Item::PlayerObj* playerObj, s32 param5, bool isThrow, bool unkFlag, float speed) {
    if (itemObj == nullptr) return;
    ItemShellInitFunc shellInit = GetItemShellInitFunc();
    if (shellInit == nullptr) return;

    shellInit(itemObj, playerObj, param5, isThrow, unkFlag, speed);

    if (playerObj == nullptr) {
        ApplyItemGravityField(*itemObj, nullptr, true);
        return;
    }

    ItemMotionSnapshot sourceSnapshot;
    sourceSnapshot.position = playerObj->playerPos;
    sourceSnapshot.speed = playerObj->playerSpeed;
    ApplyItemGravityField(*itemObj, &sourceSnapshot, true);
}
kmCall(0x807aa6e4, InitShellVelocityWithGravityField);
kmCall(0x807aa7f0, InitShellVelocityWithGravityField);
kmCall(0x807ac744, InitShellVelocityWithGravityField);
kmCall(0x807aebe4, InitShellVelocityWithGravityField);
kmCall(0x807aec30, InitShellVelocityWithGravityField);
kmCall(0x807b48ac, InitShellVelocityWithGravityField);

extern "C" void UpdateItemModelPositionWithGravityField(Item::Obj* itemObj) {
    if (itemObj == nullptr) return;

    ItemModelFromQuatFunc updateModelFromQuat = GetItemModelFromQuatFunc();
    ItemModelFromVecsFunc updateModelFromVecs = GetItemModelFromVecsFunc();
    if (updateModelFromQuat == nullptr || updateModelFromVecs == nullptr) return;

    ItemOrientationSnapshot orientationSnapshot;
    bool hasRotationOverride = false;

    s16 areaId = -1;
    Vec3 gravityDown = WorldDown();
    float gravityStrength = kDefaultGravityStrength;
    Quat gravityRotation = IdentityQuat();
    if (TryResolveItemGravityRotation(*itemObj, areaId, gravityDown, gravityStrength, gravityRotation)) {
        CaptureItemOrientationSnapshot(*itemObj, orientationSnapshot);
        RotateItemOrientation(*itemObj, gravityRotation);
        hasRotationOverride = true;

        // Tethered red/green shells derive extra model orientation from these vectors.
        // Remap them into local gravity each frame so shell orientation follows the kart.
        if (IsItemUsingRelativePosition(*itemObj)) {
            Vec3 normalizedDown = gravityDown;
            if (NormalizeSafe(normalizedDown)) {
                Vec3* shellMotionDelta = TryGetShellMotionDelta(*itemObj);
                if (shellMotionDelta != nullptr) {
                    *shellMotionDelta = RemapWorldDownComponent(*shellMotionDelta, normalizedDown, 1.0f);
                }
            }
        }
    }

    if ((itemObj->bitfield78 & 0x1000000) != 0) {
        updateModelFromQuat(itemObj, nullptr);
    } else {
        updateModelFromVecs(itemObj, 0.0f, nullptr);
    }

    if (hasRotationOverride) RestoreItemOrientationSnapshot(*itemObj, orientationSnapshot);
}
kmBranch(0x807a05d0, UpdateItemModelPositionWithGravityField);

extern "C" bool UpdateItemWithGravityField(Item::Obj* itemObj, int updateMode) {
    if (itemObj == nullptr) return true;
    ItemUpdateFunc itemUpdate = GetItemUpdateFunc();
    if (itemUpdate == nullptr) return true;

    const bool isExpired = itemUpdate(itemObj, updateMode);
    if (!isExpired) ApplyItemGravityField(*itemObj, nullptr);
    return isExpired;
}
kmCall(0x807965ec, UpdateItemWithGravityField);
kmCall(0x80796784, UpdateItemWithGravityField);
kmCall(0x807968b8, UpdateItemWithGravityField);

void SyncRespawnGravity(Kart::Link& link) {
    link.UpdateCameraOnRespawn();

    Vec3 gravityVector;
    float gravityStrength;
    ComputeKartGravity(link, true, gravityVector, gravityStrength);

    Kart::PhysicsHolder& physicsHolder = link.GetPhysicsHolder();
    if (physicsHolder.physics != nullptr) {
        physicsHolder.physics->gravity = GetBodyGravityScalar(gravityVector, gravityStrength, physicsHolder.physics->gravity);
        ApplyBodyGravityVector(*physicsHolder.physics, gravityVector);
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

bool TryGetPlayerGravityUp(u8 playerIdx, Vec3& gravityUp) {
    return TryGetStateGravityUp(playerIdx, gravityUp, nullptr);
}

void PrepareKartCollisionForGravity(Kart::Status& status) {
    (void)status;
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

void ApplyBodyGravityVector(Kart::Physics& physics, const Vec3& gravityVector) {
    physics.normalAcceleration.x = gravityVector.x;
    physics.normalAcceleration.z = gravityVector.z;
}

void ApplyBodyGravityVector(Kart::Physics& physics, const Vec3& gravityVector, const Kart::Status& status) {
    const bool grounded = (status.bitfield0 & 0x40000) != 0;
    if (grounded) {
        // Let wheel suspension own stick-to-surface while grounded.
        // Injecting lateral body acceleration here causes floor contact flicker.
        physics.normalAcceleration.x = 0.0f;
        physics.normalAcceleration.z = 0.0f;

        // Sideways/tilted gravity can still produce tiny upward detaches at rest.
        // Remove only "away from surface" velocity while grounded, preserving jump/hop/trick lift.
        const bool allowLift = (status.bitfield0 & 0x80000) != 0 || (status.bitfield0 & 0x40000000) != 0 || (status.bitfield1 & 0x40) != 0;
        if (!allowLift) {
            Vec3 gravityDown = gravityVector;
            if (NormalizeSafe(gravityDown)) {
                const float horizontalDownSq = gravityDown.x * gravityDown.x + gravityDown.z * gravityDown.z;
                if (horizontalDownSq > 0.0004f) {
                    const Vec3 gravityUp = ScaleVec(gravityDown, -1.0f);
                    const float liftSpeed = DotVec(physics.speed0, gravityUp);
                    if (liftSpeed > 0.0f) {
                        physics.speed0.x -= gravityUp.x * liftSpeed;
                        physics.speed0.y -= gravityUp.y * liftSpeed;
                        physics.speed0.z -= gravityUp.z * liftSpeed;
                    }
                }
            }
        }
        return;
    }

    physics.normalAcceleration.x = gravityVector.x;
    physics.normalAcceleration.z = gravityVector.z;
}

s16 GetActiveAreaId(u8 playerIdx) {
    if (playerIdx >= kMaxTrackedPlayers) return -1;
    return sGravityStates[playerIdx].areaId;
}

}  // namespace GravityFields
}  // namespace Race
}  // namespace Pulsar
