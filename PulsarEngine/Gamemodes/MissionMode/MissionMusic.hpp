#ifndef _PUL_MISSIONMUSIC_
#define _PUL_MISSIONMUSIC_

#include <MarioKartWii/System/Identifiers.hpp>

namespace Pulsar {
namespace MissionMode {

bool ResolveMissionMusicPath(const char* brstmRoot, const char*& extFilePath);
bool GetMissionMusicSlotOverride(CourseId& musicSlot);

}
}

#endif
