#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <Network/ServerDateTime.hpp>
#include <hooks.hpp>

namespace Pulsar {
namespace Network {

extern "C" u8 sForceSnowEffects = false;
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
    sForceSnowEffects = IsInChristmasWindow(ServerDateTime::sInstance[0]) &&
                        IsPublicOnlineRoom(RKNet::Controller::sInstance->roomType);
}
static SectionLoadHook setsnowpatches(SetSnowPatchesEnabled);

asmFunc LoadSnowEffectFlag() {
    ASM(
        nofralloc;
        stwu r1, -0x10(r1);
        stw r12, 0x8(r1);
        lis r12, sForceSnowEffects @ha;
        lbz r0, sForceSnowEffects @l(r12);
        cmpwi r0, 0;
        lwz r12, 0x8(r1);
        addi r1, r1, 0x10;
        bne end;
        lbz r0, 0x13e(r30);
        end : blr;)
}
kmCall(0x80691180, LoadSnowEffectFlag);
kmCall(0x80696C88, LoadSnowEffectFlag);

}  // namespace Network
}  // namespace Pulsar
