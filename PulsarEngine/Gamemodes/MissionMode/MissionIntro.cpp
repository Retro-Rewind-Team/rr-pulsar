#include <kamek.hpp>
#include <Gamemodes/MissionMode/MissionIntro.hpp>
#include <Gamemodes/MissionMode/MissionMode.hpp>
#include <Gamemodes/MissionMode/MissionMusic.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/UI/Page/Other/RaceIntro.hpp>
#include <SlotExpansion/CupsConfig.hpp>
#include <SlotExpansion/UI/ExpansionUIMisc.hpp>
#include <UI/UI.hpp>
#include <core/rvl/dvd/dvd.hpp>
#include <runtimeWrite.hpp>

namespace Pulsar {
namespace MissionMode {

namespace {

static u32 selectedLevel;
static u32 selectedMission;
static u8 selectedMissionId;
static u16 selectedMissionStageBmgId;
static char missionIntroPath[0x100];

static const u32 MISSION_SELECTION_COUNT = 8;
static const u32 MISSION_INFO_STAGE_OFFSET = 0x83C;
static const u32 MISSION_INFO_LEVEL_OFFSET = 0x840;
static const u32 MISSION_BOSS_INTRO_STAGE = 7;
static const u32 MISSION_NORMAL_INTRO_STAGE = 0;
static const u32 MISSION_INTRO_TITLE_OFFSET = 0x1B8;

static bool StringEndsWith(const char *str, const char *suffix) {
	if (str == nullptr || suffix == nullptr) return false;

	const char *strEnd = str;
	while (*strEnd != '\0') ++strEnd;

	const char *suffixEnd = suffix;
	while (*suffixEnd != '\0') ++suffixEnd;

	while (suffixEnd != suffix) {
		if (strEnd == str) return false;
		--strEnd;
		--suffixEnd;
		if (*strEnd != *suffixEnd) return false;
	}
	return true;
}

typedef Page *(*GetMissionInstructionPageFn)(int);

kmRuntimeUse(0x80842a78);
static Page *GetMissionInstructionPageForIntro(int pageId) {
	static const GetMissionInstructionPageFn original =
		reinterpret_cast<GetMissionInstructionPageFn>(kmRuntimeAddr(0x80842a78));
	Page *page = original(pageId);
	if (page == nullptr || pageId != PAGE_MISSION_INFORMATION_PROMPT) return page;

	if (Racedata::sInstance == nullptr ||
		!IsMissionScenario(Racedata::sInstance->menusScenario))
		return page;

	u32 *stage = reinterpret_cast<u32 *>(reinterpret_cast<u8 *>(page) + MISSION_INFO_STAGE_OFFSET);
	if (HasMissionFeature(Racedata::sInstance->menusScenario, BOSS_MISSION))
		*stage = MISSION_BOSS_INTRO_STAGE;
	else if (*stage == MISSION_BOSS_INTRO_STAGE)
		*stage = MISSION_NORMAL_INTRO_STAGE;
	return page;
}
kmCall(0x808440d8, GetMissionInstructionPageForIntro);
kmCall(0x8084e624, GetMissionInstructionPageForIntro);

static const RacedataScenario *GetMissionIntroScenario() {
	if (Racedata::sInstance == nullptr) return nullptr;

	if (IsMissionScenario(Racedata::sInstance->racesScenario))
		return &Racedata::sInstance->racesScenario;
	if (IsMissionScenario(Racedata::sInstance->menusScenario))
		return &Racedata::sInstance->menusScenario;
	return nullptr;
}

kmRuntimeUse(0x80855200);
typedef void (*RaceIntroOnInitFn)(Pages::RaceIntro *);
static void RaceIntroOnInit(Pages::RaceIntro *intro) {
	static const RaceIntroOnInitFn original =
		reinterpret_cast<RaceIntroOnInitFn>(kmRuntimeAddr(0x80855200));
	if (intro == nullptr) return;
	original(intro);

	const RacedataScenario *scenario = GetMissionIntroScenario();
	if (scenario == nullptr || !IsMissionBossObjective(*scenario)) return;

	LayoutUIControl *cupDisplay = reinterpret_cast<LayoutUIControl *>(
		reinterpret_cast<u8 *>(intro) + MISSION_INTRO_TITLE_OFFSET);
	if (cupDisplay->layout.resources == nullptr) return;
	Pulsar::UI::ChangeImage(*cupDisplay, "cup_icon", "mr_boss.tpl");

	if (selectedMissionStageBmgId == 0 || selectedMissionId != scenario->settings.raceNumber ||
		selectedLevel >= MISSION_SELECTION_COUNT || selectedMission >= MISSION_SELECTION_COUNT ||
		scenario->settings.cupId != selectedLevel)
		return;

	Text::Info info;
	memset(&info, 0, sizeof(info));
	info.intToPass[0] = selectedLevel + 1;
	info.intToPass[1] = selectedMission + 1;
	cupDisplay->SetMessage(selectedMissionStageBmgId, &info);
}
kmWritePointer(0x808da590, RaceIntroOnInit);

static void SetVSIntroBmgId(LayoutUIControl *trackName) {
	const CupsConfig *cupsConfig = CupsConfig::sInstance;
	if (trackName == nullptr || cupsConfig == nullptr) return;

	PulsarId winning = cupsConfig->GetWinning();
	if (Racedata::sInstance != nullptr && IsMissionScenario(Racedata::sInstance->racesScenario)) {
		PulsarId missionMusicTrack;
		if (GetMissionMusicTrack(Racedata::sInstance->racesScenario, missionMusicTrack))
			winning = missionMusicTrack;
	}

	if (CupsConfig::IsReg(winning) || !cupsConfig->IsValidTrack(winning)) return;

	const u32 bmgId = Pulsar::UI::GetTrackBMGId(winning, false);
	Text::Info info;
	info.bmgToPass[0] = bmgId;
	if (Pulsar::UI::SetTrackNameAuthorMessage(*trackName, winning, bmgId)) return;

	info.bmgToPass[1] = Pulsar::UI::GetTrackAuthorBMGId(winning, bmgId);
	trackName->SetMessage(Pulsar::UI::BMG_INFO_DISPLAY, &info);
}
kmCall(0x808552cc, SetVSIntroBmgId);

}

void ResetMissionIntroSelection() {
	selectedLevel = 0;
	selectedMission = 0;
	selectedMissionId = 0;
	selectedMissionStageBmgId = 0;
}

void SetMissionIntroSelection(u32 level, u32 mission, u8 missionId, u16 stageBmgId) {
	selectedLevel = level;
	selectedMission = mission;
	selectedMissionId = missionId;
	selectedMissionStageBmgId = stageBmgId;
}

void SetMissionIntroInfoSelection(Pulsar::UI::ExpSection &section, u32 level, u32 stage) {
	Page *infoPage = section.pages[PAGE_MISSION_INFORMATION_PROMPT];
	if (infoPage == nullptr) return;

	u8 *pageBytes = reinterpret_cast<u8 *>(infoPage);
	*reinterpret_cast<u32 *>(pageBytes + MISSION_INFO_STAGE_OFFSET) = stage;
	*reinterpret_cast<u32 *>(pageBytes + MISSION_INFO_LEVEL_OFFSET) = level;
}

bool ResolveMissionBossIntroPath(const nw4r::snd::DVDSoundArchive *archive, const char *&extFilePath, u32 &length) {
	if (archive == nullptr || Racedata::sInstance == nullptr ||
		!IsMissionScenario(Racedata::sInstance->menusScenario) ||
		!HasMissionFeature(Racedata::sInstance->menusScenario, BOSS_MISSION) ||
		!StringEndsWith(extFilePath, "/o_Crs_In_Fan_battle.brstm"))
		return false;

	snprintf(missionIntroPath, sizeof(missionIntroPath), "%sstrm/o_Crs_In_Fan_mission.brstm", archive->extFileRoot);

	DVD::FileInfo fileInfo;
	if (!DVD::Open(missionIntroPath, &fileInfo)) return false;
	const u32 replacementLength = fileInfo.length;
	DVD::Close(&fileInfo);

	extFilePath = missionIntroPath;
	length = replacementLength;
	return true;
}

}
}
