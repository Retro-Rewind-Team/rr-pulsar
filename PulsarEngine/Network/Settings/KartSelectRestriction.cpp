#include <Network/Settings/SelectionRestrictions.hpp>
#include <MarioKartWii/GlobalFunctions.hpp>
#include <MarioKartWii/UI/Page/Menu/KartSelect.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>

namespace Pulsar {
namespace UI {

static void ApplyVehicleRestrictions(Pages::KartSelect *page) {
    page->Menu::OnActivate();
    if (Restrictions::IsVehicleRestrictionConfigActive() || !Restrictions::IsVehicleRestrictionEnabled()) return;

    for (u32 i = 0; i < 36; ++i) {
        const KartId kart = Pages::KartSelect::kartUIOrderToIDArray[i];
        page->isUnlocked[i] = Restrictions::IsVehicleEnabled(kart);
    }
}
kmCall(0x80845524, ApplyVehicleRestrictions);

static void SetVehicleAnimationTypeAndDefault(VehicleModelControl *model, PageId pageId) {
    model->SetAnimationType(pageId);
    if (!Restrictions::IsVehicleRestrictionEnabled()) return;

    Pages::KartSelect *page = SectionMgr::sInstance->curSection->Get<Pages::KartSelect>();
    if (page == nullptr || model != &page->vehicleModel) return;

    const u32 weight = GetCharacterWeightClass(SectionMgr::sInstance->sectionParams->characters[0]);
    for (u32 position = 0; position < Restrictions::VEHICLES_PER_WEIGHT; ++position) {
        const KartId kart = kartsSortedByWeight[weight][position];
        if (!Restrictions::IsVehicleEnabled(kart)) continue;

        ButtonMachine *button = page->GetButtonMachineById(static_cast<u8>(kart));
        if (button == nullptr || button->manipulator.inaccessible) continue;
        page->SelectButton(*button);
        break;
    }
}

kmCall(0x80847658, SetVehicleAnimationTypeAndDefault);
kmCall(0x80847678, SetVehicleAnimationTypeAndDefault);

}  // namespace UI
}  // namespace Pulsar