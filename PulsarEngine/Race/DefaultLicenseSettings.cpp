#include <kamek.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/RKSYS/RKPD.hpp>

namespace Pulsar {
namespace Race {

static void SetNewLicenseVSDefaults(RKSYS::RKPD *rkpd) {
    // RKPD rules 0 and 2 are single-player and multiplayer VS respectively.
    rkpd->rules[0].cc = CC_150;
    rkpd->rules[2].cc = CC_150;
}

static asmFunc SetNewLicenseVSDefaultsWrapper() {
    ASM(
        nofralloc;
        mflr r0;
        stw r0, 0x8(sp);
        mr r3, r31;
        bl SetNewLicenseVSDefaults;
        lwz r0, 0x8(sp);
        mtlr r0;
        add r6, r27, r26;
        blr;)
}
kmCall(0x80548070, SetNewLicenseVSDefaultsWrapper);

}  // namespace Race
}  // namespace Pulsar
