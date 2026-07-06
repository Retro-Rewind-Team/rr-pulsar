#include <kamek.hpp>
#include <runtimeWrite.hpp>
#include <UI/MissionMode/MissionMode.hpp>
#include <Gamemodes/MissionMode/MissionMode.hpp>
#include <MarioKartWii/UI/Layout/Layout.hpp>
#include <MarioKartWii/UI/Page/Leaderboard/TTLeaderboard.hpp>
#include <MarioKartWii/UI/Page/RaceHUD/RaceHUD.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>

namespace Pulsar {
namespace UI {
namespace MissionMode {

static const char* GetMissionLevelLabelLayout(const char* lytName) {
    if (strcmp(lytName, "common_w092_mission_level_label") == 0) {
        return "zommon_w092_mission_level_label";
    }
    return lytName;
}

kmRuntimeUse(0x805e9f80);
static void LoadPictureLayout(LayoutUIControl& control, const char* folderName, const char* lytName) {
    typedef PictureLayout* (*AttachPictureLayout)(PictureLayoutList*, const char*, const char*);
    const AttachPictureLayout attachPictureLayout = reinterpret_cast<AttachPictureLayout>(kmRuntimeAddr(0x805e9f80));
    control.pictureLayout = attachPictureLayout(SectionMgr::sInstance->curSection->pictureLayoutList, folderName,
            GetMissionLevelLabelLayout(lytName));
}
kmBranch(0x8063d9c0, LoadPictureLayout);

static bool IsMissionRace() {
    return Racedata::sInstance != nullptr &&
           Racedata::sInstance->racesScenario.settings.gamemode == MODE_MISSION_TOURNAMENT;
}

static bool IsMissionMenuScenario() {
    return Racedata::sInstance != nullptr &&
           Racedata::sInstance->menusScenario.settings.gamemode == MODE_MISSION_TOURNAMENT;
}

static Pages::RaceHUD* SetMissionHudNextPage(Pages::RaceHUD* hud) {
    hud->nextPageId = IsMissionMenuScenario() ? PAGE_TT_LEADERBOARDS : PAGE_COMPETITION_LEADERBOARD;
    return hud;
}
kmCall(0x80624adc, SetMissionHudNextPage);

void CreateRacePages(ExpSection& section) {
    section.CreateAndInitPage(section, PAGE_TT_LEADERBOARDS);
    section.CreateAndInitPage(section, PAGE_TT_ENDMENU);
    if (Pages::RaceHUD::sInstance != nullptr) {
        Pages::RaceHUD::sInstance->nextPageId = PAGE_TT_LEADERBOARDS;
    }
}

kmRuntimeUse(0x80833764);
kmRuntimeUse(0x8085d78c);
static void FillMissionTTLeaderboardRows(Pages::TTLeaderboard* page) {
    if (!IsMissionRace()) {
        typedef void (*FillRowsFn)(Pages::TTLeaderboard*);
        reinterpret_cast<FillRowsFn>(kmRuntimeAddr(0x8085d78c))(page);
        return;
    }

    typedef u32 (*GetPositionBmgFn)(u32);
    const GetPositionBmgFn getPositionBmg = reinterpret_cast<GetPositionBmgFn>(kmRuntimeAddr(0x80833764));
    Text::Info timeInfo;
    timeInfo.intToPass[0] = 9;
    timeInfo.intToPass[1] = 59;
    timeInfo.intToPass[2] = 999;

    const int rowCount = page->GetRowCount() & 0xff;
    for (int i = 0; i < rowCount; ++i) {
        CtrlRaceResult* result = page->results[i];
        if (result == nullptr) continue;

        result->SetTextBoxMessage("position", getPositionBmg(i + 1));
        result->SetTextBoxMessage("time", BMG_DISPLAY_TIME, &timeInfo);
        result->SetTextBoxMessage("player_name", BMG_MISSION_MODE_BUTTON);
        result->ResetTextBoxMessage("handle_text");
        result->ResetTextBoxMessage("total_point");
        result->ResetTextBoxMessage("total_score");
        result->ResetTextBoxMessage("get_point");
    }

    page->ghostMessage.isHidden = true;
}
kmWritePointer(0x808dab30, FillMissionTTLeaderboardRows);

u32 GetMissionButtonId(const Pages::SinglePlayer* page) {
    return page->externControlCount - 2;
}

bool IsMissionButton(const Pages::SinglePlayer* page, u32 id) {
    return id == GetMissionButtonId(page);
}

bool IsBTMRModeButton(const Pages::SinglePlayer* page, u32 id) {
    return id == 3 || IsMissionButton(page, id);
}

u32 GetBTMRModeButtonBMG(const Pages::SinglePlayer* page, u32 id) {
    return IsMissionButton(page, id) ? BMG_MISSION_MODE_BUTTON : BMG_BATTLE_MODE_BUTTON;
}

void CreateSinglePlayerPages(ExpSection& section) {
    section.CreateAndInitPage(section, PAGE_MISSION_LEVEL_SELECT_UNUSED);
    section.CreateAndInitPage(section, PAGE_MISSION_SELECT_SUB);
    section.CreateAndInitPage(section, PAGE_MISSION_INFORMATION_PROMPT);
    section.CreateAndInitPage(section, PAGE_DRIFT_SELECT_WITH_ONE_OPTION);
    section.CreateAndInitPage(section, PAGE_MISSION_TUTORIAL);
}

void OnButtonSelect(Pages::SinglePlayer* page, PushButton& button, u32 hudSlotId) {
    const s32 id = button.buttonId;
    button.buttonId = 4;
    page->Pages::SinglePlayer::OnExternalButtonSelect(button, hudSlotId);
    button.buttonId = id;
    page->bottomText->SetMessage(BMG_MISSION_MODE_BOTTOM);
}

bool OnButtonClick(Pages::SinglePlayer* page, PushButton& button, u32 hudSlotId) {
    if (!IsMissionButton(page, button.buttonId)) return false;

    Pulsar::MissionMode::PrepareMenuScenario();
    page->LoadNextPageById(PAGE_MISSION_LEVEL_SELECT_UNUSED, button);
    return true;
}

}  // namespace MissionMode
}  // namespace UI
}  // namespace Pulsar
