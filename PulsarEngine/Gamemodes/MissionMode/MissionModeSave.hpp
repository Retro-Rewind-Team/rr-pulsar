#ifndef _PUL_MISSIONMODE_SAVE_
#define _PUL_MISSIONMODE_SAVE_
#include <kamek.hpp>

namespace Pulsar {
namespace MissionMode {

// Saves the result using the mission rank order used by the game:
// 0 = 3 stars, through 5 = C. The file stores this as a user-facing rating
// where 0 is no rank and 6 is 3 stars.
void SaveMissionResult(u32 finishTimeMillis, u32 missionRank);

bool GetMissionRecord(u32 missionId, u32& finishTimeMillis, u8& rating);

}  // namespace MissionMode
}  // namespace Pulsar
#endif
