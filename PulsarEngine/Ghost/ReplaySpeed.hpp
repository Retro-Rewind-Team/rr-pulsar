#ifndef _PULSAR_REPLAYSPEED_
#define _PULSAR_REPLAYSPEED_

#include <kamek.hpp>

class Timer;

namespace Pulsar {

void ResetReplaySpeed();
u8 GetGhostReplaySpeedIdx();
u32 GetGhostReplaySpeedChangeCount();
bool IsReplayMusicSpeedupWindowOpen(const Timer &elapsedTime);
void ReplaySimulationUpdate();

}  // namespace Pulsar

#endif
