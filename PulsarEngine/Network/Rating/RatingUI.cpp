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

static u8 GetNameRatingIcon(u8 wiiWheelIconType, u8 starRating) {
    return static_cast<u8>((wiiWheelIconType * 4U) + starRating);
}
kmBranch(0x805e3d38, GetNameRatingIcon);

static void FillVRControl(Pages::VR* page, u32 index, u32 playerId, u32 team, u8 type, bool isLocalPlayer) {
    if (page == nullptr || index >= 12) {
        return;
    }

    LayoutUIControl& control = page->vrControls[index];
    control.ResetMsg();
    control.isHidden = false;

    Pages::SELECTStageMgr* selectMgr = nullptr;
    if (SectionMgr::sInstance != nullptr && SectionMgr::sInstance->curSection != nullptr) {
        selectMgr = SectionMgr::sInstance->curSection->Get<Pages::SELECTStageMgr>();
    }

    if (selectMgr != nullptr && playerId < selectMgr->playerCount) {
        control.SetMiiPane("chara_icon", selectMgr->miiGroup, playerId, 2);
        control.SetMiiPane("chara_icon_sha", selectMgr->miiGroup, playerId, 2);

        Text::Info nameInfo;
        nameInfo.miis[0] = selectMgr->miiGroup.GetMii(static_cast<u8>(playerId));
        control.SetTextBoxMessage("mii_name", 0x251d, &nameInfo);

        Text::Info pointsInfo;
        u32 valueMessageId = 0;
        u32 unitsMessageId = 0;

        wchar_t buffer[64];
        bool useString = false;

        if (type == 1) {
            float rating = 0.0f;
            bool hasDecimal = false;

            if (isLocalPlayer) {
                RKSYS::Mgr* rksys = RKSYS::Mgr::sInstance;
                if (rksys && rksys->curLicenseId >= 0) {
                    rating = GetUserVR(rksys->curLicenseId);
                    hasDecimal = true;
                } else {
                    rating = (float)selectMgr->infos[playerId].vr;
                }
            } else {
                u8 aid = selectMgr->infos[playerId].aid;
                u8 slot = selectMgr->infos[playerId].hudSlotid;
                if (aid < 12 && slot < 2) {
                    float baseRating = (float)selectMgr->infos[playerId].vr;
                    float decimal = (float)remoteDecimalVR[aid][slot] / 100.0f;
                    rating = baseRating + decimal;
                    hasDecimal = true;
                } else {
                    rating = (float)selectMgr->infos[playerId].vr;
                }
            }

            if ((u16)rating == 0xffff) {
                valueMessageId = 0x25e7;
            } else {
                if (hasDecimal) {
                    int rInt = (int)rating;
                    int rDec = (int)((rating - (float)rInt) * 100.0f + 0.5f);
                    if (rDec >= 100) {
                        rInt++;
                        rDec -= 100;
                    }
                    if (rDec < 0) rDec = -rDec;
                    if (rInt == 0)
                        swprintf(buffer, 64, L"%d", rDec);
                    else
                        swprintf(buffer, 64, L"%d%02d", rInt, rDec);
                    pointsInfo.strings[0] = buffer;
                    useString = true;
                    valueMessageId = UI::BMG_TEXT;
                    unitsMessageId = 0x25e4;
                } else {
                    valueMessageId = 0x13f1;
                    unitsMessageId = 0x25e4;
                    pointsInfo.intToPass[0] = (u16)rating;
                }
            }
        } else if (type == 2) {
            float rating = 0.0f;
            bool hasDecimal = false;

            if (isLocalPlayer) {
                RKSYS::Mgr* rksys = RKSYS::Mgr::sInstance;
                if (rksys && rksys->curLicenseId >= 0) {
                    rating = GetUserBR(rksys->curLicenseId);
                    hasDecimal = true;
                } else {
                    rating = (float)selectMgr->infos[playerId].br;
                }
            } else {
                u8 aid = selectMgr->infos[playerId].aid;
                u8 slot = selectMgr->infos[playerId].hudSlotid;
                if (aid < 12 && slot < 2) {
                    float baseRating = (float)selectMgr->infos[playerId].br;
                    float decimal = (float)remoteDecimalVR[aid][slot] / 100.0f;
                    rating = baseRating + decimal;
                    hasDecimal = true;
                } else {
                    rating = (float)selectMgr->infos[playerId].br;
                }
            }

            if ((u16)rating == 0xffff) {
                valueMessageId = 0x25e7;
            } else {
                if (hasDecimal) {
                    int rInt = (int)rating;
                    int rDec = (int)((rating - (float)rInt) * 100.0f + 0.5f);
                    if (rDec >= 100) {
                        rInt++;
                        rDec -= 100;
                    }
                    if (rDec < 0) rDec = -rDec;
                    if (rInt == 0)
                        swprintf(buffer, 64, L"%d", rDec);
                    else
                        swprintf(buffer, 64, L"%d.%02d", rInt, rDec);
                    pointsInfo.strings[0] = buffer;
                    useString = true;
                    valueMessageId = UI::BMG_TEXT;
                    unitsMessageId = 0x25e5;
                } else {
                    valueMessageId = 0x13f1;
                    unitsMessageId = 0x25e5;
                    pointsInfo.intToPass[0] = (u16)rating;
                }
            }
        }

        if (valueMessageId != 0) {
            control.SetTextBoxMessage("point_2", valueMessageId, &pointsInfo);
            control.SetTextBoxMessage("point_sha_2", valueMessageId, &pointsInfo);
            if (unitsMessageId != 0) {
                control.SetTextBoxMessage("pts_2", unitsMessageId);
                control.SetTextBoxMessage("pts_sha_2", unitsMessageId);
            }
        }
    } else {
        control.ResetTextBoxMessage("mii_name");
        control.SetPicturePane("chara_icon", "no_linkmii");
        control.SetPicturePane("chara_icon_sha", "no_linkmii");
    }

    if (team == 0) {
        control.SetPaneVisibility("red_null", true);
        control.SetPaneVisibility("blue_null", false);
    } else if (team == 1) {
        control.SetPaneVisibility("red_null", false);
        control.SetPaneVisibility("blue_null", true);
    } else {
        control.SetPaneVisibility("red_null", false);
        control.SetPaneVisibility("blue_null", false);
    }

    AnimationGroup& highlight = control.animator.GetAnimationGroupById(1);
    AnimationGroup& shadow = control.animator.GetAnimationGroupById(2);
    const u32 frame = isLocalPlayer ? 0U : 1U;
    highlight.PlayAnimationAtFrame(frame, 0.0f);
    shadow.PlayAnimationAtFrame(frame, 0.0f);
}
kmBranch(0x8064ab08, FillVRControl);

}  // namespace PointRating
}  // namespace Pulsar