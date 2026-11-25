#include <PulsarSystem.hpp>
#include <MarioKartWii/UI/Page/Menu/DriftSelect.hpp>
#include <MarioKartWii/UI/Page/Menu/MultiDriftSelect.hpp>
#include <UI/UI.hpp>
#include <GlobalFunction.hpp>

namespace Pulsar {
namespace UI {

static const s32 DRIFTSELECT_AUTO_BUTTON = 1;  // ButtonEasy
static bool IsHybridDriftDisabled() {
    const System *system = System::sInstance;
    if (system->IsVanillaMode())
        return true;
    return IsOnlineConnected() && IsFriendRoom() && (system->netMgr.hostContext2 & (1 << PULSAR_NOHYBRIDDRIFT));
}

static u32 GetAutoDriftButtonMessage() {
    return IsHybridDriftDisabled() ? BMG_AUTOMATIC_DRIFT : BMG_HYBRID_DRIFT;
}

static void DriftSelectOnActivate(Pages::DriftSelect *page) {
    page->Pages::DriftSelect::OnActivate();
    if (page->externControlCount > DRIFTSELECT_AUTO_BUTTON) {
        page->externControls[DRIFTSELECT_AUTO_BUTTON]->SetMessage(GetAutoDriftButtonMessage());
    }
}
kmWritePointer(0x808d9de0, DriftSelectOnActivate);

static void DriftSelectOnExternalButtonSelect(Pages::DriftSelect *page, PushButton &button, u32 hudSlotId) {
    page->Pages::DriftSelect::OnExternalButtonSelect(button, hudSlotId);
    if (button.buttonId == DRIFTSELECT_AUTO_BUTTON) {
        page->bottomText->SetMessage(IsHybridDriftDisabled() ? BMG_AUTOMATIC_DRIFT_BOTTOM : BMG_HYBRID_DRIFT_BOTTOM);
    }
}
kmWritePointer(0x808d9e14, DriftSelectOnExternalButtonSelect);

static void MultiDriftSelectOnActivate(Pages::MultiDriftSelect *page) {
    page->Pages::MultiDriftSelect::OnActivate();
    const u32 message = GetAutoDriftButtonMessage();
    for (u32 i = 0; i < page->externControlCount; i += 2) {  // even ids are ButtonAuto
        if (page->externControls[i] != nullptr)
            page->externControls[i]->SetMessage(message);
    }
}
kmWritePointer(0x808d9bf8, MultiDriftSelectOnActivate);

}  // namespace UI
}  // namespace Pulsar
