#include <kamek.hpp>
#include <runtimeWrite.hpp>
#include <MarioKartWii/Input/InputManager.hpp>
#include <MarioKartWii/UI/Page/Other/WFCConnect.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>

namespace Pulsar {
namespace Network {

static void WFCConnectAfterControlUpdate(Pages::WFCConnect* page) {
    Input::Manager* inputManager = Input::Manager::sInstance;
    for (u32 i = 0; i < 4; ++i) {
        const Input::UIState& state = inputManager->realControllerHolders[i].uiinputStates[0];
        if (page->status >= Pages::WFCConnect::STATUS_INIT_CONNECTION && page->status <= Pages::WFCConnect::STATUS_AWAIT_FRIEND_INFO && (state.buttonActions & 2) != 0) {
            SectionMgr::sInstance->curSection->ScheduleDisconnection();
            page->OnDisconnect();
            return;
        }
    }
    page->WFCConnect::AfterControlUpdate();
}
kmWritePointer(0x808bfaf0, WFCConnectAfterControlUpdate);

}  // namespace Network
}  // namespace Pulsar