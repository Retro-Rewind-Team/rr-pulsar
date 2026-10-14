#include <Settings/UI/SettingsPanel.hpp>
#include <Settings/UI/SettingsPageSelect.hpp>
#include <Settings/UI/CustomEngineClassPage.hpp>
#include <Settings/UI/RestrictionPages.hpp>
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

static u8 GetFroomCCSettingValue(u8 option) {
    static const u8 values[] = {HOSTCC_150, HOSTCC_100, HOSTCC_500, HOSTCC_CUSTOM};
    return option < 4 ? values[option] : HOSTCC_150;
}

static u8 GetFroomCCOption(u8 settingValue) {
    for (u8 option = 0; option < 4; ++option) {
        if (GetFroomCCSettingValue(option) == settingValue)
            return option;
    }
    return 0;
}

static bool IsVotingSection(SectionId id) {
    return (id >= SECTION_P1_WIFI_FROOM_VS_VOTING && id <= SECTION_P2_WIFI_FROOM_COIN_VOTING) || id == SECTION_P1_WIFI_VS_VOTING || id == SECTION_P2_WIFI_VS_VOTING
      || id == SECTION_P1_WIFI_BATTLE_VOTING || id == SECTION_P2_WIFI_BATTLE_VOTING;
}

static bool IsBattleVotingSection(SectionId id) {
    return id == SECTION_P1_WIFI_BATTLE_VOTING || id == SECTION_P2_WIFI_BATTLE_VOTING || id == SECTION_P1_WIFI_FROOM_BALLOON_VOTING || id == SECTION_P2_WIFI_FROOM_BALLOON_VOTING
      || id == SECTION_P1_WIFI_FROOM_COIN_VOTING || id == SECTION_P2_WIFI_FROOM_COIN_VOTING;
}

SettingsPanel::SettingsPanel() {
    settingsPageId = Settings::SETTINGS_PAGE_RACE;
    pageIndex = 0;
    pageCount = 1;
    externControlCount = 3;
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
    else if (sectionId == SECTION_P1_WIFI || sectionId == SECTION_P2_WIFI || sectionId == SECTION_P1_WIFI_FROM_FROOM_RACE || sectionId == SECTION_P2_WIFI_FROM_FROOM_RACE
      || sectionId == SECTION_P1_WIFI_FROM_FIND_FRIEND || sectionId == SECTION_P2_WIFI_FROM_FIND_FRIEND)
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
    Settings::Mgr *mgr = Settings::Mgr::sInstance;
    mgr->SetLastSelectedCup(CupsConfig::sInstance->lastSelectedCup);
    mgr->RequestSave();
}

void SettingsPanel::SetPage(Settings::SettingsPageId id) {
    settingsPageId = id;
    pageIndex = 0;
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
    SettingsPanel *panel = ExpSection::GetSection()->GetPulPage<SettingsPanel>();
    if (panel != nullptr)
        panel->SetPage(page);
}

void SettingsPanel::ApplyVotingPreviewHostSettings() {
    const Network::Mgr &netMgr = System::sInstance->netMgr;
    memset(s_hostPreviewValues, 0, sizeof(s_hostPreviewValues));
    if (!netMgr.hasHostSettingsPreview)
        return;

    const SectionId sectionId = SectionMgr::sInstance->curSection->sectionId;
    const bool isBattle = IsBattleVotingSection(sectionId);
    const bool isKO = (netMgr.hostContext & (1 << PULSAR_MODE_KO)) || (netMgr.hostContext & (1 << PULSAR_MODE_LAPKO));
    const bool isOTT = netMgr.hostContext & (1 << PULSAR_MODE_OTT);
    const bool isRoyale = netMgr.hostContext2 & (1 << PULSAR_MODE_BATTLEROYALE);
    const bool isExtendedTeams = netMgr.hostContext & (1 << PULSAR_EXTENDEDTEAMS);

    Settings::SettingsPageId pages[6];
    const u32 pageCount = Settings::Params::BuildHostRulePages(pages, isBattle, isKO, isOTT, isRoyale, isExtendedTeams);
    u32 offset = 0;
    for (u32 page = 0; page < pageCount; ++page) {
        const Settings::SettingsPageDef &def = Settings::Params::GetPageDef(pages[page]);
        for (u32 i = 0; i < def.radioCount && offset < Network::HOST_SETTINGS_PREVIEW_COUNT; ++i) {
            const Settings::SettingId id = def.radioSettings[i];
            const Settings::SettingDef &setting = Settings::Params::GetSettingDef(id);
            const u8 value = netMgr.hostSettingsPreview[offset++];
            s_hostPreviewValues[Settings::Params::GetSettingIndex(id)] = value < setting.optionCount || (id == Settings::SETTING_FROOMCC && value == HOSTCC_CUSTOM) ? value : 0;
        }
        for (u32 i = 0; i < def.scrollerCount && offset < Network::HOST_SETTINGS_PREVIEW_COUNT; ++i) {
            const Settings::SettingId id = def.scrollerSettings[i];
            const Settings::SettingDef &setting = Settings::Params::GetSettingDef(id);
            const u8 value = netMgr.hostSettingsPreview[offset++];
            s_hostPreviewValues[Settings::Params::GetSettingIndex(id)] = value < setting.optionCount ? value : 0;
        }
    }
}

