#include <runtimeWrite.hpp>
#include <MarioKartWii/Race/Racedata.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <PulsarSystem.hpp>

namespace Pulsar {
namespace LapKO {

// Separate state for LapKO to avoid coupling to BattleElim globals
extern "C" volatile void* gLapKOStorePtr = 0;

// Replicate the BattleElim ForceInvisible pattern but use our LapKO store pointer
asmFunc ForceInvisible_LapKO() {
    ASM(
        nofralloc;
        mflr r11;
        mr r12, r3;
        lis r9, gLapKOStorePtr @ha;
        lwz r9, gLapKOStorePtr @l(r9);
        cmpwi r9, 0;
        beq do_original;
        lwz r10, 0x48(r9);
        cmpwi r10, 0;
        bne do_original;
        bl loc_after;
        cmpw cr7, r0, r0;
        cmpw cr7, r0, r0;
        cmpw cr7, r0, r0;
        loc_after : mflr r3;
        lfs f0, 0(r3);
        mtlr r11;
        blr;
        do_original : lfs f0, 0(r12);
        mtlr r11;
        blr;)
}

// Hook points also used by BattleElim (see BattleElim.cpp for provenance)
kmRuntimeUse(0x8058CB7C);  // ForceInvisible hook site
static void ApplyLapKOInvisibility() {
    kmRuntimeWrite32A(0x8058CB7C, 0xc0030000);  // lfs f0, 0(r3)

    const System* system = System::sInstance;
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    if (system == nullptr || controller == nullptr) return;

    const bool isLapKO = system->IsContext(PULSAR_MODE_LAPKO);
    const bool isFroom = (controller->roomType == RKNet::ROOMTYPE_FROOM_HOST || controller->roomType == RKNet::ROOMTYPE_FROOM_NONHOST);
    if (!(isLapKO && isFroom)) return;

    // Apply runtime hooks to force invisibility for eliminated players during online LapKO
    kmRuntimeCallA(0x8058CB7C, ForceInvisible_LapKO);
}

// Apply on page load so hooks are installed once per race scene load
static RaceLoadHook sLapKOInvisHook(ApplyLapKOInvisibility);

}  // namespace LapKO
}  // namespace Pulsar
