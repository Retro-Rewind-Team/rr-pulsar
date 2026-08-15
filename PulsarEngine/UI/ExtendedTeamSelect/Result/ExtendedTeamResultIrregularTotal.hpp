#ifndef __EXTENDEDTEAMS_IRREG_TOTAL_HPP_
#define __EXTENDEDTEAMS_IRREG_TOTAL_HPP_

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

class ExtendedTeamResultIrregularTotal : public Pages::GPVSLeaderboardTotal {
public:
    static const PulPageId id = PULPAGE_EXTENDEDTEAMS_RESULT_TOTAL_IRREGULAR;

    PageId GetNextPage() const override;
    void OnInit() override;
    void FillRows() override;

    LayoutUIControl textMessage;
};

}  // namespace UI
}  // namespace Pulsar

#endif