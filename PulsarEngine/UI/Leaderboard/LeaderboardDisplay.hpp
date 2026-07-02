#ifndef _UI_LEADERBOARD_DISPLAY_H_
#define _UI_LEADERBOARD_DISPLAY_H_

#include <kamek.hpp>
#include <MarioKartWii/UI/Ctrl/CtrlRace/CtrlRaceResult.hpp>

namespace Pulsar {
namespace UI {

enum LeaderboardDisplayType {
    LEADERBOARD_DISPLAY_NAMES,
    LEADERBOARD_DISPLAY_TIMES,
    LEADERBOARD_DISPLAY_FC
};

void setLeaderboardDisplayType(LeaderboardDisplayType type);
LeaderboardDisplayType getLeaderboardDisplayType();

void nextLeaderboardDisplayType();
void fillLeaderboardResult(CtrlRaceResult& result, u8 playerId);
void fillLeaderboardResults(int count, CtrlRaceResult** results);

bool checkLeaderboardDisplaySwapInputs();

}  // namespace UI
}  // namespace Pulsar
#endif
