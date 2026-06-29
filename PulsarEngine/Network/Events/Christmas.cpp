#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <Network/ServerDateTime.hpp>
#include <Patching/RuntimeChoice.hpp>

namespace Pulsar {
namespace Network {

static bool IsPublicOnlineRoom(const RKNet::RoomType roomType) {
    switch (roomType) {
        case RKNet::ROOMTYPE_VS_WW:
        case RKNet::ROOMTYPE_VS_REGIONAL:
        case RKNet::ROOMTYPE_BT_WW:
        case RKNet::ROOMTYPE_BT_REGIONAL:
        case RKNet::ROOMTYPE_JOINING_WW:
        case RKNet::ROOMTYPE_JOINING_REGIONAL:
            return true;
        default:
            return false;
    }
}

static bool IsInChristmasWindow(const ServerDateTime& sdt) {
    if (!sdt.isValid) return false;
    const unsigned month = static_cast<unsigned>(sdt.month);
    const unsigned day = static_cast<unsigned>(sdt.day);
    return (month == 12 && day >= 23 && day <= 31) || (month == 1 && day >= 1 && day <= 3);
}

static bool ShouldForceSnow() {
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    return controller != nullptr &&
           IsPublicOnlineRoom(controller->roomType) &&
           IsInChristmasWindow(ServerDateTime::sInstance[0]);
}

static u32 sForceSnow = 0;

static void UpdateSnowPatchState() {
    sForceSnow = ShouldForceSnow() ? 1 : 0;
}
static SectionLoadHook UpdateSnowPatchStateHook(UpdateSnowPatchState);

RuntimeChoice_ConditionalByteOrImmediate(LoadSnowSettingA, 0x80691180, sForceSnow, r0, r30, 0x13e, 1);
RuntimeChoice_ConditionalByteOrImmediate(LoadSnowSettingB, 0x80696C88, sForceSnow, r0, r30, 0x13e, 1);

}  // namespace Network
}  // namespace Pulsar
