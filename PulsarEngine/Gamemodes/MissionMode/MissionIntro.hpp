#ifndef _PUL_MISSIONINTRO_
#define _PUL_MISSIONINTRO_

#include <kamek.hpp>

namespace nw4r {
namespace snd {
class DVDSoundArchive;
}
}  // namespace nw4r

namespace Pulsar {
namespace UI {
class ExpSection;
}

namespace MissionMode {

void ResetMissionIntroSelection();
void SetMissionIntroSelection(u32 level, u32 mission, u8 missionId, u16 stageBmgId);
void GetMissionIntroSelection(u32 &level, u32 &stage);
void SetMissionIntroInfoSelection(UI::ExpSection &section, u32 level, u32 stage);
bool ResolveMissionBossIntroPath(const nw4r::snd::DVDSoundArchive *archive, const char *&extFilePath, u32 &length);

}  // namespace MissionMode
}  // namespace Pulsar

#endif
