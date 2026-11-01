#include <PulsarSystem.hpp>
#include <runtimeWrite.hpp>
#include <MarioKartWii/UI/Page/Leaderboard/TeamLeaderboard.hpp>

namespace Pulsar {
namespace BattleFFA {

void UpdatePage(Page *self) {
    if (self->pageId == Pages::BattleLeaderboardUpdate::id) {
        Pages::BattleLeaderboardUpdate *page = static_cast<Pages::BattleLeaderboardUpdate *>(self);
        if (page->currentState == STATE_ACTIVE && page->duration >= static_cast<u32>(1))
            page->EndStateAnimated(0, 0.0f);
    }
}
kmBranch(0x805BB22C, UpdatePage);

}  // namespace BattleFFA
}  // namespace Pulsar