#include <Settings/UI/CustomEngineClassPage.hpp>
#include <Settings/Settings.hpp>
#include <PulsarSystem.hpp>

namespace Pulsar {
namespace UI {

PageId CustomEngineClassPage::GetNextPage() const {
    return static_cast<PageId>(PULPAGE_SETTINGS);
}

void CustomEngineClassPage::OnInit() {
    onCustomDigitClickHandler.subject = this;
    onCustomDigitClickHandler.ptmf = &CustomEngineClassPage::OnDigitClick;
    onCustomBackSpaceClickHandler.subject = this;
    onCustomBackSpaceClickHandler.ptmf = &CustomEngineClassPage::OnBackSpaceClick;
    onCustomOkButtonClickHandler.subject = this;
    onCustomOkButtonClickHandler.ptmf = &CustomEngineClassPage::OnOkButtonClick;
    onCustomBackButtonClickHandler.subject = this;
    onCustomBackButtonClickHandler.ptmf = &CustomEngineClassPage::OnBackButtonClick;
    onCustomBackPressHandler.subject = this;
    onCustomBackPressHandler.ptmf = &CustomEngineClassPage::OnBackPress;

    manipulatorManager.Init(1, false);
    manipulatorManager.SetDistanceFunc(2);
    SetManipulatorManager(manipulatorManager);
    manipulatorManager.SetGlobalHandler(BACK_PRESS, onCustomBackPressHandler, false, false);

    InitControlGroup(15);
    for (u32 i = 0; i < 10; ++i) {
        AddControl(i, numPad[i], 0);
        char variant[16];
        snprintf(variant, sizeof(variant), "Key%d", i);
        numPad[i].Load("button", "RegisterFriendKeyboard", variant, 1, 0, false);
        Text::Info info;
        info.intToPass[0] = i;
        numPad[i].SetMessage(0x13f1, &info);
        numPad[i].buttonId = i;
        numPad[i].SetOnClickHandler(onCustomDigitClickHandler, 0);
    }

    AddControl(10, backSpace, 0);
    backSpace.Load("button", "RegisterFriendKeyboard", "KeyBackSpace", 1, 0, false);
    backSpace.buttonId = 10;
    backSpace.SetOnClickHandler(onCustomBackSpaceClickHandler, 0);

    AddControl(11, okButton, 0);
    okButton.Load("button", "RegisterFriendKeyboard", "OK", 1, 0, false);
    okButton.buttonId = 11;
    okButton.SetOnClickHandler(onCustomOkButtonClickHandler, 0);

    AddControl(13, backButton, 0);
    backButton.Load("button", "Back", "ButtonBack", 1, 0, true);
    backButton.buttonId = 13;
    backButton.SetOnClickHandler(onCustomBackButtonClickHandler, 0);

    AddControl(12, numericBox, 0);
    numericBox.Load(4, "button", "CustomEngineEditBox", "EditBox", "FriendCodeEditBoxLetter", "Letter", 1, false, false);
    lyt::Pane *firstDigitPane = numericBox.layout.GetPaneByName("text_n_00");
    lyt::Pane *lastDigitPane = numericBox.layout.GetPaneByName("text_n_03");
    if (firstDigitPane != nullptr && lastDigitPane != nullptr) {
        const float offset = -0.5f * (firstDigitPane->trans.x + lastDigitPane->trans.x);
        for (u32 i = 0; i <= numericBox.digitCount; ++i) {
            char paneName[16];
            snprintf(paneName, sizeof(paneName), "text_n_%02d", i);
            lyt::Pane *pane = numericBox.layout.GetPaneByName(paneName);
            if (pane != nullptr) pane->trans.x += offset;
        }
    }

    AddControl(14, instructionText, 0);
    instructionText.Load();
}

void CustomEngineClassPage::OnActivate() {
    numericBox.RemoveAllDigits();

    u16 cc = Settings::Mgr::Get().GetCustomEngineClass();
    if (cc < 100 || cc > 9999) cc = 150;
    System::sInstance->netMgr.customEngineClass = cc;

    u16 divisor = cc >= 1000 ? 1000 : 100;
    while (divisor != 0) {
        numericBox.AddDigit((cc / divisor) % 10);
        divisor /= 10;
    }

    instructionText.SetMessage(BMG_CUSTOM_ENGINE_INSTRUCTION);
    numPad[1].SelectInitial(0);
    UpdateOkButton();
}

u32 CustomEngineClassPage::GetEngineClass() {
    u32 cc = static_cast<u32>(numericBox.ComputeSum());
    for (u32 i = numericBox.curDigitCount; i < numericBox.digitCount; ++i) cc /= 10;
    return cc;
}

void CustomEngineClassPage::UpdateOkButton() {
    okButton.SetPlayerBitfield(numericBox.curDigitCount >= 3 && GetEngineClass() >= 100 ? 1 : 0);
}

void CustomEngineClassPage::OnDigitClick(PushButton &digit, u32 hudSlotId) {
    Pages::RegisterFriend::OnDigitClick(digit, hudSlotId);
    UpdateOkButton();
}

void CustomEngineClassPage::OnBackSpaceClick(PushButton &backSpaceButton, u32 hudSlotId) {
    Pages::RegisterFriend::OnBackSpaceClick(backSpaceButton, hudSlotId);
    UpdateOkButton();
}

void CustomEngineClassPage::OnOkButtonClick(PushButton &button, u32) {
    u32 cc = GetEngineClass();
    if (cc < 100) cc = 100;
    if (cc > 9999) cc = 9999;
    System::sInstance->netMgr.customEngineClass = static_cast<u16>(cc);
    Settings::Mgr &settings = Settings::Mgr::Get();
    settings.SetSettingValue(Settings::SETTING_FROOMCC, HOSTCC_CUSTOM);
    settings.SetCustomEngineClass(static_cast<u16>(cc));
    EndStateAnimated(1, button.GetAnimationFrameSize());
}

void CustomEngineClassPage::OnBackButtonClick(CtrlMenuBackButton &button, u32) {
    EndStateAnimated(1, button.GetAnimationFrameSize());
}

void CustomEngineClassPage::OnBackPress(u32) {
    if (numericBox.curDigitCount != 0) {
        numericBox.RemoveDigit();
        UpdateOkButton();
        return;
    }
    EndStateAnimated(1, 0.0f);
}

}  // namespace UI
}  // namespace Pulsar
