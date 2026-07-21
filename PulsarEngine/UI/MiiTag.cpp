#include <RetroRewind.hpp>
#include <hooks.hpp>

namespace Pulsar {
namespace UI {

// Displays the nametag as the color of the Mii [Conradi]
extern "C" void sInstance__8Racedata(void*);
extern "C" u8 sUseMiiTagColor = false;
asmFunc MiiTag() {
    ASM(
        nofralloc;
        lis r12, sUseMiiTagColor @ha;
        lbz r12, sUseMiiTagColor @l(r12);
        cmpwi r12, 0;
        beq original;
        lis r12, sInstance__8Racedata @ha;
        lwz r12, sInstance__8Racedata @l(r12);
        mulli r11, r30, 0xF0;
        addi r11, r11, 0x28;
        add r12, r12, r11;
        lwz r11, 0x74(r12);
        stw r11, 0x14(r1);
        stw r11, 0x2C(r1);
        lwz r28, 0x2C(r1);
        blr;
        original :;
        lwz r28, 0x2C(r1);
        blr;)
}

void PatchMiiTag() {
    u32 tagColor = static_cast<Pulsar::RaceSettingNAMETAG>(Pulsar::Settings::Mgr::Get().GetUserSettingValue(static_cast<Pulsar::Settings::UserType>(Pulsar::Settings::SETTINGSTYPE_RACE2), Pulsar::RADIO_NAMETAG));
    sUseMiiTagColor = tagColor == Pulsar::NAMETAG_MII;
}
static SectionLoadHook MiiTagHook(PatchMiiTag);
kmCall(0x807F042C, MiiTag);

}  // namespace UI
}  // namespace Pulsar
