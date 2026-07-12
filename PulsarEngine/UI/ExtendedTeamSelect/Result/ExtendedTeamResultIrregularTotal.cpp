#include <UI/ExtendedTeamSelect/Result/ExtendedTeamResultIrregularTotal.hpp>
#include <Network/Mogi.hpp>

namespace Pulsar {
namespace UI {

PageId ExtendedTeamResultIrregularTotal::GetNextPage() const {
    SectionId currentId = SectionMgr::sInstance->curSection->sectionId;
    if ((currentId != SECTION_P1_WIFI_FRIEND_VS) && (currentId != SECTION_P1_WIFI_FRIEND_BALLOON) && (currentId != SECTION_P1_WIFI_FRIEND_COIN) && (currentId != SECTION_P1_WIFI_FRIEND_TEAMVS)) {
        if (currentId != SECTION_GP) {
            return PAGE_VS_RACEENDMENU;
        }
        return PAGE_GP_ENDMENU;
    }

    return PAGE_WIFI_VS_RESULTS;
}

void ExtendedTeamResultIrregularTotal::OnInit() {
    Pages::GPVSLeaderboardTotal::OnInit();
    Mogi::OnFinalResults();

    this->AddControl(this->GetRowCount(), this->textMessage, 0);

    ControlLoader loader(&this->textMessage);
    loader.Load("game_image", "ExpandedTeamMessage", "ExpandedTeamMessage", nullptr);

    this->textMessage.SetMessage(BMG_EXTENDEDTEAMS_IRREGULAR_WARNING, nullptr);
    this->textMessage.isHidden = false;
}

void patch_InitControlGroup(Page* _this, int count) {
    if (_this->pageId == PULPAGE_EXTENDEDTEAMS_RESULT_TOTAL_IRREGULAR) {
        return _this->InitControlGroup(count + 1);
    }

    return _this->InitControlGroup(count);
}

kmCall(0x8085ca20, patch_InitControlGroup);

struct TeamScore {
    ExtendedTeamID team;
    int score;
    bool present;

    TeamScore() : team(TEAM_COUNT), score(0), present(false) {}
    TeamScore(ExtendedTeamID team) : team(team), score(0), present(false) {}
};

int sort_by_score(const void* a, const void* b) {
    return ((TeamScore*)b)->score - ((TeamScore*)a)->score;
}

void ExtendedTeamResultIrregularTotal::FillRows() {
    RacedataScenario& menuScenario = Racedata::sInstance->menusScenario;

    int teamCount = 0;
    TeamScore scores[TEAM_COUNT];
    for (int i = 0; i < TEAM_COUNT; i++) {
        scores[i].team = (ExtendedTeamID)i;
    }

    for (int i = 0; i < menuScenario.playerCount; i++) {
        ExtendedTeamID team = static_cast<ExtendedTeamID>(menuScenario.players[i].team);
        if (team >= TEAM_COUNT) continue;
        if (!scores[team].present) {
            teamCount++;
            scores[team].present = true;
        }

        scores[team].score += menuScenario.players[i].score;
    }

    qsort(scores, TEAM_COUNT, sizeof(TeamScore), sort_by_score);

    for (int i = teamCount; i < this->GetRowCount(); i++) {
        this->results[i]->isHidden = true;
    }

    ExtendedTeamID selfTeams[2] = {TEAM_COUNT, TEAM_COUNT};
    for (int i = 0; i < menuScenario.playerCount; i++) {
        if (menuScenario.players[i].playerType == PLAYER_REAL_LOCAL && menuScenario.players[i].hudSlotId == 0) {
            selfTeams[0] = static_cast<ExtendedTeamID>(menuScenario.players[i].team);
        } else if (menuScenario.players[i].playerType == PLAYER_REAL_LOCAL && menuScenario.players[i].hudSlotId == 1) {
            selfTeams[1] = static_cast<ExtendedTeamID>(menuScenario.players[i].team);
        }
    }

    for (int i = 0; i < teamCount; i++) {
        this->results[i]->isHidden = false;

        nw4r::lyt::Material* mat;
        nw4r::lyt::Pane* pane;
        if (scores[i].team == selfTeams[0] || scores[i].team == selfTeams[1]) {
            this->results[i]->Fill(i + 1, 0);
            pane = this->results[i]->layout.GetPaneByName("select_base");
            mat = pane->GetMaterial();
            pane->alpha = 255;
        } else {
            this->results[i]->Fill(i + 1, 1);
            pane = this->results[i]->layout.GetPaneByName("team_color_c");
            mat = pane->GetMaterial();
            pane->alpha = 200;
        }

        this->results[i]->SetPaneVisibility("chara_icon", false);
        this->results[i]->SetPaneVisibility("chara_icon_sha", false);

        this->results[i]->SetTextBoxMessage("player_name", BMG_EXTENDEDTEAMS_TEAM_NAME + scores[i].team, nullptr);

        pane->flag |= 1;

        u8 r, g, b;
        ExtendedTeamSelect::GetTeamColor(scores[i].team, r, g, b);
        for (int i = 0; i < 2; i++) {
            mat->tevColours[i].r = r;
            mat->tevColours[i].g = g;
            mat->tevColours[i].b = b;
            mat->tevColours[i].a = 255;
        }

        this->results[i]->FillScore(scores[i].score, BMG_SCORE_PTS);
        this->results[i]->ResetTextBoxMessage("get_point");
    }
}

}  // namespace UI
}  // namespace Pulsar
