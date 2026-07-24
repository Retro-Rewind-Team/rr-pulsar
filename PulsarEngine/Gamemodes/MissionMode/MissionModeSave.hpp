#ifndef _PUL_MISSIONMODE_SAVE_
#define _PUL_MISSIONMODE_SAVE_
#include <kamek.hpp>

namespace Pulsar {
namespace MissionMode {

void SaveMissionResult(u32 finishTimeMillis, u32 missionRank);

bool GetMissionRecord(u32 missionId, u32& finishTimeMillis, u8& rating);

}
}
#endif
