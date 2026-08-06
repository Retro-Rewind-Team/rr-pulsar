#include <Settings/UI/SettingsPanel.hpp>
#include <Settings/UI/SettingsPageSelect.hpp>
#include <Settings/Settings.hpp>
#include <UI/ChangeCombo/ChangeCombo.hpp>
#include <SlotExpansion/CupsConfig.hpp>
#include <Network/PacketExpansion.hpp>
#include <MarioKartWii/UI/Page/Other/SELECTStageMgr.hpp>
#include <Network/Network.hpp>

namespace Pulsar {
namespace UI {

static bool s_votingSettingsPreviewActive = false;
static u32 s_votingSettingsPreviewFrame = 0;
static const u32 votingSettingsPreviewDuration = 240;
static u8 s_hostPreviewValues[Settings::SETTING_COUNT];

static bool IsVotingSection(SectionId id) {
    return (id >= SECTION_P1_WIFI_FROOM_VS_VOTING && id <= SECTION_P2_WIFI_FROOM_COIN_VOTING) ||
           id == SECTION_P1_WIFI_VS_VOTING || id == SECTION_P2_WIFI_VS_VOTING ||
           id == SECTION_P1_WIFI_BATTLE_VOTING || id == SECTION_P2_WIFI_BATTLE_VOTING;
}

static bool IsBattleVotingSection(SectionId id) {
    return id == SECTION_P1_WIFI_BATTLE_VOTING || id == SECTION_P2_WIFI_BATTLE_VOTING ||
           id == SECTION_P1_WIFI_FROOM_BALLOON_VOTING || id == SECTION_P2_WIFI_FROOM_BALLOON_VOTING ||
           id == SECTION_P1_WIFI_FROOM_COIN_VOTING || id == SECTION_P2_WIFI_FROOM_COIN_VOTING;
}

SettingsPanel::SettingsPanel() {
    settingsPageId = Settings::SETTINGS_PAGE_RACE1;
    externControlCount = 1;
    internControlCount = Settings::Params::maxRadioCount + Settings::Params::maxScrollerCount;
    hasBackButton = true;
    nextPageId = static_cast<PageId>(id);
    activePlayerBitfield = 1;
    movieStartFrame = -1;
    extraControlNumber = 0;
    isLocked = false;
    controlCount = 0;
    nextSection = SECTION_NONE;
    controlSources = 2;

    const SectionId sectionId = SectionMgr::sInstance->curSection->sectionId;
    if (sectionId == SECTION_OPTIONS)
        prevPageId = PAGE_OPTIONS;
    else if (sectionId == SECTION_P1_WIFI || sectionId == SECTION_P2_WIFI ||
             sectionId == SECTION_P1_WIFI_FROM_FROOM_RACE || sectionId == SECTION_P2_WIFI_FROM_FROOM_RACE ||
             sectionId == SECTION_P1_WIFI_FROM_FIND_FRIEND || sectionId == SECTION_P2_WIFI_FROM_FIND_FRIEND)
        prevPageId = PAGE_WFC_MAIN;
    else if (sectionId >= SECTION_LICENSE_SETTINGS_MENU && sectionId <= SECTION_SINGLE_P_LIST_RACE_GHOST)
        prevPageId = PAGE_SINGLE_PLAYER_MENU;

    onMessageBoxClickHandler.ptmf = &Menu::ChangeToPrevSection;
    onRadioButtonClickHandler.subject = this;
    onRadioButtonClickHandler.ptmf = &SettingsPanel::OnRadioButtonClick;
    onRadioButtonChangeHandler.subject = this;
    onRadioButtonChangeHandler.ptmf = &SettingsPanel::OnRadioButtonChange;
    onUpDownClickHandler.subject = this;
    onUpDownClickHandler.ptmf = &SettingsPanel::OnUpDownClick;
    onUpDownSelectHandler.subject = this;
    onUpDownSelectHandler.ptmf = &SettingsPanel::OnUpDownSelect;
    onTextChangeHandler.subject = this;
    onTextChangeHandler.ptmf = &SettingsPanel::OnTextChange;
    onButtonSelectHandler.subject = this;
    onButtonSelectHandler.ptmf = &SettingsPanel::OnExternalButtonSelect;
    onButtonDeselectHandler.subject = this;
    onButtonDeselectHandler.ptmf = &Pages::VSSettings::OnButtonDeselect;
    onBackPressHandler.subject = this;
    onBackPressHandler.ptmf = &SettingsPanel::OnBackPress;
    onBackButtonClickHandler.subject = this;
    onBackButtonClickHandler.ptmf = &SettingsPanel::OnBackButtonClick;
    onStartPressHandler.subject = this;
    onStartPressHandler.ptmf = &MenuInteractable::HandleStartPress;
    onButtonClickHandler.subject = this;
    onButtonClickHandler.ptmf = &SettingsPanel::OnSaveButtonClick;

    controlsManipulatorManager.Init(1, false);
    SetManipulatorManager(controlsManipulatorManager);
    controlsManipulatorManager.SetGlobalHandler(START_PRESS, onStartPressHandler, false, false);
    controlsManipulatorManager.SetGlobalHandler(BACK_PRESS, onBackPressHandler, false, false);
}

SettingsPanel::~SettingsPanel() {
    Settings::Mgr* mgr = Settings::Mgr::sInstance;
    mgr->SetLastSelectedCup(CupsConfig::sInstance->lastSelectedCup);
    mgr->RequestSave();
}

void SettingsPanel::SetPage(Settings::SettingsPageId id) {
    settingsPageId = id;
}

void SettingsPanel::StartVotingPreview(Settings::SettingsPageId firstPage) {
    s_votingSettingsPreviewActive = true;
    s_votingSettingsPreviewFrame = 0;
    ApplyVotingPreviewHostSettings();
    SetVotingPreviewPage(firstPage);
}

bool SettingsPanel::IsVotingPreviewActive() {
    return s_votingSettingsPreviewActive;
}

void SettingsPanel::SetVotingPreviewPage(Settings::SettingsPageId page) {
    SettingsPanel* panel = ExpSection::GetSection()->GetPulPage<SettingsPanel>();
    if (panel != nullptr) panel->SetPage(page);
}

void SettingsPanel::ApplyVotingPreviewHostSettings() {
    const Network::Mgr& netMgr = System::sInstance->netMgr;
    memset(s_hostPreviewValues, 0, sizeof(s_hostPreviewValues));
    if (!netMgr.hasHostSettingsPreview) return;

    const SectionId sectionId = SectionMgr::sInstance->curSection->sectionId;
    const bool isBattle = IsBattleVotingSection(sectionId);
    const bool isKO = (netMgr.hostContext & (1 << PULSAR_MODE_KO)) ||
                      (netMgr.hostContext & (1 << PULSAR_MODE_LAPKO));
    const bool isOTT = netMgr.hostContext & (1 << PULSAR_MODE_OTT);
    const bool isRoyale = netMgr.hostContext2 & (1 << PULSAR_MODE_BATTLEROYALE);
    const bool isExtendedTeams = netMgr.hostContext & (1 << PULSAR_EXTENDEDTEAMS);

    Settings::SettingsPageId pages[6];
    const u32 pageCount = Settings::Params::BuildHostRulePages(
        pages, isBattle, isKO, isOTT, isRoyale, isExtendedTeams);
    u32 offset = 0;
    for (u32 page = 0; page < pageCount; ++page) {
        const Settings::SettingsPageDef& def = Settings::Params::GetPageDef(pages[page]);
        for (u32 i = 0; i < def.radioCount && offset < Network::HOST_SETTINGS_PREVIEW_COUNT; ++i) {
            const Settings::SettingId id = def.radioSettings[i];
            const Settings::SettingDef& setting = Settings::Params::GetSettingDef(id);
            const u8 value = netMgr.hostSettingsPreview[offset++];
            s_hostPreviewValues[Settings::Params::GetSettingIndex(id)] =
                value < setting.optionCount ? value : 0;
        }
        for (u32 i = 0; i < def.scrollerCount && offset < Network::HOST_SETTINGS_PREVIEW_COUNT; ++i) {
            const Settings::SettingId id = def.scrollerSettings[i];
            const Settings::SettingDef& setting = Settings::Params::GetSettingDef(id);
            const u8 value = netMgr.hostSettingsPreview[offset++];
            s_hostPreviewValues[Settings::Params::GetSettingIndex(id)] =
                value < setting.optionCount ? value : 0;
        }
    }
}

void SettingsPanel::OnInit() {
    backButton.SetOnClickHandler(onBackButtonClickHandler, 0);
    MenuInteractable::OnInit();
    SetTransitionSound(0, 0);
}

UIControl* SettingsPanel::CreateExternalControl(u32 id) {
    PushButton* button = new (PushButton);
    AddControl(controlCount++, *button, 0);
    button->Load(UI::buttonFolder, "Settings", "SAVE", activePlayerBitfield, 0, false);
    return button;
}

UIControl* SettingsPanel::CreateControl(u32 id) {
    if (id < Settings::Params::maxRadioCount) {
        RadioButtonControl& radio = radioButtonControls[id];
        AddControl(controlCount++, radio, 0);
        char variant[12];
        char option0[12];
        char option1[12];
        char option2[12];
        char option3[12];
        snprintf(variant, 12, "Row%d", id);
        snprintf(option0, 12, "%sOption0", variant);
        snprintf(option1, 12, "%sOption1", variant);
        snprintf(option2, 12, "%sOption2", variant);
        snprintf(option3, 12, "%sOption3", variant);
        const char* options[5] = {option0, option1, option2, option3, nullptr};
        radio.Load(4, 0, UI::controlFolder, "RadioBase", variant, "RadioOption", options, 1, 0, 0);
        radio.SetOnClickHandler(onRadioButtonClickHandler);
        radio.SetOnChangeHandler(onRadioButtonChangeHandler);
        radio.id = id;
    } else if (id < Settings::Params::maxRadioCount + Settings::Params::maxScrollerCount) {
        id -= Settings::Params::maxRadioCount;
        UpDownControl& scroller = upDownControls[id];
        AddControl(controlCount++, scroller, 0);
        char variant[12];
        snprintf(variant, 12, "UpDown%d", id);
        scroller.Load(7, 0, UI::controlFolder, "UpDownBase", variant, "UpDownR", "Right", "UpDownL",
                      "Left", &textUpDown[id], 1, 0, false, true, true);
        scroller.SetOnClickHandler(onUpDownClickHandler);
        scroller.SetOnSelectHandler(onUpDownSelectHandler);
        scroller.id = id;
        textUpDown[id].Load(UI::controlFolder, "UpDownValue", "Value", "UpDownText", "Text");
        textUpDown[id].SetOnTextChangeHandler(onTextChangeHandler);
    }
    return nullptr;
}

void SettingsPanel::SetButtonHandlers(PushButton& button) {
    button.SetOnClickHandler(onButtonClickHandler, 0);
    button.SetOnSelectHandler(onButtonSelectHandler);
    button.SetOnDeselectHandler(onButtonDeselectHandler);
}

void SettingsPanel::LoadCurrentValues() {
    const Settings::SettingsPageDef& page = Settings::Params::GetPageDef(settingsPageId);
    const Settings::Mgr& mgr = Settings::Mgr::Get();
    for (u32 i = 0; i < page.radioCount; ++i) {
        const Settings::SettingId id = page.radioSettings[i];
        radioValues[i] = s_votingSettingsPreviewActive
                             ? s_hostPreviewValues[Settings::Params::GetSettingIndex(id)]
                             : mgr.GetSettingValue(id);
    }
    for (u32 i = 0; i < page.scrollerCount; ++i) {
        const Settings::SettingId id = page.scrollerSettings[i];
        scrollerValues[i] = s_votingSettingsPreviewActive
                                ? s_hostPreviewValues[Settings::Params::GetSettingIndex(id)]
                                : mgr.GetSettingValue(id);
    }
}

void SettingsPanel::OnActivate() {
    const Settings::SettingsPageDef& page = Settings::Params::GetPageDef(settingsPageId);
    titleBmg = page.nameBmg + 0x1F;
    LoadCurrentValues();

    externControls[0]->isHidden = s_votingSettingsPreviewActive;
    externControls[0]->manipulator.inaccessible = s_votingSettingsPreviewActive;
    backButton.isHidden = s_votingSettingsPreviewActive;
    backButton.manipulator.inaccessible = s_votingSettingsPreviewActive;
    controlsManipulatorManager.inaccessible = s_votingSettingsPreviewActive;
    if (!s_votingSettingsPreviewActive) externControls[0]->SelectInitial(0);
    bottomText->SetMessage(BMG_SETTINGS_BOTTOM);

    for (u32 i = 0; i < Settings::Params::maxRadioCount; ++i) {
        RadioButtonControl& radio = radioButtonControls[i];
        const bool hidden = i >= page.radioCount;
        radio.isHidden = hidden;
        radio.manipulator.inaccessible = hidden || s_votingSettingsPreviewActive;
        if (!hidden) {
            const Settings::SettingId id = page.radioSettings[i];
            const Settings::SettingDef& def = Settings::Params::GetSettingDef(id);
            radio.buttonsCount = def.optionCount;
            radio.chosenButtonId = radioValues[i];
            radio.selectedButtonId = radioValues[i];
            radio.SetMessage(id);
            for (u32 option = 0; option < 4; ++option) {
                const bool optionHidden = option >= def.optionCount;
                radio.optionButtonsArray[option].isHidden = optionHidden;
                if (!optionHidden) radio.optionButtonsArray[option].SetMessage(Settings::Params::GetOptionBmg(id, option));
            }
        }
    }

    for (u32 i = 0; i < Settings::Params::maxScrollerCount; ++i) {
        UpDownControl& scroller = upDownControls[i];
        TextUpDownValueControl& value = textUpDown[i];
        const bool hidden = i >= page.scrollerCount;
        scroller.isHidden = hidden;
        scroller.manipulator.inaccessible = hidden || s_votingSettingsPreviewActive;
        value.isHidden = hidden;
        if (!hidden) {
            const Settings::SettingId id = page.scrollerSettings[i];
            const Settings::SettingDef& def = Settings::Params::GetSettingDef(id);
            scroller.optionsCount = def.optionCount;
            scroller.curSelectedOption = scrollerValues[i];
            scroller.SetMessage(id);
            value.activeTextValueControl->SetMessage(Settings::Params::GetOptionBmg(id, scrollerValues[i]));
        }
    }

    MenuInteractable::OnActivate();

    externControls[0]->isHidden = s_votingSettingsPreviewActive;
    externControls[0]->manipulator.inaccessible = s_votingSettingsPreviewActive;
    backButton.isHidden = s_votingSettingsPreviewActive;
    backButton.manipulator.inaccessible = s_votingSettingsPreviewActive;
    controlsManipulatorManager.inaccessible = s_votingSettingsPreviewActive;
    for (u32 i = 0; i < Settings::Params::maxRadioCount; ++i) {
        const bool hidden = i >= page.radioCount;
        radioButtonControls[i].isHidden = hidden;
        radioButtonControls[i].manipulator.inaccessible = hidden || s_votingSettingsPreviewActive;
        if (s_votingSettingsPreviewActive && !hidden) radioButtonControls[i].Init();
    }
    for (u32 i = 0; i < Settings::Params::maxScrollerCount; ++i) {
        const bool hidden = i >= page.scrollerCount;
        upDownControls[i].isHidden = hidden;
        upDownControls[i].manipulator.inaccessible = hidden || s_votingSettingsPreviewActive;
        textUpDown[i].isHidden = hidden;
    }
}

const ut::detail::RuntimeTypeInfo* SettingsPanel::GetRuntimeTypeInfo() const { return Pages::VSSettings::typeInfo; }
void SettingsPanel::OnExternalButtonSelect(PushButton&, u32) { bottomText->SetMessage(BMG_SETTINGS_BOTTOM); }
int SettingsPanel::GetActivePlayerBitfield() const { return activePlayerBitfield; }
int SettingsPanel::GetPlayerBitfield() const { return playerBitfield; }
ManipulatorManager& SettingsPanel::GetManipulatorManager() { return controlsManipulatorManager; }

bool SettingsPanel::HasModifiedMiscSettings() const {
    if (settingsPageId != Settings::SETTINGS_PAGE_MISC) return false;
    const Settings::SettingsPageDef& page = Settings::Params::GetPageDef(settingsPageId);
    const Settings::Mgr& mgr = Settings::Mgr::Get();
    for (u32 i = 0; i < page.radioCount; ++i)
        if (radioValues[i] != mgr.GetSettingValue(page.radioSettings[i])) return true;
    for (u32 i = 0; i < page.scrollerCount; ++i)
        if (scrollerValues[i] != mgr.GetSettingValue(page.scrollerSettings[i])) return true;
    return false;
}

void SettingsPanel::SaveSettings(bool) {
    if (s_votingSettingsPreviewActive) return;
    const Settings::SettingsPageDef& page = Settings::Params::GetPageDef(settingsPageId);
    Settings::Mgr* mgr = Settings::Mgr::sInstance;
    for (u32 i = 0; i < page.radioCount; ++i) mgr->SetSettingValue(page.radioSettings[i], radioValues[i]);
    for (u32 i = 0; i < page.scrollerCount; ++i) mgr->SetSettingValue(page.scrollerSettings[i], scrollerValues[i]);
    mgr->Update();
}

void SettingsPanel::LoadPrevMenuAndSaveSettings(PushButton& button) {
    nextPageId = static_cast<PageId>(SettingsPageSelect::id);
    EndStateAnimated(0, button.GetAnimationFrameSize());
    SaveSettings(true);
}

void SettingsPanel::LoadMainMenuAndSaveSettings(PushButton& button) {
    SaveSettings(true);
    ChangeSectionById(SECTION_MAIN_MENU_FROM_MENU, button);
}

void SettingsPanel::OnBackPress(u32) {
    if (s_votingSettingsPreviewActive) {
        s_votingSettingsPreviewActive = false;
        nextPageId = static_cast<PageId>(SettingsPageSelect::id);
        EndStateAnimated(0, backButton.GetAnimationFrameSize());
        return;
    }
    const bool reloadMenu = HasModifiedMiscSettings();
    backButton.SelectFocus();
    if (reloadMenu) LoadMainMenuAndSaveSettings(backButton);
    else LoadPrevMenuAndSaveSettings(backButton);
}

void SettingsPanel::OnBackButtonClick(PushButton&, u32 hudSlotId) { OnBackPress(hudSlotId); }

void SettingsPanel::OnSaveButtonClick(PushButton& button, u32) {
    if (HasModifiedMiscSettings()) LoadMainMenuAndSaveSettings(button);
    else LoadPrevMenuAndSaveSettings(button);
}

void SettingsPanel::OnRadioButtonClick(RadioButtonControl& radio, u32, u32 optionId) {
    const Settings::SettingsPageDef& page = Settings::Params::GetPageDef(settingsPageId);
    if (radio.id >= page.radioCount) return;
    radioValues[radio.id] = optionId;
}

void SettingsPanel::OnRadioButtonChange(RadioButtonControl& radio, u32, u32 optionId) {
    const Settings::SettingsPageDef& page = Settings::Params::GetPageDef(settingsPageId);
    if (radio.id >= page.radioCount) return;
    bottomText->SetMessage(Settings::Params::GetDescriptionBmg(page.radioSettings[radio.id], optionId));
}

void SettingsPanel::OnUpDownClick(UpDownControl&, u32) { externControls[0]->Select(0); }

void SettingsPanel::OnTextChange(TextUpDownValueControl::TextControl& text, u32 optionId) {
    const u32 index = GetTextId(text);
    const Settings::SettingsPageDef& page = Settings::Params::GetPageDef(settingsPageId);
    if (index >= page.scrollerCount) return;
    const Settings::SettingId id = page.scrollerSettings[index];
    scrollerValues[index] = optionId;
    text.SetMessage(Settings::Params::GetOptionBmg(id, optionId));
    if (!externControls[0]->IsSelected()) bottomText->SetMessage(Settings::Params::GetDescriptionBmg(id, optionId));
}

void SettingsPanel::OnUpDownSelect(UpDownControl& scroller, u32) {
    const Settings::SettingsPageDef& page = Settings::Params::GetPageDef(settingsPageId);
    if (scroller.id >= page.scrollerCount) return;
    bottomText->SetMessage(Settings::Params::GetDescriptionBmg(
        page.scrollerSettings[scroller.id], scroller.curSelectedOption));
}

void SettingsPanel::BeforeControlUpdate() {
    if (s_votingSettingsPreviewActive) {
        if (++s_votingSettingsPreviewFrame >= votingSettingsPreviewDuration) {
            Settings::SettingsPageId nextPage;
            if (AdvanceFroomSettingsPreview(nextPage)) {
                s_votingSettingsPreviewFrame = 0;
                SetVotingPreviewPage(nextPage);
                OnActivate();
            } else {
                s_votingSettingsPreviewActive = false;
                Section* section = SectionMgr::sInstance->curSection;
                if (section != nullptr && section->layerCount > 1) {
                    section->RemovePageLayers(section->layerCount - 1);
                    Pages::SELECTStageMgr* select = section->Get<Pages::SELECTStageMgr>();
                    if (select != nullptr) select->Pages::SELECTStageMgr::OnResume();
                }
            }
        }
        return;
    }

    const SectionId sectionId = SectionMgr::sInstance->curSection->sectionId;
    if (IsVotingSection(sectionId)) {
        Pages::SELECTStageMgr* select = SectionMgr::sInstance->curSection->Get<Pages::SELECTStageMgr>();
        if (select != nullptr && select->countdown.countdown <= 0) OnBackPress(0);
    }
}

}  // namespace UI
}  // namespace Pulsar
