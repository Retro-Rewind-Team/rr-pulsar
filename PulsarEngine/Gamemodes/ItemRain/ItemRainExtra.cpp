#include <RetroRewind.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <runtimeWrite.hpp>

namespace Pulsar {
namespace ItemRain {

kmRuntimeUse(0x808A5D47);
kmRuntimeUse(0x808A5A3F);
kmRuntimeUse(0x808A538F);
kmRuntimeUse(0x808A56EB);
kmRuntimeUse(0x808A548B);
void ItemRainFix() {
    kmRuntimeWrite8A(0x808A5D47, 0x0000000c);
    kmRuntimeWrite8A(0x808A5A3F, 0x00000008);
    kmRuntimeWrite8A(0x808A538F, 0x00000010);
    kmRuntimeWrite8A(0x808A56EB, 0x00000006);
    kmRuntimeWrite8A(0x808A548B, 0x00000003);
    if (RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_HOST ||
        RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_NONHOST ||
        RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_NONE) {
        if (Pulsar::System::sInstance->IsContext(PULSAR_ITEMMODESTORM)) {
            kmRuntimeWrite8A(0x808A5D47, 0x00000022);
            kmRuntimeWrite8A(0x808A5A3F, 0x00000022);
            kmRuntimeWrite8A(0x808A538F, 0x00000022);
            kmRuntimeWrite8A(0x808A56EB, 0x00000019);
            kmRuntimeWrite8A(0x808A548B, 0x00000019);
        }
    }
    if (RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_HOST ||
        RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_NONHOST ||
        RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_NONE ||
        RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_VS_REGIONAL) {
        if (Pulsar::System::sInstance->IsContext(PULSAR_ITEMMODERAIN)) {
            kmRuntimeWrite8A(0x808A5D47, 0x00000022);
            kmRuntimeWrite8A(0x808A5A3F, 0x00000022);
            kmRuntimeWrite8A(0x808A538F, 0x00000022);
            kmRuntimeWrite8A(0x808A56EB, 0x00000019);
            kmRuntimeWrite8A(0x808A548B, 0x00000019);
        }
    }
}
static SectionLoadHook FixItemRain(ItemRainFix);

}  // namespace ItemRain
}  // namespace Pulsar