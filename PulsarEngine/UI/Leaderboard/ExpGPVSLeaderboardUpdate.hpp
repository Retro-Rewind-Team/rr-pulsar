#ifndef _EXP_GPVS_LEADERBOARD_
#define _EXP_GPVS_LEADERBOARD_
#include <kamek.hpp>
#include <UI/UI.hpp>
#include <MarioKartWii/UI/Page/Leaderboard/GPVSLeaderboardUpdate.hpp>

// Extends Leaderboard to add the ability to toggle between displaying times and names
namespace Pulsar {
namespace UI {
class ExpGPVSLeaderboardUpdate : public Pages::GPVSLeaderboardUpdate {
public:
    void OnUpdate() override;
    void BeforeEntranceAnimations() override;
    PageId GetNextPage() const override;
};
}  // namespace UI
}  // namespace Pulsar
#endif