void SettingsPanel::OnInit() {
    backButton.SetOnClickHandler(onBackButtonClickHandler, 0);
    MenuInteractable::OnInit();
    SetTransitionSound(0, 0);
}

UIControl *SettingsPanel::CreateExternalControl(u32 id) {
    PushButton *button = new (PushButton);
    AddControl(controlCount++, *button, 0);
    button->Load(UI::buttonFolder, "Settings", id == 0 ? "SAVE" : id == 1 ? "LEFT" : "RIGHT", activePlayerBitfield, 0, false);
    button->buttonId = id;
    if (id != 0) {
        Text::Info info;
        wchar_t arrow[2] = {id == 1 ? L'\u2190' : L'\u2192', 0};
        info.strings[0] = arrow;
        button->SetMessage(BMG_TEXT, &info);
    }
    return button;
}

UIControl *SettingsPanel::CreateControl(u32 id) {
    if (id < Settings::Params::maxRadioCount) {
        RadioButtonControl &radio = radioButtonControls[id];
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
        const char *options[5] = {option0, option1, option2, option3, nullptr};
        radio.Load(4, 0, UI::controlFolder, "RadioBase", variant, "RadioOption", options, 1, 0, 0);
        radio.SetOnClickHandler(onRadioButtonClickHandler);
        radio.SetOnChangeHandler(onRadioButtonChangeHandler);
        radio.id = id;
    } else if (id < Settings::Params::maxRadioCount + Settings::Params::maxScrollerCount) {
        id -= Settings::Params::maxRadioCount;
        UpDownControl &scroller = upDownControls[id];
        AddControl(controlCount++, scroller, 0);
        char variant[12];
        snprintf(variant, 12, "UpDown%d", id);
        scroller.Load(7, 0, UI::controlFolder, "UpDownBase", variant, "UpDownR", "Right", "UpDownL", "Left", &textUpDown[id], 1, 0, false, true, true);
        scroller.SetOnClickHandler(onUpDownClickHandler);
        scroller.SetOnSelectHandler(onUpDownSelectHandler);
        scroller.id = id;
        textUpDown[id].Load(UI::controlFolder, "UpDownValue", "Value", "UpDownText", "Text");
        textUpDown[id].SetOnTextChangeHandler(onTextChangeHandler);
    }
    return nullptr;
}

void SettingsPanel::SetButtonHandlers(PushButton &button) {
    button.SetOnClickHandler(onButtonClickHandler, 0);
    button.SetOnSelectHandler(onButtonSelectHandler);
    button.SetOnDeselectHandler(onButtonDeselectHandler);
}

void SettingsPanel::LoadCurrentValues() {
    const Settings::SettingsPageDef &page = Settings::Params::GetPageDef(settingsPageId);
    const Settings::Mgr &mgr = Settings::Mgr::Get();
    for (u32 i = 0; i < page.radioCount; ++i) {
        const Settings::SettingId id = page.radioSettings[i];
        const u8 value = s_votingSettingsPreviewActive ? s_hostPreviewValues[Settings::Params::GetSettingIndex(id)] : mgr.GetSettingValue(id);
        radioValues[i] = id == Settings::SETTING_FROOMCC ? GetFroomCCOption(value) : value;
    }
    for (u32 i = 0; i < page.scrollerCount; ++i) {
        const Settings::SettingId id = page.scrollerSettings[i];
        scrollerValues[i] = s_votingSettingsPreviewActive ? s_hostPreviewValues[Settings::Params::GetSettingIndex(id)] : mgr.GetSettingValue(id);
    }
}

