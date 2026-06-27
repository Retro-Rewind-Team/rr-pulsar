#include <UI/TransmissionSelect/TransmissionSelect.hpp>
#include <Gamemodes/OnlineTT/OnlineTT.hpp>
#include <Network/PacketExpansion.hpp>

namespace Pulsar {
namespace UI {

static Transmission selectedTransmission[4] = {
    TRANSMISSION_INSIDEALL,
    TRANSMISSION_INSIDEALL,
    TRANSMISSION_INSIDEALL,
    TRANSMISSION_INSIDEALL,
};

static PageId nextPageAfterTransmission = PAGE_NONE;

static Transmission GetTransmissionFromButton(const PushButton& button) {
    return button.buttonId == 0 ? TRANSMISSION_INSIDEALL : TRANSMISSION_OUTSIDE;
}

Transmission GetSelectedTransmission(u32 hudSlotId) {
    if (hudSlotId >= 4) return TRANSMISSION_INSIDEALL;
    return selectedTransmission[hudSlotId];
}

void SetSelectedTransmission(u32 hudSlotId, Transmission transmission) {
    if (hudSlotId < 4) selectedTransmission[hudSlotId] = transmission;
}

static void SetTransmissionMessages(Pages::Menu& menu) {
    menu.titleText->SetMessage(BMG_TRANSMISSION_SELECT);
    menu.externControls[0]->SetMessage(BMG_INSIDE_TRANSMISSION);
    menu.externControls[1]->SetMessage(BMG_OUTSIDE_TRANSMISSION);
}

static void SelectCurrentTransmission(Pages::Menu& menu, u32 hudSlotId) {
    const u32 buttonId = GetSelectedTransmission(hudSlotId) == TRANSMISSION_OUTSIDE ? 1 : 0;
    menu.SelectButton(*menu.externControls[buttonId]);
}

void TransmissionSelect::OnInit() {
    Pages::DriftSelect::OnInit();
    SetTransmissionMessages(*this);
    this->onButtonClickHandler.subject = this;
    this->onButtonClickHandler.ptmf = static_cast<void (Pages::MenuInteractable::*)(PushButton&, u32)>(&TransmissionSelect::OnButtonClick);
}

void TransmissionSelect::OnActivate() {
    Pages::DriftSelect::OnActivate();
    SetTransmissionMessages(*this);
    SelectCurrentTransmission(*this, 0);
}

void TransmissionSelect::OnExternalButtonSelect(PushButton& button, u32) {
    if (button.buttonId == -100) {
        this->bottomText->SetMessage(0);
        return;
    }
    this->bottomText->SetMessage(button.buttonId == 0 ? BMG_INSIDE_TRANSMISSION_BOTTOM : BMG_OUTSIDE_TRANSMISSION_BOTTOM);
}

void TransmissionSelect::OnButtonClick(PushButton& button, u32 hudSlotId) {
    if (button.buttonId == -100) {
        this->LoadPrevPage(button);
        return;
    }
    SetSelectedTransmission(hudSlotId, GetTransmissionFromButton(button));
    PageId next = nextPageAfterTransmission;
    if (next == PAGE_NONE) next = PAGE_CUP_SELECT;
    this->LoadNextPageById(next, button);
}

void LoadTransmissionSelectAfterDrift(Pages::Menu& menu, PageId id, PushButton& button) {
    System* system = System::sInstance;
    if (system->IsContext(PULSAR_MODE_OTT) && system->ottMgr.voteState == OTT::COMBO_SELECTION) {
        system->ottMgr.voteState = OTT::COMBO_SELECTED;
        Pulsar::Network::ExpSELECTHandler& handler = Pulsar::Network::ExpSELECTHandler::Get();
        handler.toSendPacket.playersData[0].character = SectionMgr::sInstance->sectionParams->characters[0];
        handler.toSendPacket.playersData[0].kart = SectionMgr::sInstance->sectionParams->karts[0];
        handler.toSendPacket.allowChangeComboStatus = Network::SELECT_COMBO_SELECTED;
        menu.EndStateAnimated(0, 0.0f);
        menu.LoadNextPageById(PAGE_SELECT_STAGE_MGR, button);
        return;
    }
    nextPageAfterTransmission = id;
    menu.LoadNextPageById(static_cast<PageId>(TransmissionSelect::id), button);
}

}  // namespace UI
}  // namespace Pulsar
