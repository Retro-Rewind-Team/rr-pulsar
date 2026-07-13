#ifndef _PUL_WFC_
#define _PUL_WFC_
#include <kamek.hpp>
#include <MarioKartWii/UI/Page/Other/WFCMenu.hpp>
#include <Settings/UI/SettingsPanel.hpp>

// Extends WFCMainMenu to add a settings button
namespace Pulsar {
namespace UI {

class ExpWFCMain : public Pages::WFCMainMenu {
   public:
    ExpWFCMain() : selectMainButtonOnResume(false), showWorldwideCategories(false), restoreWorldwideMenuOnActivate(false) {
        this->onSettingsClick.subject = this;
        this->onSettingsClick.ptmf = &ExpWFCMain::OnSettingsButtonClick;
        this->onMainClick.subject = this;
        this->onMainClick.ptmf = &ExpWFCMain::OnMainButtonClick;
        this->onOtherClick.subject = this;
        this->onOtherClick.ptmf = &ExpWFCMain::OnOtherButtonClick;
        this->onBattleClick.subject = this;
        this->onBattleClick.ptmf = &ExpWFCMain::OnBattleButtonClick;
        this->onCompetitiveClick.subject = this;
        this->onCompetitiveClick.ptmf = &ExpWFCMain::OnCompetitiveButtonClick;
        this->onLeaderboardClick.subject = this;
        this->onLeaderboardClick.ptmf = &ExpWFCMain::OnLeaderboardButtonClick;
        this->onBackButtonClickHandler.subject = this;
        this->onBackButtonClickHandler.ptmf = &ExpWFCMain::OnBackButtonClick;
        this->onBackPressHandler.subject = this;
        this->onBackPressHandler.ptmf = &ExpWFCMain::OnBackPress;
        this->onButtonSelectHandler.ptmf = &ExpWFCMain::ExtOnButtonSelect;

        this->onStartPress.subject = this;
        this->onStartPress.ptmf = &ExpWFCMain::ExtOnStartPress;
    }
    void OnInit() override;
    void OnActivate() override;
    void OnResume() override;
    void BeforeControlUpdate() override;

   private:
    void OnSettingsButtonClick(PushButton& pushButton, u32 r5);
    void ExtOnButtonSelect(PushButton& pushButton, u32 hudSlotId);
    void OnMainButtonClick(PushButton& pushButton, u32 hudSlotId);
    void OnOtherButtonClick(PushButton& pushButton, u32 hudSlotId);
    void OnBattleButtonClick(PushButton& pushButton, u32 hudSlotId);
    void OnCompetitiveButtonClick(PushButton& pushButton, u32 hudSlotId);
    void OnLeaderboardButtonClick(PushButton& pushButton, u32 hudSlotId);
    void OnBackButtonClick(PushButton& pushButton, u32 hudSlotId);
    void OnBackPress(u32 hudSlotId);
    void ExtOnStartPress(u32 hudSlotId);
    void SetMenuLevel(bool showWorldwideCategories);

    PtmfHolder_2A<ExpWFCMain, void, PushButton&, u32> onSettingsClick;
    PtmfHolder_2A<ExpWFCMain, void, PushButton&, u32> onMainClick;
    PtmfHolder_2A<ExpWFCMain, void, PushButton&, u32> onOtherClick;
    PtmfHolder_2A<ExpWFCMain, void, PushButton&, u32> onBattleClick;
    PtmfHolder_2A<ExpWFCMain, void, PushButton&, u32> onCompetitiveClick;
    PtmfHolder_2A<ExpWFCMain, void, PushButton&, u32> onLeaderboardClick;
    PtmfHolder_2A<ExpWFCMain, void, PushButton&, u32> onBackButtonClickHandler;
    PtmfHolder_1A<ExpWFCMain, void, u32> onBackPressHandler;
    PtmfHolder_1A<ExpWFCMain, void, u32> onStartPress;
    PushButton settingsButton;
    PushButton mainButton;
    PushButton otherButton;
    PushButton battleButton;
    PushButton leaderboardButton;
    LayoutUIControl playerCount;
    LayoutUIControl rankInfo;

   public:
    PulPageId topSettingsPage;
    bool selectMainButtonOnResume;
    bool showWorldwideCategories;
    bool restoreWorldwideMenuOnActivate;
    static u32 lastClickedMainMenuButton;
};

class ExpWFCModeSel : public Pages::WFCModeSelect {
   public:
    ExpWFCModeSel() : region(0xA) {
        this->onButtonSelectHandler.ptmf = &ExpWFCModeSel::OnModeButtonSelect;
        this->onModeButtonClickHandler.ptmf = &ExpWFCModeSel::OnModeButtonClick;
    }
    void OnInit() override;
    void BeforeControlUpdate() override;
    static void InitButton(ExpWFCModeSel& self);
    static void OnActivatePatch();
    static void ClearModeContexts();

   public:
    void OnModeButtonSelect(PushButton& modeButton, u32 hudSlotId);  // 8064c718
    void OnModeButtonClick(PushButton& pushButton, u32 r5);

    PushButton ctButton;
    PushButton regButton;
    PushButton mogiButton;
    PushButton compCTButton;
    PushButton compRegButton;
    PushButton twoHundredButton;
    PushButton ottButton;
    PushButton itemRainButton;
    PushButton RRbattleButton;
    PushButton RRbattleButtonElim;
    LayoutUIControl vrButton;
    LayoutUIControl mmrButton;
    static u32 lastClickedButton;
    u32 region;
    static const u32 ctButtonId = 4;
    static const u32 regButtonId = 5;
    static const u32 mogiButtonId = 13;
    static const u32 compCTButtonId = 14;
    static const u32 compRegButtonId = 15;
    static const u32 twoHundredButtonId = 7;
    static const u32 ottButtonId = 6;
    static const u32 itemRainButtonId = 9;
    static const u32 RRbattleButtonId = 10;
    static const u32 RRbattleButtonIdElim = 11;
};
}  // namespace UI
}  // namespace Pulsar

#endif
