#pragma once

#include <kamek.hpp>
#include <MarioKartWii/UI/Page/Menu/CourseSelect.hpp>
#include <UI/UI.hpp>
#include <Config.hpp>
#include <MarioKartWii/UI/Ctrl/UIControl.hpp>
#include <MarioKartWii/UI/Ctrl/PushButton.hpp>

namespace Pulsar {
namespace UI {

class VariantSelect : public Pages::CourseSelect {
   public:
    static const u32 id = static_cast<u32>(PULPAGE_VARIANTSELECT);

    VariantSelect();

    void OnActivate() override;
    void OnDeactivate() override;
    UIControl* CreateControl(u32 controlId) override;
    void BeforeControlUpdate() override;
    void AfterControlUpdate() override;
    void OnInit() override;
    void OnBackPress(u32 hudSlotId);
    void OnBackButtonClick(PushButton& button, u32 hudSlotId);
    void OnVariantButtonSelect(PushButton& button, u32 hudSlotId);
    u32 GetVariantIndexForButton(const PushButton& button) const;
    void SetBaseRowIdx(u8 rowIdx);

   private:
    void PopulateVariantButtons();
    void ApplyVariantButtonState();
    void ResetVariantButtonState();
    void UpdateBottomText();
    void ToggleCourseSelectDecor(bool hidden);
    PulsarId selectedPulsarId;
    u8 baseRowIdx;
    u8 highlightedVariantIdx;
    u8 variantButtonVariants[4];
    wchar_t variantButtonNames[4][128];
    bool variantButtonsPopulated;
    PtmfHolder_2A<VariantSelect, void, PushButton&, u32> onBackClickHandler;
    PtmfHolder_2A<VariantSelect, void, PushButton&, u32> onVariantButtonSelectHandler;
};

}  // namespace UI
}  // namespace Pulsar
