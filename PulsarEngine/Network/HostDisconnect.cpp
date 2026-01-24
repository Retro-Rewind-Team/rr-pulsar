#include <PulsarSystem.hpp>
#include <kamek.hpp>
#include <runtimeWrite.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <core/rvl/DWC/DWCCore.hpp>

namespace Pulsar {
namespace Network {

static const s32 HOST_DISCONNECT_ERROR_CODE = 69650;  // Custom error code for host disconnect

typedef void (*SetDisconnectInfoFn)(RKNet::Controller*, s32, s32);
kmRuntimeUse(0x80656920);

static bool IsInFriendRoom() {
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    if (controller == nullptr) return false;
    return controller->roomType == RKNet::ROOMTYPE_FROOM_HOST ||
           controller->roomType == RKNet::ROOMTYPE_FROOM_NONHOST;
}

static void OnConnectionClosed(RKNet::Controller* controller, u32 aid) {
    // 1. Call original ProcessPlayerDisconnect
    controller->ProcessPlayerDisconnect(aid);

    // 2. Check for host disconnect in friend room
    if (IsInFriendRoom() && Pulsar::System::sInstance->IsContext(PULSAR_VR)) {
        const RKNet::ControllerSub& sub = controller->subs[controller->currentSub];
        const u8 hostAid = sub.hostAid;
        const bool isLocalHost = sub.localAid == hostAid;

        if (!isLocalHost && aid == hostAid) {
            // Host disconnected, trigger local disconnect for everyone else
            const SetDisconnectInfoFn setDisconnectInfo = reinterpret_cast<SetDisconnectInfoFn>(kmRuntimeAddr(0x80656920));
            setDisconnectInfo(controller, 1, HOST_DISCONNECT_ERROR_CODE);
        }
    }
}
kmCall(0x80658874, OnConnectionClosed);

}  // namespace Network
}  // namespace Pulsar