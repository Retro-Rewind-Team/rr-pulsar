/*
    VariantSelectHooks.cpp
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

#include <kamek.hpp>
#include <runtimeWrite.hpp>
#include <MarioKartWii/UI/Page/Menu/Menu.hpp>
#include <MarioKartWii/UI/Ctrl/Menu/CtrlMenuCourse.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>
#include <SlotExpansion/CupsConfig.hpp>
#include <UI/SelectStage/VariantSelect.hpp>
#include <UI/UI.hpp>

namespace Pulsar {
namespace UI {

kmRuntimeUse(0x807e5434);
static void CourseSelect_OnCourseButtonClick(CtrlMenuCourseSelectCourse* self, PushButton& courseButton, u32 hudSlotId) {
    CupsConfig* cups = CupsConfig::sInstance;
    ExpSection* section = ExpSection::GetSection();
    typedef void (*OrigFn)(CtrlMenuCourseSelectCourse*, PushButton&, u32);
    OrigFn orig = (OrigFn)kmRuntimeAddr(0x807e5434);

    VariantSelect* variantPage = section->GetPulPage<VariantSelect>();
    bool isVariantContext = (variantPage != nullptr && self == &variantPage->CtrlMenuCourseSelectCourse);
    bool handled = false;

    if (!isVariantContext) {
        PulsarCupId lastCup = cups->lastSelectedCup;
        PulsarId selected = cups->ConvertTrack_PulsarCupToTrack(lastCup, courseButton.buttonId);
        if (!cups->IsReg(selected)) {
            const Track& track = cups->GetTrack(selected);
            if (track.variantCount > 0) {
                cups->SetSelected(selected);
                Pages::CourseSelect* coursePage = SectionMgr::sInstance->curSection->Get<Pages::CourseSelect>();
                if (coursePage != nullptr) {
                    variantPage->SetBaseRowIdx(static_cast<u8>(courseButton.buttonId));
                    coursePage->LoadNextPageById(static_cast<PageId>(PULPAGE_VARIANTSELECT), courseButton);
                    return;
                }
                return;
            }
            Pages::CourseSelect* coursePage = SectionMgr::sInstance->curSection->Get<Pages::CourseSelect>();
            if (coursePage != nullptr) {
                coursePage->LoadNextPage(coursePage->CtrlMenuCourseSelectCourse, courseButton, hudSlotId);
                return;
            }
        }
    } else if (variantPage != nullptr) {
        u32 variantIdx = variantPage->GetVariantIndexForButton(courseButton);
        if (variantIdx != 0xFFFFFFFF) {
            PulsarId sel = cups->GetSelected();
            cups->SetPendingVariant(static_cast<u8>(variantIdx));
            variantPage->LoadNextPage(variantPage->CtrlMenuCourseSelectCourse, courseButton, hudSlotId);
            handled = true;
        }
    }

    if (!handled) orig(self, courseButton, hudSlotId);
}
kmBranch(0x807e5434, CourseSelect_OnCourseButtonClick);

}  // namespace UI
}  // namespace Pulsar