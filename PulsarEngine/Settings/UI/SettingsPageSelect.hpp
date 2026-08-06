#ifndef _PUL_SETTINGSPAGESELECT_
#define _PUL_SETTINGSPAGESELECT_

#include <kamek.hpp>
#include <MarioKartWii/UI/Page/Menu/Menu.hpp>
#include <MarioKartWii/UI/Ctrl/PushButton.hpp>
#include <Settings/SettingsParam.hpp>
#include <Network/Ranking.hpp>
#include <UI/UI.hpp>

namespace Pulsar {
namespace UI {

class SettingsPageSelect : public Pages::MenuInteractable {
   public:
    static const PulPageId id = PULPAGE_SETTINGSPAGESELECT;

    explicit SettingsPageSelect(bool badgeSelect = false);
    ~SettingsPageSelect() override {}
    void SetContext(Settings::SettingsContext context, PageId previousPage);

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

    static const u32 settingsButtonCount = Settings::Params::maxContextPageCount + 1;
    static const u32 badgeButtonCount = Ranking::SPECIAL_BADGE_COUNT + 1;
    PushButton pageButtons[settingsButtonCount];
    PtmfHolder_2A<SettingsPageSelect, void, PushButton&, u32> onBackButtonClickHandler;
    Settings::SettingsContext context;
    bool badgeSelectMode;
};

}  // namespace UI
}  // namespace Pulsar

#endif
