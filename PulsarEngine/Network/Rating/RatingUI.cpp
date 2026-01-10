/*
    RatingUI.cpp
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
#include <MarioKartWii/Mii/MiiGroup.hpp>
#include <MarioKartWii/UI/Ctrl/Animation.hpp>
#include <MarioKartWii/UI/Ctrl/UIControl.hpp>
#include <MarioKartWii/UI/Page/Other/SELECTStageMgr.hpp>
#include <MarioKartWii/UI/Page/Other/VR.hpp>
#include <MarioKartWii/UI/Section/Section.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>
#include <MarioKartWii/UI/Text/Text.hpp>
#include <UI/UI.hpp>
#include <Network/Rating/PlayerRating.hpp>
#include <MarioKartWii/RKSYS/RKSYSMgr.hpp>

namespace Pulsar {
namespace PointRating {

static u8 GetNameRatingIcon(u8 wheelType, u8 starRating) {
    return wheelType * 4 + starRating;
}
kmBranch(0x805e3d38, GetNameRatingIcon);

static float GetRatingForDisplay(Pages::SELECTStageMgr* mgr, u32 playerId, bool isLocal, bool isBR, bool* hasDecimal) {
    *hasDecimal = false;
    if (isLocal) {
        RKSYS::Mgr* rksys = RKSYS::Mgr::sInstance;
        if (mgr->infos[playerId].hudSlotid == 0 && rksys && rksys->curLicenseId >= 0) {
            *hasDecimal = true;
            return isBR ? GetUserBR(rksys->curLicenseId) : GetUserVR(rksys->curLicenseId);
        }
        return (float)(isBR ? mgr->infos[playerId].br : mgr->infos[playerId].vr);
    }
    u8 aid = mgr->infos[playerId].aid;
    u8 slot = mgr->infos[playerId].hudSlotid;
    if (aid < 12 && slot < 2) {
        *hasDecimal = true;
        float base = (float)(isBR ? mgr->infos[playerId].br : mgr->infos[playerId].vr);
        return base + (float)remoteDecimalVR[aid][slot] / 100.0f;
    }
    return (float)(isBR ? mgr->infos[playerId].br : mgr->infos[playerId].vr);
}

static void FormatRatingText(float rating, bool hasDecimal, wchar_t* buf, Text::Info* info, u32* valMsg, u32* unitMsg, u32 unitId) {
    if ((u16)rating == 0xffff) {
        *valMsg = 0x25e7;
        return;
    }
    if (hasDecimal) {
        int rInt = (int)rating;
        int rDec = (int)((rating - (float)rInt) * 100.0f + 0.5f);
        if (rDec >= 100) { rInt++; rDec -= 100; }
        if (rDec < 0) rDec = -rDec;
        if (rInt == 0) swprintf(buf, 64, L"%d", rDec);
        else swprintf(buf, 64, L"%d%02d", rInt, rDec);
        info->strings[0] = buf;
        *valMsg = UI::BMG_TEXT;
    } else {
        *valMsg = 0x13f1;
        info->intToPass[0] = (u16)rating;
    }
    *unitMsg = unitId;
}

static void FillVRControl(Pages::VR* page, u32 idx, u32 playerId, u32 team, u8 type, bool isLocal) {
    if (!page || idx >= 12) return;
    
    LayoutUIControl& ctrl = page->vrControls[idx];
    ctrl.ResetMsg();
    ctrl.isHidden = false;
    
    Pages::SELECTStageMgr* mgr = nullptr;
    if (SectionMgr::sInstance && SectionMgr::sInstance->curSection) {
        mgr = SectionMgr::sInstance->curSection->Get<Pages::SELECTStageMgr>();
    }
    
    if (mgr && playerId < mgr->playerCount) {
        ctrl.SetMiiPane("chara_icon", mgr->miiGroup, playerId, 2);
        ctrl.SetMiiPane("chara_icon_sha", mgr->miiGroup, playerId, 2);
        
        Text::Info nameInfo;
        nameInfo.miis[0] = mgr->miiGroup.GetMii((u8)playerId);
        ctrl.SetTextBoxMessage("mii_name", 0x251d, &nameInfo);
        
        wchar_t buf[64];
        Text::Info ptsInfo;
        u32 valMsg = 0, unitMsg = 0;
        
        if (type == 1 || type == 2) {
            bool hasDecimal;
            float rating = GetRatingForDisplay(mgr, playerId, isLocal, type == 2, &hasDecimal);
            FormatRatingText(rating, hasDecimal, buf, &ptsInfo, &valMsg, &unitMsg, (type == 1) ? 0x25e4 : 0x25e5);
        }
        
        if (valMsg) {
            ctrl.SetTextBoxMessage("point_2", valMsg, &ptsInfo);
            ctrl.SetTextBoxMessage("point_sha_2", valMsg, &ptsInfo);
            if (unitMsg) {
                ctrl.SetTextBoxMessage("pts_2", unitMsg);
                ctrl.SetTextBoxMessage("pts_sha_2", unitMsg);
            }
        }
    } else {
        ctrl.ResetTextBoxMessage("mii_name");
        ctrl.SetPicturePane("chara_icon", "no_linkmii");
        ctrl.SetPicturePane("chara_icon_sha", "no_linkmii");
    }
    
    ctrl.SetPaneVisibility("red_null", team == 0);
    ctrl.SetPaneVisibility("blue_null", team == 1);
    
    AnimationGroup& hl = ctrl.animator.GetAnimationGroupById(1);
    AnimationGroup& sh = ctrl.animator.GetAnimationGroupById(2);
    u32 frame = isLocal ? 0 : 1;
    hl.PlayAnimationAtFrame(frame, 0.0f);
    sh.PlayAnimationAtFrame(frame, 0.0f);
}
kmBranch(0x8064ab08, FillVRControl);

}  // namespace PointRating
}  // namespace Pulsar