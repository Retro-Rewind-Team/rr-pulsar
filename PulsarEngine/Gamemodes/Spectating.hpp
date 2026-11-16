#ifndef _PUL_SPECTATING_
#define _PUL_SPECTATING_

#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>

namespace Pulsar {
namespace Spectating {

void Reset();
void Start(const Raceinfo &raceinfo);
void Update(Raceinfo &raceinfo);
bool IsSpectating();
u8 GetTargetPlayer();

}  // namespace Spectating
}  // namespace Pulsar

#endif
