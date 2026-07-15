#include <UI/ExtendedTeamSelect/ExtendedTeamSelect.hpp>
#include <UI/CustomItems/CustomItemPage.hpp>
#include <Race/CustomItems.hpp>
#include <MarioKartWii/UI/Page/Other/FriendRoom.hpp>
#include <RetroRewind.hpp>
#include <core/nw4r/lyt/ArcResourceAccessor.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/RKNet/ROOM.hpp>
#include <Network/Mogi.hpp>
#include <MarioKartWii/UI/Page/Leaderboard/GPVSLeaderboardUpdate.hpp>
#include <MarioKartWii/UI/Page/Other/SELECTStageMgr.hpp>
#include <MarioKartWii/UI/Page/Other/VR.hpp>
#include <MarioKartWii/UI/Page/Other/Votes.hpp>
#include <MarioKartWii/UI/Page/Other/WifiVSResults.hpp>
#include <MarioKartWii/UI/Ctrl/CtrlRace/CtrlRace2DMap.hpp>
#include <MarioKartWii/UI/Ctrl/CtrlRace/CtrlRaceBalloon.hpp>
#include <MarioKartWii/UI/Ctrl/CtrlRace/CtrlRaceRankNum.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <Network/Mogi.hpp>

namespace Pulsar {
namespace UI {

static const u32 ALL_CUSTOM_ITEMS = 0x7FFFF;

static void ApplyMogiRaceTeams(RacedataScenario& scenario) {
    if (!Mogi::IsEnabled()) return;

    const bool active = Mogi::IsActive();
    const bool teamFormat = Mogi::IsTeamFormat();
    if (!active || !teamFormat) return;

    const u8 playerCount = scenario.playerCount < 12 ? scenario.playerCount : 12;
    for (u8 i = 0; i < playerCount; ++i) {
        scenario.players[i].team = static_cast<Team>(Mogi::GetTeamForPlayer(i));
    }
}

void Racedata_InitRace(Racedata* racedata) {
    racedata->InitRace();

    ApplyMogiRaceTeams(racedata->racesScenario);

    const RacedataSettings& settings = racedata->menusScenario.settings;
    if (settings.gamemode == MODE_VS_RACE && !(settings.modeFlags & ExtendedTeamManager::TEAM_MODE_FLAG) &&
        ExtendedTeamManager::IsActivated() && !Mogi::IsActive()) {
        ExtendedTeamManager::sInstance->ConfigureOfflineTeams();
    }
}

kmCall(0x80530878, Racedata_InitRace);
kmCall(0x80530ef4, Racedata_InitRace);
kmCall(0x80553c90, Racedata_InitRace);
kmCall(0x80554ab0, Racedata_InitRace);

void PrepareOnlinePages(Pages::FriendRoomWaiting* _this) {
    _this->StartRoom();

    Pages::FriendRoomManager* friendRoomManager = SectionMgr::sInstance->curSection->Get<Pages::FriendRoomManager>();

    if (friendRoomManager) {
        RKNet::Controller* controller = RKNet::Controller::sInstance;
        if (controller) {
            RKNet::StatusData& status = controller->localStatusData;
            const RKNet::ControllerSub& sub = controller->subs[controller->currentSub];
            const bool isHost = controller->roomType == RKNet::ROOMTYPE_FROOM_HOST ||
                                (Mogi::IsActive() && sub.localAid == sub.hostAid);
            const u32 mode = friendRoomManager->startedGameMode;
            const bool isBattle = mode >= 2;

            if (isHost) {
                status.status = isBattle ? RKNet::FRIEND_STATUS_FROOM_BATTLE_HOST : RKNet::FRIEND_STATUS_FROOM_VS_HOST;
            } else {
                status.status = isBattle ? RKNet::FRIEND_STATUS_FROOM_BATTLE_NON_HOST : RKNet::FRIEND_STATUS_FROOM_VS_NON_HOST;
            }
            controller->UpdateStatusDatas();
        }
    }
    PageId nextPageId = PAGE_CHARACTER_SELECT;
    if (ExtendedTeamManager::IsActivated() &&
        (friendRoomManager->startedGameMode == 0 || friendRoomManager->startedGameMode == 2 || friendRoomManager->startedGameMode == 3)) {
        _this->countdown.SetInitial(86400.0f);
        nextPageId = static_cast<PageId>(PULPAGE_EXTENDEDTEAMSELECT);
    }

    if (Race::GetEffectiveCustomItemsBitfield() != ALL_CUSTOM_ITEMS && !CustomItemPage::ShouldSkipFriendRoomPreview()) {
        CustomItemPage* page = ExpSection::GetSection()->GetPulPage<CustomItemPage>();
        page->StartFriendRoomPreview(nextPageId);
        _this->AddPageLayer(static_cast<PageId>(CustomItemPage::id), 0);
        return;
    }

    _this->AddPageLayer(nextPageId, 0);
}
kmWriteNop(0x805de76c);  // Remove the call that opens the character selection screen
kmCall(0x805dddb0, PrepareOnlinePages);

// This function is called when the host presses one of the start button in the Friend Room
// If Extended Teams is enabled, it will send a "Regular VS" message instead of a "Team VS" message
// Because I only patched the regular VS UIs for this mode
//
// Keep pkt as a raw word here; this hook works on the packed ROOM message value.
void SetBroadcastROOMPacket(RKNet::ROOMHandler* _this, u32 pkt) {
    u8 messageType = (pkt >> 24) & 0xFF;
    u16 message = (pkt >> 8) & 0xFFFF;
    u8 messageSequence = pkt & 0xFF;

    RKNet::Controller* controller = RKNet::Controller::sInstance;
    RKNet::ControllerSub* sub = &controller->subs[controller->currentSub];

    UI::ExtendedTeamSelect* extendedTeamSelect = SectionMgr::sInstance->curSection->Get<UI::ExtendedTeamSelect>();
    Pages::FriendRoomManager* friendRoomBackPage = SectionMgr::sInstance->curSection->Get<Pages::FriendRoomManager>();
    if (ExtendedTeamManager::IsActivated() && messageType == 1 && message == 1) {
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

kmCall(0x805dce34, SetBroadcastROOMPacket);

void RecvRoomPacket(UnkFriendRoomManager* _this, u8 playerId, u8 myAid, RKNet::ROOMPacket& packet) {
    if (packet.messageType == ExtendedTeamManager::MSG_TYPE_START_RACE) {
        UI::ExtendedTeamManager::sInstance->SetStatusExternal(ExtendedTeamManager::STATUS_DONE);
    }

    _this->HandleROOMPacket(playerId, myAid, packet);
}
kmCall(0x805db1dc, RecvRoomPacket);

void VotingVRPage_SetControlState(Pages::VR* _this, u32 idx, u32 playerId, Team team, u32 type, bool isLocalPlayer) {
    _this->FillVRControl(idx, playerId, team, type, isLocalPlayer);
    const bool isMogiTeamRoom = Mogi::IsActive() && Mogi::IsTeamFormat();
    if (isMogiTeamRoom || ExtendedTeamManager::IsActivated()) {
        Pages::SELECTStageMgr* selectStageMgr = SectionMgr::sInstance->curSection->Get<Pages::SELECTStageMgr>();
        u8 aid = selectStageMgr->infos[idx].aid;
        u8 playerIdOnConsole = selectStageMgr->infos[idx].hudSlotid;
        const ExtendedTeamID playerTeam = isMogiTeamRoom
                                              ? static_cast<ExtendedTeamID>(Mogi::GetTeamForPlayer(idx))
                                              : ExtendedTeamManager::sInstance->GetPlayerTeamByAID(aid, playerIdOnConsole);

        ExtendedTeamSelect::ChangeVRButtonColors(_this->vrControls[idx], playerTeam);
    }
}

kmCall(0x8064aa78, VotingVRPage_SetControlState);
kmCall(0x8064a9f0, VotingVRPage_SetControlState);

bool PageVote_FillVoteControl(Pages::Vote* _this, u32 playerId) {
    bool res = _this->FillVoteControl(playerId);
    const bool isMogiTeamRoom = Mogi::IsActive() && Mogi::IsTeamFormat();
    if ((isMogiTeamRoom || ExtendedTeamManager::IsActivated()) && res) {
        Pages::SELECTStageMgr* selectStageMgr = SectionMgr::sInstance->curSection->Get<Pages::SELECTStageMgr>();
        u8 aid = selectStageMgr->infos[playerId].aid;
        u8 playerIdOnConsole = selectStageMgr->infos[playerId].hudSlotid;
        const ExtendedTeamID team = isMogiTeamRoom
                                        ? static_cast<ExtendedTeamID>(Mogi::GetTeamForPlayer(playerId))
                                        : ExtendedTeamManager::sInstance->GetPlayerTeamByAID(aid, playerIdOnConsole);

        VoteControl& control = _this->votes[_this->lastHandledVote - 1];
        control.animator.GetAnimationGroupById(3).PlayAnimationAtFrame(2, 0.0f);
        ExtendedTeamSelect::ChangeVRButtonColors(control, team);
    }

    return res;
}

kmCall(0x80643b3c, PageVote_FillVoteControl);

void SELECTStageMgr_PrepareRace(Pages::SELECTStageMgr* _this) {
    if (Mogi::IsActive() && Mogi::IsTeamFormat()) {
        ExtendedTeamManager::sInstance->ConfigureMogiTeams();
        for (u8 i = 0; i < _this->playerCount; ++i) {
            const ExtendedTeamID team =
                ExtendedTeamManager::sInstance->GetPlayerTeamByAID(_this->infos[i].aid, _this->infos[i].hudSlotid);
            if (team != TEAM_COUNT) _this->infos[i].team = static_cast<Team>(team);
        }
    }
    _this->PrepareRace();
    ApplyMogiRaceTeams(Racedata::sInstance->menusScenario);
    if (ExtendedTeamManager::IsActivated()) {
        if (Mogi::IsActive()) {
            ExtendedTeamManager::sInstance->ConfigureMogiTeams();
        } else {
            ExtendedTeamManager::sInstance->VotePageSync();
        }
    }
}

kmCall(0x80643ce8, SELECTStageMgr_PrepareRace);

void CtrlRace2DMapCharacter_CalcTransform(CtrlRace2DMapCharacter* _this, const Vec3& kartPosition, Vec2& dest, u32 r6) {
    _this->CalculatePosition(kartPosition, dest, r6);
    RacedataScenario& menuScenario = Racedata::sInstance->menusScenario;
    RKNet::Controller* controller = RKNet::Controller::sInstance;
    if (ExtendedTeamManager::IsActivated()) {
        ExtendedTeamID selfTeams[2] = {TEAM_COUNT, TEAM_COUNT};
        for (int i = 0; i < menuScenario.playerCount; i++) {
            if (menuScenario.players[i].playerType == PLAYER_REAL_LOCAL && menuScenario.players[i].hudSlotId == 0) {
                selfTeams[0] = ExtendedTeamManager::sInstance->GetPlayerTeam(i);
                if (controller && (controller->roomType == RKNet::ROOMTYPE_FROOM_HOST ||
                                   controller->roomType == RKNet::ROOMTYPE_FROOM_NONHOST ||
                                   (Mogi::IsActive() && Mogi::IsTeamFormat()))) {
                    RKNet::ControllerSub& currentSub = controller->subs[controller->currentSub];
                    selfTeams[0] = ExtendedTeamManager::sInstance->GetPlayerTeamByAID(currentSub.localAid, 0);
                }
            } else if (menuScenario.players[i].playerType == PLAYER_REAL_LOCAL && menuScenario.players[i].hudSlotId == 1) {
                selfTeams[1] = ExtendedTeamManager::sInstance->GetPlayerTeam(i);
                if (controller && (controller->roomType == RKNet::ROOMTYPE_FROOM_HOST ||
                                   controller->roomType == RKNet::ROOMTYPE_FROOM_NONHOST ||
                                   (Mogi::IsActive() && Mogi::IsTeamFormat()))) {
                    RKNet::ControllerSub& currentSub = controller->subs[controller->currentSub];
                    selfTeams[1] = ExtendedTeamManager::sInstance->GetPlayerTeamByAID(currentSub.localAid, 1);
                }
            }
        }

        const ExtendedTeamID playerTeam = ExtendedTeamManager::sInstance->GetPlayerTeam(_this->playerId);
        if (playerTeam == selfTeams[0] || playerTeam == selfTeams[1]) {
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
        u8 r, g, b;
        ExtendedTeamSelect::GetTeamColor(ExtendedTeamManager::sInstance->GetPlayerTeam(playerId), r, g, b);
        nw4r::lyt::TextBox* characterName = (nw4r::lyt::TextBox*)_this->layout.GetPaneByName("chara_name");
        nw4r::lyt::Material* mat = characterName->GetMaterial();

        characterName->color1[0] = nw4r::ut::Color(r, g, b, 255);

        mat->tevColours[1].r = 255;
        mat->tevColours[1].g = 255;
        mat->tevColours[1].b = 255;

        const bool isMogiTeamRoom = Mogi::IsActive() && Mogi::IsTeamFormat();
        const bool isTeamLine = static_cast<Pulsar::ExtendedTeamsLine>(Pulsar::Settings::Mgr::Get().GetUserSettingValue(
                                   static_cast<Pulsar::Settings::UserType>(Pulsar::Settings::SETTINGSTYPE_EXTENDEDTEAMS), Pulsar::RADIO_EXTENDEDTEAMSLINE)) ==
                                 Pulsar::EXTENDEDTEAMS_LINE_TEAM;
        if (isMogiTeamRoom || isTeamLine) {
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
        for (int team = 0; team < TEAM_COUNT; ++team) {
            const u16 missingScore = Mogi::GetMissingTeamScore(team, false);
            if (missingScore == 0) continue;
            if (!scores[team].present) {
                scores[team].present = true;
                ++teamCount;
            }
            scores[team].score += missingScore;
        }

        qsort(scores, TEAM_COUNT, sizeof(TeamScore), sort_by_score);

        u8 teamColorOrder[TEAM_COUNT];
        ExtendedTeamSelect::GetTeamColorOrder(teamColorOrder);

        Text::Info info;
        info.bmgToPass[0] = BMG_EXTENDEDTEAMS_TEAM_NAME + teamColorOrder[scores[0].team];

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

}  // namespace UI
}  // namespace Pulsar
