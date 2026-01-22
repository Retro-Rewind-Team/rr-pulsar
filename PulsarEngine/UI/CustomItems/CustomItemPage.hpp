#ifndef _CUSTOMITEMPAGE_
#define _CUSTOMITEMPAGE_
#include <kamek.hpp>
#include <MarioKartWii/UI/Page/Menu/Menu.hpp>
#include <MarioKartWii/UI/Ctrl/PushButton.hpp>
#include <MarioKartWii/UI/Ctrl/Menu/CtrlMenuText.hpp>
#include <UI/UI.hpp>

namespace Pulsar {
namespace UI {

class CustomItemPage : public ::Pages::MenuInteractable {
public:
    CustomItemPage();
    ~CustomItemPage() override {}
    void OnInit() override;
    void OnActivate() override;
    void OnDeactivate() override;
    void BeforeEntranceAnimations() override;

    void OnButtonClick(PushButton& button, u32 hudSlotId);
    void OnButtonSelect(PushButton& button, u32 hudSlotId);
    void OnButtonDeselect(PushButton& button, u32 hudSlotId);
    void OnBackPress(u32 hudSlotId);
    void AfterControlUpdate() override;
    
    // Menu virtuals
    int GetActivePlayerBitfield() const override { return this->activePlayerBitfield; }
    int GetPlayerBitfield() const override { return this->playerBitfield; }
    ManipulatorManager& GetManipulatorManager() override { return this->controlsManipulatorManager; }
    UIControl* CreateControl(u32 controlId) override;

    static const PulPageId id = PULPAGE_CUSTOMITEMS;

private:
    void UpdateButtonVisuals();
    void SetButtonIcon(PushButton& button, u32 itemId);

    CtrlMenuPageTitleText titleText;
    PushButton* buttons; // 19 items + 1 randomize
    PtmfHolder_2A<CustomItemPage, void, PushButton&, u32> onButtonClickHandler;
    PtmfHolder_2A<CustomItemPage, void, PushButton&, u32> onButtonSelectHandler;
    PtmfHolder_2A<CustomItemPage, void, PushButton&, u32> onButtonDeselectHandler;
    PtmfHolder_1A<CustomItemPage, void, u32> onBackPressHandler;
};

} // namespace UI
} // namespace Pulsar

#endif
