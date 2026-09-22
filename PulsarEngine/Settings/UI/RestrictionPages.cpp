#include <Settings/UI/RestrictionPages.hpp>
#include <Settings/UI/SettingsPanel.hpp>
#include <Settings/Settings.hpp>
#include <Network/Settings/SelectionRestrictions.hpp>
#include <MarioKartWii/3D/Model/Menu/MenuDriverModel.hpp>
#include <MarioKartWii/3D/Model/Menu/MenuModelMgr.hpp>
#include <MarioKartWii/3D/Model/Menu/MenuKartModel.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>

namespace Pulsar {
namespace UI {

static const u32 restrictionDisabledColor = 0xA0000080;

CharacterRestrictionPage::CharacterRestrictionPage() {
    restrictionButtonClickHandler.subject = this;
    restrictionButtonClickHandler.ptmf = &CharacterRestrictionPage::OnRestrictionButtonClick;
    restrictionButtonSelectHandler.subject = this;
    restrictionButtonSelectHandler.ptmf = &CharacterRestrictionPage::OnRestrictionButtonSelect;
    restrictionButtonDeselectHandler.subject = this;
    restrictionButtonDeselectHandler.ptmf = &CharacterRestrictionPage::OnRestrictionButtonDeselect;
    restrictionBackPressHandler.subject = this;
    restrictionBackPressHandler.ptmf = &CharacterRestrictionPage::OnRestrictionBackPress;
    restrictionBackButtonClickHandler.subject = this;
    restrictionBackButtonClickHandler.ptmf = &CharacterRestrictionPage::OnRestrictionBackButtonClick;
}

void CharacterRestrictionPage::OnInit() {
    Section *section = SectionMgr::sInstance->curSection;
    Page *characterSelect = section->pages[PAGE_CHARACTER_SELECT];
    section->pages[PAGE_CHARACTER_SELECT] = this;
    Restrictions::SetCharacterRestrictionConfigActive(true);
    Pages::CharacterSelect::OnInit();
    Restrictions::SetCharacterRestrictionConfigActive(false);
    section->pages[PAGE_CHARACTER_SELECT] = characterSelect;
    prevPageId = static_cast<PageId>(SettingsPanel::id);
    nextPageId = static_cast<PageId>(SettingsPanel::id);
    controlsManipulatorManager.SetGlobalHandler(BACK_PRESS, restrictionBackPressHandler, false, false);
    backButton.SetOnClickHandler(restrictionBackButtonClickHandler, 0);
    BindButtons();
}

void CharacterRestrictionPage::OnActivate() {
    Restrictions::SetCharacterRestrictionConfigActive(true);
    Pages::CharacterSelect::OnActivate();
    Restrictions::SetCharacterRestrictionConfigActive(false);
    BindButtons();
    UpdateButtonVisuals();
}

void CharacterRestrictionPage::OnDeactivate() {
    Restrictions::SetCharacterRestrictionConfigActive(false);
    Page::OnDeactivate();
}

void CharacterRestrictionPage::AfterControlUpdate() {
    Pages::CharacterSelect::AfterControlUpdate();
    UpdateButtonVisuals();
}

void CharacterRestrictionPage::BindButtons() {
    CtrlMenuCharacterSelect::ButtonDriver *buttons = ctrlMenuCharSelect.driverButtonsArray;
    if (buttons == nullptr) return;
    for (u32 slot = 0; slot < Restrictions::CHARACTER_SLOT_COUNT; ++slot) {
        buttons[slot].SetOnClickHandler(restrictionButtonClickHandler, 0);
        buttons[slot].SetOnSelectHandler(restrictionButtonSelectHandler);
        buttons[slot].SetOnDeselectHandler(restrictionButtonDeselectHandler);
    }
}

void CharacterRestrictionPage::UpdateButtonVisuals() {
    const u32 mask = Settings::Mgr::Get().GetCharacterRestrictionMask();
    CtrlMenuCharacterSelect::ButtonDriver *buttons = ctrlMenuCharSelect.driverButtonsArray;
    if (buttons == nullptr) return;
    for (u32 slot = 0; slot < Restrictions::CHARACTER_SLOT_COUNT; ++slot) {
        const bool enabled = ((mask >> slot) & 1) != 0;
        if (buttons[slot].IsSelected())
            buttons[slot].SetButtonColours(0);
        else
            buttons[slot].ResetButtonColours(0);

        if (!enabled && buttons[slot].black_base != nullptr) {
            const ut::Color red(0xA00000FF);
            for (u32 vertex = 0; vertex < 4; ++vertex) buttons[slot].black_base->SetVtxColor(vertex, red);
            buttons[slot].black_base->alpha = 0x80;
        }
    }
}

void CharacterRestrictionPage::OnRestrictionButtonClick(PushButton &button, u32) {
    CtrlMenuCharacterSelect::ButtonDriver *driver = static_cast<CtrlMenuCharacterSelect::ButtonDriver *>(&button);
    const u32 slot = driver - ctrlMenuCharSelect.driverButtonsArray;
    if (slot >= Restrictions::CHARACTER_SLOT_COUNT) return;
    u32 mask = Settings::Mgr::Get().GetCharacterRestrictionMask() ^ (1u << slot);
    if ((mask & Restrictions::ALL_CHARACTERS) == 0) mask = Restrictions::ALL_CHARACTERS;
    Settings::Mgr::Get().SetCharacterRestrictionMask(mask);
    UpdateButtonVisuals();
}

void CharacterRestrictionPage::OnRestrictionButtonSelect(PushButton &button, u32 hudSlotId) {
    CtrlMenuCharacterSelect::ButtonDriver *driver = static_cast<CtrlMenuCharacterSelect::ButtonDriver *>(&button);
    Pages::CharacterSelect::OnButtonDriverSelect(driver, driver->buttonId, hudSlotId);
    UpdateButtonVisuals();
}

void CharacterRestrictionPage::OnRestrictionButtonDeselect(PushButton &, u32) { UpdateButtonVisuals(); }

void CharacterRestrictionPage::OnRestrictionBackPress(u32) {
    backButton.SelectFocus();
    nextPageId = static_cast<PageId>(SettingsPanel::id);
    EndStateAnimated(0, backButton.GetAnimationFrameSize());
}

VehicleRestrictionWeightPage::VehicleRestrictionWeightPage() {
    externControlCount = 0;
    internControlCount = 3;
    hasBackButton = true;
    prevPageId = static_cast<PageId>(SettingsPanel::id);
    nextPageId = PAGE_NONE;
    titleBmg = Settings::SETTING_KARTSELECT;
    activePlayerBitfield = 1;
    movieStartFrame = -1;
    extraControlNumber = 0;
    isLocked = false;
    controlCount = 0;
    nextSection = SECTION_NONE;
    controlSources = 2;

    onButtonClickHandler.subject = this;
    onButtonClickHandler.ptmf = &VehicleRestrictionWeightPage::OnButtonClick;
    onButtonSelectHandler.subject = this;
    onButtonSelectHandler.ptmf = &VehicleRestrictionWeightPage::OnButtonSelect;
    onBackPressHandler.subject = this;
    onBackPressHandler.ptmf = &VehicleRestrictionWeightPage::OnBackPress;
    onBackButtonClickHandler.subject = this;
    onBackButtonClickHandler.ptmf = &VehicleRestrictionWeightPage::OnBackButtonClick;

    controlsManipulatorManager.Init(1, false);
    SetManipulatorManager(controlsManipulatorManager);
    controlsManipulatorManager.SetGlobalHandler(BACK_PRESS, onBackPressHandler, false, false);
}

void VehicleRestrictionWeightPage::OnInit() {
    MenuInteractable::OnInit();
    SetTransitionSound(0, 0);
    backButton.SetOnClickHandler(onBackButtonClickHandler, 0);
}

UIControl *VehicleRestrictionWeightPage::CreateControl(u32 id) {
    PushButton &button = buttons[id];
    AddControl(controlCount++, button, 0);
    char variant[16];
    snprintf(variant, 16, "Page%d", id);
    button.Load(UI::buttonFolder, "SettingsPageSelect", variant, activePlayerBitfield, 0, false);
    button.buttonId = id;
    SetButtonHandlers(button);
    return &button;
}

void VehicleRestrictionWeightPage::SetButtonHandlers(PushButton &button) {
    button.SetOnClickHandler(onButtonClickHandler, 0);
    button.SetOnSelectHandler(onButtonSelectHandler);
}

void VehicleRestrictionWeightPage::OnActivate() {
    static const u32 messages[3] = {BMG_RESTRICTION_LIGHT, BMG_RESTRICTION_MEDIUM, BMG_RESTRICTION_HEAVY};
    for (u32 i = 0; i < 3; ++i) buttons[i].SetMessage(messages[i]);
    buttons[0].Select(0);
    bottomText->SetMessage(BMG_RESTRICTION_WEIGHT_BOTTOM);
    MenuInteractable::OnActivate();
}

void VehicleRestrictionWeightPage::OnButtonClick(PushButton &button, u32) {
    VehicleRestrictionPage *page = ExpSection::GetSection()->GetPulPage<VehicleRestrictionPage>();
    if (page == nullptr || button.buttonId < 0 || button.buttonId >= 3) return;
    page->SetWeight(button.buttonId);
    nextPageId = static_cast<PageId>(VehicleRestrictionPage::id);
    EndStateAnimated(0, button.GetAnimationFrameSize());
}

void VehicleRestrictionWeightPage::OnButtonSelect(PushButton &, u32) {
    bottomText->SetMessage(BMG_RESTRICTION_WEIGHT_BOTTOM);
}

void VehicleRestrictionWeightPage::OnBackPress(u32) {
    backButton.SelectFocus();
    nextPageId = static_cast<PageId>(SettingsPanel::id);
    EndStateAnimated(0, backButton.GetAnimationFrameSize());
}

void VehicleRestrictionWeightPage::OnBackButtonClick(PushButton &, u32 hudSlotId) { OnBackPress(hudSlotId); }

VehicleRestrictionPage::VehicleRestrictionPage()
    : weight(0), savedCharacter(CHARACTER_NONE), savedComboCharacter(CHARACTER_NONE), hasSavedCharacter(false) {
    restrictionButtonClickHandler.subject = this;
    restrictionButtonClickHandler.ptmf = &VehicleRestrictionPage::OnRestrictionButtonClick;
    restrictionButtonSelectHandler.subject = this;
    restrictionButtonSelectHandler.ptmf = &VehicleRestrictionPage::OnRestrictionButtonSelect;
    restrictionBackPressHandler.subject = this;
    restrictionBackPressHandler.ptmf = &VehicleRestrictionPage::OnRestrictionBackPress;
    restrictionBackButtonClickHandler.subject = this;
    restrictionBackButtonClickHandler.ptmf = &VehicleRestrictionPage::OnRestrictionBackButtonClick;
}

void VehicleRestrictionPage::OnInit() {
    Restrictions::SetVehicleRestrictionConfigActive(true);
    Pages::KartSelect::OnInit();
    Restrictions::SetVehicleRestrictionConfigActive(false);
    prevPageId = static_cast<PageId>(VehicleRestrictionWeightPage::id);
    nextPageId = static_cast<PageId>(VehicleRestrictionWeightPage::id);
    controlsManipulatorManager.SetGlobalHandler(BACK_PRESS, restrictionBackPressHandler, false, false);
    backButton.SetOnClickHandler(restrictionBackButtonClickHandler, 0);
}

void VehicleRestrictionPage::OnActivate() {
    static const CharacterId displayCharacters[3] = {BABY_MARIO, MARIO, WARIO};
    if (weight >= Restrictions::VEHICLE_WEIGHT_COUNT) weight = 0;

    if (!hasSavedCharacter) {
        savedCharacter = SectionMgr::sInstance->sectionParams->characters[0];
        savedComboCharacter = SectionMgr::sInstance->sectionParams->combos[0].selCharacter;
        hasSavedCharacter = true;
    }
    SectionMgr::sInstance->sectionParams->characters[0] = displayCharacters[weight];
    SectionMgr::sInstance->sectionParams->combos[0].selCharacter = displayCharacters[weight];

    MenuModelMgr *modelMgr = MenuModelMgr::sInstance;
    ArchiveMgr *archiveMgr = ArchiveMgr::sInstance;
    if (modelMgr != nullptr && modelMgr->kartModels != nullptr && archiveMgr != nullptr) {
        if (modelMgr->driverModels != nullptr) {
            MenuDriverModel *driverModel = modelMgr->driverModels->players[0].playerModel;
            if (driverModel != nullptr) driverModel->SwitchState(0, MenuDriverModel::MENUDRIVERMODEL_STATE_ONCHARSELECT);
        }
        modelMgr->RequestDriverModel(0, displayCharacters[weight]);
        if (modelMgr->driverModels != nullptr) {
            MenuDriverModel *driverModel = modelMgr->driverModels->players[0].playerModel;
            if (driverModel != nullptr) driverModel->SwitchState(0, MenuDriverModel::MENUDRIVERMODEL_STATE_ONCHARSELECT);
        }
        archiveMgr->WaitForLoad();
        modelMgr->ResetKartModels(0);
        archiveMgr->RequestLoadKartArchives(0, displayCharacters[weight], 2);
        archiveMgr->WaitForLoad();
        modelMgr->kartModels->Load();
    }

    Restrictions::SetVehicleRestrictionConfigActive(true);
    Pages::KartSelect::OnActivate();
    Restrictions::SetVehicleRestrictionConfigActive(false);
    BindButtons();
    UpdateButtonVisuals();
}

void VehicleRestrictionPage::OnDeactivate() {
    Restrictions::SetVehicleRestrictionConfigActive(false);
    RestoreCharacter();
    Page::OnDeactivate();
}

void VehicleRestrictionPage::SetButtonHandlers(PushButton &button) {
    button.SetOnClickHandler(restrictionButtonClickHandler, 0);
    button.SetOnSelectHandler(restrictionButtonSelectHandler);
    button.onDeselectHandler = nullptr;
}

void VehicleRestrictionPage::RestoreCharacter() {
    if (!hasSavedCharacter) return;
    SectionMgr::sInstance->sectionParams->characters[0] = savedCharacter;
    SectionMgr::sInstance->sectionParams->combos[0].selCharacter = savedComboCharacter;
    hasSavedCharacter = false;
}

void VehicleRestrictionPage::BindButtons() {
    for (u32 position = 0; position < Restrictions::VEHICLES_PER_WEIGHT; ++position) {
        ButtonMachine *button = GetButtonMachineById(static_cast<u8>(kartsSortedByWeight[weight][position]));
        if (button == nullptr) continue;
        SetButtonHandlers(*button);
    }
}

void VehicleRestrictionPage::UpdateButtonVisuals() {
    const u16 mask = Settings::Mgr::Get().GetVehicleRestrictionMask(weight);
    for (u32 position = 0; position < Restrictions::VEHICLES_PER_WEIGHT; ++position) {
        ButtonMachine *button = GetButtonMachineById(static_cast<u8>(kartsSortedByWeight[weight][position]));
        if (button == nullptr) continue;
        lyt::Pane *pane = button->layout.GetPaneByName("chara");
        if (pane != nullptr) ResetMatColor(pane, (mask >> position) & 1 ? 0 : restrictionDisabledColor);
    }
}

void VehicleRestrictionPage::OnRestrictionButtonClick(PushButton &button, u32) {
    const KartId kart = static_cast<KartId>(button.buttonId);
    const u32 position = Restrictions::GetVehiclePosition(kart);
    if (position >= Restrictions::VEHICLES_PER_WEIGHT) return;
    u16 mask = Settings::Mgr::Get().GetVehicleRestrictionMask(weight) ^ (1 << position);
    if ((mask & Restrictions::ALL_VEHICLES) == 0) mask = Restrictions::ALL_VEHICLES;
    Settings::Mgr::Get().SetVehicleRestrictionMask(weight, mask);
    UpdateButtonVisuals();
}

void VehicleRestrictionPage::OnRestrictionButtonSelect(PushButton &button, u32 hudSlotId) {
    Pages::KartSelect::OnExternalButtonSelect(button, hudSlotId);
}

void VehicleRestrictionPage::OnRestrictionBackPress(u32) {
    backButton.SelectFocus();
    RestoreCharacter();
    nextPageId = static_cast<PageId>(VehicleRestrictionWeightPage::id);
    EndStateAnimated(0, backButton.GetAnimationFrameSize());
}

}  // namespace UI
}  // namespace Pulsar
