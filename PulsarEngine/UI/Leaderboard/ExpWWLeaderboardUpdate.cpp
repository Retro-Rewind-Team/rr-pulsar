#include <UI/Leaderboard/ExpWWLeaderboardUpdate.hpp>
#include <UI/Leaderboard/LeaderboardDisplay.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>

namespace Pulsar {
namespace UI {

void ExpWWLeaderboardUpdate::OnUpdate() {
    if (checkLeaderboardDisplaySwapInputs()) {
        nextLeaderboardDisplayType();
        fillLeaderboardResults(GetRowCount(), this->results);
    }
}

PageId ExpWWLeaderboardUpdate::GetNextPage() const {
    SectionId sectionId = SectionMgr::sInstance->curSection->sectionId;
    if (sectionId >= SECTION_P1_WIFI_FRIEND_VS && sectionId <= SECTION_P2_WIFI_FRIEND_COIN) {
        return PAGE_GPVS_TOTAL_LEADERBOARDS;
    }
    return Pages::WWLeaderboardUpdate::GetNextPage();
}

}  // namespace UI
}  // namespace Pulsar