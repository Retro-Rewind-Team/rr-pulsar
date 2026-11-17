#ifndef _PUL_200CCPARAMS_
#define _PUL_200CCPARAMS_
#include <kamek.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <GlobalFunction.hpp>

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
    const GameMode gameMode = racedata.menusScenario.settings.gamemode;
    const bool isOffline500cc = IsOfflineRoom() && raceSettings.engineClass == CC_50;
    const bool isOfflineMirror = IsOfflineRoom() && (racedata.menusScenario.settings.modeFlags & 1);
    if (IsBattleMode(gameMode))
        return false;
    return isOffline500cc || (raceSettings.engineClass == CC_100 && GetRoomType() != RKNet::ROOMTYPE_VS_WW && !isOfflineMirror);
}

bool Has200ccPhysics();
bool IsBrakeDriftingEnabled();

}  // namespace Race
}  // namespace Pulsar

#endif
