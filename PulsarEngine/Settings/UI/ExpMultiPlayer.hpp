#ifndef _PUL_EXP_MULTIPLAYER_
#define _PUL_EXP_MULTIPLAYER_
#include <MarioKartWii/UI/Page/Menu/MultiPlayer.hpp>

namespace Pulsar {
namespace UI {

class ExpMultiPlayer : public Pages::MultiPlayer {
   public:
    ExpMultiPlayer();

    UIControl* CreateExternalControl(u32 externControlId) override;
    void SetButtonHandlers(PushButton& button) override;
    void OnExternalButtonSelect(PushButton& button, u32 hudSlotId) override;

    void OnSettingsButtonClick(PushButton& button, u32 hudSlotId);

   private:
    PtmfHolder_2A<ExpMultiPlayer, void, PushButton&, u32> onSettingsClickHandler;
};

}  // namespace UI
}  // namespace Pulsar

#endif
