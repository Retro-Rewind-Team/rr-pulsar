#include <kamek.hpp>
#include <MarioKartWii/KMP/KMPManager.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <MarioKartWii/UI/Ctrl/Menu/CtrlMenuText.hpp>
#include <UI/ChangeCombo/ChangeCombo.hpp>

namespace Pulsar {
namespace Race {

#ifdef RR_TESTS
#ifdef INSTANT_FINISH

// Force the race manager to enter the one-frame finishing state instead of the
// active race state as soon as the race starts.
kmWrite32(0x805334e4, 0x38000003);
kmWrite32(0x80533564, 0x38000003);

#endif

#ifdef TIMER
// Force CountDown::SetInitial(float) to initialize every countdown to 10.0f.
kmWrite32(0x805c3c2c, 0x3c804080);
kmWrite32(0x805c3c34, 0x90830000);
#endif

#ifdef RANDOM
// Process locally submitted online course votes as Random.
kmWrite32(0x80643740, 0x38c000ff);
kmWrite32(0x80643794, 0x38c000ff);
#endif

#ifdef ONE_LAP
// Force the loaded track and racedata lap counters to one lap before
// RaceinfoPlayer objects are initialized.
static void SetOneLapRace() {
    KMP::Manager* kmp = KMP::Manager::sInstance;
    if (kmp != nullptr && kmp->stgiSection != nullptr && kmp->stgiSection->holdersArray != nullptr &&
        kmp->stgiSection->holdersArray[0] != nullptr && kmp->stgiSection->holdersArray[0]->raw != nullptr) {
        kmp->stgiSection->holdersArray[0]->raw->lapCount = 1;
    }

    Racedata* racedata = Racedata::sInstance;
    if (racedata == nullptr) return;
    racedata->racesScenario.settings.lapCount = 1;
    racedata->menusScenario.settings.lapCount = 1;
}

static Raceinfo* CreateOneLapRaceInfo() {
    SetOneLapRace();
    return Raceinfo::CreateInstance();
}
kmCall(0x805543cc, CreateOneLapRaceInfo);
#endif

#ifdef RANDOM_COMBO
// Select ExpVR::randomComboButton as the VR page's default button after it has
// been added to the control group.
static void SetVRBottomMessageAndSelectRandom(CtrlMenuInstructionText* bottomMessage, u32 bmgId, const Text::Info* text) {
    bottomMessage->SetMessage(bmgId, text);

    static const u32 VR_BOTTOM_MESSAGE_OFFSET = 0x3f0;
    UI::ExpVR* page = reinterpret_cast<UI::ExpVR*>(reinterpret_cast<u8*>(bottomMessage) - VR_BOTTOM_MESSAGE_OFFSET);

    if (page->controlGroup.controlCount <= 0xf) return;
    PushButton& randomButton = page->GetRandomComboButton();
    if (page->controlGroup.GetControl(0xf) != &randomButton || randomButton.isHidden) return;

    randomButton.Select(0);
}
kmCall(0x8064aaac, SetVRBottomMessageAndSelectRandom);
#endif

// CPU
kmWrite32(0x8052F564, 0x60000000);
kmWrite32(0x8072627C, 0x38600001);
kmWrite32(0x80590670, 0x70030003);
kmWrite32(0x8057BD08, 0x38600000);

#endif

}  // namespace Race
}  // namespace Pulsar
