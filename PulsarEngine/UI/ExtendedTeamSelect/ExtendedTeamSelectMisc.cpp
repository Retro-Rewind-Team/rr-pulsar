#include <UI/ExtendedTeamSelect/ExtendedTeamSelect.hpp>
#include <MarioKartWii/UI/Page/Other/FriendRoom.hpp>
#include <RetroRewind.hpp>
#include <core/nw4r/lyt/ArcResourceAccessor.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/RKNet/ROOM.hpp>
#include <MarioKartWii/UI/Page/Leaderboard/GPVSLeaderboardUpdate.hpp>
#include <MarioKartWii/UI/Page/Other/SELECTStageMgr.hpp>
#include <MarioKartWii/UI/Page/Other/VR.hpp>
#include <MarioKartWii/UI/Page/Other/Votes.hpp>
#include <MarioKartWii/UI/Page/Other/WifiVSResults.hpp>
#include <MarioKartWii/UI/Ctrl/CtrlRace/CtrlRace2DMap.hpp>
#include <MarioKartWii/UI/Ctrl/CtrlRace/CtrlRaceBalloon.hpp>
#include <MarioKartWii/UI/Ctrl/CtrlRace/CtrlRaceRankNum.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>

namespace Pulsar {
namespace UI {

void PrepareOnlinePages(Pages::FriendRoomWaiting* _this) {
    _this->StartRoom();

    Pages::FriendRoomManager* friendRoomManager = SectionMgr::sInstance->curSection->Get<Pages::FriendRoomManager>();
    if (System::sInstance->IsContext(PULSAR_EXTENDEDTEAMS) && (friendRoomManager->startedGameMode == 0 || friendRoomManager->startedGameMode == 2 || friendRoomManager->startedGameMode == 3)) {
        _this->countdown.SetInitial(86400.0f);
        _this->AddPageLayer(static_cast<PageId>(PULPAGE_EXTENDEDTEAMSELECT), 0);
    } else {
        _this->AddPageLayer(PAGE_CHARACTER_SELECT, 0);
    }
}
kmWriteNop(0x805de76c);  // Remove the call that opens the character selection screen
kmCall(0x805dddb0, PrepareOnlinePages);

// This function is called when the host presses one of the start button in the Friend Room
// If Extended Teams is enabled, it will send a "Regular VS" message instead of a "Team VS" message
// Because I only patched the regular VS UIs for this mode
//
// The function looks odd because the compiler kept thinking that the packet was a pointer if you try
// to replace the "u32 pkt" by a "RKNet::ROOMPacket pkt", might be a skill issue on my own though
void SetBroadcastROOMPacket(RKNet::ROOMHandler* _this, u32 pkt) {
    u8 messageType = (pkt >> 24) & 0xFF;
    u16 message = (pkt >> 8) & 0xFFFF;
    u8 messageSequence = pkt & 0xFF;

    RKNet::Controller* controller = RKNet::Controller::sInstance;
    RKNet::ControllerSub* sub = &controller->subs[controller->currentSub];

    UI::ExtendedTeamSelect* extendedTeamSelect = SectionMgr::sInstance->curSection->Get<UI::ExtendedTeamSelect>();
    Pages::FriendRoomManager* friendRoomBackPage = SectionMgr::sInstance->curSection->Get<Pages::FriendRoomManager>();
    if (System::sInstance->IsContext(PULSAR_EXTENDEDTEAMS) && messageType == 1 && message == 1) {
        message = 0;
    }

    pkt = (messageType << 24) | (message << 8) | messageSequence;
    for (int i = 0; i < 12; ++i) {
        if (i != sub->localAid) {
            _this->toSendPackets[i] = *(RKNet::ROOMPacket*)&pkt;
        }
    }

    friendRoomBackPage->networkManager.lastSentPacket = *(RKNet::ROOMPacket*)&pkt;
}

kmWriteNop(0x805dce38);  // friendRoomBackPage->lastSentPacket = packet;
kmCall(0x805dce34, SetBroadcastROOMPacket);

void RecvRoomPacket(UnkFriendRoomManager* _this, u8 playerId, u8 myAid, RKNet::ROOMPacket& packet) {
    if (packet.messageType == ExtendedTeamManager::MSG_TYPE_START_RACE) {
        UI::ExtendedTeamManager::sInstance->SetStatusExternal(ExtendedTeamManager::STATUS_DONE);
    }

    _this->HandleROOMPacket(playerId, myAid, packet);
}
// kmCall(0x805db0f8, RecvRoomPacket);
kmCall(0x805db1dc, RecvRoomPacket);

void VotingVRPage_SetControlState(Pages::VR* _this, u32 idx, u32 playerId, Team team, u32 type, bool isLocalPlayer) {
    _this->FillVRControl(idx, playerId, team, type, isLocalPlayer);
    if (ExtendedTeamManager::IsActivated()) {
        Pages::SELECTStageMgr* selectStageMgr = SectionMgr::sInstance->curSection->Get<Pages::SELECTStageMgr>();
        u8 aid = selectStageMgr->infos[idx].aid;
        u8 playerIdOnConsole = selectStageMgr->infos[idx].hudSlotid;

        ExtendedTeamSelect::ChangeVRButtonColors(_this->vrControls[idx], ExtendedTeamManager::sInstance->GetPlayerTeamByAID(aid, playerIdOnConsole));
    }
}

kmCall(0x8064aa78, VotingVRPage_SetControlState);
kmCall(0x8064a9f0, VotingVRPage_SetControlState);

bool PageVote_FillVoteControl(Pages::Vote* _this, u32 playerId) {
    bool res = _this->FillVoteControl(playerId);
    if (ExtendedTeamManager::IsActivated() && res) {
        Pages::SELECTStageMgr* selectStageMgr = SectionMgr::sInstance->curSection->Get<Pages::SELECTStageMgr>();
        u8 aid = selectStageMgr->infos[playerId].aid;
        u8 playerIdOnConsole = selectStageMgr->infos[playerId].hudSlotid;

        VoteControl& control = _this->votes[_this->lastHandledVote - 1];
        control.animator.GetAnimationGroupById(3).PlayAnimationAtFrame(2, 0.0f);
        ExtendedTeamSelect::ChangeVRButtonColors(control, ExtendedTeamManager::sInstance->GetPlayerTeamByAID(aid, playerIdOnConsole));
    }

    return res;
}

kmCall(0x80643b3c, PageVote_FillVoteControl);

void SELECTStageMgr_PrepareRace(Pages::SELECTStageMgr* _this) {
    _this->PrepareRace();
    if (ExtendedTeamManager::IsActivated()) {
        ExtendedTeamManager::sInstance->VotePageSync();
    }
}

kmCall(0x80643ce8, SELECTStageMgr_PrepareRace);

void CtrlRace2DMapCharacter_CalcTransform(CtrlRace2DMapCharacter* _this, const Vec3& kartPosition, Vec2& dest, u32 r6) {
    _this->CalculatePosition(kartPosition, dest, r6);
    RacedataScenario& menuScenario = Racedata::sInstance->menusScenario;
    RKNet::Controller* controller = RKNet::Controller::sInstance;
    if (ExtendedTeamManager::IsActivated()) {
        RKNet::Controller* controller = RKNet::Controller::sInstance;
        ExtendedTeamID selfTeams[2] = {TEAM_COUNT, TEAM_COUNT};
        for (int i = 0; i < menuScenario.playerCount; i++) {
            if (menuScenario.players[i].playerType == PLAYER_REAL_LOCAL && menuScenario.players[i].hudSlotId == 0) {
                selfTeams[0] = ExtendedTeamManager::sInstance->GetPlayerTeam(i);
                if (controller) {
                    RKNet::ControllerSub& currentSub = controller->subs[controller->currentSub];
                    selfTeams[0] = ExtendedTeamManager::sInstance->GetPlayerTeamByAID(currentSub.localAid, 0);
                }
            } else if (menuScenario.players[i].playerType == PLAYER_REAL_LOCAL && menuScenario.players[i].hudSlotId == 1) {
                selfTeams[1] = ExtendedTeamManager::sInstance->GetPlayerTeam(i);
                if (controller) {
                    RKNet::ControllerSub& currentSub = controller->subs[controller->currentSub];
                    selfTeams[1] = ExtendedTeamManager::sInstance->GetPlayerTeamByAID(currentSub.localAid, 1);
                }
            }
        }

        ExtendedTeamID playerTeam = ExtendedTeamManager::sInstance->GetPlayerTeam(_this->playerId);
        if (playerTeam == selfTeams[0] || playerTeam == selfTeams[1]) {
            //_this->animator.GetAnimationGroupById(1).PlayAnimationAtFrameAndDisable(1, 0.0f); // shadow::red
            u8 r, g, b;
            ExtendedTeamSelect::GetTeamColor(ExtendedTeamManager::sInstance->GetPlayerTeam(_this->playerId), r, g, b);

            _this->SetPaneVisibility("chara_shadow_0_0", true);
            _this->SetPaneVisibility("chara_shadow_0_1", true);
            nw4r::lyt::Material* mat1 = _this->layout.GetPaneByName("chara_shadow_0_0")->GetMaterial();
            nw4r::lyt::Material* mat2 = _this->layout.GetPaneByName("chara_shadow_0_1")->GetMaterial();

            for (int i = 0; i < 2; i++) {
                mat1->tevColours[i].r = mat2->tevColours[i].r = r;
                mat1->tevColours[i].g = mat2->tevColours[i].g = g;
                mat1->tevColours[i].b = mat2->tevColours[i].b = b;
                mat1->tevColours[i].a = mat2->tevColours[i].a = 255;
            }

            _this->charaPane->alpha = 255;
            _this->charaShadow0Pane->alpha = 255;
            _this->charaShadow1Pane->alpha = 255;

            const int playerCount = Racedata::sInstance->racesScenario.playerCount;
            float z = 10.0 + ((playerCount - Raceinfo::sInstance->players[_this->playerId]->position) * 0.1);

            _this->zIdx = 20;
            _this->pane->trans.z = 20.0;
        }
    }
}

void CtrlRace2DMapCharacter_PlayAnimationAtFrameAndDisable(AnimationGroup* _this, u32 id, float frame) {
    if (ExtendedTeamManager::IsActivated()) {
        _this->isActive = false;
    } else {
        _this->PlayAnimationAtFrameAndDisable(id, frame);
    }
}

kmCall(0x807eb308, CtrlRace2DMapCharacter_CalcTransform);
kmCall(0x807eaf1c, CtrlRace2DMapCharacter_PlayAnimationAtFrameAndDisable);
kmCall(0x807eb9d4, CtrlRace2DMapCharacter_PlayAnimationAtFrameAndDisable);

void CtrlRaceNameBalloon_refresh(CtrlRaceNameBalloon* _this, u8 playerId) {
    _this->UpdateInfo(playerId);
    if (ExtendedTeamManager::IsActivated()) {
        //_this->animator.GetAnimationGroupById(1).PlayAnimationAtFrameAndDisable(1, 0.0f); // shadow::red
        u8 r, g, b;
        ExtendedTeamSelect::GetTeamColor(ExtendedTeamManager::sInstance->GetPlayerTeam(playerId), r, g, b);
        nw4r::lyt::TextBox* characterName = (nw4r::lyt::TextBox*)_this->layout.GetPaneByName("chara_name");
        nw4r::lyt::Material* mat = characterName->GetMaterial();

        characterName->color1[0] = nw4r::ut::Color(r, g, b, 255);

        mat->tevColours[1].r = 255;
        mat->tevColours[1].g = 255;
        mat->tevColours[1].b = 255;

        if (static_cast<Pulsar::ExtendedTeamsLine>(Pulsar::Settings::Mgr::Get().GetUserSettingValue(static_cast<Pulsar::Settings::UserType>(Pulsar::Settings::SETTINGSTYPE_EXTENDEDTEAMS), Pulsar::RADIO_EXTENDEDTEAMSLINE)) == Pulsar::EXTENDEDTEAMS_LINE_TEAM) {
            for (int i = 0; i < 4; i++) {
                _this->layout.GetPaneByName("line")->SetVtxColor(i, nw4r::ut::Color(r, g, b, 255));
            }
        }
    }
}

void CtrlRaceNameBalloon_PlayAnimationAtFrameAndDisable(AnimationGroup* _this, u32 id, float frame) {
    if (ExtendedTeamManager::IsActivated()) {
        _this->isActive = false;
    } else {
        _this->PlayAnimationAtFrameAndDisable(id, frame);
    }
}

kmCall(0x807f0c48, CtrlRaceNameBalloon_refresh);
kmCall(0x807f00f8, CtrlRaceNameBalloon_PlayAnimationAtFrameAndDisable);
kmCall(0x807efe5c, CtrlRaceNameBalloon_PlayAnimationAtFrameAndDisable);

void GPVSLeaderboardUpdate_hookSetupEntries(Pages::GPVSLeaderboardUpdate* _this) {
    _this->FillRows();
    if (_this->pageId == PAGE_GPVS_LEADERBOARD_UPDATE && ExtendedTeamManager::IsActivated()) {
        for (u8 i = 0; i < _this->GetRowCount(); i++) {
            u8 playerId = Raceinfo::sInstance->playerIdInEachPosition[i];
            CtrlRaceResult* result = _this->results[i];

            nw4r::lyt::Material* mat;
            nw4r::lyt::Pane* pane;
            if (Racedata::sInstance->racesScenario.players[playerId].playerType == PLAYER_REAL_LOCAL) {
                // result->animator.GetAnimationGroupById(4).PlayAnimationAtFrame(4, 0.0f);
                pane = result->layout.GetPaneByName("select_base");
                mat = pane->GetMaterial();
                pane->alpha = 255;
            } else {
                pane = result->layout.GetPaneByName("team_color_c");
                mat = pane->GetMaterial();
                pane->alpha = 200;
            }

            pane->flag |= 1;

            u8 r, g, b;
            ExtendedTeamSelect::GetTeamColor(ExtendedTeamManager::sInstance->GetPlayerTeam(playerId), r, g, b);
            for (int i = 0; i < 2; i++) {
                mat->tevColours[i].r = r;
                mat->tevColours[i].g = g;
                mat->tevColours[i].b = b;
                mat->tevColours[i].a = 255;
            }
        }
    }
}

kmCall(0x8085bfc8, GPVSLeaderboardUpdate_hookSetupEntries);

void CtrlRaceResult_InitPatchAnimation(AnimationGroup* _this, u32 id, float frame) {
    if (ExtendedTeamManager::IsActivated()) {
        _this->isActive = false;
    } else {
        _this->PlayAnimationAtFrame(id, frame);
    }
}

void CtrlRaceResult_CalcSkipAnimation(AnimationGroup* _this, u32 id, float frame) {
    if (!ExtendedTeamManager::IsActivated()) {
        _this->PlayAnimationAtFrame(id, frame);
    }
}

kmCall(0x807f6318, CtrlRaceResult_CalcSkipAnimation);
kmCall(0x807f6410, CtrlRaceResult_CalcSkipAnimation);
kmCall(0x807f63a4, CtrlRaceResult_CalcSkipAnimation);
kmCall(0x807f4f74, CtrlRaceResult_InitPatchAnimation);
kmCall(0x807f4f8c, CtrlRaceResult_InitPatchAnimation);

void WifiAwardResultItem_fillPlayerResult(Pages::WifiAwardResultItem* _this, u8 playerIdx, bool isTeamVS, int localPlayerCount) {
    _this->FillResult(playerIdx, isTeamVS, localPlayerCount);

    if (ExtendedTeamManager::IsActivated()) {
        nw4r::lyt::Pane *pane, *pane2;
        nw4r::lyt::Material* mat;

        if (Racedata::sInstance->racesScenario.players[playerIdx].playerType == PLAYER_REAL_LOCAL) {
            pane = _this->layout.GetPaneByName("p_color_r");
            pane2 = _this->layout.GetPaneByName("p_color_null");
            mat = pane->GetMaterial();

            _this->SetPaneVisibility("team_color_c", false);
            _this->SetPaneVisibility("p_color_null", true);
        } else {
            pane = _this->layout.GetPaneByName("team_color_c");
            mat = pane->GetMaterial();

            _this->SetPaneVisibility("p_color_r", false);
            _this->SetPaneVisibility("p_color_null", false);
        }

        u8 r, g, b;
        ExtendedTeamSelect::GetTeamColor(ExtendedTeamManager::sInstance->GetPlayerTeam(playerIdx), r, g, b);
        for (int i = 0; i < 2; i++) {
            mat->tevColours[i].r = r;
            mat->tevColours[i].g = g;
            mat->tevColours[i].b = b;
            mat->tevColours[i].a = 255;
        }

        pane->alpha = 255;
        pane->flag |= 1;

        if (pane2) {
            pane2->alpha = 255;
            pane2->flag |= 1;
        }
    }
}

kmCall(0x806466f4, WifiAwardResultItem_fillPlayerResult);
kmCall(0x80646728, WifiAwardResultItem_fillPlayerResult);

void WiFiVSResults_InitPatchAnimation(AnimationGroup* _this, u32 id, float frame) {
    if (ExtendedTeamManager::IsActivated()) {
        _this->isActive = false;
    } else {
        _this->PlayAnimationAtFrame(id, frame);
    }
}

void WiFiVSResults_CalcSkipAnimation(AnimationGroup* _this, u32 id, float frame) {
    if (!ExtendedTeamManager::IsActivated()) {
        _this->PlayAnimationAtFrame(id, frame);
    }
}

struct TeamScore {
    ExtendedTeamID team;
    int score;
    bool present;

