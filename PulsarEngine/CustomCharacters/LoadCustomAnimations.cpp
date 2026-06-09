#include <CustomCharacters/CustomCharacters.hpp>
#include <MarioKartWii/Kart/KartPointers.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>

/*
    When a driver BRRES contains a CHR animation named "shockHit", it is linked
    at runtime and played in place of the vanilla "damage" animation whenever the
    player is struck by lightning (the Shock item).  If the animation is absent the
    vanilla "damage" animation is used as a fallback, so the feature is opt-in per
    character skin.  Additionally, when the animation IS present, the physical
    spinout rotation is suppressed while all other shock effects — SHOCKED state,
    speed reduction, kart shrinking, item loss — are kept unchanged.

    When a driver BRRES contains a CHR animation named "starUse", it is linked the
    same way and used as the selected body animation while the player is actively
    in Star state.  If absent, the vanilla selected animation is used as normal.

    "megaUse" follows the same behavior while the player is actively in Mega
    Mushroom state.

    "waitBeforeStart" is linked the same way and plays instead of vanilla "wait"
    only before Raceinfo's race-start timer says the race has started.  Once the
    race starts, vanilla "wait" is used again.

    Hook summary
    ────────────
    LinkCustomCharacterAnimations
                            –  hooked at 0x807c7894 (call to FUN_805778ac inside
                               DriverController::LoadModels).  After the original
                               model-setup runs, it scans the driver BRRES for the
                               optional custom animations, links them at the next
                               free animation slots, and records their slot indices.

    PlayShockHitOnShockDamage  –  hooked at both known DAMAGE_ call-sites:
                               0x807cb774  (FUN_807cb530 – main local-player update)
                               0x807d07c0  (FUN_807d0744 – kart/bike vtable handler)
                               When animation == DAMAGE_ (0x1a) and the player is in
                               the SHOCKED state (KartStatus::bitfield2 bit 7), it
                               redirects to the "shockHit" slot.  currentAnimation is
                               then reset to DAMAGE_ so the rest of the state machine
                               (end-state, re-queue logic, etc.) behaves identically
                               to the vanilla path.

    AddSpinRotationUnlessShockHit
                            –  hooked at 0x8056835c (the extra-rotation apply call
                               inside the spin damage update).  For shock actions
                               only, if "shockHit" is present for the player, it skips
                               applying the visual extra rotation to the kart/bike.
                               The action counters and completion logic still run
                               unchanged.

    SelectPowerUseAnimation –  hooked at 0x807cd2dc (late in FUN_807cc174, where
                               the selected body animation at DriverController+0xf6
                               is loaded into r0 before deciding whether to play it).
                               When the player is actively in Star or Mega state, it
                               returns vanilla slot 0x8 as a safe state-machine
                               sentinel and writes that sentinel back to
                               DriverController+0xf6, matching the proven hardcoded
                               hook.

    PlayCustomSelectedAnimation
                            –  hooked at 0x807d1ba4 (the selected-animation
                               PlayerModel_setAnimation call in FUN_807d1a60).
                               When that sentinel animation is requested while in
                               Star or Mega state, it plays the linked custom slot
                               ("starUse" or "megaUse") and restores currentAnimation
                               to 0x8 so vanilla animation metadata lookups remain
                               in range.  Before the race-start timer has fired,
                               vanilla WAIT (0x7) is similarly redirected to
                               "waitBeforeStart" and then restored to WAIT for
                               state-machine bookkeeping.
*/

namespace Pulsar {
namespace CustomCharacters {

kmRuntimeUse(0x8059fd0c);

// CharacterAnimationId value for the vanilla body "damage" animation.
static const u32 DAMAGE_ANIM_ID = 0x1a;
// CharacterAnimationId value for the vanilla body "wait" animation.
static const u32 WAIT_ANIM_ID = 0x7;
// Safe vanilla animation id used by the proven Star-state hook.
static const u32 STAR_USE_SENTINEL_ANIM_ID = 0x8;

// Per-player slot index of the linked "shockHit" animation.
// Initialised to -1 (= not present) for every slot.
static s16 shockHitAnimId[ONLINE_PLAYER_COUNT] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
};
static s16 starUseAnimId[ONLINE_PLAYER_COUNT] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
};
static s16 megaUseAnimId[ONLINE_PLAYER_COUNT] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
};
static s16 waitBeforeStartAnimId[ONLINE_PLAYER_COUNT] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
};
static bool waitBeforeStartActive[ONLINE_PLAYER_COUNT] = {
    false, false, false, false, false, false, false, false, false, false, false, false
};
static bool waitBeforeStartLimbFlagsSaved[ONLINE_PLAYER_COUNT] = {
    false, false, false, false, false, false, false, false, false, false, false, false
};
static u8 waitBeforeStartArmFlags[ONLINE_PLAYER_COUNT] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};
static u8 waitBeforeStartLegFlags[ONLINE_PLAYER_COUNT] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

