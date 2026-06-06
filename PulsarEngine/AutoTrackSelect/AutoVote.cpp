#include <MarioKartWii/UI/Page/Other/ActionLess.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <PulsarSystem.hpp>
#include <AutoTrackSelect/AutoVote.hpp>
#include <Network/PacketExpansion.hpp>
#include <Network/PulSELECT.hpp>
#include <SlotExpansion/CupsConfig.hpp>

namespace Pulsar {
namespace UI {

AutoVote::AutoVote() : readyDuration(0) {}

void AutoVote::OnActivate() {
    Pages::AutoEnding* msg = SectionMgr::sInstance->curSection->Get<Pages::AutoEnding>(PAGE_AUTO_ENDING2);
    msg->SetMessageWindowText(BMG_READY_TO_RACE, nullptr);
    this->AddPageLayer(PAGE_AUTO_ENDING2, 0);
}

void AutoVote::BeforeControlUpdate() {}
void AutoVote::OnResume() {}

void AutoVote::OnInit() {
    this->timerControl.isHidden = true;
    SELECTStageMgr::OnInit();
}

void AutoVote::OnDispose() {
    RKNet::SELECTHandler::DestroyInstance();
}

void AutoVote::OnUpdate() {
    Network::ExpSELECTHandler& select = Network::ExpSELECTHandler::Get();
    CupsConfig* cupsConfig = CupsConfig::sInstance;

    const SectionMgr* sectionMgr = SectionMgr::sInstance;
    if (status == 6) {
        Pages::MessageBox* messageBox = sectionMgr->curSection->Get<Pages::MessageBox>();
        messageBox->Reset();
        messageBox->SetMessageWindowText(0xfb2);
        const PtmfHolder_1A<Page, void, Pages::Click&>& onMessageClickPtmf = this->onDisconnectHandler;
        messageBox->masterPageOnClickHandler = &onMessageClickPtmf;
        this->AddPageLayer(PAGE_MESSAGEBOX, 0);
        this->status = STATUS_DISCONNECT_MSG;
    }
    if (reinterpret_cast<RKNet::SELECTHandler&>(select).IsPrepared() && status == 2) {
        const RKNet::Controller* controller = RKNet::Controller::sInstance;
        const RKNet::ControllerSub& sub = controller->subs[controller->currentSub];
        const u8 hostAid = sub.hostAid;
        const u8 localAid = sub.localAid;
        PulsarId vote;
        if (hostAid == localAid) {
            vote = cupsConfig->GetWinning();
            select.toSendPacket.pulWinningTrack = vote;
            select.toSendPacket.winningVoterAid = hostAid;
        } else {
            const PulsarId hostVote = static_cast<PulsarId>(select.receivedPackets[hostAid].pulVote);
            if (hostVote != 0x43) {
                vote = hostVote;
                cupsConfig->SetWinning(hostVote, select.receivedPackets[hostAid].variantIdx);
                select.toSendPacket.pulWinningTrack = vote;
                select.toSendPacket.winningVoterAid = hostAid;
                select.toSendPacket.phase = 2;
            } else
                vote = PULSARID_FIRSTREG;
        }
        const SectionParams* params = sectionMgr->sectionParams;
        for (int i = 0; i < params->localPlayerCount; ++i) {
            const PlayerCombo& combo = params->combos[i];
            select.toSendPacket.variantIdx = cupsConfig->GetCurVariantIdx();
            reinterpret_cast<RKNet::SELECTHandler&>(select).SetPlayerData(combo.selCharacter, combo.selKart,
                                                                          static_cast<CourseId>(vote), i, combo.rank);
        }
        bool isReady = true;
        if (sub.connectionCount == 1 || this->duration > 5000) {
            isReady = false;
            this->status = STATUS_DISCONNECTED;  // 6
        } else if (hostAid == localAid) {
            for (int i = 0; i < sub.connectionCount; ++i) {
                if (i != hostAid) {
                    if (select.receivedPackets[i].phase != 2) {
                        // check winning course
                        isReady = false;
                    }
                }
            }
            select.toSendPacket.phase = 2;
        } else if (select.receivedPackets[hostAid].phase != 2)
            isReady = false;
        if (isReady) ++readyDuration;
        if (readyDuration > 180) {
            reinterpret_cast<RKNet::SELECTHandler&>(select).AllocatePlayerIdsToAids();
            this->status = STATUS_VOTES_PAGE;
            ArchiveMgr::sInstance->RequestLoadCourseAsync(static_cast<CourseId>(cupsConfig->GetWinning()));
            this->SetModeTypes();
            this->PrepareRace();
            this->UpdateOnlineParams();
            this->ChangeSectionBySceneChange(static_cast<SectionId>(sectionMgr->curSection->sectionId + 0x10), 0, 0.0f);
        }
    }
}

}  // namespace UI
}  // namespace Pulsar