void SettingsPanel::OnActivate() {
    const Settings::SettingsPageDef &page = Settings::Params::GetPageDef(settingsPageId);
    titleBmg = page.nameBmg + 0x1F;
    LoadCurrentValues();
    if (!s_votingSettingsPreviewActive)
        externControls[0]->SelectInitial(0);
    bottomText->SetMessage(BMG_SETTINGS_BOTTOM);
    MenuInteractable::OnActivate();
    SetControlsHidden(false);
}

void SettingsPanel::OnResume() {
    MenuInteractable::OnResume();
    SetControlsHidden(false);
}

void SettingsPanel::SetControlsHidden(bool hidden) {
    const Settings::SettingsPageDef &page = Settings::Params::GetPageDef(settingsPageId);
    const bool controlsHidden = hidden || s_votingSettingsPreviewActive;
    u32 firstRow = 0;
    u32 firstScroller = 0;
    u32 radioCount = 0;
    u32 scrollerCount = 0;
    u32 radioOffset = 0;
    u32 scrollerOffset = 0;
    pageCount = 0;
    do {
        u32 pageRadioCount = page.radioCount - radioOffset;
        if (pageRadioCount > Settings::Params::maxRadioCount)
            pageRadioCount = Settings::Params::maxRadioCount;
        u32 pageScrollerCount = page.scrollerCount - scrollerOffset;
        if (pageScrollerCount > Settings::Params::maxScrollerCount)
            pageScrollerCount = Settings::Params::maxScrollerCount;
        if (pageCount == pageIndex) {
            firstRow = radioOffset;
            firstScroller = scrollerOffset;
            radioCount = pageRadioCount;
            scrollerCount = pageScrollerCount;
        }
        radioOffset += pageRadioCount;
        scrollerOffset += pageScrollerCount;
        ++pageCount;
    } while (radioOffset < page.radioCount || scrollerOffset < page.scrollerCount);

    titleText->isHidden = hidden;
    bottomText->isHidden = hidden;
    for (u32 i = 0; i < externControlCount; ++i) {
        const bool buttonHidden = controlsHidden || (i != 0 && pageCount == 1);
        externControls[i]->isHidden = buttonHidden;
        externControls[i]->manipulator.inaccessible = buttonHidden;
    }
    backButton.isHidden = controlsHidden;
    backButton.manipulator.inaccessible = controlsHidden;
    controlsManipulatorManager.inaccessible = controlsHidden;

    for (u32 i = 0; i < Settings::Params::maxRadioCount; ++i) {
        RadioButtonControl &radio = radioButtonControls[i];
        const bool rowHidden = hidden || i >= radioCount;
        radio.id = firstRow + i;
        if (!rowHidden) {
            const Settings::SettingId id = page.radioSettings[radio.id];
            const Settings::SettingDef &def = Settings::Params::GetSettingDef(id);
            radio.buttonsCount = def.optionCount;
            radio.chosenButtonId = radioValues[radio.id];
            radio.selectedButtonId = radioValues[radio.id];
            radio.SetMessage(id);
            for (u32 option = 0; option < 4; ++option) {
                const bool optionHidden = option >= def.optionCount;
                radio.optionButtonsArray[option].isHidden = optionHidden;
                if (!optionHidden)
                    radio.optionButtonsArray[option].SetMessage(Settings::Params::GetOptionBmg(id, option));
            }
            radio.Init();
        }
        radio.isHidden = rowHidden;
        radio.manipulator.inaccessible = rowHidden || s_votingSettingsPreviewActive;
    }
    for (u32 i = 0; i < Settings::Params::maxScrollerCount; ++i) {
        UpDownControl &scroller = upDownControls[i];
        TextUpDownValueControl &value = textUpDown[i];
        const bool rowHidden = hidden || i >= scrollerCount;
        scroller.id = firstScroller + i;
        if (!rowHidden) {
            const Settings::SettingId id = page.scrollerSettings[scroller.id];
            const Settings::SettingDef &def = Settings::Params::GetSettingDef(id);
            scroller.SetMessage(id);
            scroller.SelectInitial(def.optionCount, scrollerValues[scroller.id]);
            scroller.Init();
        }
        scroller.isHidden = rowHidden;
        scroller.manipulator.inaccessible = rowHidden || s_votingSettingsPreviewActive;
        value.isHidden = rowHidden;
    }
}

