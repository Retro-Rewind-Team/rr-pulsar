#ifndef _CUSTOMITEMPAGE_
#define _CUSTOMITEMPAGE_
#include <kamek.hpp>
#include <MarioKartWii/UI/Page/Menu/Menu.hpp>
#include <MarioKartWii/UI/Ctrl/PushButton.hpp>
#include <MarioKartWii/UI/Ctrl/Menu/CtrlMenuText.hpp>
#include <MarioKartWii/UI/Ctrl/CountDown.hpp>
#include <Race/CustomItems.hpp>
#include <UI/UI.hpp>

namespace Pulsar {
namespace UI {

class CustomItemPage : public ::Pages::MenuInteractable {
public:
    CustomItemPage();
    ~CustomItemPage() override;
    void OnActivate() override;

    void OnButtonClick(PushButton &button, u32 hudSlotId);
    void OnBackPress(u32 hudSlotId);
    void AfterControlUpdate() override;
    void StartFriendRoomPreview(PageId nextPageId);
    static bool ShouldSkipFriendRoomPreview();

    int GetActivePlayerBitfield() const override {
        return this->activePlayerBitfield;
    }
    int GetPlayerBitfield() const override {
        return this->playerBitfield;
    }
    ManipulatorManager &GetManipulatorManager() override {
        return this->controlsManipulatorManager;
    }
    UIControl *CreateControl(u32 controlId) override;

    static const PulPageId id = PULPAGE_CUSTOMITEMS;

private:
    void UpdateButtonVisuals();

    enum {
        ENABLE_ALL = Race::CUSTOM_ITEM_COUNT,
        DISABLE_ALL,
        RANDOMIZE,
        BUTTON_COUNT
    };

    CountDown previewTimer;
    PushButton buttons[BUTTON_COUNT];
    PageId friendRoomPreviewNextPageId;
    bool isFriendRoomPreview;
    PtmfHolder_2A<CustomItemPage, void, PushButton &, u32> onButtonClickHandler;
    PtmfHolder_1A<CustomItemPage, void, u32> onBackPressHandler;
};

}  // namespace UI
}  // namespace Pulsar

#endif
