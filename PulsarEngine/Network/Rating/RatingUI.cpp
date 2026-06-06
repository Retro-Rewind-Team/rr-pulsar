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
        FormatRatingDigits(rating, buf, 64);
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

static void FillWFCRecordsControl(Page* /*page*/, int row, LayoutUIControl* control) {
    RKSYS::Mgr* rksys = RKSYS::Mgr::sInstance;
    RKSYS::LicenseMgr* license = nullptr;
    if (rksys && rksys->curLicenseId >= 0) {
        license = &rksys->licenses[rksys->curLicenseId];
    }

    control->SetTextBoxMessage("menu_text", row + 0x2020);

    if (license && (row == 0 || row == 2)) {
        const float rating = (row == 0) ? GetUserVR(rksys->curLicenseId) : GetUserBR(rksys->curLicenseId);
        wchar_t buf[64];
        Text::Info info;
        FormatRatingDigits(rating, buf, 64);
        info.strings[0] = buf;
        control->SetTextBoxMessage("score", UI::BMG_TEXT, &info);
        return;
    }

    Text::Info info;
    if (row == 0) {
        info.intToPass[0] = license ? license->vr.points : 0;
    } else if (row == 1) {
        info.intToPass[0] = license ? license->wFCVSWins : 0;
        info.intToPass[1] = license ? license->wFCVSLosses : 0;
    } else if (row == 2) {
        info.intToPass[0] = license ? license->br.points : 0;
    } else if (row == 3) {
        info.intToPass[0] = license ? license->wFCBattleWins : 0;
        info.intToPass[1] = license ? license->wFCBattleLosses : 0;
    }
    control->SetTextBoxMessage("score", row + 0x2052, &info);
}
kmBranch(0x8085fc0c, FillWFCRecordsControl);

}  // namespace PointRating
}  // namespace Pulsar
