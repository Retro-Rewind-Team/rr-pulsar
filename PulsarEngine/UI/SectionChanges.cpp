#include <MarioKartWii/System/Identifiers.hpp>
#include <RetroRewind.hpp>

namespace Pulsar {
namespace UI {

static SectionId GetFFASectionId(SectionId id) {
    if (System::sInstance->IsContext(PULSAR_FFA)) {
        switch (id) {
            case SECTION_P1BATTLE:
                return SECTION_P1VS;
            case SECTION_P2BATTLE:
                return SECTION_P2VS;
            case SECTION_P3BATTLE:
                return SECTION_P3VS;
            case SECTION_P4BATTLE:
                return SECTION_P4VS;
            case SECTION_P1_WIFI_FRIEND_BALLOON:
            case SECTION_P1_WIFI_FRIEND_COIN:
                return SECTION_P1_WIFI_FRIEND_VS;
            case SECTION_P2_WIFI_FRIEND_BALLOON:
            case SECTION_P2_WIFI_FRIEND_COIN:
                return SECTION_P2_WIFI_FRIEND_VS;
            default:
                return id;
        }
    } else if (System::sInstance->IsContext(PULSAR_VR)) {
        switch (id) {
            case SECTION_P1_WIFI_FRIEND_VS:
                return SECTION_P1_WIFI_VS;
            case SECTION_P2_WIFI_FRIEND_VS:
                return SECTION_P2_WIFI_VS;
            case SECTION_P1_WIFI_FROOM_VS_VOTING:
                return SECTION_P1_WIFI_VS_VOTING;
            case SECTION_P2_WIFI_FROOM_VS_VOTING:
                return SECTION_P2_WIFI_VS_VOTING;
            default:
                return id;
        }
    }
    return id;
}

static asmFunc FFAResults() {
    ASM(
        nofralloc;
        stwu r1, -0x20(r1);
        stw r0, 0x10(r1);
        mflr r0;
        stw r0, 0x24(r1);
        stw r3, 0x8(r1);
        stw r4, 0xC(r1);

        mr r3, r4;
        bl GetFFASectionId;

        mr r31, r3;
        mr r4, r3;

        lwz r3, 0x8(r1);
        lwz r0, 0x24(r1);
        mtlr r0;
        lwz r0, 0x10(r1);
        addi r1, r1, 0x20;
        blr;);
}
kmCall(0x80621e1c, FFAResults);

}  // namespace UI
}  // namespace Pulsar