const ut::detail::RuntimeTypeInfo *SettingsPanel::GetRuntimeTypeInfo() const {
    return Pages::VSSettings::typeInfo;
}
void SettingsPanel::OnExternalButtonSelect(PushButton &, u32) {
    bottomText->SetMessage(BMG_SETTINGS_BOTTOM);
}
int SettingsPanel::GetActivePlayerBitfield() const {
    return activePlayerBitfield;
}
int SettingsPanel::GetPlayerBitfield() const {
    return playerBitfield;
}
ManipulatorManager &SettingsPanel::GetManipulatorManager() {
    return controlsManipulatorManager;
}

bool SettingsPanel::HasModifiedMiscSettings() const {
    if (settingsPageId != Settings::SETTINGS_PAGE_MISC)
        return false;
    const Settings::SettingsPageDef &page = Settings::Params::GetPageDef(settingsPageId);
    const Settings::Mgr &mgr = Settings::Mgr::Get();
    for (u32 i = 0; i < page.radioCount; ++i)
        if (radioValues[i] != mgr.GetSettingValue(page.radioSettings[i]))
            return true;
    for (u32 i = 0; i < page.scrollerCount; ++i)
        if (scrollerValues[i] != mgr.GetSettingValue(page.scrollerSettings[i]))
            return true;
    return false;
}

void SettingsPanel::SaveSettings(bool) {
    if (s_votingSettingsPreviewActive)
        return;
    const Settings::SettingsPageDef &page = Settings::Params::GetPageDef(settingsPageId);
    Settings::Mgr *mgr = Settings::Mgr::sInstance;
    for (u32 i = 0; i < page.radioCount; ++i) {
        const Settings::SettingId id = page.radioSettings[i];
        const u8 value = id == Settings::SETTING_FROOMCC ? GetFroomCCSettingValue(radioValues[i]) : radioValues[i];
        mgr->SetSettingValue(id, value);
    }
    for (u32 i = 0; i < page.scrollerCount; ++i) mgr->SetSettingValue(page.scrollerSettings[i], scrollerValues[i]);
    mgr->Update();
}

void SettingsPanel::LoadPrevMenuAndSaveSettings(PushButton &button) {
    nextPageId = static_cast<PageId>(SettingsPageSelect::id);
    EndStateAnimated(0, button.GetAnimationFrameSize());
    SaveSettings(true);
}

void SettingsPanel::LoadMainMenuAndSaveSettings(PushButton &button) {
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
    if (reloadMenu)
        LoadMainMenuAndSaveSettings(backButton);
    else
        LoadPrevMenuAndSaveSettings(backButton);
}

void SettingsPanel::OnBackButtonClick(PushButton &, u32 hudSlotId) {
    OnBackPress(hudSlotId);
}

void SettingsPanel::OnSaveButtonClick(PushButton &button, u32 hudSlotId) {
    if (button.buttonId != 0) {
        externControls[0]->Select(hudSlotId);
        pageIndex = (pageIndex + (button.buttonId == 1 ? pageCount - 1 : 1)) % pageCount;
        SetControlsHidden(false);
        bottomText->SetMessage(BMG_SETTINGS_BOTTOM);
        return;
    }
    if (HasModifiedMiscSettings())
        LoadMainMenuAndSaveSettings(button);
    else
        LoadPrevMenuAndSaveSettings(button);
}

