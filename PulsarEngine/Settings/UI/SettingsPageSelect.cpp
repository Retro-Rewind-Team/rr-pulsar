#include <Settings/UI/SettingsPageSelect.hpp>
#include <UI/CustomItems/CustomItemPage.hpp>
#include <Settings/UI/SettingsPanel.hpp>
#include <Settings/Settings.hpp>
#include <MarioKartWii/UI/Page/Menu/VSSettings.hpp>
#include <MarioKartWii/UI/Page/Other/SELECTStageMgr.hpp>
#include <MarioKartWii/UI/Ctrl/CountDown.hpp>
#include <MarioKartWii/UI/Page/Other/ActionLess.hpp>
#include <Network/Mogi.hpp>

namespace Pulsar {
namespace UI {

static const float MOGI_FORMAT_VOTE_SECONDS = 15.0f;
static const u16 MOGI_FORMAT_HANDOFF_FRAMES = 120;

static bool IsVotingSettingsSection(SectionId id) {
    return (id >= SECTION_P1_WIFI_FROOM_VS_VOTING && id <= SECTION_P2_WIFI_FROOM_COIN_VOTING) ||
           id == SECTION_P1_WIFI_VS_VOTING || id == SECTION_P2_WIFI_VS_VOTING ||
           id == SECTION_P1_WIFI_BATTLE_VOTING || id == SECTION_P2_WIFI_BATTLE_VOTING;
}

static bool CanSelectRankingBadge(Settings::SettingsContext context) {
    return (context == Settings::SETTINGS_CONTEXT_ONLINE ||
            context == Settings::SETTINGS_CONTEXT_VOTING) &&
           Ranking::HasSpecialBadges();
}

SettingsPageSelect::SettingsPageSelect(bool badgeSelect) : context(Settings::SETTINGS_CONTEXT_OFFLINE), badgeSelectMode(badgeSelect) {
    externControlCount = 0;
    internControlCount = badgeSelectMode ? badgeButtonCount : settingsButtonCount;
    hasBackButton = true;
    nextPageId = PAGE_NONE;
    titleBmg = BMG_SETTINGS_TITLE;
    activePlayerBitfield = 1;
    movieStartFrame = -1;
    extraControlNumber = 0;
    isLocked = false;
    controlCount = 0;
    nextSection = SECTION_NONE;
    controlSources = 2;
    isFormatVotePage = false;
    isFormatVoteEnding = false;
    isFormatVoteSubmitted = false;
    formatVoteResolvedFrames = 0;
    formatVotePreviousCountdown = 0.0f;

    const SectionId sectionId = SectionMgr::sInstance->curSection->sectionId;
    if (badgeSelectMode)
        prevPageId = static_cast<PageId>(PULPAGE_SETTINGSPAGESELECT);
    else if (sectionId == SECTION_OPTIONS)
        prevPageId = PAGE_OPTIONS;
    else if (sectionId == SECTION_P1_WIFI || sectionId == SECTION_P2_WIFI ||
             sectionId == SECTION_P1_WIFI_FROM_FROOM_RACE || sectionId == SECTION_P2_WIFI_FROM_FROOM_RACE ||
             sectionId == SECTION_P1_WIFI_FROM_FIND_FRIEND || sectionId == SECTION_P2_WIFI_FROM_FIND_FRIEND)
        prevPageId = PAGE_WFC_MAIN;
    else if (sectionId >= SECTION_LICENSE_SETTINGS_MENU && sectionId <= SECTION_SINGLE_P_LIST_RACE_GHOST)
        prevPageId = PAGE_SINGLE_PLAYER_MENU;
    else
        prevPageId = PAGE_FRIEND_ROOM;

    onButtonClickHandler.subject = this;
    onButtonClickHandler.ptmf = &SettingsPageSelect::OnButtonClick;
    onButtonSelectHandler.subject = this;
    onButtonSelectHandler.ptmf = &SettingsPageSelect::OnButtonSelect;
    onButtonDeselectHandler.subject = this;
    onButtonDeselectHandler.ptmf = &SettingsPageSelect::OnButtonDeselect;
    onBackPressHandler.subject = this;
    onBackPressHandler.ptmf = &SettingsPageSelect::OnBackPress;
    onBackButtonClickHandler.subject = this;
    onBackButtonClickHandler.ptmf = &SettingsPageSelect::OnBackButtonClick;

    controlsManipulatorManager.Init(1, false);
    SetManipulatorManager(controlsManipulatorManager);
    controlsManipulatorManager.SetGlobalHandler(BACK_PRESS, onBackPressHandler, false, false);
}

void SettingsPageSelect::SetContext(Settings::SettingsContext newContext, PageId previousPage) {
    context = newContext;
    prevPageId = previousPage;
}

void SettingsPageSelect::OnInit() {
    MenuInteractable::OnInit();
    SetTransitionSound(0, 0);
    backButton.SetOnClickHandler(onBackButtonClickHandler, 0);
}

UIControl *SettingsPageSelect::CreateControl(u32 id) {
    PushButton &button = pageButtons[id];
    AddControl(controlCount++, button, 0);
    char variant[16];
    snprintf(variant, 16, "Page%d", id);
    button.Load(UI::buttonFolder, "SettingsPageSelect", variant, activePlayerBitfield, 0, false);
    button.buttonId = id;
    SetButtonHandlers(button);
    return &button;
}

void SettingsPageSelect::SetButtonHandlers(PushButton &button) {
    button.SetOnClickHandler(onButtonClickHandler, 0);
    button.SetOnSelectHandler(onButtonSelectHandler);
    button.SetOnDeselectHandler(onButtonDeselectHandler);
}

void SettingsPageSelect::OnActivate() {
    isFormatVotePage = !badgeSelectMode && Mogi::IsFormatVoteActive();
    isFormatVoteEnding = false;
    isFormatVoteSubmitted = false;
    formatVoteResolvedFrames = 0;
    if (isFormatVotePage) {
        SetPreparingRaceVisible(false);
        titleBmg = BMG_MOGI_FORMAT_TITLE;
        bottomText->SetMessage(BMG_MOGI_FORMAT_BOTTOM);
        backButton.isHidden = true;
        backButton.manipulator.inaccessible = true;
        Pages::SELECTStageMgr* select = SectionMgr::sInstance->curSection->Get<Pages::SELECTStageMgr>();
        if (select != nullptr) {
            formatVotePreviousCountdown = select->countdown.countdown;
            select->countdown.SetInitial(MOGI_FORMAT_VOTE_SECONDS);
            select->countdown.isActive = true;
            select->timerControl.isHidden = false;
            select->timerControl.AnimateCurrentCountDown();
        }
        for (u32 i = 0; i < settingsButtonCount; ++i) {
            const bool hidden = i >= 5;
            pageButtons[i].isHidden = hidden;
            pageButtons[i].manipulator.inaccessible = hidden;
            if (!hidden) pageButtons[i].SetMessage(BMG_MOGI_FORMAT_FFA + i);
        }
        pageButtons[0].Select(0);
        MenuInteractable::OnActivate();
        return;
    }
    titleBmg = BMG_SETTINGS_TITLE;
    backButton.isHidden = false;
    backButton.manipulator.inaccessible = false;
    if (badgeSelectMode) {
        UpdateBadgeButtons();
        bottomText->SetMessage(0);
        pageButtons[0].Select(0);
        MenuInteractable::OnActivate();
        return;
    }

    const Settings::SettingsContextDef &contextDef = Settings::Params::GetContextDef(context);
    const bool showBadge = CanSelectRankingBadge(context);
    for (u32 i = 0; i < settingsButtonCount; ++i) {
        PushButton &button = pageButtons[i];
        const bool isPage = i < contextDef.pageCount;
        const bool isBadge = showBadge && i == contextDef.pageCount;
        const bool hidden = !isPage && !isBadge;
        button.isHidden = hidden;
        button.manipulator.inaccessible = hidden;
        if (isPage)
            button.SetMessage(Settings::Params::GetPageDef(contextDef.pages[i]).nameBmg);
        else if (isBadge)
            button.SetMessage(BMR_RANKING_BUTTON);
    }
    if (contextDef.pageCount > 0) pageButtons[0].Select(0);
    bottomText->SetMessage(BMG_SETTINGS_BOTTOM);
    MenuInteractable::OnActivate();
}

const ut::detail::RuntimeTypeInfo *SettingsPageSelect::GetRuntimeTypeInfo() const { return Pages::VSSettings::typeInfo; }
int SettingsPageSelect::GetActivePlayerBitfield() const { return activePlayerBitfield; }
int SettingsPageSelect::GetPlayerBitfield() const { return playerBitfield; }
ManipulatorManager &SettingsPageSelect::GetManipulatorManager() { return controlsManipulatorManager; }

void SettingsPageSelect::OnBackPress(u32) {
    if (isFormatVotePage) return;
    backButton.SelectFocus();
    LoadPrevPage(backButton);
}
void SettingsPageSelect::OnBackButtonClick(PushButton &, u32 hudSlotId) { OnBackPress(hudSlotId); }

void SettingsPageSelect::OnButtonClick(PushButton &button, u32) {
    if (isFormatVotePage) {
        if (!isFormatVoteSubmitted && Mogi::CastFormatVote(static_cast<u8>(button.buttonId))) ShowFormatVoteWaiting();
        return;
    }
    const u32 selected = button.buttonId;
    if (badgeSelectMode) {
        const u8 badge = selected > 0 ? Ranking::GetSpecialBadgeAt(selected - 1) : Ranking::NORMAL_RANKING_BADGE;
        if (selected == 0 || Ranking::IsSpecialBadgeAvailable(badge)) {
            Settings::Mgr::Get().SetRankingBadge(badge);
            nextPageId = static_cast<PageId>(SettingsPageSelect::id);
            EndStateAnimated(0, button.GetAnimationFrameSize());
        }
        return;
    }

    const Settings::SettingsContextDef &contextDef = Settings::Params::GetContextDef(context);
    if (selected == contextDef.pageCount) {
        if (CanSelectRankingBadge(context)) {
            nextPageId = static_cast<PageId>(PULPAGE_BADGESELECT);
            EndStateAnimated(0, button.GetAnimationFrameSize());
        }
        return;
    }
    if (selected >= contextDef.pageCount) return;

    const Settings::SettingsPageId selectedPage = contextDef.pages[selected];
    if (selectedPage == Settings::SETTINGS_PAGE_ITEMS)
        nextPageId = static_cast<PageId>(CustomItemPage::id);
    else {
        SettingsPanel *panel = ExpSection::GetSection()->GetPulPage<SettingsPanel>();
        if (panel == nullptr) return;
        panel->SetPage(selectedPage);
        nextPageId = static_cast<PageId>(SettingsPanel::id);
    }
    EndStateAnimated(0, button.GetAnimationFrameSize());
}

void SettingsPageSelect::OnButtonSelect(PushButton &button, u32) {
    if (isFormatVotePage) {
        bottomText->SetMessage(BMG_MOGI_FORMAT_BOTTOM);
        return;
    }
    if (badgeSelectMode) {
        bottomText->SetMessage(0);
        return;
    }
    const Settings::SettingsContextDef &contextDef = Settings::Params::GetContextDef(context);
    if (button.buttonId < contextDef.pageCount)
        bottomText->SetMessage(Settings::Params::GetPageDef(contextDef.pages[button.buttonId]).nameBmg + 0x10);
    else
        bottomText->SetMessage(BMG_SETTINGS_BOTTOM);
}

void SettingsPageSelect::SetBadgeButtonMessage(PushButton &button) {
    if (button.buttonId == 0) {
        button.SetMessage(BMG_RANKING_BADGE);
        return;
    }
    const u8 badge = Ranking::GetSpecialBadgeAt(button.buttonId - 1);
    wchar_t badgeText[] = {static_cast<wchar_t>(0xF07C + badge), L'\0'};
    Text::Info info;
    info.strings[0] = badgeText;
    button.SetMessage(BMG_TEXT, &info);
}

void SettingsPageSelect::UpdateBadgeButtons() {
    const u32 count = Ranking::GetSpecialBadgeCount();
    for (u32 i = 0; i < badgeButtonCount; ++i) {
        PushButton &button = pageButtons[i];
        const bool hidden = i > 0 && i - 1 >= count;
        button.isHidden = hidden;
        button.manipulator.inaccessible = hidden;
        if (!hidden) SetBadgeButtonMessage(button);
    }
}

void SettingsPageSelect::ShowFormatVoteWaiting() {
    if (isFormatVoteSubmitted) return;
    isFormatVoteSubmitted = true;
    controlsManipulatorManager.inaccessible = true;
    titleText->isHidden = true;
    bottomText->isHidden = true;
    backButton.isHidden = true;
    for (u32 i = 0; i < settingsButtonCount; ++i) {
        pageButtons[i].isHidden = true;
        pageButtons[i].manipulator.inaccessible = true;
    }
    Pages::SELECTStageMgr* select = SectionMgr::sInstance->curSection->Get<Pages::SELECTStageMgr>();
    if (select != nullptr) select->timerControl.isHidden = true;
    SetPreparingRaceVisible(true);
}

void SettingsPageSelect::SetPreparingRaceVisible(bool visible) {
    Pages::AutoEnding* preparingRace = SectionMgr::sInstance->curSection->Get<Pages::AutoEnding>(PAGE_AUTO_ENDING2);
    if (preparingRace == nullptr) return;
    preparingRace->titleText.isHidden = !visible;
    preparingRace->busySymbol.isHidden = !visible;
    if (preparingRace->messageWindow != nullptr) preparingRace->messageWindow->isHidden = !visible;
}

void SettingsPageSelect::BeforeControlUpdate() {
    if (isFormatVotePage) {
        Pages::SELECTStageMgr* select = SectionMgr::sInstance->curSection->Get<Pages::SELECTStageMgr>();
        if (select != nullptr && !isFormatVoteSubmitted) {
            select->countdown.Update();
            select->timerControl.AnimateCurrentCountDown();
            if (select->countdown.countdown <= 0.0f) {
                Mogi::OnFormatVoteTimeout();
                ShowFormatVoteWaiting();
            }
        }
        if (Mogi::IsFormatVoteResolved()) {
            ShowFormatVoteWaiting();
            if (formatVoteResolvedFrames < MOGI_FORMAT_HANDOFF_FRAMES) ++formatVoteResolvedFrames;
        }
        if (formatVoteResolvedFrames >= MOGI_FORMAT_HANDOFF_FRAMES && !isFormatVoteEnding) {
            isFormatVoteEnding = true;
            Mogi::FinishFormatVote();
            if (select != nullptr) {
                select->countdown.SetInitial(formatVotePreviousCountdown);
                select->countdown.isActive = true;
                select->timerControl.isHidden = false;
                select->timerControl.AnimateCurrentCountDown();
            }
            SetPreparingRaceVisible(true);
            nextPageId = PAGE_NONE;
            EndStateAnimated(1, 0.0f);
        }
        return;
    }
    const SectionId sectionId = SectionMgr::sInstance->curSection->sectionId;
    if (!badgeSelectMode && IsVotingSettingsSection(sectionId)) {
        Pages::SELECTStageMgr *select = SectionMgr::sInstance->curSection->Get<Pages::SELECTStageMgr>();
        if (select != nullptr && select->countdown.countdown <= 0) OnBackPress(0);
    }
}

}  // namespace UI
}  // namespace Pulsar
