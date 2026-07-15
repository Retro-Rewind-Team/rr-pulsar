#include <UI/Leaderboard/ExpGPVSLeaderboardTotal.hpp>
#include <UI/Leaderboard/LeaderboardDisplay.hpp>
#include <Settings/Settings.hpp>
#include <PulsarSystem.hpp>

namespace Pulsar {
namespace UI {

static void fillTotalLeaderboardResults(Pages::GPVSLeaderboardTotal& page) {
    const int count = page.GetRowCount() & 0xff;
    for (int i = 0; i < count; ++i) {
        fillLeaderboardResult(*page.results[i], page.sortedArray[i].playerId);
    }
}

void ExpGPVSLeaderboardTotal::OnUpdate() {
    if (checkLeaderboardDisplaySwapInputs()) {
        nextLeaderboardDisplayType();
        fillTotalLeaderboardResults(*this);
    }
}

void ExpGPVSLeaderboardTotal::BeforeEntranceAnimations() {
    fillTotalLeaderboardResults(*this);
}

}  // namespace UI
}  // namespace Pulsar
