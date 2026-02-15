#include <kamek.hpp>
#include <runtimeWrite.hpp>
#include <MarioKartWii/3D/Camera/CameraMgr.hpp>
#include <MarioKartWii/CourseMgr.hpp>
#include <MarioKartWii/KMP/GOBJ.hpp>
#include <MarioKartWii/KMP/KMPManager.hpp>
#include <MarioKartWii/System/Random.hpp>
#include <MarioKartWii/Item/Obj/ItemObj.hpp>
#include <MarioKartWii/Kart/KartPlayer.hpp>
#include <MarioKartWii/Kart/KartStatus.hpp>
#include <MarioKartWii/Objects/Object.hpp>
#include <MarioKartWii/Objects/ObjectsMgr.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>

namespace Pulsar {
namespace Race {

struct ObjectConditionalView {
    u8 padding[0xa0];
    const void* gobjLink;
};

struct ConditionalConfig {
    u8 startIdx;
    u8 endIdx;
    bool useCheckpointMode;
    bool invert;
};

struct ConditionalState {
    bool isConditional;
    bool isActive;
    u8 localScreenCount;
    bool screenIsActive[4];
};

static void FillScreenState(bool (&screenIsActive)[4], bool value) {
    for (u8 i = 0; i < 4; ++i) screenIsActive[i] = value;
}

static void InitConditionalState(ConditionalState& state) {
    state.isConditional = false;
    state.isActive = true;
    state.localScreenCount = 1;
    FillScreenState(state.screenIsActive, true);
}

static u8 sCollisionContextPlayerId = 0xFF;
static u8 sCollisionContextStack[8];
static u32 sCollisionContextDepth = 0;

static const u32 BOXCOL_FLAG_DRIVER = 0x1;
static const u32 BOXCOL_FLAG_ITEM = 0x2;
static const u32 BOXCOL_FLAG_OBJECT = 0x4;
static const u32 BOXCOL_FLAG_OBJECT_OBSTACLE_ENEMY = 0x8;
static const u32 BOXCOL_FLAG_DRIVABLE = 0x10;

struct BoxColUnitView {
    u8 padding[0x0c];
    u32 unitType;
    void* userData;
};

void PushConditionalCollisionPlayerContext(u8 playerId) {
    if (playerId >= 12) playerId = 0xFF;

    if (sCollisionContextDepth < 8) {
        sCollisionContextStack[sCollisionContextDepth] = sCollisionContextPlayerId;
    }
    ++sCollisionContextDepth;
    sCollisionContextPlayerId = playerId;
}

void PopConditionalCollisionPlayerContext() {
    if (sCollisionContextDepth == 0) {
        sCollisionContextPlayerId = 0xFF;
        return;
    }

    --sCollisionContextDepth;
    if (sCollisionContextDepth < 8) {
        sCollisionContextPlayerId = sCollisionContextStack[sCollisionContextDepth];
    } else if (sCollisionContextDepth == 0) {
        sCollisionContextPlayerId = 0xFF;
    }
}

static u8 GetConditionalCollisionPlayerContext() {
    return sCollisionContextPlayerId;
}

static bool IsInWrappedRange(u8 value, u8 start, u8 end) {
    if (start <= end) return value >= start && value <= end;
    return value >= start || value <= end;
}

static const GOBJ* GetObjectGobj(const Object& object) {
    const ObjectConditionalView& view = reinterpret_cast<const ObjectConditionalView&>(object);
    if (view.gobjLink == nullptr) return nullptr;
    return *reinterpret_cast<GOBJ* const*>(view.gobjLink);
}

static bool TryGetConditionalConfig(const Object& object, ConditionalConfig& config) {
    const GOBJ* gobj = GetObjectGobj(object);
    if (gobj == nullptr) return false;

    const u16 flags = gobj->presenceFlags;
    const u8 mode = static_cast<u8>((flags >> 3) & 0x7);
    if (mode == 0 || mode > 4) return false;

    config.startIdx = static_cast<u8>((flags >> 6) & 0x7);
    config.endIdx = static_cast<u8>((flags >> 9) & 0x7);
    config.useCheckpointMode = (mode == 2 || mode == 4);
    config.invert = (mode == 3 || mode == 4);
    return true;
}

static bool IsPlayerInConditionalRange(const RaceinfoPlayer& player, bool useCheckpointMode, u16 ckptCount, u8 startIdx, u8 endIdx) {
    u8 currentIdx = 0;
    if (useCheckpointMode) {
        u16 checkpoint = player.checkpoint;
        if (checkpoint >= ckptCount) checkpoint = static_cast<u16>(ckptCount - 1);
        currentIdx = static_cast<u8>((checkpoint * 8u) / ckptCount);
        if (currentIdx > 7) currentIdx = 7;
    } else {
        u16 currentLap = player.currentLap;
        if (currentLap == 0) currentLap = 1;
        if (currentLap > 8) currentLap = 8;
        currentIdx = static_cast<u8>(currentLap - 1);
    }
    return IsInWrappedRange(currentIdx, startIdx, endIdx);
}

static bool EvaluateConditionalForPlayer(const ConditionalConfig& config, u8 playerId) {
    const Raceinfo* raceInfo = Raceinfo::sInstance;
    if (raceInfo == nullptr) return true;

    if (playerId >= 12) return true;
    const RaceinfoPlayer* player = raceInfo->players[playerId];
    if (player == nullptr) return true;

    u16 ckptCount = 0;
    if (config.useCheckpointMode) {
        const KMP::Manager* kmp = KMP::Manager::sInstance;
        if (kmp == nullptr || kmp->ckptSection == nullptr || kmp->ckptSection->pointCount == 0) return true;
        ckptCount = kmp->ckptSection->pointCount;
    }

    const bool inRange = IsPlayerInConditionalRange(*player, config.useCheckpointMode, ckptCount, config.startIdx, config.endIdx);
    return config.invert ? !inRange : inRange;
}

static bool IsConditionalReplayPlayer(const RacedataScenario& scenario, const Raceinfo& raceInfo, u8 playerId) {
    if (playerId >= 12) return false;

    const PlayerType type = scenario.players[playerId].playerType;
    if (type != PLAYER_GHOST && type != PLAYER_REAL_LOCAL) return false;
    return raceInfo.players[playerId] != nullptr;
}

static void EvaluateConditionalState(const Object& object, ConditionalState& state) {
    InitConditionalState(state);

    ConditionalConfig config;
    if (!TryGetConditionalConfig(object, config)) return;
    state.isConditional = true;

    const Racedata* raceData = Racedata::sInstance;
    const Raceinfo* raceInfo = Raceinfo::sInstance;
    if (raceData == nullptr || raceInfo == nullptr) return;

    const RacedataScenario& scenario = raceData->racesScenario;
    const GameMode mode = scenario.settings.gamemode;
    const bool isTTMode = (mode == MODE_TIME_TRIAL || mode == MODE_GHOST_RACE);

    if (isTTMode) {
        u8 watchedPlayerId = 0xFF;
        const RaceCameraMgr* cameraMgr = RaceCameraMgr::sInstance;
        if (cameraMgr != nullptr) {
            const u8 focusedPlayerId = cameraMgr->focusedPlayerIdx;
            if (IsConditionalReplayPlayer(scenario, *raceInfo, focusedPlayerId)) watchedPlayerId = focusedPlayerId;
        }

        if (watchedPlayerId == 0xFF) {
            const u8 hudPlayerId = scenario.settings.hudPlayerIds[0];
            if (IsConditionalReplayPlayer(scenario, *raceInfo, hudPlayerId)) watchedPlayerId = hudPlayerId;
        }

        if (watchedPlayerId != 0xFF) {
            state.isActive = EvaluateConditionalForPlayer(config, watchedPlayerId);
        } else {
            state.isActive = true;
        }

        state.localScreenCount = 1;
        FillScreenState(state.screenIsActive, state.isActive);
        return;
    }

    const u8 localScreenCount = (scenario.localPlayerCount > 4) ? 4 : scenario.localPlayerCount;
    if (localScreenCount == 0) return;

    state.localScreenCount = localScreenCount;
    FillScreenState(state.screenIsActive, true);
    state.isActive = false;
    for (u8 i = 0; i < localScreenCount; ++i) {
        const u8 playerId = scenario.settings.hudPlayerIds[i];
        const bool playerActive = EvaluateConditionalForPlayer(config, playerId);
        state.screenIsActive[i] = playerActive;
        if (playerActive) state.isActive = true;
    }
}

static void ApplyModelDirectorScreenVisibility(ModelDirector* director, const ConditionalState& state) {
    if (director == nullptr) return;

    for (u8 screenIdx = 0; screenIdx < state.localScreenCount; ++screenIdx) {
        if (state.screenIsActive[screenIdx])
            director->EnableScreen(screenIdx);
        else
            director->DisableScreen(screenIdx);
    }
}

static bool IsScreenSpecificModelRegistered(const ScnMgr& scnMgr, const ModelDirector* director) {
    void* current = nullptr;
    while (true) {
        current = nw4r::ut::List_GetNext(&scnMgr.screenSpecificModelDirectors, current);
        if (current == nullptr) return false;
        if (current == director) return true;
    }
}

static void EnsureScreenSpecificModelRegistration(ModelDirector* director) {
    if (director == nullptr) return;

    ScnMgr* scnMgr = director->GetScnManager();
    if (scnMgr == nullptr) return;
    if (IsScreenSpecificModelRegistered(*scnMgr, director)) return;

    // Mark as screen-specific and register so ScnMgr::UpdateVisibility updates this model per local screen.
    director->bitfield |= 0x8;
    scnMgr->AppendScreenSpecificModelDirector(director);
}

static void ApplyPerScreenVisibility(Object& object, const ConditionalState& state) {
    if (!state.isConditional || state.localScreenCount <= 1) return;

    EnsureScreenSpecificModelRegistration(object.mdlDirector);
    EnsureScreenSpecificModelRegistration(object.mdlLodDirector);
    EnsureScreenSpecificModelRegistration(object.shadowDirector);

    ApplyModelDirectorScreenVisibility(object.mdlDirector, state);
    ApplyModelDirectorScreenVisibility(object.mdlLodDirector, state);
    ApplyModelDirectorScreenVisibility(object.shadowDirector, state);
}

static void ApplyConditionalState(Object& object, const ConditionalState& state) {
    if (!state.isConditional) return;

    object.ToggleVisible(state.isActive);
    if (state.isActive)
        object.EnableCollision();
    else
        object.DisableCollision();
}

static void ApplyKCLConditionalState(Object& object, const ConditionalState& state) {
    if (!state.isConditional) return;

    object.ToggleVisible(state.isActive);
    if (state.isActive) {
        object.EnableCollision();
        if (object.entity != nullptr) object.entity->paramsBitfield |= 0x10;
    } else {
        object.DisableCollision();
        if (object.entity != nullptr) object.entity->paramsBitfield &= ~0x10;
    }
}

static bool IsObjectActiveForPlayer(const Object& object, u8 playerId) {
    ConditionalConfig config;
    if (!TryGetConditionalConfig(object, config)) return true;
    return EvaluateConditionalForPlayer(config, playerId);
}

static ObjectCollision* CallOriginalGetCollision(void* object) {
    typedef ObjectCollision* (*GetCollisionFn)(void*);
    const u32* vtable = *reinterpret_cast<const u32* const*>(object);
    GetCollisionFn getCollision = reinterpret_cast<GetCollisionFn>(vtable[0xb4 / 4]);
    return getCollision(object);
}

static bool CallOriginalDriveableCollisionCheck(void* object, float radius, const Vec3& pos, const Vec3& prevPos, KCLBitfield accepted,
                                                CollisionInfo* info, KCLTypeHolder* ret, u32 timeOffset, u32 vtableOffset) {
    typedef bool (*DriveableCollisionCheckFn)(void*, float, const Vec3&, const Vec3&, KCLBitfield, CollisionInfo*, KCLTypeHolder*, u32);
    const u32* vtable = *reinterpret_cast<const u32* const*>(object);
    DriveableCollisionCheckFn checkFn = reinterpret_cast<DriveableCollisionCheckFn>(vtable[vtableOffset / 4]);
    return checkFn(object, radius, pos, prevPos, accepted, info, ret, timeOffset);
}

static bool IsDriveableObjectActiveForCurrentPlayer(const void* object) {
    const u8 playerId = GetConditionalCollisionPlayerContext();
    if (playerId >= 12) return true;

    const Object& mapObject = *reinterpret_cast<const Object*>(object);
    return IsObjectActiveForPlayer(mapObject, playerId);
}

static ObjectCollision* ConditionalGetObjectCollision(void* object) {
    if (object == nullptr) return nullptr;

    ObjectCollision* collision = CallOriginalGetCollision(object);
    if (collision == nullptr) return nullptr;

    register const Kart::Player* kartPlayer;
    asm(mr kartPlayer, r25;);
    if (kartPlayer == nullptr) return collision;

    const u8 playerId = kartPlayer->GetPlayerIdx();
    const Object& mapObject = *reinterpret_cast<const Object*>(object);
    if (!IsObjectActiveForPlayer(mapObject, playerId)) return nullptr;
    return collision;
}
kmCall(0x8082ab8c, ConditionalGetObjectCollision);

static ObjectCollision* ConditionalGetObjectCollisionForItem(void* object) {
    if (object == nullptr) return nullptr;

    ObjectCollision* collision = CallOriginalGetCollision(object);
    if (collision == nullptr) return nullptr;

    register Item::Obj* itemObj;
    asm(mr itemObj, r27;);
    if (itemObj == nullptr) return collision;

    const u8 playerId = itemObj->playerUsedItemId;
    const Object& mapObject = *reinterpret_cast<const Object*>(object);
    if (!IsObjectActiveForPlayer(mapObject, playerId)) return nullptr;
    return collision;
}
kmCall(0x8082ae18, ConditionalGetObjectCollisionForItem);

static bool ConditionalCourseCollisionSetPlayerFromWheel(float radius, CourseMgr& mgr, const Vec3& position, const Vec3& prevPosition,
                                                         KCLBitfield acceptedFlags, CollisionInfo* info, KCLTypeHolder& kclFlags) {
    register u32 playerIdRaw;
    asm(mr playerIdRaw, r25;);

    PushConditionalCollisionPlayerContext(static_cast<u8>(playerIdRaw));
    const bool isColliding = mgr.IsCollidingAddEntry(position, prevPosition, acceptedFlags, info, &kclFlags, 0, radius);
    PopConditionalCollisionPlayerContext();
    return isColliding;
}
kmCall(0x805b7028, ConditionalCourseCollisionSetPlayerFromWheel);  // CourseMgr::IsCollidingAddEntry call in KartCollide::calcWheelCollision

static void FilterConditionalDriveablesForCurrentPlayer(void* boxColMgr) {
    const u8 playerId = GetConditionalCollisionPlayerContext();
    if (playerId >= 12 || boxColMgr == nullptr) return;

    u8* const mgr = reinterpret_cast<u8*>(boxColMgr);
    s32& maxId = *reinterpret_cast<s32*>(mgr + 0x438);
    if (maxId <= 0) return;

    BoxColUnitView** units = *reinterpret_cast<BoxColUnitView***>(mgr + 0x1c);
    if (units == nullptr) return;

    s32 writeIdx = 0;
    for (s32 readIdx = 0; readIdx < maxId; ++readIdx) {
        BoxColUnitView* unit = units[readIdx];
        if (unit == nullptr) continue;

        bool keep = true;
        if ((unit->unitType & BOXCOL_FLAG_DRIVABLE) != 0 && unit->userData != nullptr) {
            const Object& object = *reinterpret_cast<const Object*>(unit->userData);
            keep = IsObjectActiveForPlayer(object, playerId);
        }

        if (keep) {
            units[writeIdx] = unit;
            ++writeIdx;
        }
    }

    maxId = writeIdx;
}

static s32 FindFirstUnitOfType(BoxColUnitView* const* units, s32 maxId, u32 mask) {
    for (s32 i = 0; i < maxId; ++i) {
        const BoxColUnitView* unit = units[i];
        if (unit != nullptr && (unit->unitType & mask) != 0) return i;
    }
    return 0x100;
}

static void ConditionalResetIterators(void* boxColMgr) {
    if (boxColMgr == nullptr) return;

    FilterConditionalDriveablesForCurrentPlayer(boxColMgr);

    u8* const mgr = reinterpret_cast<u8*>(boxColMgr);
    const s32 maxId = *reinterpret_cast<const s32*>(mgr + 0x438);
    BoxColUnitView** units = *reinterpret_cast<BoxColUnitView***>(mgr + 0x1c);

    s32& nextPlayerId = *reinterpret_cast<s32*>(mgr + 0x428);
    s32& nextItemId = *reinterpret_cast<s32*>(mgr + 0x42c);
    s32& nextObjectId = *reinterpret_cast<s32*>(mgr + 0x430);
    s32& nextDrivableId = *reinterpret_cast<s32*>(mgr + 0x434);

    if (units == nullptr || maxId <= 0) {
        nextPlayerId = 0x100;
        nextItemId = 0x100;
        nextObjectId = 0x100;
        nextDrivableId = 0x100;
        return;
    }

    nextPlayerId = FindFirstUnitOfType(units, maxId, BOXCOL_FLAG_DRIVER);
    nextItemId = FindFirstUnitOfType(units, maxId, BOXCOL_FLAG_ITEM);
    nextObjectId = FindFirstUnitOfType(units, maxId, BOXCOL_FLAG_OBJECT | BOXCOL_FLAG_OBJECT_OBSTACLE_ENEMY);
    nextDrivableId = FindFirstUnitOfType(units, maxId, BOXCOL_FLAG_DRIVABLE);
}
kmBranch(0x80785f2c, ConditionalResetIterators);  // BoxColManager::resetIterators

static void ConditionalCalcCollisions(Kart::Status* status) {
    Kart::Link* link = status->link;
    u8 playerId = link->GetPlayerIdx();
    PushConditionalCollisionPlayerContext(playerId);
    status->UpdateCollisions();
    PopConditionalCollisionPlayerContext();
}
kmCall(0x80594858, ConditionalCalcCollisions);  // KartStatus::UpdateCollisions call in KartStatus::calc

static void ConditionalObjectUpdate(Object* object) {
    if (object == nullptr) return;

    ConditionalState state;
    EvaluateConditionalState(*object, state);
    ApplyConditionalState(*object, state);
    if (state.isActive) object->Update();
}
kmCall(0x8082a9e0, ConditionalObjectUpdate);  // Object::Update call in ObjectsMgr::Update

static void ConditionalObjectModelUpdate(Object* object) {
    if (object == nullptr) return;

    ConditionalState state;
    EvaluateConditionalState(*object, state);
    if (!state.isActive) return;

    object->UpdateModel();
    ApplyPerScreenVisibility(*object, state);
}
kmCall(0x8082aa20, ConditionalObjectModelUpdate);  // Object::UpdateModel call in ObjectsMgr::Update

static void ConditionalProcessAllAndCalcKCL(void* kclMgr) {
    if (kclMgr != nullptr) {
        const u16 kclCount = *reinterpret_cast<const u16*>(reinterpret_cast<const u8*>(kclMgr) + 0x4);
        Object** kclObjects = *reinterpret_cast<Object***>(reinterpret_cast<u8*>(kclMgr) + 0x8);
        for (u16 i = 0; i < kclCount; ++i) {
            Object* obj = kclObjects[i];
            if (obj == nullptr) continue;
            ConditionalState state;
            EvaluateConditionalState(*obj, state);

            ApplyKCLConditionalState(*obj, state);
            ApplyPerScreenVisibility(*obj, state);
        }
    }

    typedef void (*OriginalCalcFn)(void*);
    OriginalCalcFn originalCalc = reinterpret_cast<OriginalCalcFn>(0x8081b618);
    originalCalc(kclMgr);
}
kmCall(0x8082aa40, ConditionalProcessAllAndCalcKCL);  // ObjectDriveableDirector::calc call in ObjectsMgr::Update

static void ConditionalKCLObjectUpdate(Object* object) {
    if (object == nullptr) return;

    ConditionalState state;
    EvaluateConditionalState(*object, state);
    ApplyKCLConditionalState(*object, state);
    if (state.isActive) object->Update();
}
kmCall(0x8081b658, ConditionalKCLObjectUpdate);  // ObjectKCL::Update call in ObjectDriveableDirector::calc

static void ConditionalKCLObjectModelUpdate(Object* object) {
    if (object == nullptr) return;

    ConditionalState state;
    EvaluateConditionalState(*object, state);
    if (!state.isActive) return;

    object->UpdateModel();
    ApplyPerScreenVisibility(*object, state);
}
kmCall(0x8081b698, ConditionalKCLObjectModelUpdate);  // ObjectKCL::UpdateModel call in ObjectDriveableDirector::calc

}  // namespace Race
}  // namespace Pulsar