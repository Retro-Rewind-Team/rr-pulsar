#include <UI/ExtendedTeamSelect/Result/CtrlRaceResultExtendedTeams.hpp>
#include <CustomCharacters/CustomCharacters.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <MarioKartWii/GlobalFunctions.hpp>

namespace Pulsar {
namespace UI {

void CtrlRaceResultExtendedTeams::InitSelf() {
    if (!Raceinfo::sInstance->IsAtLeastStage(RACESTAGE_FINISHED)) {
        return;
    }

    this->textInfo = Text::Info();

    this->teamScore = 0;
    this->currentScore = this->teamScore;

    int playerCount = 0;

    RacedataScenario& scenario = Racedata::sInstance->menusScenario;
    for (int i = 0; i < scenario.playerCount; i++) {
        if (playerCount >= 6)
            break;

        if (ExtendedTeamManager::sInstance->GetPlayerTeam(i) == this->teamId) {
            this->players[playerCount].playerIdx = i;
            PageId pageId = this->parentGroup->parentPage->pageId;
            if (pageId == (PageId)PULPAGE_EXTENDEDTEAMS_RESULT_TOTAL) {
                this->players[playerCount].battleScore = scenario.players[i].score;
                this->players[playerCount].prevBattleScore = scenario.players[i].previousScore;
                this->players[playerCount].prevBattleScoreFloat = scenario.players[i].previousScore;

                this->currentScore += scenario.players[i].previousScore;
                this->teamScore += scenario.players[i].score;
            }

            playerCount++;
        }
    }

    qsort(this->players, playerCount, sizeof(CtrlRaceResultTeam::Player), (int (*)(const void*, const void*))CtrlRaceResultTeam::ComparePlayers);
    for (int i = playerCount; i < 6; i++) {
        this->players[i].playerIdx = -1;
        this->players[i].battleScore = 0;
        this->players[i].prevBattleScore = 0;
    }

    int rank = 0;
    for (int i = 0; i < 6; i++) {
        if (this->players[i].playerIdx == -1) {
            this->items[i].isHidden = true;
            continue;
        } else {
            this->items[i].isHidden = false;
        }

        const u8 playerId = this->players[i].playerIdx;

        u8 localPlayerCount = scenario.localPlayerCount;
        PlayerType playerType = scenario.players[playerId].playerType;
        CharacterId characterId = scenario.players[playerId].characterId;
        MiiGroup& miiGroup = SectionMgr::sInstance->sectionParams->playerMiis;

        if (playerType == PLAYER_REAL_LOCAL) {
            this->items[i].animator.GetAnimationGroupById(0).PlayAnimationAtFrame(0, 0.0f);
            this->items[i].animator.GetAnimationGroupById(1).PlayAnimationAtFrame(0, 0.0f);
            this->items[i].animator.GetAnimationGroupById(2).PlayAnimationAtFrame(0, 0.0f);
        }

        Text::Info info;
        bool isOnline = false;
        // Not a Mii character
        if (characterId <= ROSALINA) {
            if (scenario.settings.gamemode >= MODE_PRIVATE_VS && scenario.settings.gamemode <= MODE_PRIVATE_BATTLE) {
                isOnline = true;
            }

            if ((isOnline || (localPlayerCount > 1)) && (playerType != PLAYER_CPU)) {
                info.miis[0] = miiGroup.GetMii(playerId);
                this->items[i].SetTextBoxMessage("mii_name", BMG_MII_NAME, &info);
            } else {
                if (!CustomCharacters::SetRaceNameTextIfCustom(this->items[i], "mii_name", playerId)) {
                    u32 characterBmg = GetCharacterBMGId(characterId, true);
                    this->items[i].SetTextBoxMessage("mii_name", characterBmg, nullptr);
                }
            }
        } else {
            info.miis[0] = miiGroup.GetMii(playerId);
            this->items[i].SetTextBoxMessage("mii_name", BMG_MII_NAME, &info);
        }

        // Is a Mii character
        if (isOnline || characterId >= MII_S_A_MALE) {
            this->items[i].SetMiiPane("chara_icon", miiGroup, playerId, 2);
            this->items[i].SetMiiPane("chara_icon_sha", miiGroup, playerId, 2);
        } else {
            this->items[i].SetPicturePane("chara_icon", GetCharacterIconPaneName(characterId));
            this->items[i].SetPicturePane("chara_icon_sha", GetCharacterIconPaneName(characterId));
        }

        int prevScore = this->players[i].prevBattleScore;
        this->textInfo.intToPass[0] = prevScore;

        this->items[i].SetTextBoxMessage("pts", BMG_SCORE_PTS, &this->textInfo);
        this->items[i].SetTextBoxMessage("point", BMG_SCORE_POINTS, &this->textInfo);

        rank++;
    }

    if (this->resultTeamPoint) {
        this->textInfo = Text::Info();
        this->resultTeamPoint->ResetMsg();

        this->textInfo.intToPass[0] = this->currentScore;
        u32 teamNameBmgID = BMG_NUMBER_RACE;
        this->resultTeamPoint->SetTextBoxMessage("point", teamNameBmgID, &this->textInfo);
    }
}

void CtrlRaceResultExtendedTeams::OnUpdate() {
    if (this->parentGroup->parentPage->currentState != STATE_ACTIVE) {
        return;
    }

    for (int i = 0; i < 6; i++) {
        if (this->players[i].playerIdx != -1) {
            u32 score = this->players[i].battleScore;
            if (score - this->players[i].prevBattleScore > 0.0) {
                this->players[i].prevBattleScoreFloat += 0.5f;
                this->players[i].prevBattleScore = this->players[i].prevBattleScoreFloat;
                this->textInfo.intToPass[0] = this->players[i].prevBattleScore;
                this->items[i].SetTextBoxMessage("pts", BMG_SCORE_PTS, &this->textInfo);
                this->items[i].SetTextBoxMessage("point", BMG_SCORE_POINTS, &this->textInfo);
                break;
            }

            this->textInfo.intToPass[0] = score;
            this->items[i].SetTextBoxMessage("pts", BMG_SCORE_PTS, &this->textInfo);
        }
    }

    if (this->resultTeamPoint) {
        this->textInfo = Text::Info();

        if (this->teamScore - this->currentScore > 0.0) {
            this->currentScore += 1.0;
            this->textInfo.intToPass[0] = this->currentScore;

            this->PlaySound(0xde, 0xffffffff);

            u32 teamNameBmgID = BMG_NUMBER_RACE;
            this->resultTeamPoint->SetTextBoxMessage("point", teamNameBmgID, &this->textInfo);
        }
    }
}

const char* CtrlRaceResultExtendedTeams::GetClassName() const {
    return "CtrlRaceResultExpandedTeams";
}

void CtrlRaceResultExtendedTeams::Load(ExtendedTeamID teamID, int numTeams, int teamIdx) {
    char variant[20];
    this->teamId = teamID;

    snprintf(variant, 20, "ResultVSTeamNULL%d_%d", numTeams, teamIdx);

    ControlLoader selfLoader(this);
    selfLoader.Load("result", "ExpandedTeamResult", variant, nullptr);

    this->InitControlGroup(6 + 1);

    const char* anims[] = {
        "Loop", "Loop", nullptr,
        "Select", "SelectOn", "SelectOff", nullptr,
        "Select2", "Select2On", "Select2Off", nullptr,
        nullptr};

    for (int i = 0; i < 6; i++) {
        snprintf(variant, 20, "BlueRed%d", i);
        this->AddControl(i, &this->items[i]);

        ControlLoader itemLoader(&this->items[i]);
        itemLoader.Load("result", "ResultVSTeam", variant, anims);

        this->items[i].SetPaneVisibility("blue_null", false);

        nw4r::lyt::Pane* teamColorPane1 = this->items[i].layout.GetPaneByName("black_parts_r_l");
        nw4r::lyt::Pane* teamColorPane2 = this->items[i].layout.GetPaneByName("black_parts_r_r");
        nw4r::lyt::Material* teamColor1 = teamColorPane1->GetMaterial();
        nw4r::lyt::Material* teamColor2 = teamColorPane2->GetMaterial();
        teamColorPane1->alpha = teamColorPane2->alpha = 255;

        u8 r, g, b;
        ExtendedTeamSelect::GetTeamColor(this->teamId, r, g, b);

        for (int j = 0; j < 2; j++) {
            teamColor1->tevColours[j].r = teamColor2->tevColours[j].r = r;
            teamColor1->tevColours[j].g = teamColor2->tevColours[j].g = g;
            teamColor1->tevColours[j].b = teamColor2->tevColours[j].b = b;
            teamColor1->tevColours[j].a = teamColor2->tevColours[j].a = 255;
        }

        this->items[i].animator.GetAnimationGroupById(0).PlayAnimationAtFrame(0, 0.0f);
        this->items[i].animator.GetAnimationGroupById(1).PlayAnimationAtFrame(1, 0.0f);
        this->items[i].animator.GetAnimationGroupById(2).PlayAnimationAtFrame(1, 0.0f);

        this->items[i].SetSoundIds(0xe1, 0xe0);

        this->textInfo.intToPass[0] = 0;
        this->items[i].SetTextBoxMessage("pts", BMG_SCORE_PTS, &this->textInfo);
    }

    this->resultTeamPoint = new LayoutUIControl();
    this->AddControl(6, this->resultTeamPoint);

    const char* teamPointAnims[] = {
        "team", "blue", "red", nullptr,
        nullptr};

    ControlLoader pointLoader(this->resultTeamPoint);
    pointLoader.Load("result", "ResultTeamPoint", "red", teamPointAnims);
    this->resultTeamPoint->animator.GetAnimationGroupById(0).isActive = false;

    nw4r::lyt::TextBox* pane1 = (nw4r::lyt::TextBox*)this->resultTeamPoint->layout.GetPaneByName("point");
    nw4r::lyt::TextBox* pane2 = (nw4r::lyt::TextBox*)this->resultTeamPoint->layout.GetPaneByName("pts");
    nw4r::lyt::Material* mat1 = pane1->GetMaterial();
    nw4r::lyt::Material* mat2 = pane2->GetMaterial();

    u8 r, g, b;
    ExtendedTeamSelect::GetTeamColor(this->teamId, r, g, b);

    mat1->tevColours[1].r = mat2->tevColours[1].r = r;
    mat1->tevColours[1].g = mat2->tevColours[1].g = g;
    mat1->tevColours[1].b = mat2->tevColours[1].b = b;
}

bool CtrlRaceResultExtendedTeams::IsResultAnimDone() const {
    for (int i = 0; i < 6; i++) {
        if ((this->players[i].playerIdx != -1) && (this->players[i].battleScore - this->players[i].prevBattleScore > 0.0)) {
            return true;
        }
    }

    if (this->teamScore - this->currentScore <= 0.0) {
        return false;
    }

    return true;
}

}  // namespace UI
}  // namespace Pulsar
