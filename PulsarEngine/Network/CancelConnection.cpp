#include <kamek.hpp>
#include <runtimeWrite.hpp>
#include <MarioKartWii/Input/InputManager.hpp>

namespace Pages {
class WFCConnect;
}

namespace Pulsar {
namespace Network {
static const u16 UI_BUTTON_BACK = 2;
static const s32 MANUAL_DISCONNECT_ERROR_CODE = 69420;

enum DisconnectCategory {
    DISCONNECT_CATEGORY_NONE = 0,
    DISCONNECT_CATEGORY_ERROR_CODE = 1,
    DISCONNECT_CATEGORY_MAINTENANCE = 2,
    DISCONNECT_CATEGORY_DISK_EJECTED = 4,
    DISCONNECT_CATEGORY_UNRECOVERABLE = 5,
};

struct DisconnectInfo {
    s32 category;
    s32 errorCode;
};

typedef void NetManager;
typedef DisconnectInfo (*GetDisconnectInfoFn)(NetManager*);
typedef void (*SetDisconnectInfoFn)(NetManager*, s32, s32);
typedef void (*AfterControlUpdateFn)(Pages::WFCConnect*);

kmRuntimeUse(0x806569b4);
kmRuntimeUse(0x80656920);
kmRuntimeUse(0x80646eb0);
static const GetDisconnectInfoFn getDisconnectInfo = reinterpret_cast<GetDisconnectInfoFn>(kmRuntimeAddr(0x806569b4));
static const SetDisconnectInfoFn setDisconnectInfo = reinterpret_cast<SetDisconnectInfoFn>(kmRuntimeAddr(0x80656920));
static const AfterControlUpdateFn originalAfterControlUpdate = reinterpret_cast<AfterControlUpdateFn>(kmRuntimeAddr(0x80646eb0));

kmRuntimeUse(0x809c20d8);
static NetManager* GetNetManager() {
    return *reinterpret_cast<NetManager**>(kmRuntimeAddr(0x809c20d8));
}

static u32 GetConnectStatus(const Pages::WFCConnect* page) {
    const u8* raw = reinterpret_cast<const u8*>(page);
    return *reinterpret_cast<const u32*>(raw + 0x58);
}

static bool IsConnectingState(const Pages::WFCConnect* page) {
    const u32 status = GetConnectStatus(page);
    return status >= 6 && status <= 10;
}

static bool IsBackPressedAnyController() {
    Input::Manager* inputManager = Input::Manager::sInstance;
    if (inputManager == nullptr) return false;
    for (u32 i = 0; i < 4; ++i) {
        const Input::UIState& state = inputManager->realControllerHolders[i].uiinputStates[0];
        if (state.buttonActions == UI_BUTTON_BACK) return true;
    }
    return false;
}

static void TriggerManualDisconnect() {
    NetManager* manager = GetNetManager();
    if (manager == nullptr) return;

    const DisconnectInfo info = getDisconnectInfo(manager);
    if (info.category != DISCONNECT_CATEGORY_NONE) return;

    setDisconnectInfo(manager, DISCONNECT_CATEGORY_ERROR_CODE, MANUAL_DISCONNECT_ERROR_CODE);
}

static void WFCConnectAfterControlUpdate(Pages::WFCConnect* page) {
    if (IsConnectingState(page) && IsBackPressedAnyController()) {
        TriggerManualDisconnect();
    }
    originalAfterControlUpdate(page);
}
kmWritePointer(0x808bfaf0, WFCConnectAfterControlUpdate);

}  // namespace Network
}  // namespace Pulsar