#include <kamek.hpp>
#include <MarioKartWii/UI/Page/Menu/SinglePlayer.hpp>
#include <MarioKartWii/Item/ItemManager.hpp>
#include <PulsarSystem.hpp>
#include <UI/UI.hpp>
#include <Settings/UI/SettingsPanel.hpp>
#include <Settings/UI/SettingsPageSelect.hpp>
// Implements 4 TT modes by splitting the "Time Trials" button

namespace Pulsar {
static void SetCC();
namespace UI {

static const u32 missionButtonVanillaId = 4;

static inline u32 GetMissionButtonId(const Pages::SinglePlayer* page) {
    return page->externControlCount - 2;
}

static inline u32 GetSettingsButtonId(const Pages::SinglePlayer* page) {
    return page->externControlCount - 1;
}

static inline u32 GetTTExtraButtonCount(const Pages::SinglePlayer* page) {
    return page->externControlCount - 6;
}

static inline bool IsMissionButton(const Pages::SinglePlayer* page, u32 id) {
    return id == GetMissionButtonId(page);
}

static inline bool IsBTMRModeButton(const Pages::SinglePlayer* page, u32 id) {
    return id == 3 || IsMissionButton(page, id);
}

static inline bool IsSettingsButton(const Pages::SinglePlayer* page, u32 id) {
    return id == GetSettingsButtonId(page);
}

static inline bool IsTTModeButton(const Pages::SinglePlayer* page, u32 id) {
    return id == 1 || (id > 3 && id < GetMissionButtonId(page));
}

static inline u32 GetBTMRModeButtonBMG(const Pages::SinglePlayer* page, u32 id) {
    return IsMissionButton(page, id) ? BMG_MISSION_MODE_BUTTON : BMG_BATTLE_MODE_BUTTON;
}

void CorrectButtonCount(Pages::SinglePlayer* page) {
    const System* system = System::sInstance;
    const bool hasFeather = system->GetInfo().HasFeather();
    const bool has200cc = system->GetInfo().Has200cc();
    page->externControlCount = 4 + hasFeather + has200cc + (hasFeather && has200cc) + 2;
    new (page) Page;
}
kmCall(0x806266b8, CorrectButtonCount);
kmWrite32(0x806266d4, 0x60000000);

UIControl* CreateExternalControls(Pages::SinglePlayer* page, u32 id) {
    if (IsSettingsButton(page, id)) {
        PushButton* button = new (PushButton);
        page->AddControl(page->controlCount++, *button, 0);
        const char* name = "Settings1P";
        button->Load(UI::buttonFolder, name, name, page->activePlayerBitfield, 0, false);
        return button;
    }
    return page->Pages::SinglePlayer::CreateExternalControl(id);
}
kmWritePointer(0x808D9F84, CreateExternalControls);

static void LoadCorrectBRCTR(PushButton& button, const char* folder, const char* ctr, const char* variant, u32 localPlayerField) {
    register int idx;
    asm(mr idx, r28;);
    Pages::SinglePlayer* page = button.parentGroup->GetParentPage<Pages::SinglePlayer>();
    const System* system = System::sInstance;

    u32 varId = 0;
    const u32 ttExtraButtonCount = GetTTExtraButtonCount(page);
    if (IsBTMRModeButton(page, idx)) {
        ctr = "PulBTMRTwo";
        if (idx != 3) varId = 1;
        char btmrVariant[0x15];
        snprintf(btmrVariant, 0x15, "%s_%d", ctr, varId);
        variant = btmrVariant;
    } else if (ttExtraButtonCount > 0 && IsTTModeButton(page, idx)) {
        switch (ttExtraButtonCount) {
            case (1):
                ctr = "PulTTTwo";
                if (idx != 1) {
                    if (system->GetInfo().Has200cc())
                        varId = 1;
                    else
                        varId = 2;
                }
                break;
            case (3):
                ctr = "PulTTFour";
                if (idx != 1) varId = idx - 3;
                break;
        }
        char ttVariant[0x15];
        snprintf(ttVariant, 0x15, "%s_%d", ctr, varId);
        variant = ttVariant;
    }

    button.Load(folder, ctr, variant, localPlayerField, 0, false);
    if (IsBTMRModeButton(page, idx)) button.SetMessage(GetBTMRModeButtonBMG(page, idx));
    page->curMovieCount = 0;
}
kmCall(0x8084f084, LoadCorrectBRCTR);

static int FixCalcDistance(const ControlManipulator& subject, const ControlManipulator& other, Directions direction) {
    const s32 subId = static_cast<PushButton*>(subject.actionHandlers[0]->subject)->buttonId;
    const s32 destId = static_cast<PushButton*>(other.actionHandlers[0]->subject)->buttonId;
    const Pages::SinglePlayer* page = static_cast<PushButton*>(subject.actionHandlers[0]->subject)->parentGroup->GetParentPage<Pages::SinglePlayer>();
    const s32 settingsButtonId = GetSettingsButtonId(page);

    if (subId == 0 && direction == DIRECTION_DOWN && IsTTModeButton(page, destId)) return 1;
    if (subId == 2 && direction == DIRECTION_UP && IsTTModeButton(page, destId)) return 1;
    if (IsTTModeButton(page, subId) && (direction == DIRECTION_UP && destId == 0 || direction == DIRECTION_DOWN && destId == 2)) return 1;

    if (subId == 2 && direction == DIRECTION_DOWN && IsBTMRModeButton(page, destId)) return 1;
    if (subId == settingsButtonId && direction == DIRECTION_UP && IsBTMRModeButton(page, destId)) return 1;
    if (IsBTMRModeButton(page, subId) && (direction == DIRECTION_UP && destId == 2 || direction == DIRECTION_DOWN && destId == settingsButtonId)) return 1;

    return subject.CalcDistanceBothWrapping(other, direction);
}

static void SetDistanceFunc(ControlsManipulatorManager& mgr) {
    mgr.distanceFunc = &FixCalcDistance;
}
kmCall(0x8084ef68, SetDistanceFunc);

void OnButtonSelect(Pages::SinglePlayer* page, PushButton& button, u32 hudSlotId) {
    const s32 id = button.buttonId;
    if (IsMissionButton(page, id)) {
        button.buttonId = missionButtonVanillaId;
        page->Pages::SinglePlayer::OnExternalButtonSelect(button, hudSlotId);
        button.buttonId = id;
        page->bottomText->SetMessage(BMG_MISSION_MODE_BOTTOM);
    } else if (IsSettingsButton(page, id)) {
        page->bottomText->SetMessage(BMG_SETTINGSBUTTON_BOTTOM);
    } else if (GetTTExtraButtonCount(page) > 0 && IsTTModeButton(page, id)) {
        button.buttonId = 1;
        page->Pages::SinglePlayer::OnExternalButtonSelect(button, hudSlotId);
        button.buttonId = id;
        u32 bmgId = BMG_TT_MODE_BOTTOM_SINGLE;
        const System* system = System::sInstance;
        switch (GetTTExtraButtonCount(page)) {
            case (1):
                if (id > 3) {
                    if (system->GetInfo().Has200cc())
                        bmgId += 1;
                    else
                        bmgId += 2;
                }
                break;
            case (3):
                if (id > 3) bmgId = bmgId + id - 3;
                break;
        }
        page->bottomText->SetMessage(bmgId);
    } else
        page->Pages::SinglePlayer::OnExternalButtonSelect(button, hudSlotId);
}
kmWritePointer(0x808D9F64, &OnButtonSelect);

// Sets the ttMode based on which button was clicked
void OnButtonClick(Pages::SinglePlayer* page, PushButton& button, u32 hudSlotId) {
    const u32 id = button.buttonId;
    if (IsSettingsButton(page, id)) {
        // Navigate to page selection first
        ExpSection::GetSection()->GetPulPage<SettingsPageSelect>()->prevPageId = PAGE_SINGLE_PLAYER_MENU;
        ExpSection::GetSection()->GetPulPage<SettingsPanel>()->prevPageId = PAGE_SINGLE_PLAYER_MENU;
        page->nextPageId = static_cast<PageId>(SettingsPageSelect::id);
        page->EndStateAnimated(0, button.GetAnimationFrameSize());
        return;
    }

    if (IsMissionButton(page, id))
        button.buttonId = missionButtonVanillaId;
    else if (IsTTModeButton(page, id))
        button.buttonId = 1;
    page->Pages::SinglePlayer::OnButtonClick(button, hudSlotId);
    button.buttonId = id;
    System* system = System::sInstance;
    if (IsTTModeButton(page, id)) {
        TTMode mode = TTMODE_150;
        switch (GetTTExtraButtonCount(page)) {
            case (1):
                if (id > 3) {
                    if (system->GetInfo().Has200cc())
                        mode = TTMODE_200;
                    else
                        mode = TTMODE_150_FEATHER;
                }
                break;
            case (3):
                if (id > 3) mode = (TTMode)(id - 3);
                break;
        }
        system->ttMode = mode;
        SetCC();
    }
}
kmWritePointer(0x808BBED0, OnButtonClick);
}  // namespace UI

// Sets the CC (based on the mode) when retrying after setting a time, as racedata's CC is overwritten usually
static void SetCC() {
    const System* system = System::sInstance;
    EngineClass cc = CC_150;
    if (system->ttMode == TTMODE_200 || system->ttMode == TTMODE_200_FEATHER) cc = CC_100;
    Racedata::sInstance->menusScenario.settings.engineClass = cc;
}
kmBranch(0x805e1ef4, SetCC);
kmBranch(0x805e1d58, SetCC);

}  // namespace Pulsar
