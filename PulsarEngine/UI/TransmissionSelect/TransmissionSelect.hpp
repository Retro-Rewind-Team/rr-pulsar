#ifndef _PUL_TRANSMISSIONSELECT_
#define _PUL_TRANSMISSIONSELECT_

#include <MarioKartWii/UI/Page/Menu/DriftSelect.hpp>
#include <Settings/SettingsParam.hpp>
#include <UI/UI.hpp>

namespace Pulsar {
namespace UI {

Transmission GetSelectedTransmission(u32 hudSlotId);
void SetSelectedTransmission(u32 hudSlotId, Transmission transmission);

class TransmissionSelect : public Pages::DriftSelect {
   public:
    static const PulPageId id = PULPAGE_TRANSMISSIONSELECT;

    void OnInit() override;
    void OnActivate() override;
    void AfterControlUpdate() override;
    void OnExternalButtonSelect(PushButton& button, u32 hudSlotId) override;
    void OnButtonClick(PushButton& button, u32 hudSlotId);
};

void LoadTransmissionSelectAfterDrift(Pages::Menu& menu, PageId id, PushButton& button);
void LoadTransmissionSelectAfterGlobeDrift(Pages::Menu& menu, u32 animDirection, float animLength);

}  // namespace UI
}  // namespace Pulsar

#endif
