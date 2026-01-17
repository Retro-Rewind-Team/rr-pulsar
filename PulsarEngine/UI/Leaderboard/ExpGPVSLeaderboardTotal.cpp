#include <UI/Leaderboard/ExpGPVSLeaderboardTotal.hpp>
#include <UI/Leaderboard/LeaderboardDisplay.hpp>
#include <Settings/Settings.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <PulsarSystem.hpp>

namespace Pulsar {
namespace UI {

void ExpGPVSLeaderboardTotal::OnUpdate() {
    if (this->currentState == STATE_ACTIVE && this->curStateDuration >= 1) {
        const RKNet::Controller* ctrl = RKNet::Controller::sInstance;
        if (ctrl && (ctrl->roomType == RKNet::ROOMTYPE_FROOM_HOST || ctrl->roomType == RKNet::ROOMTYPE_FROOM_NONHOST) &&
            System::sInstance->IsContext(PULSAR_VR)) {
            this->EndStateAnimated(1, 0.0f);
            return;
        }
    }
    if (checkLeaderboardDisplaySwapInputs()) {
        nextLeaderboardDisplayType();
        fillLeaderboardResults(GetRowCount(), this->results);
    }
}

}  // namespace UI
}  // namespace Pulsar