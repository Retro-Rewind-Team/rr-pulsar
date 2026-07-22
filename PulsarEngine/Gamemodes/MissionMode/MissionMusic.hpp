#ifndef _PUL_MISSIONMUSIC_
#define _PUL_MISSIONMUSIC_

#include <kamek.hpp>
#include <MarioKartWii/System/Identifiers.hpp>

namespace Pulsar {
namespace MissionMode {

enum {
    MISSION_CHARACTER_TABLE_UNSET = 0xff,
    MISSION_CHARACTER_TABLE_COUNT = 12
};

bool ResolveMissionMusicPath(const char* brstmRoot, const char*& extFilePath);
bool GetMissionMusicSlotOverride(CourseId& musicSlot);
void LoadMissionCharacterTablesFromConfig(const u8* file, u32 fileSize);
u8 GetMissionCharacterTable(u8 playerId);

}
}

#endif
