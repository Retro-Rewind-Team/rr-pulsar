#include <CustomCharacters/CustomCharacters.hpp>
#include <MarioKartWii/Item/ItemPlayer.hpp>
#include <MarioKartWii/Kart/KartPointers.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>

/*
    Optional driver CHR animations:
    - shockHit: replaces shock damage animation and hides the visible spin.
    - starUse / megaUse: replace the selected animation while active.
    - waitBeforeStart: replaces wait before the race starts.
    - shockDodgeStar: plays when shock is dodged by Star.
*/

namespace Pulsar {
namespace CustomCharacters {

kmRuntimeUse(0x8059fd0c);
kmRuntimeUse(0x80798728);

static const u32 DAMAGE_ANIM_ID = 0x1a;
static const u32 WAIT_ANIM_ID = 0x7;
static const u32 STAR_USE_SENTINEL_ANIM_ID = 0x8;
static const u16 WAIT_BEFORE_START_FALLBACK_FRAMES = 45;
static const u16 SHOCK_DODGE_STAR_FALLBACK_FRAMES = 45;

struct AnimationSlot {
    s16 chr;
    s16 pat;
};

struct LimbLockState {
    bool saved;
    u8 arm;
    u8 leg;
};

struct PlayerAnimationState {
    AnimationSlot shockHit;
    AnimationSlot starUse;
    AnimationSlot megaUse;
    AnimationSlot waitBeforeStart;
    AnimationSlot shockDodgeStar;
    u16 waitFrames;
    u16 waitFrameCount;
    u16 shockDodgeFrames;
    u16 shockDodgeFrameCount;
    bool waitLooped;
    bool waitPlayed;
    bool waitActive;
    bool shockDodgeActive;
    LimbLockState waitLimb;
    LimbLockState shockDodgeLimb;
    bool initialized;
};

static PlayerAnimationState animationStates[ONLINE_PLAYER_COUNT];

static void ResetAnimationState(PlayerAnimationState &state) {
    AnimationSlot *slots[] = {
        &state.shockHit, &state.starUse, &state.megaUse, &state.waitBeforeStart, &state.shockDodgeStar};
    for (u32 i = 0; i < ARRAY_COUNT(slots); ++i) {
        slots[i]->chr = -1;
        slots[i]->pat = -1;
    }
    state.waitFrames = 0;
    state.waitFrameCount = WAIT_BEFORE_START_FALLBACK_FRAMES;
    state.shockDodgeFrames = 0;
    state.shockDodgeFrameCount = SHOCK_DODGE_STAR_FALLBACK_FRAMES;
    state.waitLooped = true;
    state.waitPlayed = false;
    state.waitActive = false;
    state.shockDodgeActive = false;
    state.waitLimb.saved = false;
    state.shockDodgeLimb.saved = false;
    state.initialized = true;
}

static PlayerAnimationState &GetAnimationState(u8 playerIdx) {
    PlayerAnimationState &state = animationStates[playerIdx];
    if (!state.initialized) ResetAnimationState(state);
    return state;
}

extern "C" void DriverController_SetupModelAnims(
    g3d::ResFile *brresArray, ModelDirector **pDriver,
    ModelDirector **pDriverLod, DriverController *controller);

extern "C" bool PlayerModel_setAnimation(
    float blendRate, DriverController *controller, u32 animation, int param4);

static s16 LinkOptionalChrAnimation(
    DriverController *controller, ModelDirector *driverModel, const char *animationName) {
    if (controller->driverModelBRRES.GetResAnmChr(animationName).data == nullptr) return -1;

    ModelTransformator *transformator = driverModel->modelTransformator;
    if (transformator == nullptr) return -1;

    const u16 nextSlot = transformator->anmHolderList.count;

    driverModel->LinkAnimation(
        nextSlot,
        controller->driverModelBRRES,
        animationName,
        ANMTYPE_CHR,
        /*hasBlend=*/true,
        "brasd/driver_model",
        ARCHIVE_HOLDER_KART,
        /*playerIdForBRASD=*/0);

    return static_cast<s16>(nextSlot);
}

static s16 LinkOptionalPatAnimation(
    DriverController *controller, ModelDirector *driverModel, const char *animationName) {
    char patName[40];
    const char *resolvedName = animationName;
    if (controller->driverModelBRRES.GetResAnmTexPat(resolvedName).data == nullptr) {
        const int written = snprintf(patName, sizeof(patName), "%s.pat0", animationName);
        if (written <= 0 || static_cast<u32>(written) >= sizeof(patName) ||
            controller->driverModelBRRES.GetResAnmTexPat(patName).data == nullptr) {
            return -1;
        }
        resolvedName = patName;
    }

    ModelTransformator *transformator = driverModel->modelTransformator;
    if (transformator == nullptr) return -1;

    const u16 nextSlot = transformator->anmHolderList.count;

    driverModel->LinkAnimation(
        nextSlot,
        controller->driverModelBRRES,
        resolvedName,
        ANMTYPE_TEXPAT,
        /*hasBlend=*/false,
        "brasd/driver_model",
        ARCHIVE_HOLDER_KART,
        /*playerIdForBRASD=*/0);

    return static_cast<s16>(nextSlot);
}

static u16 GetOptionalChrFrameCount(DriverController *controller, const char *animationName) {
    g3d::ResAnmChr animation = controller->driverModelBRRES.GetResAnmChr(animationName);
    if (animation.data == nullptr || animation.data->fileInfo.frameCount == 0) {
        return 0;
    }
    return animation.data->fileInfo.frameCount;
}

static bool IsOptionalChrLooped(DriverController *controller, const char *animationName) {
    g3d::ResAnmChr animation = controller->driverModelBRRES.GetResAnmChr(animationName);
    return animation.data == nullptr || animation.data->fileInfo.isLooped != 0;
}

static ModelTransformator *GetDriverTransformator(DriverController *controller) {
    if (controller == nullptr || controller->driverModel == nullptr) return nullptr;
    return controller->driverModel->modelTransformator;
}

static void StopPatAnimation(DriverController *controller, u8 playerIdx);

static void SetActivePatSlot(DriverController *controller, s16 animationSlot) {
    if (controller == nullptr) return;
    u16 *const activePatSlot =
        reinterpret_cast<u16 *>(reinterpret_cast<u8 *>(controller) + 0x1ca);
    *activePatSlot = animationSlot < 0 ? 0xff : static_cast<u16>(animationSlot);
}

static void SetPatMapping(DriverController *controller, u32 animation, s16 patSlot) {
    if (controller == nullptr || animation >= 0x29) return;

    u8 *const patMappings = reinterpret_cast<u8 *>(controller) + 0x19e;
    patMappings[animation] = patSlot < 0 ? 0xff : static_cast<u8>(patSlot);
}

static void PlayPatAnimationIfPresent(DriverController *controller, u8 playerIdx, s16 animationSlot) {
    if (playerIdx >= ONLINE_PLAYER_COUNT) return;
    if (animationSlot < 0) {
        StopPatAnimation(controller, playerIdx);
        return;
    }

    ModelTransformator *transformator = GetDriverTransformator(controller);
    if (transformator == nullptr) return;
    AnmHolder *holder =
        transformator->GetAnmHolderByIdx(static_cast<u32>(static_cast<u16>(animationSlot)));
    if (holder == nullptr || holder->type != ANMTYPE_TEXPAT) return;

    transformator->PlayAnmNoBlend(static_cast<u32>(static_cast<u16>(animationSlot)), 0.0f, 1.0f);
}

static void StopPatAnimation(DriverController *controller, u8 playerIdx) {
    if (playerIdx >= ONLINE_PLAYER_COUNT) return;

    ModelTransformator *transformator = GetDriverTransformator(controller);
    if (transformator != nullptr) {
        transformator->StopAnmType(ANMTYPE_TEXPAT);
    }
    SetActivePatSlot(controller, -1);
}

static void LinkCustomCharacterAnimations(
    g3d::ResFile *brresArray, ModelDirector **pDriver,
    ModelDirector **pDriverLod, DriverController *controller) {
    DriverController_SetupModelAnims(brresArray, pDriver, pDriverLod, controller);

    const u8 playerIdx = controller->GetPlayerIdx();
    if (playerIdx >= ONLINE_PLAYER_COUNT) return;
    PlayerAnimationState &state = animationStates[playerIdx];
    ResetAnimationState(state);

    if (controller->isCpu) return;

    ModelDirector *driverModel = controller->driverModel;
    if (driverModel == nullptr) return;

    state.shockHit.chr = LinkOptionalChrAnimation(controller, driverModel, "shockHit");
    state.starUse.chr = LinkOptionalChrAnimation(controller, driverModel, "starUse");
    state.megaUse.chr = LinkOptionalChrAnimation(controller, driverModel, "megaUse");
    state.waitBeforeStart.chr = LinkOptionalChrAnimation(controller, driverModel, "waitBeforeStart");
    if (state.waitBeforeStart.chr >= 0) {
        const u16 frameCount = GetOptionalChrFrameCount(controller, "waitBeforeStart");
        state.waitFrameCount = frameCount == 0 ? WAIT_BEFORE_START_FALLBACK_FRAMES : frameCount;
        state.waitLooped = IsOptionalChrLooped(controller, "waitBeforeStart");
    }
    state.shockDodgeStar.chr = LinkOptionalChrAnimation(controller, driverModel, "shockDodgeStar");
    if (state.shockDodgeStar.chr >= 0) {
        const u16 frameCount = GetOptionalChrFrameCount(controller, "shockDodgeStar");
        state.shockDodgeFrameCount = frameCount == 0 ? SHOCK_DODGE_STAR_FALLBACK_FRAMES : frameCount;
    }
    if (state.shockHit.chr >= 0) state.shockHit.pat = LinkOptionalPatAnimation(controller, driverModel, "shockHit");
    if (state.starUse.chr >= 0) state.starUse.pat = LinkOptionalPatAnimation(controller, driverModel, "starUse");
    if (state.megaUse.chr >= 0) state.megaUse.pat = LinkOptionalPatAnimation(controller, driverModel, "megaUse");
    if (state.waitBeforeStart.chr >= 0) state.waitBeforeStart.pat = LinkOptionalPatAnimation(controller, driverModel, "waitBeforeStart");
    if (state.shockDodgeStar.chr >= 0) state.shockDodgeStar.pat = LinkOptionalPatAnimation(controller, driverModel, "shockDodgeStar");
}
kmCall(0x807c7894, LinkCustomCharacterAnimations);

static bool PlayCustomAnimationIfPresent(
    float blendRate, DriverController *controller, s16 animationSlot, s16 patSlot,
    u32 vanillaAnimation, int param4) {
    const u32 customAnimation = static_cast<u32>(static_cast<u16>(animationSlot));
    SetPatMapping(controller, vanillaAnimation, patSlot);
    const bool played = PlayerModel_setAnimation(blendRate, controller, customAnimation, param4);

    if (played) {
        PlayPatAnimationIfPresent(controller, controller->GetPlayerIdx(), patSlot);
        controller->currentAnimation = static_cast<u16>(vanillaAnimation);
        return true;
    }
    return false;
}

static bool IsBeforeRaceStart() {
    Raceinfo *raceInfo = Raceinfo::sInstance;
    return raceInfo != nullptr && raceInfo->timerMgr != nullptr &&
           (!raceInfo->timerMgr->hasRaceStarted || !raceInfo->IsAtLeastStage(RACESTAGE_RACE));
}

static s16 GetPowerUseAnimId(DriverController *controller, u8 playerIdx);

static s16 GetPatIdForChrId(u8 playerIdx, s16 animationSlot) {
    if (playerIdx >= ONLINE_PLAYER_COUNT) return -1;
    const PlayerAnimationState &state = GetAnimationState(playerIdx);
    const AnimationSlot *slots[] = {
        &state.shockHit, &state.starUse, &state.megaUse, &state.waitBeforeStart, &state.shockDodgeStar};
    for (u32 i = 0; i < ARRAY_COUNT(slots); ++i) {
        if (animationSlot == slots[i]->chr) return slots[i]->pat;
    }
    return -1;
}

static bool IsInStar(DriverController *controller) {
    return controller != nullptr && controller->pointers != nullptr &&
           controller->pointers->kartStatus != nullptr &&
           (controller->pointers->kartStatus->bitfield1 & 0x80000000) != 0;
}

static bool IsActivelyStar(DriverController *controller) {
    if (controller == nullptr || controller->pointers == nullptr) return false;

    Kart::Movement *movement = controller->pointers->kartMovement;
    if (movement != nullptr) {
        return movement->starTimer > 0;
    }

    return IsInStar(controller);
}

static bool IsActivelyMega(DriverController *controller) {
    if (controller == nullptr || controller->pointers == nullptr) return false;

    Kart::Movement *movement = controller->pointers->kartMovement;
    if (movement != nullptr) {
        return movement->megaTimer > 0;
    }

    return controller->pointers->kartStatus != nullptr &&
           (controller->pointers->kartStatus->bitfield2 & 0x8000) != 0;
}

static u32 GetMappedPatSlotForCurrentDriverAnimation(DriverController *controller) {
    if (controller == nullptr) return 0xff;

    const u8 playerIdx = controller->GetPlayerIdx();
    if (playerIdx < ONLINE_PLAYER_COUNT) {
        const PlayerAnimationState &state = GetAnimationState(playerIdx);
        const u16 currentAnimation = controller->currentAnimation;
        s16 customPat = -1;

        if (state.shockDodgeActive && state.shockDodgeStar.pat >= 0) {
            customPat = state.shockDodgeStar.pat;
        } else if (currentAnimation == WAIT_ANIM_ID && state.waitActive && IsBeforeRaceStart()) {
            customPat = state.waitBeforeStart.pat;
        } else if (currentAnimation == STAR_USE_SENTINEL_ANIM_ID) {
            customPat = GetPatIdForChrId(playerIdx, GetPowerUseAnimId(controller, playerIdx));
        } else if (currentAnimation == DAMAGE_ANIM_ID && state.shockHit.pat >= 0 && controller->pointers != nullptr &&
                   controller->pointers->kartStatus != nullptr &&
                   (controller->pointers->kartStatus->bitfield2 & 0x80) != 0) {
            customPat = state.shockHit.pat;
        }

        if (customPat >= 0) return static_cast<u32>(static_cast<u8>(customPat));
    }

    const u16 currentAnimation = controller->currentAnimation;
    if (currentAnimation >= 0x29) return 0xff;
    return *(reinterpret_cast<u8 *>(controller) + 0x19e + currentAnimation);
}

static asmFunc GetMappedPatSlotForCurrentDriverAnimationWrapper() {
    ASM(
        nofralloc;
        stwu r1, -0x20(r1);
        mflr r12;
        stw r12, 0x1C(r1);
        mr r3, r31;
        bl GetMappedPatSlotForCurrentDriverAnimation;
        mr r4, r3;
        lwz r12, 0x1C(r1);
        mtlr r12;
        addi r1, r1, 0x20;
        blr;)
}
kmCall(0x807ccfcc, GetMappedPatSlotForCurrentDriverAnimationWrapper);

static void PlayShockDodgeStarNow(DriverController *controller, u8 playerIdx) {
    if (controller == nullptr || playerIdx >= ONLINE_PLAYER_COUNT) return;
    PlayerAnimationState &state = GetAnimationState(playerIdx);
    if (state.shockDodgeStar.chr < 0) return;

    u16 *selectedAnimationPtr =
        reinterpret_cast<u16 *>(reinterpret_cast<u8 *>(controller) + 0xf6);
    *selectedAnimationPtr = static_cast<u16>(STAR_USE_SENTINEL_ANIM_ID);
    PlayCustomAnimationIfPresent(
        1.0f,
        controller,
        state.shockDodgeStar.chr,
        state.shockDodgeStar.pat,
        STAR_USE_SENTINEL_ANIM_ID,
        1);
}

static void SetTemporaryLimbLock(DriverController *controller, bool locked, LimbLockState &state) {
    u8 *const armFlag = reinterpret_cast<u8 *>(controller) + 0x14a;
    u8 *const legFlag = reinterpret_cast<u8 *>(controller) + 0x14b;

    if (locked) {
        if (!state.saved) {
            state.arm = *armFlag;
            state.leg = *legFlag;
            state.saved = true;
        }
        *armFlag = 0;
        *legFlag = 0;
        return;
    }

    if (state.saved) {
        *armFlag = state.arm;
        *legFlag = state.leg;
        state.saved = false;
    }
}

static void SetWaitBeforeStartLimbLock(DriverController *controller, u8 playerIdx, bool locked) {
    SetTemporaryLimbLock(controller, locked, GetAnimationState(playerIdx).waitLimb);
}

static void SetShockDodgeStarLimbLock(DriverController *controller, u8 playerIdx, bool locked) {
    SetTemporaryLimbLock(controller, locked, GetAnimationState(playerIdx).shockDodgeLimb);
}

static bool PlayCustomDriverAnimation(
    float blendRate, DriverController *controller, u32 animation, int param4) {
    const u8 playerIdx = controller->GetPlayerIdx();
    PlayerAnimationState *state = nullptr;
    if (playerIdx < ONLINE_PLAYER_COUNT) state = &GetAnimationState(playerIdx);

    if (state != nullptr && state->waitActive && IsBeforeRaceStart() && animation == WAIT_ANIM_ID) {
        SetWaitBeforeStartLimbLock(controller, playerIdx, true);
        return true;
    }

    if (state != nullptr && state->waitActive && IsBeforeRaceStart() && animation != WAIT_ANIM_ID &&
        animation != static_cast<u32>(static_cast<u16>(state->waitBeforeStart.chr))) {
        SetWaitBeforeStartLimbLock(controller, playerIdx, false);
        StopPatAnimation(controller, playerIdx);
        state->waitActive = false;
        state->waitFrames = 0;
        state->waitPlayed = true;
    }

    if (state != nullptr && state->shockDodgeActive && state->shockDodgeFrames > 0) {
        return true;
    }

    if (animation == DAMAGE_ANIM_ID) {
        if (state != nullptr && state->shockHit.chr >= 0) {
            if ((controller->pointers->kartStatus->bitfield2 & 0x80) != 0) {
                if (PlayCustomAnimationIfPresent(
                        blendRate,
                        controller,
                        state->shockHit.chr,
                        state->shockHit.pat,
                        DAMAGE_ANIM_ID,
                        param4)) {
                    return true;
                }
            }
        }
    }

    if (animation == STAR_USE_SENTINEL_ANIM_ID) {
        if (playerIdx < ONLINE_PLAYER_COUNT) {
            const s16 powerUseAnimId = GetPowerUseAnimId(controller, playerIdx);
            if (powerUseAnimId >= 0) {
                if (PlayCustomAnimationIfPresent(
                        blendRate,
                        controller,
                        powerUseAnimId,
                        GetPatIdForChrId(playerIdx, powerUseAnimId),
                        STAR_USE_SENTINEL_ANIM_ID,
                        param4)) {
                    return true;
                }
            }
        }
    }

    if (animation == WAIT_ANIM_ID && state != nullptr && state->waitBeforeStart.chr >= 0) {
        if (IsBeforeRaceStart() && !state->waitPlayed) {
            if (PlayCustomAnimationIfPresent(
                    blendRate,
                    controller,
                    state->waitBeforeStart.chr,
                    state->waitBeforeStart.pat,
                    WAIT_ANIM_ID,
                    param4)) {
                state->waitActive = true;
                if (!state->waitLooped) {
                    state->waitFrames = state->waitFrameCount;
                }
                SetWaitBeforeStartLimbLock(controller, playerIdx, true);
                return true;
            }
        }
    }

    return PlayerModel_setAnimation(blendRate, controller, animation, param4);
}
kmCall(0x807cb774, PlayCustomDriverAnimation);
kmCall(0x807d07c0, PlayCustomDriverAnimation);

static bool ShouldSuppressShockRotation(Kart::Damage *damage) {
    const u32 currentActionId = *reinterpret_cast<u32 *>(reinterpret_cast<u8 *>(damage) + 0x1c);
    if (currentActionId != 10 && currentActionId != 17) return false;

    const u8 playerIdx = damage->GetPlayerIdx();
    if (playerIdx < ONLINE_PLAYER_COUNT && GetAnimationState(playerIdx).shockHit.chr >= 0) {
        return true;
    }
    return false;
}

void AddSpinRotationUnlessShockHit(void *physicsHolder, Quat *rotation, Kart::Damage *damage) {
    if (ShouldSuppressShockRotation(damage)) return;

    typedef void (*AddInstantaneousExtraRotFn)(void *, Quat *);
    AddInstantaneousExtraRotFn addInstantaneousExtraRot =
        reinterpret_cast<AddInstantaneousExtraRotFn>(kmRuntimeAddr(0x8059fd0c));
    addInstantaneousExtraRot(physicsHolder, rotation);
}

static asmFunc AddSpinRotationUnlessShockHitWrapper() {
    ASM(
        nofralloc;
        mr r5, r30;
        b AddSpinRotationUnlessShockHit;)
}
kmCall(0x8056835c, AddSpinRotationUnlessShockHitWrapper);

static bool ShouldLightningAffectPlayerWithStarDodge(Item::Player *itemPlayer) {
    typedef bool (*ShouldLightningAffectPlayerFn)(Item::Player *);
    ShouldLightningAffectPlayerFn shouldLightningAffectPlayer =
        reinterpret_cast<ShouldLightningAffectPlayerFn>(kmRuntimeAddr(0x80798728));

    const bool affected = shouldLightningAffectPlayer(itemPlayer);
    if (!affected && itemPlayer != nullptr) {
        DriverController *controller = itemPlayer->model2;
        const u8 playerIdx = itemPlayer->id;
        if (playerIdx < ONLINE_PLAYER_COUNT) {
            PlayerAnimationState &state = GetAnimationState(playerIdx);
            if (state.shockDodgeStar.chr < 0 || !IsInStar(controller)) return affected;
            state.shockDodgeFrames = state.shockDodgeFrameCount;
            state.shockDodgeActive = true;
            SetShockDodgeStarLimbLock(controller, playerIdx, true);
            PlayShockDodgeStarNow(controller, playerIdx);
        }
    }
    return affected;
}
kmCall(0x807b7cd0, ShouldLightningAffectPlayerWithStarDodge);

static s16 GetPowerUseAnimId(DriverController *controller, u8 playerIdx) {
    PlayerAnimationState &state = GetAnimationState(playerIdx);
    if (IsActivelyMega(controller) && state.megaUse.chr >= 0) return state.megaUse.chr;
    if (IsActivelyStar(controller) && state.starUse.chr >= 0) return state.starUse.chr;

    return -1;
}

u32 GetPowerUseOrSelectedAnimation(DriverController *controller) {
    u16 *selectedAnimationPtr =
        reinterpret_cast<u16 *>(reinterpret_cast<u8 *>(controller) + 0xf6);
    u32 selectedAnimation = *selectedAnimationPtr;

    const u8 playerIdx = controller->GetPlayerIdx();
    if (playerIdx >= ONLINE_PLAYER_COUNT) return selectedAnimation;
    PlayerAnimationState &state = GetAnimationState(playerIdx);

    if (state.shockDodgeFrames > 0) {
        if (IsInStar(controller)) {
            SetShockDodgeStarLimbLock(controller, playerIdx, true);
            state.shockDodgeFrames--;
            if (state.shockDodgeFrames == 0) {
                state.shockDodgeActive = false;
                SetShockDodgeStarLimbLock(controller, playerIdx, false);
                if (state.starUse.chr >= 0) {
                    PlayCustomAnimationIfPresent(
                        1.0f,
                        controller,
                        state.starUse.chr,
                        state.starUse.pat,
                        STAR_USE_SENTINEL_ANIM_ID,
                        1);
                }
            }
            *selectedAnimationPtr = static_cast<u16>(STAR_USE_SENTINEL_ANIM_ID);
            return STAR_USE_SENTINEL_ANIM_ID;
        }
        state.shockDodgeFrames = 0;
        state.shockDodgeActive = false;
        SetShockDodgeStarLimbLock(controller, playerIdx, false);
    }

    if (IsBeforeRaceStart() && state.waitBeforeStart.chr >= 0 && !state.waitPlayed) {
        if (state.waitActive && !state.waitLooped) {
            if (state.waitFrames > 0) state.waitFrames--;
            if (state.waitFrames == 0) {
                controller->currentAnimation = static_cast<u16>(state.waitBeforeStart.chr);
                PlayerModel_setAnimation(1.0f, controller, WAIT_ANIM_ID, 1);
                controller->currentAnimation = static_cast<u16>(WAIT_ANIM_ID);
                SetWaitBeforeStartLimbLock(controller, playerIdx, false);
                SetPatMapping(controller, WAIT_ANIM_ID, -1);
                StopPatAnimation(controller, playerIdx);
                state.waitActive = false;
                state.waitPlayed = true;
                return selectedAnimation;
            }
        }
        if (!state.waitActive &&
            PlayCustomAnimationIfPresent(
                1.0f,
                controller,
                state.waitBeforeStart.chr,
                state.waitBeforeStart.pat,
                WAIT_ANIM_ID,
                1)) {
            state.waitActive = true;
            if (!state.waitLooped) state.waitFrames = state.waitFrameCount;
        }
        if (state.waitActive) {
            SetWaitBeforeStartLimbLock(controller, playerIdx, true);
            *selectedAnimationPtr = static_cast<u16>(WAIT_ANIM_ID);
            return WAIT_ANIM_ID;
        }
    }

    if (GetPowerUseAnimId(controller, playerIdx) >= 0) {
        *selectedAnimationPtr = static_cast<u16>(STAR_USE_SENTINEL_ANIM_ID);
        return STAR_USE_SENTINEL_ANIM_ID;
    }
    SetPatMapping(controller, STAR_USE_SENTINEL_ANIM_ID, -1);

    if (selectedAnimation == WAIT_ANIM_ID && state.waitBeforeStart.chr >= 0) {
        if (!IsBeforeRaceStart() && state.waitActive) {
            controller->currentAnimation = static_cast<u16>(state.waitBeforeStart.chr);
            PlayerModel_setAnimation(1.0f, controller, WAIT_ANIM_ID, 1);
            controller->currentAnimation = static_cast<u16>(WAIT_ANIM_ID);
            SetWaitBeforeStartLimbLock(controller, playerIdx, false);
            SetPatMapping(controller, WAIT_ANIM_ID, -1);
            StopPatAnimation(controller, playerIdx);
            state.waitActive = false;
            state.waitFrames = 0;
        }
    } else {
        if (state.waitActive && IsBeforeRaceStart()) state.waitPlayed = true;
        SetWaitBeforeStartLimbLock(controller, playerIdx, false);
        if (state.waitActive) {
            SetPatMapping(controller, WAIT_ANIM_ID, -1);
            StopPatAnimation(controller, playerIdx);
        }
        state.waitActive = false;
        state.waitFrames = 0;
    }
    return selectedAnimation;
}

static asmFunc SelectPowerUseAnimation() {
    ASM(
        nofralloc;
        stwu r1, -0x50(r1);
        mfcr r12;
        stw r12, 0x8(r1);
        mflr r12;
        stw r12, 0xC(r1);
        stw r3, 0x10(r1);
        stw r4, 0x14(r1);
        stw r5, 0x18(r1);
        stw r6, 0x1C(r1);
        stw r7, 0x20(r1);
        stw r8, 0x24(r1);
        stw r9, 0x28(r1);
        stw r10, 0x2C(r1);
        stw r11, 0x30(r1);
        mr r3, r31;
        bl GetPowerUseOrSelectedAnimation;
        mr r0, r3;
        lwz r3, 0x10(r1);
        lwz r4, 0x14(r1);
        lwz r5, 0x18(r1);
        lwz r6, 0x1C(r1);
        lwz r7, 0x20(r1);
        lwz r8, 0x24(r1);
        lwz r9, 0x28(r1);
        lwz r10, 0x2C(r1);
        lwz r11, 0x30(r1);
        lwz r12, 0xC(r1);
        mtlr r12;
        lwz r12, 0x8(r1);
        mtcr r12;
        addi r1, r1, 0x50;
        blr;)
}
kmCall(0x807cd2dc, SelectPowerUseAnimation);

kmCall(0x807d1ba4, PlayCustomDriverAnimation);
kmCall(0x807cc4e0, PlayCustomDriverAnimation);
kmCall(0x807cf784, PlayCustomDriverAnimation);
kmCall(0x807cfbd4, PlayCustomDriverAnimation);
kmCall(0x807cfd40, PlayCustomDriverAnimation);
kmCall(0x807cffd8, PlayCustomDriverAnimation);
kmCall(0x807d02f8, PlayCustomDriverAnimation);
kmCall(0x807d1a48, PlayCustomDriverAnimation);

}  // namespace CustomCharacters
}  // namespace Pulsar
