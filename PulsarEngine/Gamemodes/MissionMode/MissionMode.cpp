#include <Gamemodes/MissionMode/MissionMode.hpp>
#include <MarioKartWii/Race/RaceData.hpp>

namespace Pulsar {
namespace MissionMode {

void PrepareMenuScenario() {
    Racedata::sInstance->menusScenario.settings.gamemode = MODE_MISSION_TOURNAMENT;
}

}  // namespace MissionMode
}  // namespace Pulsar
