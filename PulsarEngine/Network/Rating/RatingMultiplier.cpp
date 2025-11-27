#include <RetroRewind.hpp>
#include <Gamemodes/Battle/BattleElimination.hpp>
#include <core/System/SystemManager.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <Network/Rating/PlayerRating.hpp>

namespace Pulsar {
namespace PointRating {

float GetRatingMultiplier() {
    float multiplier = 1.0f;
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    const RKNet::ControllerSub& sub = controller->subs[controller->currentSub];
    
    if (BattleElim::ShouldApplyBattleElimination()) {
        if (sub.playerCount <= 5) {
            multiplier = 1.0f;
        } else {
            multiplier = 1.0f + ((float)(sub.playerCount - 5) * 0.166f);
        }
    }

    SystemManager* sm = SystemManager::sInstance;
    if (sm->isValidDate) {
        const unsigned month = static_cast<unsigned>(sm->month);
        const unsigned day = static_cast<unsigned>(sm->day);
        // Christmas Season/New Year Season, Halloween, Start of Summer, St. Patrick's Day, End of Summer, MKWii's Birthday
        if ((month == 12 && day >= 23 && day <= 31) ||
            (month == 1 && day >= 1 && day <= 3) ||
            (month == 10 && day >= 25 && day <= 31) ||
            (month == 6 && day >= 5 && day <= 8) ||
            (month == 3 && day >= 13 && day <= 17) ||
            (month == 4 && day >= 10 && day <= 14) ||
            (month == 8 && day >= 23 && day <= 29)) {
            
            multiplier = 2.0f;
            if (BattleElim::ShouldApplyBattleElimination()) {
                if (sub.playerCount > 5) {
                    multiplier = 2.0f + ((float)(sub.playerCount - 5) * 0.166f);}
            }
        }
    }
    return multiplier;
}

}  // namespace PointRating
}  // namespace Pulsar