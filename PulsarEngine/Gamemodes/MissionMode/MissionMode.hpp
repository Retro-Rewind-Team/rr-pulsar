#ifndef _PUL_MISSIONMODE_
#define _PUL_MISSIONMODE_
#include <kamek.hpp>

class RacedataScenario;

namespace Pulsar {
namespace MissionMode {

enum MissionFeatureFlag {
    ITEM_MODE_OVERRIDE = 1 << 0,
    CUSTOM_ITEMS_OVERRIDE = 1 << 1,
    ENGINE_200CC = 1 << 2,
    ENGINE_500CC = 1 << 3
};

bool IsMissionScenario(const RacedataScenario& scenario);
bool HasMissionFeature(const RacedataScenario& scenario, MissionFeatureFlag feature);
bool IsMissionScoreObjective(const RacedataScenario& scenario);
bool IsMissionBossObjective(const RacedataScenario& scenario);
u8 GetMissionItemMode(const RacedataScenario& scenario);
u32 GetMissionCustomItems(const RacedataScenario& scenario);
void ApplyMissionScenarioSettings(RacedataScenario& scenario);
void PopulateMissionCPUs(RacedataScenario& scenario);
void PrepareMenuScenario();

}  // namespace MissionMode
}  // namespace Pulsar
#endif
