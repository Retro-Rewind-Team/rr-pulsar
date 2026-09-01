#ifndef _PUL_200CCPARAMS_
#define _PUL_200CCPARAMS_
#include <kamek.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>

namespace Pulsar {
namespace Race {
const float speedFactor = (5.0f / 3.0f);
const float cannonExit = 2.0f / 3.0f;
const float brakeDriftingDeceleration = -1.5f;
const float fastFallingBodyGravity = 0.39f;
const float fastFallingWheelGravity = 0.3f;

inline bool Is200cc() {
    const Racedata &racedata = *Racedata::sInstance;
    const RKNet::Controller &controller = *RKNet::Controller::sInstance;
    const RacedataSettings &raceSettings = racedata.racesScenario.settings;
    const bool isOffline500cc = controller.roomType == RKNet::ROOMTYPE_NONE && raceSettings.engineClass == CC_50;
    const bool isOfflineMirror = controller.roomType == RKNet::ROOMTYPE_NONE && (racedata.menusScenario.settings.modeFlags & 1);
    return isOffline500cc || (raceSettings.engineClass == CC_100 && controller.roomType != RKNet::ROOMTYPE_VS_WW && !isOfflineMirror);
}

}  // namespace Race
}  // namespace Pulsar

#endif
