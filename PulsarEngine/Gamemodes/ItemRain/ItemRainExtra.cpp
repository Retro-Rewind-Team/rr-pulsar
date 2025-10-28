#include <RetroRewind.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <runtimeWrite.hpp>

namespace Pulsar {
namespace ItemRain {

// The following patches are from the original gecko code [Unnamed, MrBean35000vr]
// Allow POWs and Shocks to affect other players on their screens.
asmFunc ItemRainOnlineFix() {
    ASM(
        loc_0x0 : addi r3, r4, 200;
        lis r12, 0x8000;
        lbz r12, 0x120C(r12);
        cmpwi r12, 0;
        beq loc_0x2C;
        lbz r3, 27(r27);
        cmpwi r3, 0x7;
        beq + loc_0x24;
        cmpwi r3, 0x3;
        beq + loc_0x24;
        cmpwi r3, 0x4;
        beq + loc_0x24;
        addi r3, r4, 0xC8;
        b loc_0x2C;

        loc_0x24 : mr r3, r4;
        b loc_0x2C;

        loc_0x2C:)
}
kmCall(0x8065BB40, ItemRainOnlineFix);

kmRuntimeUse(0x808A5D47);
kmRuntimeUse(0x808A5A3F);
kmRuntimeUse(0x808A538F);
kmRuntimeUse(0x808A56EB);
kmRuntimeUse(0x808A548B);
void ItemRainFix() {
    ItemRainOnlineFixHook = 0x00;
    kmRuntimeWrite8A(0x808A5D47, 0x0000000c);
    kmRuntimeWrite8A(0x808A5A3F, 0x00000008);
    kmRuntimeWrite8A(0x808A538F, 0x00000010);
    kmRuntimeWrite8A(0x808A56EB, 0x00000006);
    kmRuntimeWrite8A(0x808A548B, 0x00000003);
    if (RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_HOST ||
        RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_NONHOST ||
        RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_NONE) {
        if (Pulsar::System::sInstance->IsContext(PULSAR_ITEMMODESTORM)) {
            ItemRainOnlineFixHook = 0x00FF0100;
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
            ItemRainOnlineFixHook = 0x00FF0100;
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