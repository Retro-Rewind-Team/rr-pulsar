#ifndef _PUL_SETTINGSPAGESELECT_
#define _PUL_SETTINGSPAGESELECT_
#include <kamek.hpp>
#include <MarioKartWii/UI/Page/Menu/Menu.hpp>
#include <MarioKartWii/UI/Ctrl/PushButton.hpp>
#include <Settings/SettingsParam.hpp>
#include <Network/Ranking.hpp>
#include <UI/UI.hpp>

/*
SettingsPageSelect - A page that displays all settings pages as buttons.
When a user clicks a settings button, this page appears first, allowing them
to select which settings category they want to view.
*/

namespace Pulsar {
namespace UI {

class SettingsPageSelect : public Pages::MenuInteractable {
   public:
    static const PulPageId id = PULPAGE_SETTINGSPAGESELECT;

    explicit SettingsPageSelect(bool badgeSelect = false);
    ~SettingsPageSelect() override {}

    void OnInit() override;
    void OnActivate() override;
    const ut::detail::RuntimeTypeInfo* GetRuntimeTypeInfo() const override;
    int GetActivePlayerBitfield() const override;
    int GetPlayerBitfield() const override;
    ManipulatorManager& GetManipulatorManager() override;
    UIControl* CreateExternalControl(u32 id) override { return nullptr; }
    UIControl* CreateControl(u32 id) override;
    void SetButtonHandlers(PushButton& button) override;
    void BeforeControlUpdate() override;

    void OnBackPress(u32 hudSlotId);
    void OnBackButtonClick(PushButton& button, u32 hudSlotId);

   private:
    void UpdateBadgeButtons();
    void SetBadgeButtonMessage(PushButton& button);
    void OnButtonClick(PushButton& button, u32 hudSlotId);
    void OnButtonSelect(PushButton& button, u32 hudSlotId);
    void OnButtonDeselect(PushButton& button, u32 hudSlotId) {}

    PtmfHolder_2A<SettingsPageSelect, void, PushButton&, u32> onBackButtonClickHandler;

    // Buttons for each settings page - max 12 pages
    static const u32 settingsButtonCount = Settings::Params::pageCount + 1;
    static const u32 badgeButtonCount = Ranking::SPECIAL_BADGE_COUNT + 1;
    PushButton pageButtons[settingsButtonCount];
    bool badgeSelectMode;
};

}  // namespace UI
}  // namespace Pulsar
#endif