    TeamScore() : team(TEAM_COUNT), score(0), present(false) {}
    TeamScore(ExtendedTeamID team) : team(team), score(0), present(false) {}
};

static int sort_by_score(const void* a, const void* b) {
    return ((TeamScore*)b)->score - ((TeamScore*)a)->score;
}

void WiFiVSResults_setCongratulationText(Pages::WiFiVSResults* _this) {
    _this->SetCongratulationUIAndSound();

    if (ExtendedTeamManager::IsActivated()) {
        RacedataScenario& scenario = Racedata::sInstance->menusScenario;

        int teamCount = 0;
        TeamScore scores[TEAM_COUNT];
        for (int i = 0; i < TEAM_COUNT; i++) {
            scores[i].team = (ExtendedTeamID)i;
        }

        for (int i = 0; i < scenario.playerCount; i++) {
            ExtendedTeamID team = ExtendedTeamManager::sInstance->GetPlayerTeam(i);
            if (!scores[team].present) {
                teamCount++;
                scores[team].present = true;
            }

            scores[team].score += scenario.players[i].score;
        }

        qsort(scores, TEAM_COUNT, sizeof(TeamScore), sort_by_score);

        Text::Info info;
        info.bmgToPass[0] = BMG_EXTENDEDTEAMS_TEAM_NAME + scores[0].team;

        _this->congratulations.SetMessage(BMG_EXTENDEDTEAMS_WINNER + (scores[0].score == scores[1].score), &info);
    }
}

kmCall(0x80646128, WiFiVSResults_setCongratulationText);

kmCall(0x80645fe0, WiFiVSResults_InitPatchAnimation);
kmCall(0x80645ff8, WiFiVSResults_InitPatchAnimation);
kmCall(0x80646010, WiFiVSResults_InitPatchAnimation);
kmCall(0x80645a8c, WiFiVSResults_CalcSkipAnimation);
kmCall(0x80645a74, WiFiVSResults_CalcSkipAnimation);
kmCall(0x80645a58, WiFiVSResults_CalcSkipAnimation);
kmCall(0x80645a38, WiFiVSResults_CalcSkipAnimation);
kmCall(0x80645b0c, WiFiVSResults_CalcSkipAnimation);
kmCall(0x80645b28, WiFiVSResults_CalcSkipAnimation);
kmCall(0x80645b74, WiFiVSResults_CalcSkipAnimation);
kmCall(0x80645bac, WiFiVSResults_CalcSkipAnimation);

// static u8 s_lastExtendedTeamPlayerId = 0;

// u32 patch_GetHudIdFromPlayerId(Racedata* _this, u8 hudSlotId) {
//     s_lastExtendedTeamPlayerId = _this->GetPlayerIdOfLocalPlayer(hudSlotId);
//     return _this->GetPlayerIdOfLocalPlayer(hudSlotId);
// }

// void patch_HUDColor_SetVtxColor(nw4r::lyt::Pane* _this, u32 idx, nw4r::ut::Color color) {

//     if (ExtendedTeamManager::IsActivated()) {
//         u8 r, g, b;
//         ExtendedTeamSelect::GetTeamColor(ExtendedTeamManager::sInstance->GetPlayerTeam(s_lastExtendedTeamPlayerId), r, g, b);

//         color.r = r;
//         color.g = g;
//         color.b = b;

//         if (idx > 1) {
//             u32 uVar11 = r + 40;
//             u32 uVar6 = 0xff;
//             if (uVar11 < 0xff) {
//                 uVar6 = uVar11;
//             }

//             u32 uVar7 = g + 130;
//             uVar11 = 0xff;
//             if (uVar7 < 0xff) {
//                 uVar11 = uVar7;
//             }

//             u32 uVar8 = 0xff;
//             uVar7 = b + 130;

//             if (uVar7 < 0xff) {
//                 uVar8 = uVar7;
//             }

//             color.rgba = (uVar6 << 24) | (uVar11 << 16) | (uVar8 << 8) | color.a;
//         }
//     }

//     return _this->SetVtxColor(idx, color);
// }

// kmCall(0x807ec1dc, patch_HUDColor_SetVtxColor);

// u8 patch_CtrlRaceRankNum_SetVtxColor(CtrlRaceRankNum* _this) {
//     _this->HudSlotColorEnable("position", true);
//     return _this->GetPlayerId();
// }

// kmCall(0x807f4974, patch_CtrlRaceRankNum_SetVtxColor);

}  // namespace UI
}  // namespace Pulsar