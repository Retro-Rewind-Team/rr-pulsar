#include <RetroRewind.hpp>
#include <Patching/RuntimeChoice.hpp>

namespace Pulsar {
namespace UI {

// Displays the nametag as the color of the Mii [Conradi]
extern "C" void sInstance__8Racedata(void*);
static u32 sUseMiiTagColor = 0;

static bool UseMiiTagColor() {
    u32 tagColor = static_cast<Pulsar::RaceSettingNAMETAG>(Pulsar::Settings::Mgr::Get().GetUserSettingValue(static_cast<Pulsar::Settings::UserType>(Pulsar::Settings::SETTINGSTYPE_RACE2), Pulsar::RADIO_NAMETAG));
    return tagColor == Pulsar::NAMETAG_MII;
}

static void UpdateMiiTagColorMode() {
    sUseMiiTagColor = UseMiiTagColor() ? 1 : 0;
}
static SectionLoadHook UpdateMiiTagColorModeHook(UpdateMiiTagColorMode);

asmFunc MiiTag() {
    ASM(
        nofralloc;
        stwu r1, -0x20(r1);
        stw r12, 0x8(r1);
        mfcr r12;
        stw r12, 0xC(r1);

        lis r12, sUseMiiTagColor @ha;
        lwz r12, sUseMiiTagColor @l(r12);
        cmpwi r12, 0;
        bne useMiiColor;

        RuntimeChoice_RestoreScratchAndCR();
        lwz r28, 0x2C(r1);
        blr;

        useMiiColor:;
        RuntimeChoice_RestoreScratchAndCR();
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
kmCall(0x807F042C, MiiTag);

}  // namespace UI
}  // namespace Pulsar
