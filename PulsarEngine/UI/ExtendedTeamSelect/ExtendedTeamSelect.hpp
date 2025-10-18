#ifndef _EXT_TEAMSELECT_HPP_
#define _EXT_TEAMSELECT_HPP_

#include <MarioKartWii/UI/Page/Menu/Menu.hpp>
#include <MarioKartWii/UI/Ctrl/CtrlRace/CtrlRaceResult.hpp>
#include <UI/UI.hpp>
#include <UI/ToggleControls.hpp>
#include <UI/ExtendedTeamSelect/ExtendedTeamManager.hpp>

namespace Pulsar {
namespace UI {

class ExtendedTeamSelect : public Pages::MenuInteractable {
   public:
    static const PulPageId id = PULPAGE_EXTENDEDTEAMSELECT;

    ExtendedTeamSelect();

    void OnInit() override;
    void BeforeEntranceAnimations() override;
    void BeforeExitAnimations() override;
    void BeforeControlUpdate() override;
    void AfterControlUpdate() override;
    void OnResume() override;

    int GetActivePlayerBitfield() const override;
    int GetPlayerBitfield() const override;
    ManipulatorManager& GetManipulatorManager() override;
    UIControl* CreateExternalControl(u32 id) override;
    UIControl* CreateControl(u32 id) override;

    void OnBackPress(u32 hudSlotId);
    void OnFrontPress(u32 hudSlotId) {}
    void OnDisconnectClick(u32 hudSlotId) {}

    void OnBackButtonClick(PushButton& button, u32 hudSlotId) {
        OnBackPress(hudSlotId);
    }

    void OnStartRaceClick(PushButton& button, u32 hudSlotId);
    void OnStartRaceSelect(PushButton& button, u32 hudSlotId) {}

    void OnArrowClick(PushButton& button, u32 hudSlotId);
    void OnArrowSelect(PushButton& button, u32 hudSlotId) {}

    void UpdatePlayerTeam(u32 idx, ExtendedTeamID team);
    void UpdatePlayerTeamByAID(u8 aid, u8 playerIdOnConsole, ExtendedTeamID team);

    static void GetTeamColor(ExtendedTeamID team, u8& r, u8& g, u8& b);

   private:
    PtmfHolder_1A<ExtendedTeamSelect, void, u32> onFrontPressHandler;
    PtmfHolder_1A<Page, void, Pages::Click&> onDisconnectClickHandler;

    PtmfHolder_2A<ExtendedTeamSelect, void, PushButton&, u32> onBackClickHandler;

    PtmfHolder_2A<ExtendedTeamSelect, void, PushButton&, u32> onStartRaceClickHandler;
    PtmfHolder_2A<ExtendedTeamSelect, void, PushButton&, u32> onStartRaceSelectHandler;

    PtmfHolder_2A<ExtendedTeamSelect, void, PushButton&, u32> onArrowClickHandler;
    PtmfHolder_2A<ExtendedTeamSelect, void, PushButton&, u32> onArrowSelectHandler;

    enum InternalControls {
        INSTRUCTION_TEXT,
        BUSY_SYMBOL,
        START_RACE_BUTTON,
        TEAM_ENTRY_0,
        TEAM_ENTRY_1,
        TEAM_ENTRY_2,
        TEAM_ENTRY_3,
        TEAM_ENTRY_4,
        TEAM_ENTRY_5,
        TEAM_ENTRY_6,
        TEAM_ENTRY_7,
        TEAM_ENTRY_8,
        TEAM_ENTRY_9,
        TEAM_ENTRY_10,
        TEAM_ENTRY_11,
        TEAM_ENTRY_ARROW_0,
        TEAM_ENTRY_ARROW_1,
        TEAM_ENTRY_ARROW_2,
        TEAM_ENTRY_ARROW_3,
        TEAM_ENTRY_ARROW_4,
        TEAM_ENTRY_ARROW_5,
        TEAM_ENTRY_ARROW_6,
        TEAM_ENTRY_ARROW_7,
        TEAM_ENTRY_ARROW_8,
        TEAM_ENTRY_ARROW_9,
        TEAM_ENTRY_ARROW_10,
        TEAM_ENTRY_ARROW_11,
        CONTROL_COUNT
    };

    CtrlMenuInstructionText instructionText;
    BusySymbol busySymbol;
    PushButton startRaceButton;
    LayoutUIControl teamPlayerControl[12];
    PushButton teamPlayerArrows[12];
    MiiGroup* miiGroup;
    ExtendedTeamManager* manager;

    bool shouldDisconnect;
    bool isHost;
    u32 playerCount;

   public:
    static const void ChangeVRButtonColors(LayoutUIControl& button, ExtendedTeamID team);
};

}  // namespace UI
}  // namespace Pulsar

#endif  // _EXT_TEAMSELECT_HPP_