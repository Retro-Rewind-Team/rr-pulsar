#ifndef _PUL_MISSIONMODE_
#define _PUL_MISSIONMODE_
#include <kamek.hpp>

class RacedataScenario;

namespace Pulsar {
namespace MissionMode {

void PopulateMissionCPUs(RacedataScenario& scenario);
void PrepareMenuScenario();

}  // namespace MissionMode
}  // namespace Pulsar
#endif
