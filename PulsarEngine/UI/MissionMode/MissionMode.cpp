#include <kamek.hpp>
#include <runtimeWrite.hpp>
#include <UI/MissionMode/MissionMode.hpp>
#include <Gamemodes/MissionMode/MissionMode.hpp>
#include <MarioKartWii/UI/Layout/Layout.hpp>
#include <MarioKartWii/UI/Page/RaceHUD/RaceHUD.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>

namespace Pulsar {
namespace UI {
namespace MissionMode {

static const char* GetMissionLevelLabelLayout(const char* lytName) {
    return strcmp(lytName, "common_w092_mission_level_label") == 0 ? "zommon_w092_mission_level_label" : lytName;
}

kmRuntimeUse(0x805e9f80);
static void LoadPictureLayout(LayoutUIControl& control, const char* folderName, const char* lytName) {
    typedef PictureLayout* (*AttachPictureLayout)(PictureLayoutList*, const char*, const char*);
    const AttachPictureLayout attachPictureLayout = reinterpret_cast<AttachPictureLayout>(kmRuntimeAddr(0x805e9f80));
    control.pictureLayout = attachPictureLayout(SectionMgr::sInstance->curSection->pictureLayoutList, folderName,
            GetMissionLevelLabelLayout(lytName));
}
kmBranch(0x8063d9c0, LoadPictureLayout);

static Pages::RaceHUD* SetMissionHudNextPage(Pages::RaceHUD* hud) {
    hud->nextPageId = PAGE_TT_SPLITS;
    return hud;
}
kmCall(0x80624adc, SetMissionHudNextPage);

void CreateRacePages(ExpSection& section) {
    section.CreateAndInitPage(section, PAGE_TT_SPLITS);
    section.CreateAndInitPage(section, PAGE_MISSION_ENDMENU);
    if (Pages::RaceHUD::sInstance != nullptr) {
        Pages::RaceHUD::sInstance->nextPageId = PAGE_TT_SPLITS;
    }
}

u32 GetMissionButtonId(const Pages::SinglePlayer* page) { return page->externControlCount - 2; }

bool IsMissionButton(const Pages::SinglePlayer* page, u32 id) { return id == GetMissionButtonId(page); }

bool IsBTMRModeButton(const Pages::SinglePlayer* page, u32 id) { return id == 3 || IsMissionButton(page, id); }

u32 GetBTMRModeButtonBMG(const Pages::SinglePlayer* page, u32 id) { return IsMissionButton(page, id) ? BMG_MISSION_MODE_BUTTON : BMG_BATTLE_MODE_BUTTON; }

void CreateSinglePlayerPages(ExpSection& section) {
    if (section.pages[PAGE_SINGLE_PLAYER_MENU] == nullptr)
        section.CreateAndInitPage(section, PAGE_SINGLE_PLAYER_MENU);

    const u32 pages[] = {PAGE_MISSION_LEVEL_SELECT_UNUSED, PAGE_MISSION_SELECT_SUB,
        PAGE_MISSION_INFORMATION_PROMPT, PAGE_DRIFT_SELECT_WITH_ONE_OPTION, PAGE_MISSION_TUTORIAL};
    for (u32 i = 0; i < sizeof(pages) / sizeof(pages[0]); ++i) section.CreateAndInitPage(section, pages[i]);
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
