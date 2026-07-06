#include <UI/TransmissionSelect/TransmissionSelect.hpp>
#include <Gamemodes/OnlineTT/OnlineTT.hpp>
#include <Network/PacketExpansion.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/UI/Page/Menu/KartSelect.hpp>
#include <UI/ChangeCombo/ChangeCombo.hpp>

namespace Pulsar {
namespace UI {

static Transmission selectedTransmission[4] = {
    TRANSMISSION_INSIDE,
    TRANSMISSION_INSIDE,
    TRANSMISSION_INSIDE,
    TRANSMISSION_INSIDE,
};

static bool IsVanillaModeOnline() {
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    return controller != nullptr && controller->connectionState != RKNet::CONNECTIONSTATE_SHUTDOWN &&
           System::sInstance->IsVanillaMode();
}

static bool ShouldSkipTransmissionSelect(const System* system) {
    if (IsVanillaModeOnline()) return true;
    if (system->IsContext(PULSAR_STARTREGS)) return true;
    return system->IsContext(PULSAR_TRANSMISSIONINSIDE) || system->IsContext(PULSAR_TRANSMISSIONOUTSIDE) ||
           system->IsContext(PULSAR_TRANSMISSIONVANILLA);
}

static Transmission GetTransmissionFromButton(const PushButton& button) {
    return button.buttonId == 0 ? TRANSMISSION_OUTSIDE : TRANSMISSION_INSIDE;
}

Transmission GetSelectedTransmission(u32 hudSlotId) {
    if (hudSlotId >= 4) return TRANSMISSION_INSIDE;
    return selectedTransmission[hudSlotId];
}

void SetSelectedTransmission(u32 hudSlotId, Transmission transmission) {
    if (hudSlotId < 4) selectedTransmission[hudSlotId] = transmission;
}

static void SetTransmissionMessages(Pages::Menu& menu) {
    menu.titleText->SetMessage(BMG_TRANSMISSION_SELECT);
    menu.externControls[0]->SetMessage(BMG_OUTSIDE_TRANSMISSION);
    menu.externControls[1]->SetMessage(BMG_INSIDE_TRANSMISSION);
}

static void SelectCurrentTransmission(Pages::Menu& menu, u32 hudSlotId) {
    const u32 buttonId = GetSelectedTransmission(hudSlotId) == TRANSMISSION_OUTSIDE ? 0 : 1;
    menu.SelectButton(*menu.externControls[buttonId]);
}

static void HideTransmissionExtras(Pages::Menu& menu) {
    for (u32 i = 0; i < menu.curMovieCount; ++i) {
        menu.movies[i]->CtrlMenuMovieHandler::isHidden = true;
    }
    if (menu.externControlCount > 2) {
        menu.externControls[2]->isHidden = true;
        menu.externControls[2]->manipulator.inaccessible = true;
    }
}

static void CopyKartTimerToTransmission(Pages::Menu& menu) {
    TransmissionSelect* transmissionPage = ExpSection::GetSection()->GetPulPage<TransmissionSelect>();
    if (transmissionPage == nullptr) return;
    transmissionPage->timer = static_cast<Pages::KartSelect&>(menu).timer;
}

void TransmissionSelect::OnInit() {
    Pages::DriftSelect::OnInit();
    SetTransmissionMessages(*this);
    HideTransmissionExtras(*this);
    this->onButtonClickHandler.subject = this;
    this->onButtonClickHandler.ptmf = static_cast<void (Pages::MenuInteractable::*)(PushButton&, u32)>(&TransmissionSelect::OnButtonClick);
}

void TransmissionSelect::OnActivate() {
    Pages::DriftSelect::OnActivate();
    StopRandomComboRoulette();
    SetTransmissionMessages(*this);
    HideTransmissionExtras(*this);
    SelectCurrentTransmission(*this, 0);
}

void TransmissionSelect::AfterControlUpdate() {
    if (this->currentState != STATE_ACTIVE || this->timer == nullptr) return;
    if (this->timer->countdown > 0.0f) return;

    PushButton* button;
    if (this->externControls[0]->IsSelected()) {
        button = this->externControls[0];
    } else if (this->externControls[1]->IsSelected()) {
        button = this->externControls[1];
    } else {
        const u32 buttonId = GetSelectedTransmission(0) == TRANSMISSION_OUTSIDE ? 0 : 1;
        button = this->externControls[buttonId];
    }

    this->OnExternalButtonSelect(*button, 0);
    button->SelectFocus();
    this->OnButtonClick(*button, 0);
}

void TransmissionSelect::OnExternalButtonSelect(PushButton& button, u32) {
    if (button.buttonId == -100) {
        this->bottomText->SetMessage(0);
        return;
    }
    this->bottomText->SetMessage(button.buttonId == 0 ? BMG_OUTSIDE_TRANSMISSION_BOTTOM : BMG_INSIDE_TRANSMISSION_BOTTOM);
}

void TransmissionSelect::OnButtonClick(PushButton& button, u32 hudSlotId) {
    if (button.buttonId == -100) {
        this->LoadPrevPage(button);
        return;
    }
    SetSelectedTransmission(hudSlotId, GetTransmissionFromButton(button));
    this->LoadNextPageById(PAGE_DRIFT_SELECT, button);
}

void LoadTransmissionSelectBeforeDrift(Pages::Menu& menu, PageId id, PushButton& button) {
    System* system = System::sInstance;
    if (ShouldSkipTransmissionSelect(system)) {
        menu.LoadNextPageById(id, button);
        return;
    }
    CopyKartTimerToTransmission(menu);
    menu.LoadNextPageById(static_cast<PageId>(TransmissionSelect::id), button);
}
kmCall(0x80846d2c, LoadTransmissionSelectBeforeDrift);
kmCall(0x80846d64, LoadTransmissionSelectBeforeDrift);
kmCall(0x80846e1c, LoadTransmissionSelectBeforeDrift);
kmCall(0x80846e40, LoadTransmissionSelectBeforeDrift);

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
    menu.LoadNextPageById(id, button);
}

}  // namespace UI
}  // namespace Pulsar
