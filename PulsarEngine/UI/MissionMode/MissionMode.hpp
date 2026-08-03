#ifndef _PUL_UI_MISSIONMODE_
#define _PUL_UI_MISSIONMODE_
#include <kamek.hpp>
#include <MarioKartWii/UI/Page/Menu/SinglePlayer.hpp>
#include <UI/UI.hpp>
#include <MarioKartWii/UI/Page/Page.hpp>

namespace Pulsar {
namespace UI {
namespace MissionMode {

    u32 GetMissionButtonId(const Pages::SinglePlayer* page);
    bool IsMissionButton(const Pages::SinglePlayer* page, u32 id);
    bool IsBTMRModeButton(const Pages::SinglePlayer* page, u32 id);
    u32 GetBTMRModeButtonBMG(const Pages::SinglePlayer* page, u32 id);
    void PrepareMissionStageSelectReturn();
    void ConfigureMissionInformationPage(Page& page);
    void CreateSinglePlayerPages(ExpSection& section);
    void CreateRacePages(ExpSection& section);
    Page* CreateMissionPausePage();
    void OnButtonSelect(Pages::SinglePlayer* page, PushButton& button, u32 hudSlotId);
    bool OnButtonClick(Pages::SinglePlayer* page, PushButton& button, u32 hudSlotId);
}

}
}
#endif
