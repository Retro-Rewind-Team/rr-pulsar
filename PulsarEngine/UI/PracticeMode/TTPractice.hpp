#ifndef _PULSAR_UI_TTPRACTICE_
#define _PULSAR_UI_TTPRACTICE_

#include <kamek.hpp>
#include <MarioKartWii/UI/Ctrl/Menu/CtrlMenuText.hpp>
#include <MarioKartWii/UI/Ctrl/PushButton.hpp>
#include <MarioKartWii/UI/Page/Menu/Menu.hpp>
#include <UI/UI.hpp>

namespace Pulsar {
namespace TTPractice {

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

}  // namespace TTPractice
}  // namespace Pulsar

#endif
