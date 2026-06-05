#ifndef _PULSAR_TTPRACTICE_
#define _PULSAR_TTPRACTICE_

#include <kamek.hpp>
#include <MarioKartWii/System/Identifiers.hpp>
#include <MarioKartWii/UI/Page/Menu/Menu.hpp>
#include <MarioKartWii/UI/Ctrl/PushButton.hpp>
#include <MarioKartWii/UI/Page/Menu/VSSettings.hpp>
#include <MarioKartWii/UI/Ctrl/Menu/CtrlMenuText.hpp>
#include <UI/UI.hpp>

namespace Pulsar {
namespace TTPractice {

void SetPracticeMode(bool enabled);
bool IsPracticeMode();
ItemId GetStartingItem(u32 hudSlotId);
bool AreItemBoxesEnabled();

class SelectPage : public ::Pages::MenuInteractable {
   public:
    static const UI::PulPageId id = UI::PULPAGE_TTPRACTICESELECT;

    SelectPage();
    ~SelectPage() override;

    void OnInit() override;
    void OnActivate() override;
    void BeforeEntranceAnimations() override;
    void OnButtonClick(PushButton& button, u32 hudSlotId);
    void OnButtonSelect(PushButton& button, u32 hudSlotId);
    void OnBackPress(u32 hudSlotId);

    int GetActivePlayerBitfield() const override { return this->activePlayerBitfield; }
    int GetPlayerBitfield() const override { return this->playerBitfield; }
    ManipulatorManager& GetManipulatorManager() override { return this->controlsManipulatorManager; }
    UIControl* CreateExternalControl(u32 controlId) override;
    UIControl* CreateControl(u32 controlId) override;

    private:
    CtrlMenuPageTitleText title;
    CtrlMenuInstructionText bottom;
    PushButton buttons[2];
    PtmfHolder_2A<SelectPage, void, PushButton&, u32> onButtonClickHandler;
    PtmfHolder_2A<SelectPage, void, PushButton&, u32> onButtonSelectHandler;
    PtmfHolder_1A<SelectPage, void, u32> onBackPressHandler;
};

class ConfirmPage : public ::Pages::MenuInteractable {
   public:
    static const UI::PulPageId id = UI::PULPAGE_TTPRACTICECONFIRM;

    ConfirmPage();
    ~ConfirmPage() override;

    void OnInit() override;
    void OnActivate() override;
    void BeforeEntranceAnimations() override;
    void OnButtonClick(PushButton& button, u32 hudSlotId);
    void OnButtonSelect(PushButton& button, u32 hudSlotId);
    void OnBackPress(u32 hudSlotId);

    int GetActivePlayerBitfield() const override { return this->activePlayerBitfield; }
    int GetPlayerBitfield() const override { return this->playerBitfield; }
    ManipulatorManager& GetManipulatorManager() override { return this->controlsManipulatorManager; }
    UIControl* CreateExternalControl(u32 controlId) override;
    UIControl* CreateControl(u32 controlId) override;

   private:
    CtrlMenuPageTitleText title;
    CtrlMenuInstructionText bottom;
    PushButton buttons[2];
    PtmfHolder_2A<ConfirmPage, void, PushButton&, u32> onButtonClickHandler;
    PtmfHolder_2A<ConfirmPage, void, PushButton&, u32> onButtonSelectHandler;
    PtmfHolder_1A<ConfirmPage, void, u32> onBackPressHandler;
};

class SettingsPage : public ::Pages::MenuInteractable {
   public:
    static const UI::PulPageId id = UI::PULPAGE_TTPRACTICESETTINGS;

    SettingsPage();
    ~SettingsPage() override;

    void OnInit() override;
    void OnActivate() override;
    const ut::detail::RuntimeTypeInfo* GetRuntimeTypeInfo() const override;
    void OnExternalButtonSelect(PushButton& button, u32 hudSlotId) override;
    int GetActivePlayerBitfield() const override { return this->activePlayerBitfield; }
    int GetPlayerBitfield() const override { return this->playerBitfield; }
    ManipulatorManager& GetManipulatorManager() override { return this->controlsManipulatorManager; }
    UIControl* CreateExternalControl(u32 controlId) override;
    UIControl* CreateControl(u32 controlId) override;
    void SetButtonHandlers(PushButton& button) override;
    void OnBackPress(u32 hudSlotId);
    void OnBackButtonClick(PushButton& button, u32 hudSlotId);

   private:
    void OnSaveButtonClick(PushButton& button, u32 hudSlotId);
    void OnRadioButtonClick(RadioButtonControl& radioButtonControl, u32 hudSlotId, u32 optionId);
    void OnRadioButtonChange(RadioButtonControl& radioButtonControl, u32 hudSlotId, u32 optionId);

    RadioButtonControl radioButtonControl;
    u8 itemBoxesSetting;
    PtmfHolder_3A<SettingsPage, void, RadioButtonControl&, u32, u32> onRadioButtonClickHandler;
    PtmfHolder_3A<SettingsPage, void, RadioButtonControl&, u32, u32> onRadioButtonChangeHandler;
    PtmfHolder_2A<SettingsPage, void, PushButton&, u32> onBackButtonClickHandler;
};

}  // namespace TTPractice
}  // namespace Pulsar

#endif
