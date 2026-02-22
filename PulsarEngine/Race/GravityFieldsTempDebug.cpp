// TEMP DEBUG MODULE:
// Remove this file and its include/calls in GravityFields.cpp when gravity debugging is no longer needed.

#include <kamek.hpp>
#include <core/rvl/OS/OS.hpp>
#include <core/egg/Math/Math.hpp>
#include <MarioKartWii/KMP/KMPManager.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <Race/GravityFieldsTempDebug.hpp>

namespace Pulsar {
namespace Race {
namespace GravityFields {
namespace TempDebug {
namespace {

const bool kEnableTempGravityDebugLogs = true;
const bool kEnableTempGravityDebugVisuals = true;

const u8 kGravityAreaType = 11;
const u8 kGravityVolumeShapeBox = 0;
const u8 kGravityVolumeShapeCylinder = 1;
const u8 kGravityVolumeShapeSphere = 2;
const u8 kMaxTrackedPlayers = 12;

const float kVectorEpsilon = 0.0001f;
const u16 kItemLogBudgetPerRace = 40;
const u16 kMaxVisualizedAreasPerTick = 32;
const u32 kVisualSpawnIntervalFrames = 8;

const char* kVolumeMarkerEffectName = "rk_driftSpark2L_Spark00";
const char* kFlowMarkerEffectName = "rk_driftSpark2R_Spark00";

extern "C" void GravityDebugCreateOneTimeEffectPosScale(const char* name, const Vec3& position, const Vec3& scale);
extern "C" void* GravityDebugEffectMgrInstance;

struct DebugState {
    u32 visualFrameCounter;
    u16 itemLogsRemaining;
    bool itemBudgetExhaustedPrinted;
    s16 lastKnownAreaByPlayer[kMaxTrackedPlayers];
};

DebugState sDebugState;

Vec3 MakeVec(float x, float y, float z) {
    Vec3 vec;
    vec.x = x;
    vec.y = y;
    vec.z = z;
    return vec;
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

float DotVec(const Vec3& lhs, const Vec3& rhs) {
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
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
    if (length <= kVectorEpsilon) return false;

    const float invLength = 1.0f / length;
    vec.x *= invLength;
    vec.y *= invLength;
    vec.z *= invLength;
    return true;
}

float MinFloat(float lhs, float rhs) {
    return lhs < rhs ? lhs : rhs;
}

float MaxFloat(float lhs, float rhs) {
    return lhs > rhs ? lhs : rhs;
}

float AbsFloat(float value) {
    return value < 0.0f ? -value : value;
}

int RoundToInt(float value) {
    if (value >= 0.0f) return static_cast<int>(value + 0.5f);
    return static_cast<int>(value - 0.5f);
}

int ToMilli(float value) {
    return RoundToInt(value * 1000.0f);
}

u8 GetGravityVolumeShape(const AREA& area) {
    if (area.enemyRouteId <= kGravityVolumeShapeSphere) return area.enemyRouteId;
    if (area.shape == 1) return kGravityVolumeShapeCylinder;
    return kGravityVolumeShapeBox;
}

const char* ShapeName(u8 shape) {
    switch (shape) {
        case kGravityVolumeShapeBox:
            return "box";
        case kGravityVolumeShapeCylinder:
            return "cylinder";
        case kGravityVolumeShapeSphere:
            return "sphere";
        default:
            return "unknown";
    }
}

bool IsGravityAreaHolder(const KMP::Holder<AREA>* holder) {
    return holder != nullptr && holder->raw != nullptr && holder->raw->type == kGravityAreaType;
}

void ResetRaceDebugState() {
    sDebugState.visualFrameCounter = 0;
    sDebugState.itemLogsRemaining = kItemLogBudgetPerRace;
    sDebugState.itemBudgetExhaustedPrinted = false;
    for (u8 i = 0; i < kMaxTrackedPlayers; ++i) {
        sDebugState.lastKnownAreaByPlayer[i] = -1;
    }
}

void BuildAreaBasis(const KMP::Holder<AREA>& holder, Vec3& up, Vec3& right, Vec3& forward) {
    up = holder.yVector;
    if (!NormalizeSafe(up)) up = MakeVec(0.0f, 1.0f, 0.0f);

    right = holder.xVector;
    forward = holder.zVector;
    bool hasRight = NormalizeSafe(right);
    bool hasForward = NormalizeSafe(forward);
    if (!hasRight || !hasForward) {
        const Vec3 helper = (AbsFloat(up.y) < 0.95f) ? MakeVec(0.0f, 1.0f, 0.0f) : MakeVec(1.0f, 0.0f, 0.0f);
        right = CrossVec(helper, up);
        if (!NormalizeSafe(right)) right = MakeVec(1.0f, 0.0f, 0.0f);
        forward = CrossVec(up, right);
        if (!NormalizeSafe(forward)) forward = MakeVec(0.0f, 0.0f, 1.0f);
        return;
    }

    forward = CrossVec(up, right);
    if (!NormalizeSafe(forward)) {
        const Vec3 helper = (AbsFloat(up.y) < 0.95f) ? MakeVec(0.0f, 1.0f, 0.0f) : MakeVec(1.0f, 0.0f, 0.0f);
        right = CrossVec(helper, up);
        if (!NormalizeSafe(right)) right = MakeVec(1.0f, 0.0f, 0.0f);
        forward = CrossVec(up, right);
        if (!NormalizeSafe(forward)) forward = MakeVec(0.0f, 0.0f, 1.0f);
    } else {
        right = CrossVec(forward, up);
        NormalizeSafe(right);
    }
}

void SpawnDebugMarker(const Vec3& position, float scale, const char* effectName) {
    if (!kEnableTempGravityDebugVisuals) return;

    if (GravityDebugEffectMgrInstance == nullptr) return;

    const Vec3 markerScale = MakeVec(scale, scale, scale);
    GravityDebugCreateOneTimeEffectPosScale(effectName, position, markerScale);
}

void RenderBoxOutline(const Vec3& topCenter, const Vec3& bottomCenter, const Vec3& right, const Vec3& forward, float halfWidth, float halfLength) {
    const Vec3 topRight = ScaleVec(right, halfWidth);
    const Vec3 topForward = ScaleVec(forward, halfLength);

    for (int xSign = -1; xSign <= 1; xSign += 2) {
        for (int zSign = -1; zSign <= 1; zSign += 2) {
            const Vec3 lateral = AddVec(ScaleVec(topRight, static_cast<float>(xSign)), ScaleVec(topForward, static_cast<float>(zSign)));
            SpawnDebugMarker(AddVec(topCenter, lateral), 0.65f, kVolumeMarkerEffectName);
            SpawnDebugMarker(AddVec(bottomCenter, lateral), 0.65f, kVolumeMarkerEffectName);
        }
    }
}

void RenderCylinderOutline(const Vec3& topCenter, const Vec3& bottomCenter, const Vec3& right, const Vec3& forward, float radius) {
    const Vec3 rightRadial = ScaleVec(right, radius);
    const Vec3 forwardRadial = ScaleVec(forward, radius);
    SpawnDebugMarker(AddVec(topCenter, rightRadial), 0.65f, kVolumeMarkerEffectName);
    SpawnDebugMarker(SubVec(topCenter, rightRadial), 0.65f, kVolumeMarkerEffectName);
    SpawnDebugMarker(AddVec(topCenter, forwardRadial), 0.65f, kVolumeMarkerEffectName);
    SpawnDebugMarker(SubVec(topCenter, forwardRadial), 0.65f, kVolumeMarkerEffectName);
    SpawnDebugMarker(AddVec(bottomCenter, rightRadial), 0.65f, kVolumeMarkerEffectName);
    SpawnDebugMarker(SubVec(bottomCenter, rightRadial), 0.65f, kVolumeMarkerEffectName);
    SpawnDebugMarker(AddVec(bottomCenter, forwardRadial), 0.65f, kVolumeMarkerEffectName);
    SpawnDebugMarker(SubVec(bottomCenter, forwardRadial), 0.65f, kVolumeMarkerEffectName);
}

void RenderSphereOutline(const Vec3& center, const Vec3& up, const Vec3& right, const Vec3& forward, float radius) {
    SpawnDebugMarker(AddVec(center, ScaleVec(up, radius)), 0.7f, kVolumeMarkerEffectName);
    SpawnDebugMarker(SubVec(center, ScaleVec(up, radius)), 0.7f, kVolumeMarkerEffectName);
    SpawnDebugMarker(AddVec(center, ScaleVec(right, radius)), 0.7f, kVolumeMarkerEffectName);
    SpawnDebugMarker(SubVec(center, ScaleVec(right, radius)), 0.7f, kVolumeMarkerEffectName);
    SpawnDebugMarker(AddVec(center, ScaleVec(forward, radius)), 0.7f, kVolumeMarkerEffectName);
    SpawnDebugMarker(SubVec(center, ScaleVec(forward, radius)), 0.7f, kVolumeMarkerEffectName);
}

void RenderAreaDebugVisual(const KMP::Holder<AREA>& holder, u16 renderedAreaIdx) {
    Vec3 up;
    Vec3 right;
    Vec3 forward;
    BuildAreaBasis(holder, up, right, forward);

    const Vec3 down = ScaleVec(up, -1.0f);
    const Vec3 topCenter = holder.raw->position;
    const float height = MaxFloat(holder.height, 1.0f);
    const float halfHeight = height * 0.5f;
    const Vec3 bottomCenter = AddVec(topCenter, ScaleVec(down, height));

    SpawnDebugMarker(topCenter, 1.0f, kVolumeMarkerEffectName);
    SpawnDebugMarker(bottomCenter, 1.15f, kVolumeMarkerEffectName);

    const u32 baseAnimStep = sDebugState.visualFrameCounter / kVisualSpawnIntervalFrames;
    const float flowPhase = static_cast<float>((baseAnimStep + renderedAreaIdx) % 10) / 9.0f;
    const Vec3 flowPoint = AddVec(topCenter, ScaleVec(down, height * flowPhase));
    SpawnDebugMarker(flowPoint, 0.95f, kFlowMarkerEffectName);

    const u8 shape = GetGravityVolumeShape(*holder.raw);
    if (shape == kGravityVolumeShapeBox) {
        RenderBoxOutline(topCenter, bottomCenter, right, forward, holder.halfWidth, holder.halfLength);
    } else if (shape == kGravityVolumeShapeCylinder) {
        const float radius = MaxFloat(MinFloat(holder.halfWidth, holder.halfLength), 1.0f);
        RenderCylinderOutline(topCenter, bottomCenter, right, forward, radius);
    } else if (shape == kGravityVolumeShapeSphere) {
        const float radius = MaxFloat(MinFloat(MinFloat(holder.halfWidth, holder.halfLength), halfHeight), 1.0f);
        const Vec3 center = AddVec(topCenter, ScaleVec(down, halfHeight));
        RenderSphereOutline(center, up, right, forward, radius);
    }
}

void DrawGravityAreaVisuals() {
    KMP::Manager* kmpManager = KMP::Manager::sInstance;
    if (kmpManager == nullptr || kmpManager->areaSection == nullptr) return;

    KMP::Section<AREA>& areaSection = *kmpManager->areaSection;
    u16 renderedAreaCount = 0;
    for (u16 areaId = 0; areaId < areaSection.pointCount; ++areaId) {
        if (renderedAreaCount >= kMaxVisualizedAreasPerTick) break;
        KMP::Holder<AREA>* holder = areaSection.holdersArray[areaId];
        if (!IsGravityAreaHolder(holder)) continue;
        RenderAreaDebugVisual(*holder, renderedAreaCount);
        ++renderedAreaCount;
    }
}

void DumpRaceStartAreaSummary() {
    KMP::Manager* kmpManager = KMP::Manager::sInstance;
    if (kmpManager == nullptr || kmpManager->areaSection == nullptr) {
        OS::Report("[GravityDebug] Race start: KMP area section unavailable.\n");
        return;
    }

    KMP::Section<AREA>& areaSection = *kmpManager->areaSection;
    u16 gravityAreaCount = 0;
    for (u16 areaId = 0; areaId < areaSection.pointCount; ++areaId) {
        KMP::Holder<AREA>* holder = areaSection.holdersArray[areaId];
        if (IsGravityAreaHolder(holder)) ++gravityAreaCount;
    }

    OS::Report("[GravityDebug] Race start: found %u gravity area(s) (AREA type %u).\n", gravityAreaCount, kGravityAreaType);
    for (u16 areaId = 0; areaId < areaSection.pointCount; ++areaId) {
        KMP::Holder<AREA>* holder = areaSection.holdersArray[areaId];
        if (!IsGravityAreaHolder(holder)) continue;

        Vec3 up;
        Vec3 right;
        Vec3 forward;
        BuildAreaBasis(*holder, up, right, forward);
        const Vec3 down = ScaleVec(up, -1.0f);
        const u8 shape = GetGravityVolumeShape(*holder->raw);

        OS::Report(
            "[GravityDebug] AREA[%u] shape=%s pri=%u str=%d blend=%u top=(%d,%d,%d) down=(%d,%d,%d) scale=(%d,%d,%d)\n",
            areaId,
            ShapeName(shape),
            holder->raw->priority,
            holder->raw->setting1,
            holder->raw->setting2,
            RoundToInt(holder->raw->position.x),
            RoundToInt(holder->raw->position.y),
            RoundToInt(holder->raw->position.z),
            ToMilli(down.x),
            ToMilli(down.y),
            ToMilli(down.z),
            RoundToInt(holder->raw->scale.x),
            RoundToInt(holder->raw->scale.y),
            RoundToInt(holder->raw->scale.z));
    }
}

void DumpAreaTransitionDetail(const char* label, s16 areaId, const Vec3& position) {
    KMP::Manager* kmpManager = KMP::Manager::sInstance;
    if (kmpManager == nullptr || kmpManager->areaSection == nullptr) return;
    KMP::Section<AREA>& areaSection = *kmpManager->areaSection;
    if (areaId < 0 || areaId >= areaSection.pointCount) return;

    KMP::Holder<AREA>* holder = areaSection.holdersArray[areaId];
    if (!IsGravityAreaHolder(holder)) return;

    Vec3 up;
    Vec3 right;
    Vec3 forward;
    BuildAreaBasis(*holder, up, right, forward);
    const Vec3 down = ScaleVec(up, -1.0f);
    const Vec3 diff = SubVec(position, holder->raw->position);

    const float localRight = DotVec(diff, right);
    const float localForward = DotVec(diff, forward);
    const float localUp = DotVec(diff, up);
    const float localDownFromTop = DotVec(diff, down);

    const bool containsByHolder = holder->IsPointInAREA(position);
    const u8 shape = GetGravityVolumeShape(*holder->raw);

    OS::Report(
        "[GravityDebug] %s detail area=%d shape=%s contains=%u local(right,up,fwd)=(%d,%d,%d) downFromTop=%d ext(hw,hl,h)=(%d,%d,%d) basisX=(%d,%d,%d) basisY=(%d,%d,%d) basisZ=(%d,%d,%d)\n",
        label,
        areaId,
        ShapeName(shape),
        containsByHolder ? 1 : 0,
        RoundToInt(localRight),
        RoundToInt(localUp),
        RoundToInt(localForward),
        RoundToInt(localDownFromTop),
        RoundToInt(holder->halfWidth),
        RoundToInt(holder->halfLength),
        RoundToInt(holder->height),
        ToMilli(right.x),
        ToMilli(right.y),
        ToMilli(right.z),
        ToMilli(up.x),
        ToMilli(up.y),
        ToMilli(up.z),
        ToMilli(forward.x),
        ToMilli(forward.y),
        ToMilli(forward.z));
}

void UpdateVisuals() {
    if (!kEnableTempGravityDebugVisuals) return;

    ++sDebugState.visualFrameCounter;
    if ((sDebugState.visualFrameCounter % kVisualSpawnIntervalFrames) != 0) return;
    DrawGravityAreaVisuals();
}

void GravityTempDebugRaceFrame() {
    UpdateVisuals();
}
static RaceFrameHook gravityTempDebugRaceFrameHook(GravityTempDebugRaceFrame);

}  // namespace

void OnRaceLoad() {
    ResetRaceDebugState();
    if (kEnableTempGravityDebugLogs) DumpRaceStartAreaSummary();
}

void OnPlayerGravityResolved(
    u8 playerIdx,
    const Vec3& position,
    s16 previousAreaId,
    s16 newAreaId,
    const Vec3& gravityDown,
    float gravityStrength,
    u16 blendFrames,
    bool snapTransition) {
    if (!kEnableTempGravityDebugLogs) return;
    if (playerIdx >= kMaxTrackedPlayers) return;

    sDebugState.lastKnownAreaByPlayer[playerIdx] = newAreaId;
    if (previousAreaId == newAreaId && !snapTransition) return;
    u32 raceFrame = 0;
    if (Raceinfo::sInstance != nullptr) raceFrame = Raceinfo::sInstance->raceFrames;

    const char* eventName = "switch";
    if (previousAreaId < 0 && newAreaId >= 0) eventName = "enter";
    else if (previousAreaId >= 0 && newAreaId < 0) eventName = "exit";
    else if (previousAreaId == newAreaId && snapTransition) eventName = "refresh";

    OS::Report(
        "[GravityDebug] P%u %s prev=%d new=%d frame=%u pos=(%d,%d,%d) down=(%d,%d,%d) g=%d blend=%u snap=%u\n",
        playerIdx,
        eventName,
        previousAreaId,
        newAreaId,
        raceFrame,
        RoundToInt(position.x),
        RoundToInt(position.y),
        RoundToInt(position.z),
        ToMilli(gravityDown.x),
        ToMilli(gravityDown.y),
        ToMilli(gravityDown.z),
        ToMilli(gravityStrength),
        blendFrames,
        snapTransition ? 1 : 0);

    if (newAreaId >= 0 && previousAreaId != newAreaId) DumpAreaTransitionDetail("enter", newAreaId, position);
    if (previousAreaId >= 0 && newAreaId < 0) DumpAreaTransitionDetail("exit", previousAreaId, position);
}

void OnRespawn(const Kart::Link& link, s16 areaId, const Vec3& gravityDown, float gravityStrength) {
    if (!kEnableTempGravityDebugLogs) return;

    const Vec3& position = link.GetPosition();
    OS::Report(
        "[GravityDebug] Respawn P%u area=%d pos=(%d,%d,%d) down=(%d,%d,%d) g=%d\n",
        link.GetPlayerIdx(),
        areaId,
        RoundToInt(position.x),
        RoundToInt(position.y),
        RoundToInt(position.z),
        ToMilli(gravityDown.x),
        ToMilli(gravityDown.y),
        ToMilli(gravityDown.z),
        ToMilli(gravityStrength));
}

void OnItemGravityApplied(const Item::Obj& itemObj, s16 areaId, const Vec3& gravityDown, float gravityStrength) {
    if (!kEnableTempGravityDebugLogs) return;
    if (areaId < 0) return;

    if (sDebugState.itemLogsRemaining == 0) {
        if (!sDebugState.itemBudgetExhaustedPrinted) {
            OS::Report("[GravityDebug] Item gravity logs budget exhausted for this race.\n");
            sDebugState.itemBudgetExhaustedPrinted = true;
        }
        return;
    }

    --sDebugState.itemLogsRemaining;
    OS::Report(
        "[GravityDebug] Item id=%u owner=%u area=%d pos=(%d,%d,%d) down=(%d,%d,%d) g=%d left=%u\n",
        static_cast<u32>(itemObj.itemObjId),
        static_cast<u32>(itemObj.playerUsedItemId),
        areaId,
        RoundToInt(itemObj.position.x),
        RoundToInt(itemObj.position.y),
        RoundToInt(itemObj.position.z),
        ToMilli(gravityDown.x),
        ToMilli(gravityDown.y),
        ToMilli(gravityDown.z),
        ToMilli(gravityStrength),
        sDebugState.itemLogsRemaining);
}

}  // namespace TempDebug
}  // namespace GravityFields
}  // namespace Race
}  // namespace Pulsar
