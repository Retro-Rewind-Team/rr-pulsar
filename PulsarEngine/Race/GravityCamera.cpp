#include <kamek.hpp>
#include <runtimeWrite.hpp>
#include <MarioKartWii/3D/Camera/RaceCamera.hpp>
#include <MarioKartWii/Kart/KartLink.hpp>
#include <MarioKartWii/Kart/KartMovement.hpp>
#include <MarioKartWii/Kart/KartPhysics.hpp>
#include <MarioKartWii/Kart/KartPointers.hpp>
#include <MarioKartWii/Kart/KartStatus.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <MarioKartWii/System/Identifiers.hpp>
#include <core/egg/Math/Math.hpp>
#include <core/egg/Math/Quat.hpp>
#include <Race/GravityCamera.hpp>
#include <Race/GravityFields.hpp>

namespace Pulsar {
namespace Race {
namespace GravityCamera {
namespace {

const float kCameraFieldInterpMission = 0.07f;
const float kCameraFieldInterpNormal = 0.05f;
const float kCameraFallStep = 10.0f;
const float kCameraFallMin = -50.0f;
const float kCameraZipperRise = 100.0f;
const float kCameraZipperFall = -100.0f;
const float kCameraRollDecay = 1.0f;
const float kCameraRollDecayAuto = 0.5f;
const float kCameraRollStep = -0.5f;
const float kCameraRollLimit = 15.0f;
const float kCameraRollLimitAuto = 10.0f;
const float kCameraPitchRecover = 0.03f;
const float kCameraGravityUpMinDot = 0.15f;
const float kCameraGravityUpMinBlend = 0.15f;
const float kRadToDeg = 57.29578f;
const float kVecEpsilon = 0.0001f;

const u32 kStatusBitfield0DriftManual = 0x8;
const u32 kStatusBitfield0Hop = 0x80000;
const u32 kStatusBitfield1OverZipper = 0x400;
const u32 kStatusBitfield4AutomaticDrift = 0x10;

const u16 kCameraFlagFalling = 0x1;
const u16 kCameraFlagFallingPastTarget = 0x2;
const u16 kCameraFlagScript = 0x4;
const u16 kCameraFlagFreezeMovement = 0x10;

const u32 kBasePositionOffset = 0x64;
const u32 kBaseTargetOffset = 0x70;
const u32 kBaseUpOffset = 0x7c;
const u32 kCameraHopOffset = 0xfc;
const u32 kCameraField104Offset = 0x104;
const u32 kCameraField108Offset = 0x108;
const u32 kCameraField10cOffset = 0x10c;
const u32 kCameraField110Offset = 0x110;
const u32 kCameraField118Offset = 0x118;
const u32 kCameraField124Offset = 0x124;
const u32 kCameraField128Offset = 0x128;
const u32 kCameraUpAxisOffset = 0x138;
const u32 kCameraBaseUpOffset = 0x150;
const u32 kCameraLastUpOffset = 0x15c;
const u32 kCameraSmoothedUpOffset = 0x168;
const u32 kCameraSmoothedUpBlendOffset = 0x174;
const u32 kCameraSmoothedQuatOffset = 0x178;
const u32 kCameraField344Offset = 0x344;

const u32 kGameCamUnknown20Offset = 0x20;

kmRuntimeUse(0x805a2cfc);
kmRuntimeUse(0x805a3070);
kmRuntimeUse(0x805a34b0);
kmRuntimeUse(0x805a40d0);
kmRuntimeUse(0x805a9a40);
kmRuntimeUse(0x805909c8);

typedef void (*CameraTargetFunc)(float f1, float f2, float f3, RaceCamera* camera, GameCamValues* values, Kart::Link* kartPlayer, Vec3* playerPos);
typedef void (*CameraRollFunc)(RaceCamera* camera, Kart::Link* kartPlayer);
typedef void (*CameraValuesFunc)(float f1, float f2, float f3, RaceCamera* camera, GameCamValues* values, u32 reverse, Kart::Link* kartPlayer, Vec3* playerPos);
typedef void (*CameraFinalizeFunc)(float f1, RaceCamera* camera, Kart::Link* kartPlayer, u32 mode);
typedef void (*CameraSub338Func)(void* sub);
typedef float (*LinkFloatFunc)(const Kart::Link* kartPlayer);

static const CameraTargetFunc sOriginalUpdateCameraTarget = reinterpret_cast<CameraTargetFunc>(kmRuntimeAddr(0x805a2cfc));
static const CameraRollFunc sOriginalUpdateCameraRoll = reinterpret_cast<CameraRollFunc>(kmRuntimeAddr(0x805a3070));
static const CameraValuesFunc sOriginalUpdateCameraValues = reinterpret_cast<CameraValuesFunc>(kmRuntimeAddr(0x805a34b0));
static const CameraFinalizeFunc sOriginalFinalizeCamera = reinterpret_cast<CameraFinalizeFunc>(kmRuntimeAddr(0x805a40d0));
static const CameraSub338Func sOriginalUpdateSub338 = reinterpret_cast<CameraSub338Func>(kmRuntimeAddr(0x805a9a40));
static const LinkFloatFunc sGetCameraVerticalDistance = reinterpret_cast<LinkFloatFunc>(kmRuntimeAddr(0x805909c8));

template <typename T>
T& Field(void* base, u32 offset) {
    return *reinterpret_cast<T*>(reinterpret_cast<u8*>(base) + offset);
}

template <typename T>
const T& Field(const void* base, u32 offset) {
    return *reinterpret_cast<const T*>(reinterpret_cast<const u8*>(base) + offset);
}

struct CameraGravityContext {
    Vec3 up;
    Vec3 playerPos;
    Vec3 anchorPos;
};

Vec3 MakeVec(float x, float y, float z) {
    Vec3 vec;
    vec.x = x;
    vec.y = y;
    vec.z = z;
    return vec;
}

Vec3 WorldUp() {
    return MakeVec(0.0f, 1.0f, 0.0f);
}

float AbsFloat(float value) {
    return value < 0.0f ? -value : value;
}

float ClampFloat(float value, float min, float max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

float DotVec(const Vec3& lhs, const Vec3& rhs) {
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

Vec3 AddVec(const Vec3& lhs, const Vec3& rhs) {
    return MakeVec(lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z);
}

Vec3 SubVec(const Vec3& lhs, const Vec3& rhs) {
    return MakeVec(lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z);
}

Vec3 ScaleVec(const Vec3& vec, float scale) {
    return MakeVec(vec.x * scale, vec.y * scale, vec.z * scale);
}

Vec3 CrossVec(const Vec3& lhs, const Vec3& rhs) {
    return MakeVec(
        lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.z * rhs.x - lhs.x * rhs.z,
        lhs.x * rhs.y - lhs.y * rhs.x);
}

float VecLength(const Vec3& vec) {
    return EGG::Math::Sqrt(DotVec(vec, vec));
}

bool NormalizeSafe(Vec3& vec) {
    const float length = VecLength(vec);
    if (length <= kVecEpsilon) return false;
    const float invLength = 1.0f / length;
    vec.x *= invLength;
    vec.y *= invLength;
    vec.z *= invLength;
    return true;
}

Vec3 NormalizedOrFallback(const Vec3& source, const Vec3& fallback) {
    Vec3 result = source;
    if (NormalizeSafe(result)) return result;
    result = fallback;
    NormalizeSafe(result);
    return result;
}

Vec3 BuildPerpendicularInPlane(const Vec3& planeNormal, const Vec3& vector, const Vec3& fallback) {
    Vec3 planeComponent = vector;
    if (!NormalizeSafe(planeComponent)) return NormalizedOrFallback(fallback, WorldUp());

    Vec3 result = SubVec(planeNormal, ScaleVec(planeComponent, DotVec(planeNormal, planeComponent)));
    if (NormalizeSafe(result)) return result;
    return NormalizedOrFallback(fallback, WorldUp());
}

Quat IdentityQuat() {
    Quat quat;
    quat.Set(1.0f, 0.0f, 0.0f, 0.0f);
    return quat;
}

Vec3& BasePosition(RaceCamera& camera) {
    return Field<Vec3>(&camera, kBasePositionOffset);
}

Vec3& BaseTarget(RaceCamera& camera) {
    return Field<Vec3>(&camera, kBaseTargetOffset);
}

Vec3& BaseUp(RaceCamera& camera) {
    return Field<Vec3>(&camera, kBaseUpOffset);
}

float& CameraFloat(RaceCamera& camera, u32 offset) {
    return Field<float>(&camera, offset);
}

Vec3& CameraVec(RaceCamera& camera, u32 offset) {
    return Field<Vec3>(&camera, offset);
}

Quat& CameraQuat(RaceCamera& camera, u32 offset) {
    return Field<Quat>(&camera, offset);
}

float& GameCamFloat(GameCamValues& values, u32 offset) {
    return Field<float>(&values, offset);
}

bool TryGetCameraUp(const Kart::Link* kartPlayer, Vec3& cameraUp) {
    if (kartPlayer == 0) return false;

    const u8 playerIdx = kartPlayer->GetPlayerIdx();
    if (GravityFields::GetActiveAreaId(playerIdx) < 0) return false;

    if (GravityFields::TryGetPlayerGravityUp(playerIdx, cameraUp)) return true;

    if (kartPlayer->pointers != 0 && kartPlayer->pointers->kartMovement != 0) {
        cameraUp = kartPlayer->pointers->kartMovement->smoothedUp;
        if (NormalizeSafe(cameraUp)) return true;
    }

    cameraUp = WorldUp();
    return false;
}

bool BuildGravityContext(RaceCamera* camera, Kart::Link* kartPlayer, CameraGravityContext& context) {
    if (camera == 0 || kartPlayer == 0) return false;
    if (!TryGetCameraUp(kartPlayer, context.up)) return false;

    context.playerPos = kartPlayer->GetPosition();
    const float hopOffset = CameraFloat(*camera, kCameraHopOffset);
    context.anchorPos = SubVec(context.playerPos, ScaleVec(context.up, hopOffset));
    return true;
}

void UpdateCameraTargetImpl(float positionInterp, float topDotInterp, float heightInterp, RaceCamera* camera, GameCamValues* values,
                            Kart::Link* kartPlayer, Vec3* playerPos) {
    if (camera == 0 || values == 0 || kartPlayer == 0 || playerPos == 0) return;

    CameraGravityContext context;
    if (!BuildGravityContext(camera, kartPlayer, context)) {
        sOriginalUpdateCameraTarget(positionInterp, topDotInterp, heightInterp, camera, values, kartPlayer, playerPos);
        return;
    }

    CameraVec(*camera, kCameraUpAxisOffset) = BuildPerpendicularInPlane(context.up, camera->angleOfRotAroundPlayer, context.up);
    *playerPos = context.anchorPos;

    if ((camera->bitfield & kCameraFlagFreezeMovement) != 0) return;

    float targetYPos = camera->camParams != 0 ? camera->camParams->yTargetPos : 0.0f;
    const Raceinfo* raceinfo = Raceinfo::sInstance;
    if (raceinfo == 0 || raceinfo->stage < RACESTAGE_COUNTDOWN) {
        float smooth = kCameraFieldInterpNormal;
        if (Racedata::sInstance != 0 &&
            Racedata::sInstance->racesScenario.settings.gamemode == MODE_MISSION_TOURNAMENT) {
            smooth = kCameraFieldInterpMission;
        }

        float& smoothedTargetYPos = CameraFloat(*camera, kCameraField344Offset);
        smoothedTargetYPos += (targetYPos - smoothedTargetYPos) * smooth;
        targetYPos = smoothedTargetYPos;
    }

    const float verticalDistance = sGetCameraVerticalDistance(kartPlayer);
    const float verticalTarget = targetYPos - verticalDistance;

    float topDot = 0.0f;
    if (kartPlayer->pointers != 0 && kartPlayer->pointers->kartMovement != 0) {
        topDot = topDotInterp * AbsFloat(DotVec(kartPlayer->pointers->kartMovement->unknown_0x80, context.up));
    }

    float zipperOffset = 0.0f;
    float targetLift = 0.0f;
    if (kartPlayer->pointers != 0 && kartPlayer->pointers->kartStatus != 0 &&
        (kartPlayer->pointers->kartStatus->bitfield1 & kStatusBitfield1OverZipper) != 0) {
        zipperOffset = kCameraZipperFall;
        const Kart::PhysicsHolder& physicsHolder = const_cast<Kart::Link*>(kartPlayer)->GetPhysicsHolder();
        const Vec3* speed = physicsHolder.physics != 0 ? &physicsHolder.physics->speed : 0;
        if (speed != 0 && DotVec(*speed, context.up) < 0.0f) {
            targetLift = kCameraZipperRise * CameraFloat(*camera, kCameraField118Offset);
        }
    }

    float& field10c = CameraFloat(*camera, kCameraField10cOffset);
    float& field110 = CameraFloat(*camera, kCameraField110Offset);
    field10c += positionInterp * (zipperOffset - field10c);
    field110 += positionInterp * (targetLift - field110);

    float& field108 = CameraFloat(*camera, kCameraField108Offset);
    if ((camera->bitfield & kCameraFlagFalling) == 0) {
        float& field104 = CameraFloat(*camera, kCameraField104Offset);
        field104 += positionInterp * (topDot - field104);
        field108 += heightInterp * ((field10c + verticalTarget + field104) - field108);
    } else {
        field108 -= kCameraFallStep;
        if (field108 < kCameraFallMin) field108 = kCameraFallMin;

        if ((camera->bitfield & kCameraFlagFallingPastTarget) == 0 && GameCamFloat(*values, kGameCamUnknown20Offset) < context.playerPos.y) {
            camera->bitfield |= kCameraFlagFallingPastTarget;
        }
    }

    camera->unknown_0xAC = AddVec(context.anchorPos, ScaleVec(CameraVec(*camera, kCameraUpAxisOffset), values->verticalOffset2));
    camera->unknown_0xAC.y += field108;
}

void UpdateCameraTargetWithGravity(float positionInterp, float topDotInterp, float heightInterp, RaceCamera* camera,
                                   GameCamValues* values, Kart::Link* kartPlayer, Vec3* playerPos) {
    UpdateCameraTargetImpl(positionInterp, topDotInterp, heightInterp, camera, values, kartPlayer, playerPos);
}
kmCall(0x805a2444, UpdateCameraTargetWithGravity);

void UpdateCameraRollWithGravity(RaceCamera* camera, Kart::Link* kartPlayer) {
    if (camera == 0 || kartPlayer == 0) return;

    Vec3 cameraUp;
    if (!TryGetCameraUp(kartPlayer, cameraUp)) {
        sOriginalUpdateCameraRoll(camera, kartPlayer);
        return;
    }

    const Kart::Status* status = kartPlayer->pointers != 0 ? kartPlayer->pointers->kartStatus : 0;
    if (status == 0) return;

    float& roll = camera->unknown_0xf4[0];
    if ((status->bitfield0 & (kStatusBitfield0DriftManual | kStatusBitfield0Hop)) == 0) {
        const float step = (status->bitfield4 & kStatusBitfield4AutomaticDrift) != 0 ? kCameraRollDecayAuto : kCameraRollDecay;
        if (roll < 0.0f) {
            roll += step;
            if (roll > 0.0f) roll = 0.0f;
        } else if (roll > 0.0f) {
            roll -= step;
            if (roll < 0.0f) roll = 0.0f;
        }
        return;
    }

    const int hopStickX = kartPlayer->pointers != 0 && kartPlayer->pointers->kartMovement != 0
                              ? kartPlayer->pointers->kartMovement->hopStickX
                              : 0;
    float rollDelta = kCameraRollStep * static_cast<float>(hopStickX);
    if (kartPlayer->GetType() == INSIDE_BIKE) rollDelta = -rollDelta;
    roll += rollDelta;

    float maxRoll = (status->bitfield4 & kStatusBitfield4AutomaticDrift) != 0 ? kCameraRollLimitAuto : kCameraRollLimit;
    const float dot = ClampFloat(DotVec(camera->angleOfRotAroundPlayer, cameraUp), -1.0f, 1.0f);
    const float tiltDegrees = 90.0f - (EGG::Math::Acos(dot) * kRadToDeg);
    if (tiltDegrees < 0.0f) maxRoll += kCameraPitchRecover * AbsFloat(tiltDegrees);

    if (roll > maxRoll) roll = maxRoll;
    if (roll < -maxRoll) roll = -maxRoll;
}
kmCall(0x805a2450, UpdateCameraRollWithGravity);

void FinalizeCameraWithGravity(float upInterp, RaceCamera* camera, Kart::Link* kartPlayer, u32 mode) {
    if (camera == 0 || kartPlayer == 0) return;

    CameraGravityContext context;
    if (!BuildGravityContext(camera, kartPlayer, context) || (camera->bitfield & kCameraFlagScript) != 0) {
        sOriginalFinalizeCamera(upInterp, camera, kartPlayer, mode);
        return;
    }

    Vec3 lookDir = SubVec(BaseTarget(*camera), BasePosition(*camera));
    if (!NormalizeSafe(lookDir)) {
        BaseUp(*camera) = context.up;
        return;
    }

    Vec3 desiredUp = context.up;
    if (kartPlayer->pointers != 0 && kartPlayer->pointers->kartMovement != 0) {
        desiredUp = kartPlayer->pointers->kartMovement->smoothedUp;
        if (!NormalizeSafe(desiredUp)) desiredUp = context.up;
    }

    Vec3& smoothedUp = CameraVec(*camera, kCameraSmoothedUpOffset);
    if (!NormalizeSafe(smoothedUp)) smoothedUp = desiredUp;

    float blend = ClampFloat(upInterp, 0.0f, 1.0f);
    if (blend < kCameraGravityUpMinBlend) blend = kCameraGravityUpMinBlend;

    smoothedUp.x += (desiredUp.x - smoothedUp.x) * blend;
    smoothedUp.y += (desiredUp.y - smoothedUp.y) * blend;
    smoothedUp.z += (desiredUp.z - smoothedUp.z) * blend;
    smoothedUp = NormalizedOrFallback(smoothedUp, desiredUp);
    CameraVec(*camera, kCameraBaseUpOffset) = smoothedUp;

    Vec3 right = CrossVec(smoothedUp, lookDir);
    if (!NormalizeSafe(right)) {
        right = CrossVec(context.up, lookDir);
        if (!NormalizeSafe(right)) right = BuildPerpendicularInPlane(lookDir, context.up, MakeVec(1.0f, 0.0f, 0.0f));
    }

    BaseUp(*camera) = CrossVec(lookDir, right);
    BaseUp(*camera) = NormalizedOrFallback(BaseUp(*camera), smoothedUp);

    if (DotVec(BaseUp(*camera), context.up) < kCameraGravityUpMinDot) {
        BaseUp(*camera) = NormalizedOrFallback(context.up, WorldUp());
    }

    CameraVec(*camera, kCameraLastUpOffset) = BaseUp(*camera);
    CameraVec(*camera, kCameraSmoothedUpOffset) = BaseUp(*camera);
    CameraFloat(*camera, kCameraSmoothedUpBlendOffset) = blend;
    CameraQuat(*camera, kCameraSmoothedQuatOffset) = IdentityQuat();

    if (mode == 0) {
        BaseTarget(*camera) = AddVec(BaseTarget(*camera), ScaleVec(BaseUp(*camera), CameraFloat(*camera, kCameraField110Offset)));
    }

    if (camera->sub338 != 0) {
        sOriginalUpdateSub338(camera->sub338);
        sOriginalUpdateSub338(reinterpret_cast<u8*>(camera->sub338) + 0x1c);

        const float lateralOffset = Field<float>(camera->sub338, 0x10);
        const float verticalOffset = Field<float>(camera->sub338, 0x2c);
        BaseTarget(*camera) = AddVec(BaseTarget(*camera), ScaleVec(right, lateralOffset));
        BaseTarget(*camera) = AddVec(BaseTarget(*camera), ScaleVec(BaseUp(*camera), verticalOffset));
    }
}
kmCall(0x805a2ab8, FinalizeCameraWithGravity);

}  // namespace

void RefreshCameraOnRespawn(const Kart::Link& link) {
    if (link.pointers == 0 || link.pointers->camera == 0) return;

    RaceCamera* camera = link.pointers->camera;
    Kart::Link* kartPlayer = const_cast<Kart::Link*>(&link);

    CameraGravityContext context;
    if (!BuildGravityContext(camera, kartPlayer, context)) return;

    CameraVec(*camera, kCameraUpAxisOffset) = BuildPerpendicularInPlane(context.up, camera->angleOfRotAroundPlayer, context.up);

    Vec3 playerPos = context.anchorPos;
    UpdateCameraTargetImpl(1.0f, 1.0f, 1.0f, camera, &camera->forwards, kartPlayer, &playerPos);
    sOriginalUpdateCameraValues(1.0f, 1.0f, 1.0f, camera, &camera->forwards, 0, kartPlayer, &playerPos);

    CameraVec(*camera, kCameraBaseUpOffset) = context.up;
    CameraVec(*camera, kCameraLastUpOffset) = context.up;
    CameraVec(*camera, kCameraSmoothedUpOffset) = context.up;
    CameraFloat(*camera, kCameraSmoothedUpBlendOffset) = 1.0f;
    CameraQuat(*camera, kCameraSmoothedQuatOffset) = IdentityQuat();

    BasePosition(*camera) = camera->forwards.position;
    BaseTarget(*camera) = camera->unknown_0xAC;
    BaseUp(*camera) = context.up;
    CameraFloat(*camera, kCameraField124Offset) = 0.0f;
    CameraFloat(*camera, kCameraField128Offset) = 0.0f;

    FinalizeCameraWithGravity(1.0f, camera, kartPlayer, 0);
    camera->playerPos = playerPos;
}

}  // namespace GravityCamera
}  // namespace Race
}  // namespace Pulsar
