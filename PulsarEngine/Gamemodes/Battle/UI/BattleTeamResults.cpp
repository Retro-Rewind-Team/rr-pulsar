#include <PulsarSystem.hpp>
#include <runtimeWrite.hpp>
#include <MarioKartWii/UI/Page/Leaderboard/TeamLeaderboard.hpp>

namespace Pulsar {
namespace BattleFFA {

void BattleLeaderboardPageSkip(Pages::BattleLeaderboardUpdate* self) {
    if (self->currentState == STATE_ACTIVE) {
        if (self->duration >= static_cast<u32>(1)) {
            self->EndStateAnimated(0, 0.0f);
        }
    }
}

void UpdatePage(Page* self) {
    if (self->pageId == Pages::BattleLeaderboardUpdate::id) {
        BattleLeaderboardPageSkip(static_cast<Pages::BattleLeaderboardUpdate*>(self));
    }
}
kmBranch(0x805BB22C, UpdatePage);

}  // namespace BattleFFA
}  // namespace Pulsar