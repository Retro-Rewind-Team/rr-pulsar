#include <hooks.hpp>
#include <kamek.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/RKNet/SELECT.hpp>
#include <MarioKartWii/System/Identifiers.hpp>
#include <MarioKartWii/UI/Page/Menu/KartSelect.hpp>
#include <MarioKartWii/UI/Page/Other/SELECTStageMgr.hpp>
#include <Network/Mogi.hpp>
#include <PulsarSystem.hpp>
#include <Settings/UI/SettingsPageSelect.hpp>
#include <UI/UI.hpp>

namespace Pulsar {
namespace Mogi {

static void ConvertMogiPublicRoomToPrivate(RKNet::Controller* controller) {
    if (!IsEnabled()) {
        controller->SetVoteMatchmakingSuspend();
        return;
    }
    const RKNet::ControllerSub& sub = controller->subs[controller->currentSub];
    const bool isHost = sub.localAid == sub.hostAid;
    controller->roomType = isHost ? RKNet::ROOMTYPE_FROOM_HOST : RKNet::ROOMTYPE_FROOM_NONHOST;
    controller->localStatusData.status = isHost ? RKNet::FRIEND_STATUS_FROOM_VS_HOST : RKNet::FRIEND_STATUS_FROOM_VS_NON_HOST;
    controller->UpdateStatusDatas();
    if (sub.playerCount >= 12) controller->SetVoteMatchmakingSuspend();
}
kmCall(0x80663e64, ConvertMogiPublicRoomToPrivate);

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

static void OpenMogiFormatVotePage() {
    if (!IsFormatVoteActive()) return;
    UI::ExpSection* section = UI::ExpSection::GetSection();
    if (section == nullptr || section->GetPulPage<UI::SettingsPageSelect>() == nullptr) return;
    UI::ExpSection::AddPageLayer(*section, UI::SettingsPageSelect::id);
}

asmFunc OpenMogiFormatVotePageHook() {
    ASM(
        nofralloc;
        mflr r0;
        stw r0, 0x8(r1);
        bl OpenMogiFormatVotePage;
        lwz r0, 0x8(r1);
        mtlr r0;
        lwz r0, 0x24(r1);
        blr;);
}
kmCall(0x806501f8, OpenMogiFormatVotePageHook);

static bool IsMogiSelectPrepared(RKNet::SELECTHandler* handler) {
    if (IsFormatVoteActive()) return false;
    return handler->IsPrepared();
}
kmCall(0x80650484, IsMogiSelectPrepared);

static void AllowedMogiVehicles(Pages::KartSelect* page) {
    page->Menu::OnActivate();
    if (!IsEnabled()) return;

    for (u32 i = 0; i < 36; ++i) {
        switch (Pages::KartSelect::kartUIOrderToIDArray[i]) {
            case BULLET_BIKE:
            case MINI_BEAST:
            case MACH_BIKE:
            case WILD_WING:
            case BOWSER_BIKE:
            case FLAME_FLYER:
                break;
            default:
                page->isUnlocked[i] = false;
                break;
        }
    }
}
kmCall(0x80845524, AllowedMogiVehicles);

}  // namespace Mogi
}  // namespace Pulsar
