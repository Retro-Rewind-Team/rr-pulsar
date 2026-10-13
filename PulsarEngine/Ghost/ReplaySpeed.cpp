#include <Ghost/ReplaySpeed.hpp>
#include <MarioKartWii/UI/Page/RaceMenu/GhostReplayPause.hpp>
#include <MarioKartWii/UI/Ctrl/CtrlRace/CtrlRaceBase.hpp>
#include <MarioKartWii/UI/Layout/ControlLoader.hpp>
#include <MarioKartWii/UI/Text/Text.hpp>
#include <MarioKartWii/File/BMG.hpp>
#include <MarioKartWii/Input/InputManager.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>
#include <UI/CtrlRaceBase/CustomCtrlRaceBase.hpp>
#include <UI/UI.hpp>
#include <runtimeWrite.hpp>

namespace Pulsar {
enum ReplaySpeedIndex {
	REPLAY_SPEED_025X,
	REPLAY_SPEED_05X,
	REPLAY_SPEED_1X,
	REPLAY_SPEED_2X,
	REPLAY_SPEED_COUNT
};

static const float replaySpeedMultipliers[REPLAY_SPEED_COUNT] = {0.25f, 0.5f, 1.0f, 2.0f};
static wchar_t replaySpeedLabels[REPLAY_SPEED_COUNT][8] = {
	L"0.25x",
	L"0.5x",
	L"1x",
	L"2x"
};
static u8 replaySpeedIdx = REPLAY_SPEED_1X;
static u8 replayFrameIdx = 0;
static u8 replaySimulationTicks = 1;
static u16 replayLastControllerButtons = 0;
static bool replaySecondTick = false;
static u32 replaySpeedChangeCount = 0;

static bool IsWatchingGhostReplay() {
	if (SectionMgr::sInstance == nullptr || SectionMgr::sInstance->curSection == nullptr)
		return false;
	const SectionId sectionId = SectionMgr::sInstance->curSection->sectionId;
	return sectionId >= SECTION_WATCH_GHOST_FROM_CHANNEL && sectionId <= SECTION_WATCH_GHOST_FROM_MENU;
}

void ResetReplaySpeed() {
	replaySpeedIdx = REPLAY_SPEED_1X;
	replayFrameIdx = 0;
	replaySimulationTicks = 1;
	replayLastControllerButtons = 0;
	replaySecondTick = false;
	replaySpeedChangeCount = 0;
}

u8 GetGhostReplaySpeedIdx() {
	return replaySpeedIdx;
}

static float GetGhostReplaySpeedMultiplier() {
	if (!IsWatchingGhostReplay())
		return replaySpeedMultipliers[REPLAY_SPEED_1X];
	return replaySpeedMultipliers[replaySpeedIdx];
}

bool IsReplayMusicSpeedupWindowOpen(const Timer &elapsedTime) {
	const u32 elapsedTimeMs = (elapsedTime.minutes * 60 + elapsedTime.seconds) * 1000 + elapsedTime.milliseconds;
	return elapsedTimeMs < 5000.0f * GetGhostReplaySpeedMultiplier();
}

u32 GetGhostReplaySpeedChangeCount() {
	return replaySpeedChangeCount;
}

kmRuntimeUse(0x805237e8);
static void ReplayCalcPads(void *director, bool isPaused) {
	const bool pauseGhost = IsWatchingGhostReplay() && replaySimulationTicks == 0 && !replaySecondTick;
	reinterpret_cast<void (*)(void *, bool)>(kmRuntimeAddr(0x805237e8))(director, isPaused || pauseGhost);
}
kmCall(0x80523928, ReplayCalcPads);

kmRuntimeUse(0x805238f0);
static void UpdateReplaySpeedFromController() {
	if (Input::Manager::sInstance == nullptr)
		return;
	Input::RealControllerHolder *holder = &Input::Manager::sInstance->realControllerHolders[0];
	if (holder->prevController == nullptr)
		return;
	Input::Controller *controller = holder->prevController;
	const u8 controllerType = controller->GetType();

	u16 decreaseButton = 0;
	u16 increaseButton = 0;
	switch (controllerType) {
		case GCN:
			decreaseButton = PAD::PAD_BUTTON_L;
			increaseButton = PAD::PAD_BUTTON_R;
			break;
		case CLASSIC:
			decreaseButton = WPAD::WPAD_CL_TRIGGER_L;
			increaseButton = WPAD::WPAD_CL_TRIGGER_R;
			break;
		case NUNCHUCK:
			decreaseButton = WPAD::WPAD_BUTTON_C;
			increaseButton = WPAD::WPAD_BUTTON_Z;
			break;
		default:
			return;
	}

	const u16 currentButtons = controller->uiinputState.rawButtons;
	if (Raceinfo::sInstance == nullptr || !Raceinfo::sInstance->IsAtLeastStage(RACESTAGE_RACE) ||
		Raceinfo::sInstance->IsAtLeastStage(RACESTAGE_IS_FINISHING)) {
		replayLastControllerButtons = currentButtons;
		return;
	}
	const u16 pressed = currentButtons & ~replayLastControllerButtons;
	replayLastControllerButtons = currentButtons;
	if ((pressed & decreaseButton) != 0 && replaySpeedIdx > REPLAY_SPEED_025X) {
		--replaySpeedIdx;
		replayFrameIdx = 0;
		++replaySpeedChangeCount;
	} else if ((pressed & increaseButton) != 0 && replaySpeedIdx < REPLAY_SPEED_2X) {
		++replaySpeedIdx;
		replayFrameIdx = 0;
		++replaySpeedChangeCount;
	}
}

static void ReplayInputUpdate(Input::Manager *manager) {
	const bool isReplay = IsWatchingGhostReplay();
	if (isReplay) {
		if (Raceinfo::sInstance != nullptr && Raceinfo::sInstance->IsAtLeastStage(RACESTAGE_IS_FINISHING)) {
			replaySpeedIdx = REPLAY_SPEED_1X;
			replayFrameIdx = 0;
			replaySimulationTicks = 1;
			replaySecondTick = false;
		} else if (!manager->isPaused) {
			switch (replaySpeedIdx) {
				case REPLAY_SPEED_025X:
					replaySimulationTicks = replayFrameIdx == 0 ? 1 : 0;
					replayFrameIdx = (replayFrameIdx + 1) & 3;
					break;
				case REPLAY_SPEED_05X:
					replaySimulationTicks = replayFrameIdx == 0 ? 1 : 0;
					replayFrameIdx = (replayFrameIdx + 1) & 1;
					break;
				case REPLAY_SPEED_2X:
					replaySimulationTicks = 2;
					break;
				default:
					replaySimulationTicks = 1;
					break;
			}
		}
	} else {
		replaySimulationTicks = 1;
		replayFrameIdx = 0;
		replayLastControllerButtons = 0;
	}
	reinterpret_cast<void (*)(Input::Manager *)>(kmRuntimeAddr(0x805238f0))(manager);
	if (isReplay)
		UpdateReplaySpeedFromController();
}
kmCall(0x8051b428, ReplayInputUpdate);
kmCall(0x8051b59c, ReplayInputUpdate);

kmRuntimeUse(0x80554ad4);
void ReplaySimulationUpdate() {
	const bool isReplay = IsWatchingGhostReplay();
	if (!isReplay || replaySimulationTicks > 0)
		reinterpret_cast<void (*)()>(kmRuntimeAddr(0x80554ad4))();
	if (isReplay && replaySimulationTicks > 1) {
		replaySecondTick = true;
		reinterpret_cast<void (*)(Input::Manager *)>(kmRuntimeAddr(0x805238f0))(Input::Manager::sInstance);
		replaySecondTick = false;
		reinterpret_cast<void (*)()>(kmRuntimeAddr(0x80554ad4))();
	}
}

kmRuntimeUse(0x805bb22c);
static void ReplayPauseOnUpdate(Pages::GhostReplayPause &page) {
	reinterpret_cast<void (*)(Pages::GhostReplayPause &)>(kmRuntimeAddr(0x805bb22c))(page);
	if (page.currentState != STATE_ENTERING && page.currentState != STATE_ACTIVE)
		return;
	UpdateReplaySpeedFromController();
}
kmWritePointer(0x808bdb58, ReplayPauseOnUpdate);

kmRuntimeUse(0x805a9bec);
static void ReplayCameraUpdate(void *camera, bool isPaused) {
	void (*original)(void *, bool) = reinterpret_cast<void (*)(void *, bool)>(kmRuntimeAddr(0x805a9bec));
	const bool isReplay = IsWatchingGhostReplay();
	original(camera, isPaused || (isReplay && replaySimulationTicks == 0));
	if (isReplay && !isPaused && replaySimulationTicks > 1)
		original(camera, false);
}
kmWritePointer(0x808b6c80, ReplayCameraUpdate);

namespace UI {
static const u8 kReplaySpeedFadeFrames = 20;
static const u8 kReplaySpeedDefaultHoldFrames = 60;

class CtrlRaceReplaySpeedLabel : public CtrlRaceBase {
public:
	static u32 Count();
	static void Create(Page &page, u32 index, u32 count);
	void Load();
	void OnUpdate() override;

private:
	nw4r::lyt::Pane *root;
	nw4r::lyt::TextBox *textBox;
	u32 lastSpeedChangeCount;
	u8 lastSpeedIdx;
	u8 speedFadeFrames;
	u8 speedHoldFrames;
	bool fadeDefaultSpeed;
};

static CustomCtrlBuilder sReplaySpeedLabelBuilder(CtrlRaceReplaySpeedLabel::Count, CtrlRaceReplaySpeedLabel::Create);

u32 CtrlRaceReplaySpeedLabel::Count() {
	return IsWatchingGhostReplay() ? 1 : 0;
}

void CtrlRaceReplaySpeedLabel::Create(Page &page, u32 index, u32 count) {
	for (u32 i = 0; i < count; ++i) {
		CtrlRaceReplaySpeedLabel *control = new (CtrlRaceReplaySpeedLabel);
		page.AddControl(index + i, *control, 0);
		control->Load();
	}
}

void CtrlRaceReplaySpeedLabel::Load() {
	this->hudSlotId = 0;
	ControlLoader loader(this);
	loader.Load(UI::raceFolder, "CTInfo", "CTInfo", nullptr);
	this->root = this->layout.GetPaneByName("root");
	if (this->root == nullptr)
		this->root = this->rootPane;
	this->textBox = static_cast<nw4r::lyt::TextBox *>(this->layout.GetPaneByName("TextBox_00"));
	this->lastSpeedChangeCount = ::Pulsar::GetGhostReplaySpeedChangeCount();
	this->lastSpeedIdx = ::Pulsar::GetGhostReplaySpeedIdx();
	this->speedFadeFrames = 0;
	this->speedHoldFrames = 0;
	this->fadeDefaultSpeed = false;
	if (this->root != nullptr)
		this->root->alpha = 0;
	if (this->textBox != nullptr)
		this->textBox->alpha = 0;
}

void CtrlRaceReplaySpeedLabel::OnUpdate() {
	this->UpdatePausePosition();
	Raceinfo *raceInfo = Raceinfo::sInstance;
	if (!IsWatchingGhostReplay() || raceInfo == nullptr || !raceInfo->IsAtLeastStage(RACESTAGE_COUNTDOWN) ||
		raceInfo->IsAtLeastStage(RACESTAGE_IS_FINISHING)) {
		if (this->root != nullptr)
			this->root->alpha = 0;
		if (this->textBox != nullptr)
			this->textBox->alpha = 0;
		return;
	}

	const u32 speedChangeCount = ::Pulsar::GetGhostReplaySpeedChangeCount();
	bool speedChanged = false;
	if (speedChangeCount != this->lastSpeedChangeCount) {
		const u8 speedIdx = ::Pulsar::GetGhostReplaySpeedIdx();
		const bool switchBetweenSlowSpeeds =
			this->lastSpeedIdx <= REPLAY_SPEED_05X && speedIdx <= REPLAY_SPEED_05X;
		speedChanged = true;
		this->lastSpeedChangeCount = speedChangeCount;
		this->lastSpeedIdx = speedIdx;
		this->fadeDefaultSpeed = speedIdx == REPLAY_SPEED_1X;
		if (this->fadeDefaultSpeed)
			this->speedFadeFrames = kReplaySpeedFadeFrames;
		else if (!switchBetweenSlowSpeeds)
			this->speedFadeFrames = 0;
		this->speedHoldFrames = this->fadeDefaultSpeed ? kReplaySpeedDefaultHoldFrames : 0;
		if (this->textBox != nullptr) {
			static wchar_t message[128];
			const s32 labelMsgId = this->curFileBmgs.GetMsgId(UI::BMG_TT_PRACTICE_REPLAYSPEED);
			wchar_t *label = static_cast<wchar_t *>(nullptr);
			if (labelMsgId >= 0)
				label = this->curFileBmgs.GetMsgByMsgId(labelMsgId);
			if (label != nullptr)
				swprintf(message, sizeof(message) / sizeof(message[0]), L"%ls %ls", label, ::Pulsar::replaySpeedLabels[speedIdx]);
			else
				swprintf(message, sizeof(message) / sizeof(message[0]), L"%ls", ::Pulsar::replaySpeedLabels[speedIdx]);
			Text::Info info;
			info.strings[0] = message;
			this->SetMessage(UI::BMG_TEXT, &info);
		}
	}
	if (this->fadeDefaultSpeed) {
		if (!speedChanged && this->speedHoldFrames > 0)
			--this->speedHoldFrames;
		else if (this->speedHoldFrames == 0 && this->speedFadeFrames > 0)
			--this->speedFadeFrames;
	} else if (this->speedFadeFrames < kReplaySpeedFadeFrames) {
		++this->speedFadeFrames;
	}
	const u8 alpha = static_cast<u8>((255 * this->speedFadeFrames) / kReplaySpeedFadeFrames);
	if (this->root != nullptr)
		this->root->alpha = alpha;
	if (this->textBox != nullptr)
		this->textBox->alpha = alpha;
}
}  // namespace UI
}  // namespace Pulsar
