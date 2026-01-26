#include <UI/Leaderboard/ExpGPVSLeaderboardTotal.hpp>
#include <UI/Leaderboard/LeaderboardDisplay.hpp>
#include <Settings/Settings.hpp>

namespace Pulsar {
namespace UI {

void ExpGPVSLeaderboardTotal::OnUpdate() {
    if (checkLeaderboardDisplaySwapInputs()) {
        nextLeaderboardDisplayType();
        fillLeaderboardResults(GetRowCount(), this->results);
    }
}

}  // namespace UI
}  // namespace Pulsar