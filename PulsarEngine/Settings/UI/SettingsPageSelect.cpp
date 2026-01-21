#include <Settings/UI/SettingsPageSelect.hpp>
#include <UI/CustomItems/CustomItemPage.hpp>
#include <Settings/UI/SettingsPanel.hpp>
#include <Settings/Settings.hpp>
#include <MarioKartWii/UI/Page/Menu/VSSettings.hpp>
#include <MarioKartWii/UI/Page/Other/SELECTStageMgr.hpp>
#include <MarioKartWii/UI/Ctrl/CountDown.hpp>

namespace Pulsar {
namespace UI {

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
    this->backButton.SelectFocus();
    this->LoadPrevPage(this->backButton);
}

void SettingsPageSelect::OnBackButtonClick(PushButton& button, u32 hudSlotId) {
    this->OnBackPress(hudSlotId);
}

void SettingsPageSelect::OnButtonClick(PushButton& button, u32 hudSlotId) {
    // Get the SettingsPanel and set up the selected page
    const u32 selectedPage = button.buttonId;

    if (selectedPage == 12) {
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
    // Display the page name/description in the bottom text
    u32 bmgOffset = 0;
    u32 pageIdx = button.buttonId;
    if (button.buttonId >= Settings::Params::pulsarPageCount) {
        bmgOffset = BMG_USERSETTINGSOFFSET;
        pageIdx = button.buttonId - Settings::Params::pulsarPageCount;
    }
    this->bottomText->SetMessage(bmgOffset + BMG_SETTINGS_TITLE + pageIdx);
}

void SettingsPageSelect::BeforeControlUpdate() {
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