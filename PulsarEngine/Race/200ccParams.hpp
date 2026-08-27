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
    const RacedataSettings &menuSettings = racedata.menusScenario.settings;
    const bool isOfflineVS100 = controller.roomType == RKNet::ROOMTYPE_NONE && menuSettings.gamemode == MODE_VS_RACE && menuSettings.engineClass == CC_50;
    return racedata.racesScenario.settings.engineClass == CC_100 && controller.roomType != RKNet::ROOMTYPE_VS_WW && !isOfflineVS100;
}

}  // namespace Race
}  // namespace Pulsar

#endif