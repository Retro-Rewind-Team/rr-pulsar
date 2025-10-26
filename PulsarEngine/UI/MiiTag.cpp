#include <RetroRewind.hpp>
#include <runtimeWrite.hpp>

namespace Pulsar {
namespace UI {

// Displays the nametag as the color of the Mii [Conradi]
extern "C" void sInstance__8Racedata(void*);
asmFunc MiiTag() {
    ASM(
        nofralloc;
        lis r12, sInstance__8Racedata @ha;
        lwz r12, sInstance__8Racedata @l(r12);
        mulli r11, r30, 0xF0;
        addi r11, r11, 0x28;
        add r12, r12, r11;
        lwz r11, 0x74(r12);
        stw r11, 0x14(r1);
        stw r11, 0x2C(r1);
        lwz r28, 0x2C(r1);
        blr;)
}

kmRuntimeUse(0x807F042C);
void PatchMiiTag() {
    kmRuntimeWrite32A(0x807F042C, 0x8381002C);
    u32 tagColor = static_cast<Pulsar::RaceSettingNAMETAG>(Pulsar::Settings::Mgr::Get().GetUserSettingValue(static_cast<Pulsar::Settings::UserType>(Pulsar::Settings::SETTINGSTYPE_RACE2), Pulsar::RADIO_NAMETAG));
    if (tagColor == Pulsar::NAMETAG_MII) {
        kmRuntimeCallA(0x807F042C, MiiTag);
    }
}
static SectionLoadHook MiiTagHook(PatchMiiTag);

}  // namespace UI
}  // namespace Pulsar
