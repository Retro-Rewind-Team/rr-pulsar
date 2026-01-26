#include <UI/Leaderboard/ExpWWLeaderboardUpdate.hpp>
#include <UI/Leaderboard/LeaderboardDisplay.hpp>

namespace Pulsar {
namespace UI {

void ExpWWLeaderboardUpdate::OnUpdate() {
    if (checkLeaderboardDisplaySwapInputs()) {
        nextLeaderboardDisplayType();
        fillLeaderboardResults(GetRowCount(), this->results);
    }
}

}  // namespace UI
}  // namespace Pulsar