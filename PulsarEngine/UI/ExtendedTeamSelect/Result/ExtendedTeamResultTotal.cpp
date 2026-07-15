#include <UI/ExtendedTeamSelect/Result/ExtendedTeamResultTotal.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>

namespace Pulsar {
namespace UI {

PageId ExtendedTeamResultTotal::GetNextPage() const {
    SectionId currentId = SectionMgr::sInstance->curSection->sectionId;
    if ((currentId != SECTION_P1_WIFI_FRIEND_VS) && (currentId != SECTION_P1_WIFI_FRIEND_BALLOON) && (currentId != SECTION_P1_WIFI_FRIEND_COIN) && (currentId != SECTION_P1_WIFI_FRIEND_TEAMVS)) {
        if (currentId != SECTION_GP) {
            return PAGE_VS_RACEENDMENU;
        }
        return PAGE_GP_ENDMENU;
    }

    return PAGE_WIFI_VS_RESULTS;
}

void ExtendedTeamResultTotal::OnInit() {
    RacedataScenario& scenario = Racedata::sInstance->racesScenario;

    int teamCount = 0;
    bool teamPresent[TEAM_COUNT];
    for (int i = 0; i < TEAM_COUNT; i++) {
        teamPresent[i] = false;
    }

    for (int i = 0; i < scenario.playerCount; i++) {
        ExtendedTeamID team = ExtendedTeamManager::sInstance->GetPlayerTeam(i);
        if (!teamPresent[team]) {
            teamCount++;
            teamPresent[team] = true;
        }
    }

    int controlLoadedCount = 0;
    this->InitControlGroup(teamCount);
    for (int i = 0; i < TEAM_COUNT; i++) {
        if (!teamPresent[i])
            continue;

        this->AddControl(controlLoadedCount, this->results[controlLoadedCount], 0);
        results[controlLoadedCount].Load((ExtendedTeamID)i, teamCount, controlLoadedCount);

        controlLoadedCount++;
    }
}

bool ExtendedTeamResultTotal::CanEnd() {
    for (int i = 0; i < TEAM_COUNT; i++) {
        if (this->results[i].IsResultAnimDone()) {
            return true;
        }
    }

    return false;
}

void ExtendedTeamResultTotal::FillRows() {
}

}  // namespace UI
}  // namespace Pulsar