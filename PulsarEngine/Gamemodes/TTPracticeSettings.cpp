#include <Gamemodes/TTPracticeSettings.hpp>

namespace Pulsar {
namespace TTPractice {

static const u32 PRACTICE_SETTING_ITEMBOXES_ENABLED = 0;
static const u32 PRACTICE_SETTING_ITEMBOXES_DISABLED = 1;

SettingsPage::SettingsPage() {
    this->externControlCount = 1;
    this->internControlCount = 1;
    this->hasBackButton = true;
    this->nextPageId = static_cast<PageId>(SettingsPage::id);
    this->prevPageId = static_cast<PageId>(ConfirmPage::id);
    this->titleBmg = UI::BMG_TT_PRACTICE_SETTINGS_TITLE;
    this->activePlayerBitfield = 1;
    this->playerBitfield = 1;
    this->movieStartFrame = -1;
    this->extraControlNumber = 0;
    this->isLocked = false;
    this->controlCount = 0;
    this->nextSection = SECTION_NONE;
    this->controlSources = 2;
    this->itemBoxesSetting = AreItemBoxesEnabled() ? PRACTICE_SETTING_ITEMBOXES_ENABLED : PRACTICE_SETTING_ITEMBOXES_DISABLED;

    this->onRadioButtonClickHandler.subject = this;
    this->onRadioButtonClickHandler.ptmf = &SettingsPage::OnRadioButtonClick;
    this->onRadioButtonChangeHandler.subject = this;
    this->onRadioButtonChangeHandler.ptmf = &SettingsPage::OnRadioButtonChange;
    this->onButtonSelectHandler.subject = this;
    this->onButtonSelectHandler.ptmf = &SettingsPage::OnExternalButtonSelect;
    this->onButtonDeselectHandler.subject = this;
    this->onButtonDeselectHandler.ptmf = &Pages::VSSettings::OnButtonDeselect;
    this->onBackPressHandler.subject = this;
    this->onBackPressHandler.ptmf = &SettingsPage::OnBackPress;
    this->onBackButtonClickHandler.subject = this;
    this->onBackButtonClickHandler.ptmf = &SettingsPage::OnBackButtonClick;
    this->onStartPressHandler.subject = this;
    this->onStartPressHandler.ptmf = &MenuInteractable::HandleStartPress;
    this->onButtonClickHandler.subject = this;
    this->onButtonClickHandler.ptmf = &SettingsPage::OnSaveButtonClick;

    this->controlsManipulatorManager.Init(1, false);
    this->SetManipulatorManager(this->controlsManipulatorManager);
    this->controlsManipulatorManager.SetGlobalHandler(START_PRESS, this->onStartPressHandler, false, false);
    this->controlsManipulatorManager.SetGlobalHandler(BACK_PRESS, this->onBackPressHandler, false, false);
}

SettingsPage::~SettingsPage() {}

void SettingsPage::OnInit() {
    this->backButton.SetOnClickHandler(this->onBackButtonClickHandler, 0);
    MenuInteractable::OnInit();
    this->SetTransitionSound(0, 0);
}

UIControl* SettingsPage::CreateExternalControl(u32 controlId) {
    if (controlId != 0) return nullptr;

    PushButton* button = new (PushButton);
    this->AddControl(this->controlCount++, *button, 0);
    button->Load(UI::buttonFolder, "Settings", "SAVE", this->activePlayerBitfield, 0, false);
    return button;
}

UIControl* SettingsPage::CreateControl(u32 controlId) {
    if (controlId != 0) return nullptr;

    RadioButtonControl& radio = this->radioButtonControl;
    this->AddControl(this->controlCount++, radio, 0);

    char option0Variant[12];
    char option1Variant[12];
    snprintf(option0Variant, 12, "Row0Option0");
    snprintf(option1Variant, 12, "Row0Option1");

    const char* optionVariants[5] = {option0Variant, option1Variant, nullptr, nullptr, nullptr};
    radio.Load(2, 0, UI::controlFolder, "RadioBase", "Row0", "RadioOption", optionVariants, 1, 0, 0);
    radio.SetOnClickHandler(this->onRadioButtonClickHandler);
    radio.SetOnChangeHandler(this->onRadioButtonChangeHandler);
    radio.id = 0;
    return &radio;
}

void SettingsPage::SetButtonHandlers(PushButton& button) {
    button.SetOnClickHandler(this->onButtonClickHandler, 0);
    button.SetOnSelectHandler(this->onButtonSelectHandler);
    button.SetOnDeselectHandler(this->onButtonDeselectHandler);
}

void SettingsPage::OnActivate() {
    this->titleBmg = UI::BMG_TT_PRACTICE_SETTINGS_TITLE;
    this->externControls[0]->SelectInitial(0);
    this->bottomText->SetMessage(UI::BMG_TT_PRACTICE_SETTINGS_SAVE_BOTTOM);

    RadioButtonControl& radio = this->radioButtonControl;
    radio.isHidden = false;
    radio.manipulator.inaccessible = false;
    radio.buttonsCount = 2;
    radio.chosenButtonId = this->itemBoxesSetting;
    radio.selectedButtonId = this->itemBoxesSetting;
    radio.SetMessage(UI::BMG_TT_PRACTICE_ITEMBOXES);
    radio.optionButtonsArray[0].isHidden = false;
    radio.optionButtonsArray[0].SetMessage(UI::BMG_TT_PRACTICE_ITEMBOXES_ENABLED);
    radio.optionButtonsArray[1].isHidden = false;
    radio.optionButtonsArray[1].SetMessage(UI::BMG_TT_PRACTICE_ITEMBOXES_DISABLED);
    for (int i = 2; i < 4; ++i) radio.optionButtonsArray[i].isHidden = true;

    MenuInteractable::OnActivate();
}

const ut::detail::RuntimeTypeInfo* SettingsPage::GetRuntimeTypeInfo() const {
    return Pages::VSSettings::typeInfo;
}

void SettingsPage::OnExternalButtonSelect(PushButton& button, u32 hudSlotId) {
    this->bottomText->SetMessage(UI::BMG_TT_PRACTICE_SETTINGS_SAVE_BOTTOM);
}

void SettingsPage::SaveAndReturn(PushButton& button) {
    SetItemBoxesEnabled(this->itemBoxesSetting == PRACTICE_SETTING_ITEMBOXES_ENABLED);
    this->LoadPrevPageById(static_cast<PageId>(ConfirmPage::id), button);
}

void SettingsPage::OnSaveButtonClick(PushButton& button, u32 hudSlotId) {
    this->SaveAndReturn(button);
}

void SettingsPage::OnBackPress(u32 hudSlotId) {
    this->backButton.SelectFocus();
    this->SaveAndReturn(this->backButton);
}

void SettingsPage::OnBackButtonClick(PushButton& button, u32 hudSlotId) {
    this->OnBackPress(hudSlotId);
}

void SettingsPage::OnRadioButtonClick(RadioButtonControl& radioButtonControl, u32 hudSlotId, u32 optionId) {
    this->itemBoxesSetting = optionId;
}

void SettingsPage::OnRadioButtonChange(RadioButtonControl& radioButtonControl, u32 hudSlotId, u32 optionId) {
    this->bottomText->SetMessage(optionId == PRACTICE_SETTING_ITEMBOXES_ENABLED ? UI::BMG_TT_PRACTICE_ITEMBOXES_BOTTOM_ENABLED
                                                                                : UI::BMG_TT_PRACTICE_ITEMBOXES_BOTTOM_DISABLED);
}

}  // namespace TTPractice
}  // namespace Pulsar
