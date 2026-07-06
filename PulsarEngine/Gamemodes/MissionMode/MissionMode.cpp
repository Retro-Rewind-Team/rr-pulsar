#include <Gamemodes/MissionMode/MissionMode.hpp>
#include <MarioKartWii/Race/RaceData.hpp>

namespace Pulsar {
namespace MissionMode {

void PrepareMenuScenario() {
    RacedataScenario& scenario = Racedata::sInstance->menusScenario;

    scenario.playerCount = 1;
    scenario.screenCount = 1;
    scenario.localPlayerCount = 1;

    scenario.settings.gamemode = MODE_MISSION_TOURNAMENT;
    scenario.settings.gametype = GAMETYPE_DEFAULT;
    scenario.settings.engineClass = CC_150;
    scenario.settings.courseId = LUIGI_CIRCUIT;
    scenario.settings.cupId = 0;
    scenario.settings.raceNumber = 0;
    scenario.settings.modeFlags = 0;
    scenario.settings.selectId = 0;
    scenario.settings.hudPlayerIds[0] = 0;
    scenario.settings.hudPlayerIds[1] = 0xff;
    scenario.settings.hudPlayerIds[2] = 0xff;
    scenario.settings.hudPlayerIds[3] = 0xff;

    scenario.players[0].playerType = PLAYER_REAL_LOCAL;
    scenario.players[0].hudSlotId = 0;
    for (u32 i = 1; i < 12; ++i) {
        scenario.players[i].playerType = PLAYER_NONE;
        scenario.players[i].hudSlotId = -1;
    }

    for (u32 i = 0; i < sizeof(scenario.mission); ++i) {
        scenario.mission[i] = 0;
    }
}

}  // namespace MissionMode
}  // namespace Pulsar
