#include <Settings/UI/SettingsPageSelect.hpp>
#include <UI/CustomItems/CustomItemPage.hpp>
#include <Settings/UI/SettingsPanel.hpp>
#include <Settings/Settings.hpp>
#include <MarioKartWii/UI/Page/Menu/VSSettings.hpp>
#include <MarioKartWii/UI/Page/Other/SELECTStageMgr.hpp>
#include <MarioKartWii/UI/Page/Other/ActionLess.hpp>
#include <MarioKartWii/UI/Ctrl/CountDown.hpp>
#include <Network/Mogi.hpp>

namespace Pulsar {
namespace UI {

static const float MOGI_FORMAT_VOTE_SECONDS = 15.0f;
static const u16 MOGI_FORMAT_HANDOFF_FRAMES = 120;

SettingsPageSelect::SettingsPageSelect() {
    externControlCount = 0;
    internControlCount = Settings::Params::pageCount;
    hasBackButton = true;
    nextPageId = PAGE_NONE;
    titleBmg = BMG_SETTINGS_TITLE;  // "Settings" title
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

    // Determine previous page based on section
    SectionId sectionId = SectionMgr::sInstance->curSection->sectionId;
    if (sectionId == SECTION_OPTIONS)
        prevPageId = PAGE_OPTIONS;
    else if ((sectionId == SECTION_P1_WIFI) || (sectionId == SECTION_P1_WIFI_FROM_FROOM_RACE) ||
             (sectionId == SECTION_P1_WIFI_FROM_FIND_FRIEND) || (sectionId == SECTION_P2_WIFI) ||
             (sectionId == SECTION_P2_WIFI_FROM_FROOM_RACE))
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

    this->controlsManipulatorManager.Init(1, false);
    this->SetManipulatorManager(controlsManipulatorManager);
    this->controlsManipulatorManager.SetGlobalHandler(BACK_PRESS, onBackPressHandler, false, false);
}

void SettingsPageSelect::OnInit() {
    MenuInteractable::OnInit();
    this->SetTransitionSound(0, 0);
    this->backButton.SetOnClickHandler(this->onBackButtonClickHandler, 0);
}

UIControl* SettingsPageSelect::CreateControl(u32 id) {
    if (id < Settings::Params::pageCount) {
        PushButton& button = this->pageButtons[id];
        this->AddControl(this->controlCount++, button, 0);

        char variant[16];
        snprintf(variant, 16, "Page%d", id);

        button.Load(UI::buttonFolder, "SettingsPageSelect", variant, this->activePlayerBitfield, 0, false);
        button.buttonId = id;
        button.SetOnClickHandler(this->onButtonClickHandler, 0);
        button.SetOnSelectHandler(this->onButtonSelectHandler);
        button.SetOnDeselectHandler(this->onButtonDeselectHandler);

        // Set the button message based on which page it represents
        u32 bmgOffset = 0;
        u32 pageIdx = id;
        if (id >= Settings::Params::pulsarPageCount) {
            bmgOffset = BMG_USERSETTINGSOFFSET;
            pageIdx = id - Settings::Params::pulsarPageCount;
        }
        button.SetMessage(bmgOffset + BMG_SETTINGS_PAGE + pageIdx);

        return &button;
    }
    return nullptr;
}

void SettingsPageSelect::SetButtonHandlers(PushButton& button) {
    button.SetOnClickHandler(this->onButtonClickHandler, 0);
    button.SetOnSelectHandler(this->onButtonSelectHandler);
    button.SetOnDeselectHandler(this->onButtonDeselectHandler);
}

void SettingsPageSelect::OnActivate() {
    this->isFormatVotePage = Mogi::IsFormatVoteActive();
    this->isFormatVoteEnding = false;
    this->isFormatVoteSubmitted = false;
    this->formatVoteResolvedFrames = 0;
    this->controlsManipulatorManager.inaccessible = false;
    this->titleText->isHidden = false;
    this->bottomText->isHidden = false;
    if (this->isFormatVotePage) {
        this->SetPreparingRaceVisible(false);
        this->titleBmg = BMG_MOGI_FORMAT_TITLE;
        this->bottomText->SetMessage(BMG_MOGI_FORMAT_BOTTOM);
        this->backButton.isHidden = true;
        this->backButton.manipulator.inaccessible = true;
        Pages::SELECTStageMgr* selectStageMgr = SectionMgr::sInstance->curSection->Get<Pages::SELECTStageMgr>();
        if (selectStageMgr != nullptr) {
            CountDown* timer = &selectStageMgr->countdown;
            this->formatVotePreviousCountdown = timer->countdown;
            timer->SetInitial(MOGI_FORMAT_VOTE_SECONDS);
            timer->isActive = true;
            selectStageMgr->timerControl.isHidden = false;
            selectStageMgr->timerControl.AnimateCurrentCountDown();
        }
        for (u32 i = 0; i < Settings::Params::pageCount; ++i) {
            const bool hidden = i >= 5;
            this->pageButtons[i].isHidden = hidden;
            this->pageButtons[i].manipulator.inaccessible = hidden;
            if (!hidden) this->pageButtons[i].SetMessage(BMG_MOGI_FORMAT_FFA + i);
        }
        this->pageButtons[0].Select(0);
        MenuInteractable::OnActivate();
        return;
    }
    this->titleBmg = BMG_SETTINGS_TITLE;
    this->backButton.isHidden = false;
    this->backButton.manipulator.inaccessible = false;
    // Select the first button by default
    if (Settings::Params::pageCount > 0) {
        this->pageButtons[0].Select(0);
    }

    this->bottomText->SetMessage(BMG_SETTINGS_BOTTOM);

    // Hide pages that are restricted in certain sections
    SectionId sectionId = SectionMgr::sInstance->curSection->sectionId;
    bool isVotingSection = (sectionId >= SECTION_P1_WIFI_FROOM_VS_VOTING && sectionId <= SECTION_P2_WIFI_FROOM_COIN_VOTING) ||
                           (sectionId == SECTION_P1_WIFI_VS_VOTING) || (sectionId == SECTION_P1_WIFI_BATTLE_VOTING);
    bool isOnlineSection = (sectionId == SECTION_P1_WIFI || sectionId == SECTION_P2_WIFI ||
                            sectionId == SECTION_P1_WIFI_FROM_FROOM_RACE || sectionId == SECTION_P2_WIFI_FROM_FROOM_RACE ||
                            sectionId == SECTION_P1_WIFI_FROM_FIND_FRIEND || sectionId == SECTION_P2_WIFI_FROM_FIND_FRIEND);

    for (int i = 0; i < Settings::Params::pageCount; ++i) {
        bool isHidden = false;

        if (isVotingSection) {
            // Hide restricted pages in voting sections
            if (i == Settings::SETTINGSTYPE_KO ||
                i == Settings::SETTINGSTYPE_KOROYALE ||
                i == Settings::SETTINGSTYPE_OTT ||
                i == Settings::SETTINGSTYPE_FROOM1 ||
                i == Settings::SETTINGSTYPE_BATTLE ||
                i == (Settings::SETTINGSTYPE_EXTENDEDTEAMS + Settings::Params::pulsarPageCount) ||
                i == (Settings::SETTINGSTYPE_FROOM2 + Settings::Params::pulsarPageCount) ||
                i == (Settings::SETTINGSTYPE_MISC + Settings::Params::pulsarPageCount) ||
                i == (Settings::SETTINGSTYPE_ITEMS + Settings::Params::pulsarPageCount)) {
                isHidden = true;
            }
        }

        if (isOnlineSection) {
            // Hide restricted pages in online sections
            if (i == (Settings::SETTINGSTYPE_MISC + Settings::Params::pulsarPageCount)) {
                isHidden = true;
            }
        }

        this->pageButtons[i].isHidden = isHidden;
        this->pageButtons[i].manipulator.inaccessible = isHidden;
    }

    MenuInteractable::OnActivate();
}

const ut::detail::RuntimeTypeInfo* SettingsPageSelect::GetRuntimeTypeInfo() const {
    return Pages::VSSettings::typeInfo;
}

int SettingsPageSelect::GetActivePlayerBitfield() const {
    return this->activePlayerBitfield;
}

int SettingsPageSelect::GetPlayerBitfield() const {
    return this->playerBitfield;
}

ManipulatorManager& SettingsPageSelect::GetManipulatorManager() {
    return this->controlsManipulatorManager;
}

void SettingsPageSelect::OnBackPress(u32 hudSlotId) {
    if (this->isFormatVotePage) return;
    this->backButton.SelectFocus();
    this->LoadPrevPage(this->backButton);
}

void SettingsPageSelect::OnBackButtonClick(PushButton& button, u32 hudSlotId) {
    this->OnBackPress(hudSlotId);
}

void SettingsPageSelect::OnButtonClick(PushButton& button, u32 hudSlotId) {
    if (this->isFormatVotePage) {
        if (!this->isFormatVoteSubmitted &&
            Mogi::CastFormatVote(static_cast<u8>(button.buttonId))) {
            this->ShowFormatVoteWaiting();
        }
        return;
    }
    // Get the SettingsPanel and set up the selected page
    const u32 selectedPage = button.buttonId;

    if (selectedPage == Settings::SETTINGSTYPE_ITEMS) {
        this->nextPageId = static_cast<PageId>(CustomItemPage::id);
        this->EndStateAnimated(0, button.GetAnimationFrameSize());
        return;
    }

    SettingsPanel* settingsPanel = ExpSection::GetSection()->GetPulPage<SettingsPanel>();
    if (settingsPanel != nullptr) {
        settingsPanel->sheetIdx = selectedPage;
        if (selectedPage < Settings::Params::pulsarPageCount) {
            settingsPanel->catIdx = selectedPage;
        } else {
            settingsPanel->catIdx = selectedPage - Settings::Params::pulsarPageCount;
        }

        // Navigate to the settings panel
        this->nextPageId = static_cast<PageId>(SettingsPanel::id);
        this->EndStateAnimated(0, button.GetAnimationFrameSize());
    }
}

void SettingsPageSelect::OnButtonSelect(PushButton& button, u32 hudSlotId) {
    if (this->isFormatVotePage) {
        this->bottomText->SetMessage(BMG_MOGI_FORMAT_BOTTOM);
        return;
    }
    // Display the page name/description in the bottom text
    u32 bmgOffset = 0;
    u32 pageIdx = button.buttonId;
    if (button.buttonId >= Settings::Params::pulsarPageCount) {
        bmgOffset = BMG_USERSETTINGSOFFSET;
        pageIdx = button.buttonId - Settings::Params::pulsarPageCount;
    }
    this->bottomText->SetMessage(bmgOffset + BMG_SETTINGS_TITLE + pageIdx);
}

void SettingsPageSelect::ShowFormatVoteWaiting() {
    if (this->isFormatVoteSubmitted) return;
    this->isFormatVoteSubmitted = true;
    this->controlsManipulatorManager.inaccessible = true;
    this->titleText->isHidden = true;
    this->bottomText->isHidden = true;
    this->backButton.isHidden = true;
    for (u32 i = 0; i < Settings::Params::pageCount; ++i) {
        this->pageButtons[i].isHidden = true;
        this->pageButtons[i].manipulator.inaccessible = true;
    }
    Pages::SELECTStageMgr* selectStageMgr = SectionMgr::sInstance->curSection->Get<Pages::SELECTStageMgr>();
    if (selectStageMgr != nullptr) selectStageMgr->timerControl.isHidden = true;
    this->SetPreparingRaceVisible(true);
}

void SettingsPageSelect::SetPreparingRaceVisible(bool visible) {
    Pages::AutoEnding* preparingRace =
        SectionMgr::sInstance->curSection->Get<Pages::AutoEnding>(PAGE_AUTO_ENDING2);
    if (preparingRace == nullptr) return;
    preparingRace->titleText.isHidden = !visible;
    preparingRace->busySymbol.isHidden = !visible;
    if (preparingRace->messageWindow != nullptr) preparingRace->messageWindow->isHidden = !visible;
}

void SettingsPageSelect::BeforeControlUpdate() {
    if (this->isFormatVotePage) {
        Pages::SELECTStageMgr* selectStageMgr = SectionMgr::sInstance->curSection->Get<Pages::SELECTStageMgr>();
        if (selectStageMgr != nullptr && !this->isFormatVoteSubmitted) {
            CountDown* timer = &selectStageMgr->countdown;
            timer->Update();
            selectStageMgr->timerControl.AnimateCurrentCountDown();
            if (timer->countdown <= 0.0f) {
                Mogi::OnFormatVoteTimeout();
                this->ShowFormatVoteWaiting();
            }
        }
        if (Mogi::IsFormatVoteResolved()) {
            this->ShowFormatVoteWaiting();
            if (this->formatVoteResolvedFrames < MOGI_FORMAT_HANDOFF_FRAMES) {
                ++this->formatVoteResolvedFrames;
            }
        }
        if (this->formatVoteResolvedFrames >= MOGI_FORMAT_HANDOFF_FRAMES &&
            !this->isFormatVoteEnding) {
            this->isFormatVoteEnding = true;
            Mogi::FinishFormatVote();
            if (selectStageMgr != nullptr) {
                selectStageMgr->countdown.SetInitial(this->formatVotePreviousCountdown);
                selectStageMgr->countdown.isActive = true;
                selectStageMgr->timerControl.isHidden = false;
                selectStageMgr->timerControl.AnimateCurrentCountDown();
            }
            this->SetPreparingRaceVisible(true);
            this->nextPageId = PAGE_NONE;
            this->EndStateAnimated(1, 0.0f);
        }
        return;
    }
    SectionId id = SectionMgr::sInstance->curSection->sectionId;
    bool isVotingSection = (id >= SECTION_P1_WIFI_FROOM_VS_VOTING && id <= SECTION_P2_WIFI_FROOM_COIN_VOTING) || (id == SECTION_P1_WIFI_VS_VOTING);
    if (isVotingSection) {
        Pages::SELECTStageMgr* selectStageMgr = SectionMgr::sInstance->curSection->Get<Pages::SELECTStageMgr>();
        CountDown* timer = &selectStageMgr->countdown;
        if (timer->countdown <= 0) {
            this->OnBackPress(0);
        }
    }
}

}  // namespace UI
}  // namespace Pulsar
