/*
    VariantSelect.cpp
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

#include <UI/SelectStage/VariantSelect.hpp>
#include <SlotExpansion/CupsConfig.hpp>
#include <SlotExpansion/UI/ExpansionUIMisc.hpp>
#include <MarioKartWii/System/Identifiers.hpp>
#include <MarioKartWii/UI/Page/Menu/CourseSelect.hpp>
#include <MarioKartWii/UI/Ctrl/Menu/CtrlMenuCourse.hpp>
#include <MarioKartWii/UI/Page/Other/SELECTStageMgr.hpp>
#include <MarioKartWii/UI/Ctrl/CountDown.hpp>
#include <UI/UI.hpp>
#include <MarioKartWii/UI/Text/Text.hpp>

namespace Pulsar {
namespace UI {

VariantSelect::VariantSelect() {
    this->onBackPressHandler.subject = this;
    this->onBackPressHandler.ptmf = &VariantSelect::OnBackPress;
    this->onBackClickHandler.subject = this;
    this->onBackClickHandler.ptmf = &VariantSelect::OnBackButtonClick;
    this->controlsManipulatorManager.SetGlobalHandler(BACK_PRESS, this->onBackPressHandler, false, false);
    selectedPulsarId = PULSARID_NONE;
    baseRowIdx = 0;
    variantButtonsPopulated = false;
    ResetVariantButtonState();
}

void VariantSelect::OnActivate() {
    Pages::CourseSelect::OnActivate();
    ToggleCourseSelectDecor(true);
    selectedPulsarId = CupsConfig::sInstance->GetSelected();
    PopulateVariantButtons();
}

void VariantSelect::OnDeactivate() {
    variantButtonsPopulated = false;
    ResetVariantButtonState();
    baseRowIdx = 0;
    if (CupsConfig::sInstance != nullptr) CupsConfig::sInstance->ClearPendingVariant();
    ToggleCourseSelectDecor(false);
    Pages::CourseSelect::OnDeactivate();
}

UIControl* VariantSelect::CreateControl(u32 controlId) { return Pages::CourseSelect::CreateControl(controlId); }

void VariantSelect::BeforeControlUpdate() {
    Pages::SELECTStageMgr* selectStageMgr = SectionMgr::sInstance->curSection->Get<Pages::SELECTStageMgr>();
    if (selectStageMgr != nullptr) {
        CountDown* timer = &selectStageMgr->countdown;
        if (timer->countdown <= 0) {
            CupsConfig* cups = CupsConfig::sInstance;
            if (cups != nullptr) {
                cups->ClearPendingVariant();
                cups->SetSelected(static_cast<PulsarId>(RANDOM));
            }
            this->OnTimeout();
            return;
        }
    }
    Pages::CourseSelect::BeforeControlUpdate();
}

void VariantSelect::AfterControlUpdate() {
    if (variantButtonsPopulated) ApplyVariantButtonState();
}

void VariantSelect::ToggleCourseSelectDecor(bool hidden) {
    this->ctrlMenuCourseSelectCup.isHidden = hidden;
    if (this->titleText) this->titleText->isHidden = hidden;
    if (this->bottomText) this->bottomText->isHidden = hidden;
    for (u32 i = 0; i < this->externControlCount; ++i) {
        PushButton* ctrl = this->externControls[i];
        if (ctrl) {
            ctrl->isHidden = hidden;
            ctrl->manipulator.inaccessible = hidden;
        }
    }
}

void VariantSelect::OnInit() {
    Pages::CourseSelect::OnInit();
    this->backButton.SetOnClickHandler(this->onBackClickHandler, 0);
}

void VariantSelect::OnBackPress(u32 hudSlotId) {
    if (CupsConfig::sInstance != nullptr) CupsConfig::sInstance->ClearPendingVariant();
    this->LoadPrevPageById(PAGE_COURSE_SELECT, this->backButton);
}

void VariantSelect::OnBackButtonClick(PushButton& button, u32 hudSlotId) {
    OnBackPress(hudSlotId);
}

void VariantSelect::PopulateVariantButtons() {
    CupsConfig* cups = CupsConfig::sInstance;
    if (!cups) return;
    if (selectedPulsarId == PULSARID_NONE) return;

    ResetVariantButtonState();

    if (!cups->IsReg(selectedPulsarId)) {
        const Track& track = cups->GetTrack(selectedPulsarId);
        u32 variantCount = track.variantCount;
        const u32 maxButtons = 4;
        u32 displayCount = variantCount + 1;
        if (displayCount > maxButtons) displayCount = maxButtons;
        for (u32 i = 0; i < displayCount; ++i) {
            variantButtonVariants[i] = static_cast<u8>(i);
        }
        variantButtonsPopulated = (displayCount > 0);
    } else {
        variantButtonsPopulated = false;
    }

    ApplyVariantButtonState();
    if (variantButtonsPopulated) {
        const u8 desiredVariantIdx = cups->GetLastSelectedVariant(selectedPulsarId);
        u32 desiredButtonIdx = 0;
        for (u32 i = 0; i < 4; ++i) {
            if (variantButtonVariants[i] == desiredVariantIdx) {
                desiredButtonIdx = i;
                break;
            }
        }
        this->CtrlMenuCourseSelectCourse.courseButtons[desiredButtonIdx].Select(0);
    }
}

void VariantSelect::ApplyVariantButtonState() {
    CupsConfig* cups = CupsConfig::sInstance;
    if (!cups) return;
    for (u32 i = 0; i < 4; ++i) {
        CourseButton& btn = this->CtrlMenuCourseSelectCourse.courseButtons[i];
        const u8 variantIdx = variantButtonVariants[i];
        if (selectedPulsarId == PULSARID_NONE || variantIdx == 0xFF) {
            btn.manipulator.inaccessible = true;
            btn.UIControl::isHidden = true;
            continue;
        }
        btn.UIControl::isHidden = false;
        btn.manipulator.inaccessible = false;
        btn.buttonId = static_cast<s32>(baseRowIdx);
        Text::Info info;
        memset(&info, 0, sizeof(info));
        u32 bmgId;
        if (variantIdx == 0 && cups->GetTrack(selectedPulsarId).variantCount == 0) {
            u32 realId = CupsConfig::ConvertTrack_PulsarIdToRealId(selectedPulsarId);
            const u32 VARIANT_TRACKS_BASE = 0x400000;
            bmgId = VARIANT_TRACKS_BASE + (realId << 4);
        } else {
            bmgId = GetTrackVariantBMGId(selectedPulsarId, variantIdx);
        }

        if (bmgId != 0) {
            info.bmgToPass[0] = bmgId;
            btn.SetMessage(bmgId, &info);
            continue;
        } else {
            wchar_t* nameBuf = variantButtonNames[i];
            const char* fileName = cups->GetFileName(selectedPulsarId, variantIdx);
            if (fileName != nullptr) {
                mbstowcs(nameBuf, fileName, 127);
                nameBuf[127] = L'\0';
            } else if (variantIdx == 0) {
                swprintf(nameBuf, 128, L"%ls", L"Default");
            } else {
                swprintf(nameBuf, 128, L"Variant %u", static_cast<u32>(variantIdx));
            }
            info.strings[0] = nameBuf;
        }
        btn.SetMessage(UI::BMG_TEXT, &info);
    }
}

u32 VariantSelect::GetVariantIndexForButton(const PushButton& button) const {
    for (u32 i = 0; i < 4; ++i) {
        if (&this->CtrlMenuCourseSelectCourse.courseButtons[i] == &button) {
            if (variantButtonVariants[i] == 0xFF) return 0xFFFFFFFF;
            return variantButtonVariants[i];
        }
    }
    return 0xFFFFFFFF;
}

void VariantSelect::ResetVariantButtonState() {
    for (u32 i = 0; i < 4; ++i) {
        variantButtonVariants[i] = 0xFF;
        variantButtonNames[i][0] = L'\0';
    }
}

void VariantSelect::SetBaseRowIdx(u8 rowIdx) {
    baseRowIdx = rowIdx;
}

}  // namespace UI
}  // namespace Pulsar