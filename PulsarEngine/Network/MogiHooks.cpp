#include <hooks.hpp>
#include <kamek.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/System/Identifiers.hpp>
#include <MarioKartWii/UI/Page/Other/SELECTStageMgr.hpp>
#include <Network/Mogi.hpp>
#include <PulsarSystem.hpp>

namespace Pulsar {
namespace Mogi {

static void SetMogiMatchmakingSuspend(RKNet::Controller* controller) {
    const RKNet::ControllerSub& sub = controller->subs[controller->currentSub];
    if (!IsEnabled() || sub.playerCount >= 12) controller->SetVoteMatchmakingSuspend();
}
kmCall(0x80663e64, SetMogiMatchmakingSuspend);

static SectionId GetMogiPublicSectionId(SectionId sectionId) {
    if (!IsActive()) return sectionId;

    switch (sectionId) {
        case SECTION_P1_WIFI_FROOM_VS_VOTING:
        case SECTION_P1_WIFI_FROOM_TEAMVS_VOTING:
            return SECTION_P1_WIFI_VS_VOTING;
        case SECTION_P2_WIFI_FROOM_VS_VOTING:
        case SECTION_P2_WIFI_FROOM_TEAMVS_VOTING:
            return SECTION_P2_WIFI_VS_VOTING;
        case SECTION_P1_WIFI_FRIEND_VS:
        case SECTION_P1_WIFI_FRIEND_TEAMVS:
            return SECTION_P1_WIFI_VS;
        case SECTION_P2_WIFI_FRIEND_VS:
        case SECTION_P2_WIFI_FRIEND_TEAMVS:
            return SECTION_P2_WIFI_VS;
        default:
            return sectionId;
    }
}

// WW race-end routing compares the loaded section ID at 0x8064f3b0. Mogi uses
// the friend-room section resources, but must take the public host-migration path.
asmFunc NormalizeMogiRaceEndSection() {
    ASM(
        nofralloc;
        stwu r1, -0x20(r1);
        stw r0, 0x8(r1);
        mflr r0;
        stw r0, 0x24(r1);
        stw r3, 0xC(r1);
        mr r3, r8;
        bl GetMogiPublicSectionId;
        mr r8, r3;
        lwz r3, 0xC(r1);
        lwz r0, 0x24(r1);
        mtlr r0;
        lwz r0, 0x8(r1);
        addi r1, r1, 0x20;
        cmpwi r8, 0x68;
        blr;);
}
kmBranch(0x8064f3b0, NormalizeMogiRaceEndSection);
kmPatchExitPoint(NormalizeMogiRaceEndSection, 0x8064f3b4);

// SELECTStageMgr::OnInit uses the section ID to select the network SELECT mode.
asmFunc NormalizeMogiVotingInitSection() {
    ASM(
        nofralloc;
        lwz r3, 0x0(r3);
        bl GetMogiPublicSectionId;
        lwz r0, 0x14(r1);
        mtlr r0;
        blr;);
}
kmBranch(0x8064fd30, NormalizeMogiVotingInitSection);
kmPatchExitPoint(NormalizeMogiVotingInitSection, 0x8064fd34);

// VotingBackPage::afterCalc otherwise treats the Mogi FROOM voting section as
// a private room and resets the room records for every connected player.
asmFunc NormalizeMogiVotingUpdateSection() {
    ASM(
        nofralloc;
        stw r0, 0x0(r1);
        lwz r0, 0x0(r3);
        mr r3, r0;
        bl GetMogiPublicSectionId;
        mr r0, r3;
        blr;);
}
kmBranch(0x8065035c, NormalizeMogiVotingUpdateSection);
kmPatchExitPoint(NormalizeMogiVotingUpdateSection, 0x80650360);

// VotePage::beforeCalc validates the room after the vote using the current
// section ID. Normalize that read so the FROOM section is not treated as a
// private room and reset when the vote completes.
asmFunc NormalizeMogiVotePageSection() {
    ASM(
        nofralloc;
        stw r0, 0x0(r1);
        mr r3, r6;
        bl GetMogiPublicSectionId;
        mr r6, r3;
        lwz r0, 0x0(r1);
        blr;);
}
kmBranch(0x806438ac, NormalizeMogiVotePageSection);
kmPatchExitPoint(NormalizeMogiVotePageSection, 0x806438b0);

// 0x80643cf0 is the UpdateOnlineParams call immediately after PrepareRace.
static void UpdateMogiVotingOnlineParams(Pages::SELECTStageMgr* page) {
    page->UpdateOnlineParams();
    if (IsActive()) Racedata::sInstance->menusScenario.settings.gamemode = MODE_PUBLIC_VS;
}
kmCall(0x80643cf0, UpdateMogiVotingOnlineParams);

}  // namespace Mogi
}  // namespace Pulsar