void SettingsPanel::OnRadioButtonClick(RadioButtonControl &radio, u32, u32 optionId) {
    const Settings::SettingsPageDef &page = Settings::Params::GetPageDef(settingsPageId);
    if (radio.id >= page.radioCount)
        return;
    radioValues[radio.id] = optionId;
    const Settings::SettingId settingId = page.radioSettings[radio.id];
    if (settingId == Settings::SETTING_FROOMCC && optionId == 3) {
        ExpSection *section = ExpSection::GetSection();
        if (section != nullptr && section->GetPulPage<CustomEngineClassPage>() != nullptr) {
            nextPageId = static_cast<PageId>(CustomEngineClassPage::id);
            EndStateAnimated(0, 0.0f);
        }
    } else if (settingId == Settings::SETTING_CHARSELECT && optionId == CHARACTER_RESTRICT_ENABLED) {
        ExpSection *section = ExpSection::GetSection();
        if (section != nullptr && section->GetPulPage<CharacterRestrictionPage>() != nullptr) {
            SaveSettings(true);
            nextPageId = static_cast<PageId>(CharacterRestrictionPage::id);
            EndStateAnimated(0, 0.0f);
        }
    } else if (settingId == Settings::SETTING_KARTSELECT && optionId == VEHICLE_RESTRICT_ENABLED) {
        ExpSection *section = ExpSection::GetSection();
        if (section != nullptr && section->GetPulPage<VehicleRestrictionWeightPage>() != nullptr) {
            SaveSettings(true);
            nextPageId = static_cast<PageId>(VehicleRestrictionWeightPage::id);
            EndStateAnimated(0, 0.0f);
        }
    }
}

void SettingsPanel::OnRadioButtonChange(RadioButtonControl &radio, u32, u32 optionId) {
    const Settings::SettingsPageDef &page = Settings::Params::GetPageDef(settingsPageId);
    if (radio.id >= page.radioCount)
        return;
    bottomText->SetMessage(Settings::Params::GetDescriptionBmg(page.radioSettings[radio.id], optionId));
}

void SettingsPanel::OnUpDownClick(UpDownControl &, u32) {
    externControls[0]->Select(0);
}

void SettingsPanel::OnTextChange(TextUpDownValueControl::TextControl &text, u32 optionId) {
    const u32 index = upDownControls[GetTextId(text)].id;
    const Settings::SettingsPageDef &page = Settings::Params::GetPageDef(settingsPageId);
    if (index >= page.scrollerCount)
        return;
    const Settings::SettingId id = page.scrollerSettings[index];
    scrollerValues[index] = optionId;
    text.SetMessage(Settings::Params::GetOptionBmg(id, optionId));
    if (!externControls[0]->IsSelected())
        bottomText->SetMessage(Settings::Params::GetDescriptionBmg(id, optionId));
}

void SettingsPanel::OnUpDownSelect(UpDownControl &scroller, u32) {
    const Settings::SettingsPageDef &page = Settings::Params::GetPageDef(settingsPageId);
    if (scroller.id >= page.scrollerCount)
        return;
    bottomText->SetMessage(Settings::Params::GetDescriptionBmg(page.scrollerSettings[scroller.id], scroller.curSelectedOption));
}

void SettingsPanel::BeforeControlUpdate() {
    if (s_votingSettingsPreviewActive) {
        if (++s_votingSettingsPreviewFrame >= votingSettingsPreviewDuration) {
            if (pageIndex + 1 < pageCount) {
                ++pageIndex;
                s_votingSettingsPreviewFrame = 0;
                SetControlsHidden(false);
                return;
            }
            Settings::SettingsPageId nextPage;
            if (AdvanceFroomSettingsPreview(nextPage)) {
                s_votingSettingsPreviewFrame = 0;
                SetVotingPreviewPage(nextPage);
                OnActivate();
            } else {
                s_votingSettingsPreviewActive = false;
                Section *section = SectionMgr::sInstance->curSection;
                if (section != nullptr && section->layerCount > 1) {
                    section->RemovePageLayers(section->layerCount - 1);
                    Pages::SELECTStageMgr *select = section->Get<Pages::SELECTStageMgr>();
                    if (select != nullptr)
                        select->Pages::SELECTStageMgr::OnResume();
                }
            }
        }
        return;
    }

    const SectionId sectionId = SectionMgr::sInstance->curSection->sectionId;
    if (IsVotingSection(sectionId)) {
        Pages::SELECTStageMgr *select = SectionMgr::sInstance->curSection->Get<Pages::SELECTStageMgr>();
        if (select != nullptr && select->countdown.countdown <= 0)
            OnBackPress(0);
    }
}

}  // namespace UI
}  // namespace Pulsar
