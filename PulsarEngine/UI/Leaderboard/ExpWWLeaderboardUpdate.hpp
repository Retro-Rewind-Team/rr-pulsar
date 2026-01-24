#ifndef _EXP_WW_LEADERBOARD_
#define _EXP_WW_LEADERBOARD_
#include <MarioKartWii/UI/Page/Leaderboard/WWLeaderboardUpdate.hpp>

// Extends Leaderboard to add the ability to toggle between displaying times and names
namespace Pulsar {
namespace UI {
class ExpWWLeaderboardUpdate : public Pages::WWLeaderboardUpdate {
   public:
    void OnUpdate() override;
};
}  // namespace UI
}  // namespace Pulsar
#endif