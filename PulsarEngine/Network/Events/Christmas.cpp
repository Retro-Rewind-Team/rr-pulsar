#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <Network/ServerDateTime.hpp>
#include <runtimeWrite.hpp>

namespace Pulsar {
namespace Network {

kmRuntimeUse(0x80691180);
kmRuntimeUse(0x80696C88);
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

static void SetSnowPatchesEnabled() {
    if (IsInChristmasWindow(ServerDateTime::sInstance[0]) && IsPublicOnlineRoom(RKNet::Controller::sInstance->roomType)) {
        kmRuntimeWrite32A(0x80691180, 0x38000001);
        kmRuntimeWrite32A(0x80696C88, 0x38000001);
    } else {
        kmRuntimeWrite32A(0x80691180, 0x881E013E);
        kmRuntimeWrite32A(0x80696C88, 0x881E013E);
    }
}
static SectionLoadHook setsnowpatches(SetSnowPatchesEnabled);

}  // namespace Network
}  // namespace Pulsar