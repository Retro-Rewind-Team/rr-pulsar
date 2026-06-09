#include <kamek.hpp>
#include <runtimeWrite.hpp>
#include <Gamemodes/PracticeMode/TTPracticeInternal.hpp>
#include <MarioKartWii/Objects/Collidable/Itembox/Itembox.hpp>
#include <MarioKartWii/Objects/KCL/ExternalKCL/VolcanoPiece.hpp>

namespace Pulsar {
namespace TTPractice {

kmRuntimeUse(0x80828860);  // Objects::Itembox::Update
kmRuntimeUse(0x806807e8);  // Objects::ObjectExternKCL::UpdateKCL
kmRuntimeUse(0x808044c0);  // Objects::VolcanoPiece::UpdateKCL
kmRuntimeUse(0x80805924);  // Objects::VolcanoPiece::UpdateDiffPosVector
kmRuntimeUse(0x80818334);  // Objects::VolcanoPiece::UpdateCollisionPosition
kmRuntimeUse(0x80818674);  // Objects::VolcanoPiece::SetYScale
kmRuntimeUse(0x80819400);  // Objects::VolcanoPiece::Update
kmRuntimeUse(0x808199a8);  // Objects::VolcanoPiece::IsCollidingNoTriangleCheckImpl
kmRuntimeUse(0x80819da0);  // Objects::VolcanoPiece::IsCollidingImpl

typedef void (*ItemBoxUpdateFn)(Objects::Itembox* itembox);
typedef void (*ObjectExternKCLUpdateKCLFn)(Objects::VolcanoPiece* piece, const Vec3& position, KCLBitfield accepted,
                                           bool isBiggerThanDefaultScale, float radius);
typedef void (*VolcanoPieceUpdateKCLFn)(Objects::VolcanoPiece* piece, const Vec3& position, KCLBitfield accepted,
                                        bool isBiggerThanDefaultScale, float radius);
typedef void (*VolcanoPieceUpdateDiffPosVectorFn)(Objects::VolcanoPiece* piece, const Vec3& src);
typedef void (*VolcanoPieceUpdateCollisionPositionFn)(Objects::VolcanoPiece* piece, u32 timeOffset);
typedef void (*VolcanoPieceSetYScaleFn)(Objects::VolcanoPiece* piece, u32 timeOffset);
typedef void (*VolcanoPieceUpdateFn)(Objects::VolcanoPiece* piece);
typedef bool (*VolcanoPieceCollisionFn)(Objects::VolcanoPiece* piece, const Vec3& pos, const Vec3& prevPos, KCLBitfield accepted,
                                        CollisionInfo* info, KCLTypeHolder* ret, u32 timeOffset, float radius);

static ItemBoxUpdateFn GetItemBoxUpdate() {
    static const ItemBoxUpdateFn function = reinterpret_cast<ItemBoxUpdateFn>(kmRuntimeAddr(0x80828860));
    return function;
}

static ObjectExternKCLUpdateKCLFn GetObjectExternKCLUpdateKCL() {
    static const ObjectExternKCLUpdateKCLFn function = reinterpret_cast<ObjectExternKCLUpdateKCLFn>(kmRuntimeAddr(0x806807e8));
    return function;
}

static VolcanoPieceUpdateKCLFn GetVolcanoPieceUpdateKCL() {
    static const VolcanoPieceUpdateKCLFn function = reinterpret_cast<VolcanoPieceUpdateKCLFn>(kmRuntimeAddr(0x808044c0));
    return function;
}

static VolcanoPieceUpdateDiffPosVectorFn GetVolcanoPieceUpdateDiffPosVector() {
    static const VolcanoPieceUpdateDiffPosVectorFn function =
        reinterpret_cast<VolcanoPieceUpdateDiffPosVectorFn>(kmRuntimeAddr(0x80805924));
    return function;
}

static VolcanoPieceUpdateCollisionPositionFn GetVolcanoPieceUpdateCollisionPosition() {
    static const VolcanoPieceUpdateCollisionPositionFn function =
        reinterpret_cast<VolcanoPieceUpdateCollisionPositionFn>(kmRuntimeAddr(0x80818334));
    return function;
}

static VolcanoPieceSetYScaleFn GetVolcanoPieceSetYScale() {
    static const VolcanoPieceSetYScaleFn function = reinterpret_cast<VolcanoPieceSetYScaleFn>(kmRuntimeAddr(0x80818674));
    return function;
}

static VolcanoPieceUpdateFn GetVolcanoPieceUpdate() {
    static const VolcanoPieceUpdateFn function = reinterpret_cast<VolcanoPieceUpdateFn>(kmRuntimeAddr(0x80819400));
    return function;
}

static VolcanoPieceCollisionFn GetVolcanoPieceIsCollidingNoTriangleCheckImpl() {
    static const VolcanoPieceCollisionFn function = reinterpret_cast<VolcanoPieceCollisionFn>(kmRuntimeAddr(0x808199a8));
    return function;
}

static VolcanoPieceCollisionFn GetVolcanoPieceIsCollidingImpl() {
    static const VolcanoPieceCollisionFn function = reinterpret_cast<VolcanoPieceCollisionFn>(kmRuntimeAddr(0x80819da0));
    return function;
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

static bool ShouldFreezePracticeObjects() {
    return IsEnabled() && IsObjectFreezeEnabled();
}

static u32 GetFrozenObjectTimeOffset(u32 fallback) {
    const Raceinfo* raceinfo = Raceinfo::sInstance;
    if (raceinfo == nullptr) return fallback;
    return raceinfo->raceFrames;
}

static u32 GetPracticeObjectTimeOffset(u32 timeOffset) {
    return ShouldFreezePracticeObjects() ? GetFrozenObjectTimeOffset(timeOffset) : timeOffset;
}

static void UpdatePracticeVolcanoPieceKCL(Objects::VolcanoPiece* piece, const Vec3& position, KCLBitfield accepted,
                                          bool isBiggerThanDefaultScale, float radius) {
    if (piece != nullptr && ShouldFreezePracticeObjects()) {
        GetObjectExternKCLUpdateKCL()(piece, position, accepted, isBiggerThanDefaultScale, radius);
        return;
    }

    GetVolcanoPieceUpdateKCL()(piece, position, accepted, isBiggerThanDefaultScale, radius);
}
kmWritePointer(0x808d67ec, UpdatePracticeVolcanoPieceKCL);

static void UpdatePracticeVolcanoPieceDiffPosVector(Objects::VolcanoPiece* piece, const Vec3& src) {
    if (piece != nullptr && ShouldFreezePracticeObjects()) {
        static const Vec3 zero(0.0f, 0.0f, 0.0f);
        GetVolcanoPieceUpdateDiffPosVector()(piece, zero);
        return;
    }

    GetVolcanoPieceUpdateDiffPosVector()(piece, src);
}
kmWritePointer(0x808d6834, UpdatePracticeVolcanoPieceDiffPosVector);

static void UpdatePracticeVolcanoPieceCollisionPosition(Objects::VolcanoPiece* piece, u32 timeOffset) {
    GetVolcanoPieceUpdateCollisionPosition()(piece, GetPracticeObjectTimeOffset(timeOffset));
}
kmWritePointer(0x808d682c, UpdatePracticeVolcanoPieceCollisionPosition);

static void SetPracticeVolcanoPieceYScale(Objects::VolcanoPiece* piece, u32 timeOffset) {
    GetVolcanoPieceSetYScale()(piece, GetPracticeObjectTimeOffset(timeOffset));
}
kmWritePointer(0x808d6830, SetPracticeVolcanoPieceYScale);

static void UpdatePracticeVolcanoPiece(Objects::VolcanoPiece* piece) {
    if (piece != nullptr && ShouldFreezePracticeObjects()) return;
    GetVolcanoPieceUpdate()(piece);
}
kmWritePointer(0x808d6720, UpdatePracticeVolcanoPiece);

static bool IsPracticeVolcanoPieceCollidingNoTriangleCheckImpl(Objects::VolcanoPiece* piece, const Vec3& pos, const Vec3& prevPos,
                                                               KCLBitfield accepted, CollisionInfo* info, KCLTypeHolder* ret,
                                                               u32 timeOffset, float radius) {
    return GetVolcanoPieceIsCollidingNoTriangleCheckImpl()(piece, pos, prevPos, accepted, info, ret,
                                                           GetPracticeObjectTimeOffset(timeOffset), radius);
}
kmWritePointer(0x808d6854, IsPracticeVolcanoPieceCollidingNoTriangleCheckImpl);

static bool IsPracticeVolcanoPieceCollidingImpl(Objects::VolcanoPiece* piece, const Vec3& pos, const Vec3& prevPos,
                                                KCLBitfield accepted, CollisionInfo* info, KCLTypeHolder* ret, u32 timeOffset,
                                                float radius) {
    return GetVolcanoPieceIsCollidingImpl()(piece, pos, prevPos, accepted, info, ret, GetPracticeObjectTimeOffset(timeOffset), radius);
}
kmWritePointer(0x808d6858, IsPracticeVolcanoPieceCollidingImpl);

}  // namespace TTPractice
}  // namespace Pulsar
