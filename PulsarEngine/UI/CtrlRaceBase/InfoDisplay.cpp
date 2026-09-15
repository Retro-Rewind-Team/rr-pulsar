#include <PulsarSystem.hpp>
#include <UI/CtrlRaceBase/InfoDisplay.hpp>
#include <SlotExpansion/UI/ExpansionUIMisc.hpp>
#include <SlotExpansion/CupsConfig.hpp>
#include <Settings/Settings.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <core/rvl/OS/OS.hpp>

namespace Pulsar {
namespace UI {
// So that it is only done once in TTs
u32 CtrlRaceTrackInfoDisplay::lastCourse = -1;
u32 CtrlRaceMusicInfoDisplay::lastCourse = -1;

u32 CtrlRaceTrackInfoDisplay::Count() {
    const u32 gamemode = Racedata::sInstance->racesScenario.settings.gamemode;
    const PulsarId winning = CupsConfig::sInstance->GetWinning();
    if (CupsConfig::IsReg(winning)) return 0;
    if ((gamemode == MODE_GRAND_PRIX) || (gamemode == MODE_VS_RACE) || (gamemode == MODE_PUBLIC_VS) || (gamemode == MODE_PRIVATE_VS) || (gamemode == MODE_BATTLE) || (gamemode == MODE_PRIVATE_BATTLE) || (gamemode == MODE_PUBLIC_BATTLE)) return 1;
    if (gamemode == MODE_TIME_TRIAL && winning != lastCourse) {
        lastCourse = winning;
        return 1;
    }
    return 0;
}
void CtrlRaceTrackInfoDisplay::Create(Page &page, u32 index, u32) {
    CtrlRaceTrackInfoDisplay *info = new (CtrlRaceTrackInfoDisplay);
    page.AddControl(index, *info, 0);
    info->Load();
}
static CustomCtrlBuilder INFODISPLAYPANEL(CtrlRaceTrackInfoDisplay::Count, CtrlRaceTrackInfoDisplay::Create);

void CtrlRaceTrackInfoDisplay::Load() {
    this->hudSlotId = 0;

    ControlLoader loader(this);
    loader.Load("game_image", "CTInfo", "CTInfo", nullptr);
    this->textBox_00 = this->layout.GetPaneByName("TextBox_00");

    const CupsConfig *cupsConfig = CupsConfig::sInstance;
    const PulsarId winning = cupsConfig->GetWinning();
    const u32 bmgId = GetCurTrackBMG();

    if (SetTrackNameAuthorMessage(*this, winning, bmgId)) return;

    Text::Info info;
    info.bmgToPass[0] = bmgId;
    info.bmgToPass[1] = GetTrackAuthorBMGId(winning, bmgId);
    this->SetMessage(BMG_INFO_DISPLAY, &info);
}

u32 CtrlRaceMusicInfoDisplay::Count() {
    if (!Settings::Mgr::IsCreated() || Settings::Mgr::Get().GetSettingValue(Pulsar::Settings::SETTING_CTMUSIC) != CTMUSIC_ENABLED) return 0;

    const u32 gamemode = Racedata::sInstance->racesScenario.settings.gamemode;
    const PulsarId winning = CupsConfig::sInstance->GetWinning();
    if (CupsConfig::IsReg(winning)) return 0;

    const wchar_t *credit = GetCustomMsg(GetTrackMusicCreditBMGId(winning));
    if (credit == nullptr || credit[0] == L'\0') return 0;

    if (gamemode == MODE_TIME_TRIAL) {
        if (winning == lastCourse) return 0;
        lastCourse = winning;
    }

    return 1;
}

void CtrlRaceMusicInfoDisplay::Create(Page &page, u32 index, u32) {
    CtrlRaceMusicInfoDisplay *info = new (CtrlRaceMusicInfoDisplay);
    page.AddControl(index, *info, 0);
    info->Load();
}
static CustomCtrlBuilder MUSICINFODISPLAYPANEL(CtrlRaceMusicInfoDisplay::Count, CtrlRaceMusicInfoDisplay::Create);

void CtrlRaceMusicInfoDisplay::Load() {
    this->hudSlotId = 0;
    this->startTime = 0;

    ControlLoader loader(this);
    loader.Load("game_image", "CTInfo", "CTInfo", nullptr);
    this->textBox_00 = this->layout.GetPaneByName("TextBox_00");

    const wchar_t *credit = GetCustomMsg(GetTrackMusicCreditBMGId(CupsConfig::sInstance->GetWinning()));
    static wchar_t s_musicCreditBuffer[0x200];
    swprintf(s_musicCreditBuffer, sizeof(s_musicCreditBuffer) / sizeof(s_musicCreditBuffer[0]), L"\nMusic: %ls", credit);

    Text::Info info;
    info.strings[0] = s_musicCreditBuffer;
    this->SetMessage(BMG_TEXT, &info);
}

bool CtrlRaceMusicInfoDisplay::IsDisplayActive() {
    if (!Settings::Mgr::IsCreated() || Settings::Mgr::Get().GetSettingValue(Pulsar::Settings::SETTING_CTMUSIC) != CTMUSIC_ENABLED) return false;

    Raceinfo *raceInfo = Raceinfo::sInstance;
    if (raceInfo == nullptr || raceInfo->timerMgr == nullptr || !raceInfo->timerMgr->hasRaceStarted) return false;

    if (this->startTime == 0) this->startTime = OS::GetTime();
    return OS::TicksToMilliseconds(OS::GetTime() - this->startTime) < 3000;
}

bool CtrlRaceMusicInfoDisplay::HasStarted() {
    return this->IsDisplayActive();
}

bool CtrlRaceMusicInfoDisplay::IsInactive() {
    return !this->IsDisplayActive();
}

}  // namespace UI
}  // namespace Pulsar
