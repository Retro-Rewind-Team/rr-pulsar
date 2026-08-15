#ifndef __EXTENDEDTEAMS_RESULT_TOTAL_HPP
#define __EXTENDEDTEAMS_RESULT_TOTAL_HPP

#include <kamek.hpp>
#include <UI/UI.hpp>
#include <MarioKartWii/UI/Ctrl/CtrlRace/CtrlRaceResultTeam.hpp>
#include <MarioKartWii/UI/Ctrl/CtrlRace/CtrlRaceWifi.hpp>
#include <MarioKartWii/UI/Page/Leaderboard/GPVSLeaderboardTotal.hpp>
#include <MarioKartWii/UI/Page/Leaderboard/TeamLeaderboard.hpp>
#include <UI/ExtendedTeamSelect/ExtendedTeamSelect.hpp>
#include <UI/ExtendedTeamSelect/Result/CtrlRaceResultExtendedTeams.hpp>

namespace Pulsar {
namespace UI {

class ExtendedTeamResultTotal : public Pages::Leaderboard {
public:
    static const PulPageId id = PULPAGE_EXTENDEDTEAMS_RESULT_TOTAL;

    PageId GetNextPage() const override;
    void OnInit() override;
    bool CanEnd() override;
    void FillRows() override;

    CtrlRaceResultExtendedTeams results[TEAM_COUNT];
};

}  // namespace UI
}  // namespace Pulsar

#endif