// ── Game-function declarations resolved by symbols.txt ────────────────────────

// FUN_805778ac: sets up the ModelDirectors for a DriverController and links all
// standard animations.  Called inside DriverController::LoadModels (807c7828).
extern "C" void DriverController_SetupModelAnims(
    g3d::ResFile* brresArray, ModelDirector** pDriver,
    ModelDirector** pDriverLod, DriverController* controller);

// 807cc018: plays animation <animation> on the driver model with optional blending.
extern "C" bool PlayerModel_setAnimation(
    float blendRate, DriverController* controller, u32 animation, int param4);

static s16 LinkOptionalChrAnimation(
    DriverController* controller, ModelDirector* driverModel, const char* animationName)
{
    if (controller->driverModelBRRES.GetResAnmChr(animationName).data == nullptr) return -1;

    // The ModelTransformator is stored as a pointer at driverModel + 0x28.
    // Confirmed from game disassembly (see e.g. 807c7c54–807c7c60).
    ModelTransformator* transformator =
        *reinterpret_cast<ModelTransformator**>(
            reinterpret_cast<u8*>(driverModel) + 0x28);
    if (transformator == nullptr) return -1;

    // anmHolderList.count equals the number of animations already linked,
    // which is the index of the next free slot.
    const u16 nextSlot = transformator->anmHolderList.count;

    // Link the custom animation as a CHR animation at the next consecutive slot.
    // Parameters mirror what CreateModelDirectors uses for standard CHR anims.
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

// ── Hook 1: link optional custom CHR animations after standard animations ──────

static void LinkCustomCharacterAnimations(
    g3d::ResFile* brresArray, ModelDirector** pDriver,
    ModelDirector** pDriverLod, DriverController* controller)
{
    // Always run the original model-setup first.
    DriverController_SetupModelAnims(brresArray, pDriver, pDriverLod, controller);

    const u8 playerIdx = controller->GetPlayerIdx();
    if (playerIdx >= ONLINE_PLAYER_COUNT) return;

    // Reset for this player; will be filled in below if found.
    shockHitAnimId[playerIdx] = -1;
    starUseAnimId[playerIdx] = -1;
    megaUseAnimId[playerIdx] = -1;
    waitBeforeStartAnimId[playerIdx] = -1;
    waitBeforeStartActive[playerIdx] = false;
    waitBeforeStartLimbFlagsSaved[playerIdx] = false;

    // CPU players use a stripped model without the full animation set; skip.
    if (controller->isCpu) return;

    // driverModelBRRES.data is set by CreateModelDirectors to point at the
    // raw BRRES data (same pointer as brresArray[0].data).
    ModelDirector* driverModel = controller->driverModel;
    if (driverModel == nullptr) return;

    shockHitAnimId[playerIdx] = LinkOptionalChrAnimation(controller, driverModel, "shockHit");
    starUseAnimId[playerIdx] = LinkOptionalChrAnimation(controller, driverModel, "starUse");
    megaUseAnimId[playerIdx] = LinkOptionalChrAnimation(controller, driverModel, "megaUse");
    waitBeforeStartAnimId[playerIdx] =
        LinkOptionalChrAnimation(controller, driverModel, "waitBeforeStart");
}
// Replace the call to FUN_805778ac inside DriverController::LoadModels.
kmCall(0x807c7894, LinkCustomCharacterAnimations);

static bool PlayCustomAnimationIfPresent(
    float blendRate, DriverController* controller, s16 animationSlot, u32 vanillaAnimation, int param4)
{
    const u32 customAnimation = static_cast<u32>(static_cast<u16>(animationSlot));
    const bool played = PlayerModel_setAnimation(blendRate, controller, customAnimation, param4);

    if (played) {
        controller->currentAnimation = static_cast<u16>(vanillaAnimation);
        return true;
    }
    return false;
}

static bool IsBeforeRaceStart()
{
    Raceinfo* raceInfo = Raceinfo::sInstance;
    return raceInfo != nullptr && raceInfo->timerMgr != nullptr &&
        (!raceInfo->timerMgr->hasRaceStarted || !raceInfo->IsAtLeastStage(RACESTAGE_RACE));
}

static s16 GetPowerUseAnimId(DriverController* controller, u8 playerIdx);

static void SetWaitBeforeStartLimbLock(DriverController* controller, u8 playerIdx, bool locked)
{
    u8* const armFlag = reinterpret_cast<u8*>(controller) + 0x14a;
    u8* const legFlag = reinterpret_cast<u8*>(controller) + 0x14b;

    if (locked) {
        if (!waitBeforeStartLimbFlagsSaved[playerIdx]) {
            waitBeforeStartArmFlags[playerIdx] = *armFlag;
            waitBeforeStartLegFlags[playerIdx] = *legFlag;
            waitBeforeStartLimbFlagsSaved[playerIdx] = true;
        }
        *armFlag = 0;
        *legFlag = 0;
        return;
    }

    if (waitBeforeStartLimbFlagsSaved[playerIdx]) {
        *armFlag = waitBeforeStartArmFlags[playerIdx];
        *legFlag = waitBeforeStartLegFlags[playerIdx];
        waitBeforeStartLimbFlagsSaved[playerIdx] = false;
    }
}

// ── Hook 2: redirect DAMAGE_ → "shockHit" when the player is shocked ──────────

static bool PlayCustomDriverAnimation(
    float blendRate, DriverController* controller, u32 animation, int param4)
{
    if (animation == DAMAGE_ANIM_ID) {
        const u8 playerIdx = controller->GetPlayerIdx();
        if (playerIdx < ONLINE_PLAYER_COUNT && shockHitAnimId[playerIdx] >= 0) {
            // KartStatus::bitfield2 bit 7 (0x80) = SHOCKED
            if ((controller->pointers->kartStatus->bitfield2 & 0x80) != 0) {
                if (PlayCustomAnimationIfPresent(
                        blendRate, controller, shockHitAnimId[playerIdx], DAMAGE_ANIM_ID, param4)) {
                    // Restore currentAnimation to DAMAGE_ so the rest of the
                    // animation state machine (end-state detection, transition
                    // back to DRIVE, etc.) continues to work correctly.
                    return true;
                }
                // Fallthrough: "shockHit" slot was empty/invalid; use vanilla.
            }
        }
    }

    const u8 playerIdx = controller->GetPlayerIdx();

    if (animation == STAR_USE_SENTINEL_ANIM_ID) {
        if (playerIdx < ONLINE_PLAYER_COUNT) {
            const s16 powerUseAnimId = GetPowerUseAnimId(controller, playerIdx);
            if (powerUseAnimId >= 0) {
                if (PlayCustomAnimationIfPresent(
                        blendRate,
                        controller,
                        powerUseAnimId,
                        STAR_USE_SENTINEL_ANIM_ID,
                        param4)) {
                    return true;
                }
                // Fallthrough: custom slot was empty/invalid; use vanilla.
            }
        }
    }

    if (animation == WAIT_ANIM_ID && playerIdx < ONLINE_PLAYER_COUNT &&
        waitBeforeStartAnimId[playerIdx] >= 0) {
        if (IsBeforeRaceStart()) {
            if (PlayCustomAnimationIfPresent(
                    blendRate,
                    controller,
                    waitBeforeStartAnimId[playerIdx],
                    WAIT_ANIM_ID,
                    param4)) {
                waitBeforeStartActive[playerIdx] = true;
                SetWaitBeforeStartLimbLock(controller, playerIdx, true);
                return true;
            }
            // Fallthrough: custom slot was empty/invalid; use vanilla.
        }
    }

    return PlayerModel_setAnimation(blendRate, controller, animation, param4);
}
// FUN_807cb530 (main local-player model update): DAMAGE_ play-call @ 807cb774
kmCall(0x807cb774, PlayCustomDriverAnimation);
// FUN_807d0744 (kart/bike vtable animation handler): DAMAGE_ play-call @ 807d07c0
kmCall(0x807d07c0, PlayCustomDriverAnimation);

// ── Hook 2b: suppress visible Shock spin when "shockHit" exists ──────────────

static bool ShouldSuppressShockRotation(Kart::Damage* damage)
{
    const u32 currentActionId = *reinterpret_cast<u32*>(reinterpret_cast<u8*>(damage) + 0x1c);
    if (currentActionId != 10 && currentActionId != 17) return false;

    const u8 playerIdx = damage->GetPlayerIdx();
    if (playerIdx < ONLINE_PLAYER_COUNT && shockHitAnimId[playerIdx] >= 0) {
        return true;
    }
    return false;
}

void AddSpinRotationUnlessShockHit(void* physicsHolder, Quat* rotation, Kart::Damage* damage)
{
    if (ShouldSuppressShockRotation(damage)) return;

    typedef void (*AddInstantaneousExtraRotFn)(void*, Quat*);
    AddInstantaneousExtraRotFn addInstantaneousExtraRot =
        reinterpret_cast<AddInstantaneousExtraRotFn>(kmRuntimeAddr(0x8059fd0c));
    addInstantaneousExtraRot(physicsHolder, rotation);
}

static asmFunc AddSpinRotationUnlessShockHitWrapper() {
    ASM(
        nofralloc;
        mr r5, r30;
        b AddSpinRotationUnlessShockHit;
    )
}
// Kart::Damage spin update: KartPhysicsHolder::addInstantaneousExtraRot call
kmCall(0x8056835c, AddSpinRotationUnlessShockHitWrapper);

static s16 GetPowerUseAnimId(DriverController* controller, u8 playerIdx)
{
    // KartStatus::bitfield1 bit 31 (0x80000000) = in a star.
    if ((controller->pointers->kartStatus->bitfield1 & 0x80000000) != 0 &&
        starUseAnimId[playerIdx] >= 0) {
        return starUseAnimId[playerIdx];
    }

    // KartStatus::bitfield2 bit 15 (0x8000) = in a mega.
    if ((controller->pointers->kartStatus->bitfield2 & 0x8000) != 0 &&
        megaUseAnimId[playerIdx] >= 0) {
        return megaUseAnimId[playerIdx];
    }

    return -1;
}

// ── Hook 3: use custom selected animations in active special states ─────────

u32 GetPowerUseOrSelectedAnimation(DriverController* controller)
{
    u16* selectedAnimationPtr =
        reinterpret_cast<u16*>(reinterpret_cast<u8*>(controller) + 0xf6);
    const u32 selectedAnimation = *selectedAnimationPtr;

    const u8 playerIdx = controller->GetPlayerIdx();
    if (playerIdx >= ONLINE_PLAYER_COUNT) return selectedAnimation;

    if (GetPowerUseAnimId(controller, playerIdx) >= 0) {
        *selectedAnimationPtr = static_cast<u16>(STAR_USE_SENTINEL_ANIM_ID);
        return STAR_USE_SENTINEL_ANIM_ID;
    }

    if (selectedAnimation == WAIT_ANIM_ID && waitBeforeStartAnimId[playerIdx] >= 0) {
        if (IsBeforeRaceStart()) {
            if (!waitBeforeStartActive[playerIdx] &&
                PlayCustomAnimationIfPresent(
                    1.0f, controller, waitBeforeStartAnimId[playerIdx], WAIT_ANIM_ID, 1)) {
                waitBeforeStartActive[playerIdx] = true;
            }
            if (waitBeforeStartActive[playerIdx]) {
                SetWaitBeforeStartLimbLock(controller, playerIdx, true);
            }
        } else if (waitBeforeStartActive[playerIdx]) {
            controller->currentAnimation = static_cast<u16>(waitBeforeStartAnimId[playerIdx]);
            PlayerModel_setAnimation(1.0f, controller, WAIT_ANIM_ID, 1);
            controller->currentAnimation = static_cast<u16>(WAIT_ANIM_ID);
            SetWaitBeforeStartLimbLock(controller, playerIdx, false);
            waitBeforeStartActive[playerIdx] = false;
        }
    } else {
        SetWaitBeforeStartLimbLock(controller, playerIdx, false);
        waitBeforeStartActive[playerIdx] = false;
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
        blr;
    )
}
// FUN_807cc174: replaces "lhz r0, 0xf6(r31)" before the selected-animation check.
kmCall(0x807cd2dc, SelectPowerUseAnimation);

// FUN_807d1a60: selected-animation PlayerModel_setAnimation call @ 807d1ba4
kmCall(0x807d1ba4, PlayCustomDriverAnimation);
kmCall(0x807cc4e0, PlayCustomDriverAnimation);
kmCall(0x807cf784, PlayCustomDriverAnimation);
kmCall(0x807cfbd4, PlayCustomDriverAnimation);
kmCall(0x807cfd40, PlayCustomDriverAnimation);
kmCall(0x807cffd8, PlayCustomDriverAnimation);
kmCall(0x807d02f8, PlayCustomDriverAnimation);

}  // namespace CustomCharacters
}  // namespace Pulsar
