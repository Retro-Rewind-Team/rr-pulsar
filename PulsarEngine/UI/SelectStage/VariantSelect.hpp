/*
    VariantSelect.hpp
    Copyright (C) 2025 ZPL

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

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
    u32 GetVariantIndexForButton(const PushButton& button) const;
    void SetBaseRowIdx(u8 rowIdx);

   private:
    void PopulateVariantButtons();
    void ApplyVariantButtonState();
    void ResetVariantButtonState();
    void ToggleCourseSelectDecor(bool hidden);
    PulsarId selectedPulsarId;
    u8 baseRowIdx;
    u8 variantButtonVariants[4];
    wchar_t variantButtonNames[4][128];
    bool variantButtonsPopulated;
    PtmfHolder_2A<VariantSelect, void, PushButton&, u32> onBackClickHandler;
};

}  // namespace UI
}  // namespace Pulsar
