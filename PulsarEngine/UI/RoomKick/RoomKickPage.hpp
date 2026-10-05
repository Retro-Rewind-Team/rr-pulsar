#ifndef __ROOMKICKPAGE_HPP__
#define __ROOMKICKPAGE_HPP__

#include <kamek.hpp>
#include <MarioKartWii/UI/Page/Menu/Menu.hpp>
#include <MarioKartWii/UI/Page/Menu/MiiSelect.hpp>
#include <MarioKartWii/UI/Page/Other/FriendList.hpp>
#include <MarioKartWii/UI/Page/Other/FriendRoom.hpp>
#include <MarioKartWii/UI/Page/Other/YesNo.hpp>
#include <UI/UI.hpp>
#include <UI/ToggleControls.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/RKNet/SELECT.hpp>

namespace Pulsar {
namespace UI {

class RoomKickPage : public Pages::MenuInteractable {
public:
    static const PulPageId id = PULPAGE_ROOMKICK;

    RoomKickPage();

    void OnInit() override;
    void BeforeEntranceAnimations() override;
    void BeforeControlUpdate() override;

    int GetActivePlayerBitfield() const override;
    int GetPlayerBitfield() const override;
    ManipulatorManager &GetManipulatorManager() override;
    UIControl *CreateExternalControl(u32 id) override;
    UIControl *CreateControl(u32 id) override;
    void SetButtonHandlers(PushButton &button) override;

    void ClearKickHistory();
    u32 *GetKickHistory(u32 &outCount);

private:
    void OnBackPress(u32 hudSlotId);
    void OnButtonClick(PushButton &button, u32 hudSlotId);
    void OnButtonSelect(PushButton &button, u32 hudSlotId);
    void OnButtonDeselect(PushButton &button, u32 hudSlotId) {}
    void OnYesNoClick(u32 choice, PushButton &button);

    PushButton miis[12];
    MiiName name;

    MiiGroup *miiGroup;
    int miiIdx[12];
    int aidIdx[12];
    int playerCount;

    int selectedIdx;

    u32 kickedPIDs[64];
    u32 kickedCount;

    PtmfHolder_2A<RoomKickPage, void, u32, PushButton &> onYesNoClickHandler;
};

}  // namespace UI
}  // namespace Pulsar
#endif