#ifndef _PUL_UI_MISSIONMODE_
#define _PUL_UI_MISSIONMODE_

#include <kamek.hpp>
#include <MarioKartWii/UI/Page/Menu/SinglePlayer.hpp>
#include <UI/UI.hpp>

namespace Pulsar {
namespace UI {
namespace MissionMode {

u32 GetMissionButtonId(const Pages::SinglePlayer* page);
bool IsMissionButton(const Pages::SinglePlayer* page, u32 id);
bool IsBTMRModeButton(const Pages::SinglePlayer* page, u32 id);
u32 GetBTMRModeButtonBMG(const Pages::SinglePlayer* page, u32 id);
void CreateSinglePlayerPages(ExpSection& section);
void CreateRacePages(ExpSection& section);
void OnButtonSelect(Pages::SinglePlayer* page, PushButton& button, u32 hudSlotId);
bool OnButtonClick(Pages::SinglePlayer* page, PushButton& button, u32 hudSlotId);

}  // namespace MissionMode
}  // namespace UI
}  // namespace Pulsar

#endif
