#include <kamek.hpp>
#include <UI/MissionMode/MissionMode.hpp>
#include <UI/MissionMode/MissionModel.hpp>
#include <Gamemodes/MissionMode/MissionMode.hpp>
#include <Gamemodes/MissionMode/MissionModeSave.hpp>
#include <MarioKartWii/Archive/ArchiveMgr.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/Scene/GameScene.hpp>
#include <core/System/SystemManager.hpp>
#include <MarioKartWii/UI/Page/RaceHUD/RaceHUD.hpp>
#include <MarioKartWii/UI/Page/RaceMenu/RaceMenu.hpp>

namespace Pulsar {
namespace UI {
namespace MissionMode {

namespace {

static u32 selectedLevel;
static u32 selectedMission;
static bool returnToStageSelect;

static const u32 MISSION_UI_LEVEL_SIZE = 0x50;
static const u32 MISSION_UI_STAGE_SIZE = 0x0A;
static const u32 MISSION_KMT_HEADER_SIZE = 0x10;
static const u32 MISSION_KMT_ENTRY_SIZE = 0x70;
static const u32 MISSION_INFO_STAGE_OFFSET = 0x83C;
static const u32 MISSION_INFO_LEVEL_OFFSET = 0x840;
static const u32 BMG_OK = 0x7D0;
static const u32 MISSION_PAUSE_END_MENU_SOUND_ID = 0xD5;
static const char* const MISSION_STAGE_RANK_PANE = "mission_rank";

static const wchar_t MISSION_RANK_GLYPHS[7][2] = {
    {0, 0},
    {0xF07A, 0},
    {0xF079, 0},
    {0xF078, 0},
    {0xF061, 0},
    {0xF062, 0},
    {0xF063, 0},
};

static const char* const MISSION_LEVEL_BUTTON_VARIANTS[8] = {
    "Button0", "Button1", "Button2", "Button3", "Button4", "Button5", "Button6", "Button7",
};

static const char* const MISSION_STAGE_BORDER_PANES[] = {
    "color_base",    "fuchi_black",   "fuchi_pattern", "color_down",   "shadow_top_r",
    "shadow_top_l",  "shadow_botom_r", "shadow_botom_l", "hight_light_l", "hight_light_r",
    "text_light_01",
};

class MissionPausePage : public Pages::RaceMenu {
   public:
    MissionPausePage() {
        this->onButtonClickHandler.subject = this;
        this->onButtonClickHandler.ptmf = &MissionPausePage::OnButtonClick;
    }

    int GetMessageBMG() const override { return 0; }
    u32 GetButtonCount() const override { return BUTTON_COUNT; }

    const u32* GetVariantsIdxArray() const override {
        static const u32 variants[BUTTON_COUNT] = {0, 2, 18, 1};
        return variants;
    }

    bool IsPausePage() const override { return true; }
    const char* GetButtonsBRCTRName() const override { return "PauseMenuMR"; }

   private:
    static const u32 BUTTON_COUNT = 4;

    void OnButtonClick(PushButton& button, u32 hudSlotId) {
        const u32 buttonId = static_cast<u32>(button.buttonId);
        const float delay = button.GetAnimationFrameSize();

        switch (buttonId) {
            case 0:
                button.clickSoundId = 0;
                this->EndStateAnimated(1, delay);
                if (Pages::RacePauseMgr::sInstance != nullptr)
                    Pages::RacePauseMgr::sInstance->RequestUnpause();
                return;

            case 2:
                button.clickSoundId = MISSION_PAUSE_END_MENU_SOUND_ID;
                this->ChangeSectionBySceneChange(SECTION_MISSION_MODE, 0, delay);
                return;

            case 18:
                button.clickSoundId = MISSION_PAUSE_END_MENU_SOUND_ID;
                this->ChangeSectionBySceneChange(SECTION_SINGLE_P_MR_CHOOSE_MISSION, 0, delay);
                return;

            case 1:
                this->Pages::RaceMenu::OnButtonClick(button, hudSlotId);
                return;

            default:
                return;
        }
    }
};

static u16 ReadBigEndian16(const u8* data) {
    return static_cast<u16>((static_cast<u16>(data[0]) << 8) | data[1]);
}

static void SetMissionInfoSelection(ExpSection& section, u32 level, u32 stage) {
    Page* infoPage = section.pages[PAGE_MISSION_INFORMATION_PROMPT];
    if (infoPage == nullptr) return;

    u8* pageBytes = reinterpret_cast<u8*>(infoPage);
    *reinterpret_cast<u32*>(pageBytes + MISSION_INFO_STAGE_OFFSET) = stage;
    *reinterpret_cast<u32*>(pageBytes + MISSION_INFO_LEVEL_OFFSET) = level;
}

static void ResetMissionButtonFreeText(PushButton& button) {
    AnimationGroup& textLightGroup = button.animator.GetAnimationGroupById(2);
    if (textLightGroup.animationsCount > 1) textLightGroup.PlayAnimationAtFrame(1, 0.0f);

    nw4r::lyt::Pane* text = button.layout.GetPaneByName("text");
    if (text == nullptr || text->GetMaterial() == nullptr) return;

    nw4r::lyt::Material* material = text->GetMaterial();
    material->UnbindAllAnimation();
    material->tevColours[0].r = 30;
    material->tevColours[0].g = 20;
    material->tevColours[0].b = 5;
    material->tevColours[0].a = 0;
    material->tevColours[1].r = 150;
    material->tevColours[1].g = 140;
    material->tevColours[1].b = 128;
    material->tevColours[1].a = 255;
}

static void SetMissionRank(PushButton& button, u8 rating, bool hideLevelIcon) {
    if (hideLevelIcon && button.layout.GetPaneByName("level_icon") != nullptr)
        button.SetPaneVisibility("level_icon", false);

    nw4r::lyt::Pane* rankPane = button.layout.GetPaneByName(MISSION_STAGE_RANK_PANE);
    if (rankPane == nullptr) return;

    const bool hasRank = rating >= 1 && rating <= 6;
    button.SetPaneVisibility(MISSION_STAGE_RANK_PANE, hasRank);
    if (hasRank) {
        Text::Info rankInfo;
        memset(&rankInfo, 0, sizeof(rankInfo));
        rankInfo.strings[0] = const_cast<wchar_t*>(MISSION_RANK_GLYPHS[rating]);
        button.SetTextBoxMessage(MISSION_STAGE_RANK_PANE, BMG_TEXT, &rankInfo);
    }
}

class MissionSelectPage : public Pages::MenuInteractable {
   public:
    static const u32 BUTTON_COUNT = 8;

    MissionSelectPage()
        : levelSelected(false), missionUiFile(nullptr), missionUiSize(0), missionKmtFile(nullptr), missionKmtSize(0) {
        this->onButtonClickHandler.subject = this;
        this->onButtonClickHandler.ptmf = &MissionSelectPage::OnButtonClick;
        this->onButtonSelectHandler.subject = this;
        this->onButtonSelectHandler.ptmf = &MissionSelectPage::OnButtonSelect;
        this->onBackPressHandler.subject = this;
        this->onBackPressHandler.ptmf = &MissionSelectPage::OnBackPress;
        this->onBackButtonClickHandler.subject = this;
        this->onBackButtonClickHandler.ptmf = &MissionSelectPage::OnBackButtonClick;

        this->internControlCount = BUTTON_COUNT * 2;
        this->externControlCount = 0;
        this->hasBackButton = true;
        this->activePlayerBitfield = 1;
        this->playerBitfield = 1;
        this->controlSources = 2;
        this->prevPageId = PAGE_SINGLE_PLAYER_MENU;
        this->nextPageId = PAGE_MISSION_INFORMATION_PROMPT;
        this->titleBmg = BMG_MISSION_MODE_BUTTON;

        this->controlsManipulatorManager.Init(1, false);
        this->SetManipulatorManager(this->controlsManipulatorManager);
        this->controlsManipulatorManager.SetGlobalHandler(BACK_PRESS, this->onBackPressHandler, false, false);
    }

    void OnInit() override {
        this->LoadMissionResources();
        ::Pages::Menu::OnInit();
        this->backButton.SetOnClickHandler(this->onBackButtonClickHandler, 0);
    }

    void OnActivate() override {
        ::Pages::Menu::OnActivate();
        MissionModel::ResetDriverAnimation(0);
        MissionModel::RequestBackgroundModel();
        if (this->titleText != nullptr) this->titleText->SetMessage(this->titleBmg);
        if (this->bottomText != nullptr) this->bottomText->SetMessage(BMG_MISSION_MODE_BOTTOM);
        this->HideMissionBottomText();
        this->UpdateButtonMessages();
        if (returnToStageSelect) {
            returnToStageSelect = false;
            this->ShowStageSelect();
        } else {
            this->ShowLevelSelect();
        }
    }

    int GetActivePlayerBitfield() const override { return this->activePlayerBitfield; }
    int GetPlayerBitfield() const override { return this->playerBitfield; }
    ManipulatorManager& GetManipulatorManager() override { return this->controlsManipulatorManager; }

    UIControl* CreateControl(u32 controlId) override {
        if (controlId >= BUTTON_COUNT * 2) return nullptr;

        const bool isStage = controlId >= BUTTON_COUNT;
        const u32 index = isStage ? controlId - BUTTON_COUNT : controlId;
        PushButton& button = isStage ? this->stageButtons[index] : this->levelButtons[index];

        this->AddControl(controlId, button, 0);
        button.Load(UI::buttonFolder, isStage ? "MissionStage" : "MissionLevel",
                    MISSION_LEVEL_BUTTON_VARIANTS[index], 1, 0, false);
        button.buttonId = static_cast<s32>(controlId);
        this->SetMissionButtonHandlers(button);
        this->PositionButton(button, isStage ? 95.0f : -185.0f);
        if (isStage) {
            this->SetStageBorderVisible(button, false);
            button.manipulator.inaccessible = true;
            ResetMissionButtonFreeText(button);
        }
        this->UpdateButtonMessage(controlId);
        return &button;
    }

    void SetButtonHandlers(PushButton&) override {}

    UIControl* CreateExternalControl(u32) override { return nullptr; }

    void UpdateButtonMessage(u32 buttonId) {
        Text::Info info;
        memset(&info, 0, sizeof(info));
        if (buttonId < BUTTON_COUNT) {
            swprintf(this->buttonNames[buttonId], 32, L"Level %u", buttonId + 1);
            info.strings[0] = this->buttonNames[buttonId];
            this->levelButtons[buttonId].SetMessage(UI::BMG_TEXT, &info);
            SetMissionRank(this->levelButtons[buttonId], this->GetLowestLevelRating(buttonId), false);
        } else {
            const u32 stageId = buttonId - BUTTON_COUNT;
            this->UpdateStageButtonMessage(selectedLevel, stageId);
        }
    }

    void UpdateButtonMessages() {
        for (u32 i = 0; i < BUTTON_COUNT; ++i) this->UpdateButtonMessage(i);
        this->UpdateStageButtonMessages(selectedLevel);
    }

    void OnButtonClick(PushButton& button, u32) {
        if (button.buttonId < static_cast<s32>(BUTTON_COUNT)) {
            selectedLevel = static_cast<u32>(button.buttonId) % BUTTON_COUNT;
            selectedMission = 0;
            MissionModel::Reset();
            this->ShowStageSelect();
            return;
        }

        const u32 stageId = static_cast<u32>(button.buttonId) - BUTTON_COUNT;
        selectedMission = stageId;
        MissionModel::Reset();
        if (Racedata::sInstance != nullptr) {
            RacedataSettings& settings = Racedata::sInstance->menusScenario.settings;
            settings.cupId = selectedLevel;
            settings.raceNumber = static_cast<u8>(selectedLevel * BUTTON_COUNT + selectedMission);
            const bool scenarioLoaded = this->LoadMissionScenario();
            MissionModel::SetScenarioLoaded(scenarioLoaded);
        }
        ExpSection* section = ExpSection::GetSection();
        if (section != nullptr) SetMissionInfoSelection(*section, selectedLevel, selectedMission);
        returnToStageSelect = true;
        this->LoadNextPageById(PAGE_MISSION_INFORMATION_PROMPT, button);
    }

    void OnButtonSelect(PushButton& button, u32) {
        if (button.buttonId < static_cast<s32>(BUTTON_COUNT)) {
            this->ResetOtherButtonText(this->levelButtons, button);
            this->UpdateStageButtonMessages(static_cast<u32>(button.buttonId));
        } else {
            this->ResetOtherButtonText(this->stageButtons, button);
        }
        this->HideMissionBottomText();
    }

    void OnBackPress(u32) {
        MissionModel::Reset();
        if (this->levelSelected) {
            this->ShowLevelSelect();
            return;
        }

        this->backButton.SelectFocus();
        this->LoadPrevPage(this->backButton);
    }

    void OnBackButtonClick(PushButton&, u32 hudSlotId) { this->OnBackPress(hudSlotId); }

   private:
    void HideMissionBottomText() {
        if (this->bottomText != nullptr) this->bottomText->isHidden = true;
    }

    void ResetOtherButtonText(PushButton* buttons, PushButton& selected) {
        for (u32 i = 0; i < BUTTON_COUNT; ++i)
            if (&buttons[i] != &selected) ResetMissionButtonFreeText(buttons[i]);
    }

    void SetMissionButtonHandlers(PushButton& button) {
        button.SetOnClickHandler(this->onButtonClickHandler, 0);
        button.SetOnSelectHandler(this->onButtonSelectHandler);
    }

    void UpdateStageButtonMessage(u32 level, u32 stageId) {
        Text::Info info;
        memset(&info, 0, sizeof(info));
        swprintf(this->buttonNames[BUTTON_COUNT + stageId], 32, L"%u-%u", level + 1, stageId + 1);
        info.strings[0] = this->buttonNames[BUTTON_COUNT + stageId];
        this->stageButtons[stageId].SetMessage(UI::BMG_TEXT, &info);

        u8 missionId = 0;
        u32 finishTimeMillis = 0;
        u8 rating = 0;
        if (!this->GetMissionId(level, stageId, missionId) ||
            !Pulsar::MissionMode::GetMissionRecord(missionId, finishTimeMillis, rating))
            rating = 0;
        SetMissionRank(this->stageButtons[stageId], rating, true);
    }

    void UpdateStageButtonMessages(u32 level) {
        for (u32 i = 0; i < BUTTON_COUNT; ++i) this->UpdateStageButtonMessage(level, i);
    }

    void PositionButton(PushButton& button, float x) {
        for (u32 i = 0; i < 4; ++i) button.positionAndscale[i].position.x = x;
        button.SetPosition(0.0f);
    }

    void SetStageBorderVisible(PushButton& button, bool visible) {
        for (u32 i = 0; i < sizeof(MISSION_STAGE_BORDER_PANES) / sizeof(MISSION_STAGE_BORDER_PANES[0]); ++i)
            button.SetPaneVisibility(MISSION_STAGE_BORDER_PANES[i], visible);
    }

    bool GetMissionId(u32 level, u32 stageId, u8& missionId) const {
        if (this->missionUiFile == nullptr || level >= BUTTON_COUNT || stageId >= BUTTON_COUNT ||
            this->missionUiSize < MISSION_UI_LEVEL_SIZE * BUTTON_COUNT) return false;

        const u32 uiOffset = level * MISSION_UI_LEVEL_SIZE + stageId * MISSION_UI_STAGE_SIZE;
        if (uiOffset + sizeof(u16) > this->missionUiSize) return false;

        const u16 mappedMissionId = ReadBigEndian16(this->missionUiFile + uiOffset);
        if (static_cast<s16>(mappedMissionId) < 0) return false;

        missionId = static_cast<u8>(mappedMissionId & 0xff);
        return true;
    }

    u8 GetLowestLevelRating(u32 level) const {
        u8 lowestRating = 0;
        bool hasMissions = false;
        for (u32 stageId = 0; stageId < BUTTON_COUNT; ++stageId) {
            u8 missionId = 0;
            u32 finishTimeMillis = 0;
            u8 rating = 0;
            if (!this->GetMissionId(level, stageId, missionId)) continue;

            hasMissions = true;
            if (!Pulsar::MissionMode::GetMissionRecord(missionId, finishTimeMillis, rating) || rating == 0 ||
                rating > 6)
                return 0;

            if (lowestRating == 0 || rating < lowestRating) lowestRating = rating;
        }
        return hasMissions ? lowestRating : 0;
    }

    void ShowLevelSelect() {
        this->levelSelected = false;
        this->HideMissionBottomText();

        for (u32 i = 0; i < BUTTON_COUNT; ++i) {
            this->levelButtons[i].isHidden = false;
            this->levelButtons[i].manipulator.inaccessible = false;
            ResetMissionButtonFreeText(this->levelButtons[i]);

            SetStageBorderVisible(this->stageButtons[i], false);
            this->stageButtons[i].isHidden = false;
            this->stageButtons[i].manipulator.inaccessible = true;
            ResetMissionButtonFreeText(this->stageButtons[i]);
        }

        this->levelButtons[selectedLevel % BUTTON_COUNT].Select(0);
    }

    void ShowStageSelect() {
        this->levelSelected = true;
        this->HideMissionBottomText();

        for (u32 i = 0; i < BUTTON_COUNT; ++i) {
            const bool selected = i == (selectedLevel % BUTTON_COUNT);
            this->levelButtons[i].isHidden = !selected;
            this->levelButtons[i].manipulator.inaccessible = true;
            if (!selected) ResetMissionButtonFreeText(this->levelButtons[i]);

            SetStageBorderVisible(this->stageButtons[i], true);
            this->stageButtons[i].isHidden = false;
            this->stageButtons[i].manipulator.inaccessible = false;
            ResetMissionButtonFreeText(this->stageButtons[i]);
        }

        this->UpdateButtonMessages();
        this->stageButtons[selectedMission % BUTTON_COUNT].Select(0);
    }

    void LoadMissionResources() {
        if (ArchiveMgr::sInstance != nullptr) {
            this->missionUiFile = static_cast<const u8*>(
                ArchiveMgr::sInstance->GetFile(ARCHIVE_HOLDER_UI, "parameter/mission_ui_single.bin", &this->missionUiSize));
        }

        const GameScene* scene = GameScene::GetCurrent();
        if (scene == nullptr || scene->structsHeaps.heaps[1] == nullptr) return;

        this->missionKmtFile = static_cast<const u8*>(SystemManager::RipFromDisc(
            "/Race/MissionRun/mission_single.kmt", scene->structsHeaps.heaps[1], true, &this->missionKmtSize));
    }

    bool LoadMissionScenario() {
        if (Racedata::sInstance == nullptr || this->missionUiFile == nullptr || this->missionKmtFile == nullptr ||
            this->missionUiSize < MISSION_UI_LEVEL_SIZE * BUTTON_COUNT ||
            this->missionKmtSize < MISSION_KMT_HEADER_SIZE) {
            return false;
        }

        u8 missionId = 0;
        if (!this->GetMissionId(selectedLevel, selectedMission, missionId)) return false;
        const u16 missionCount = ReadBigEndian16(this->missionKmtFile + 0x08);
        const u32 missionOffset = MISSION_KMT_HEADER_SIZE + static_cast<u32>(missionId) * MISSION_KMT_ENTRY_SIZE;
        if (missionId >= missionCount || missionOffset + MISSION_KMT_ENTRY_SIZE > this->missionKmtSize) return false;

        RacedataScenario& scenario = Racedata::sInstance->menusScenario;
        const u8* mission = this->missionKmtFile + missionOffset;
        memcpy(scenario.mission, mission, MISSION_KMT_ENTRY_SIZE);
        scenario.settings.courseId = static_cast<CourseId>(mission[0x04]);
        scenario.settings.raceNumber = static_cast<u8>(missionId);
        scenario.players[0].characterId = static_cast<CharacterId>(mission[0x05]);
        scenario.players[0].kartId = static_cast<KartId>(mission[0x06]);
        if (SectionMgr::sInstance != nullptr && SectionMgr::sInstance->sectionParams != nullptr) {
            SectionMgr::sInstance->sectionParams->characters[0] = scenario.players[0].characterId;
            SectionMgr::sInstance->sectionParams->karts[0] = scenario.players[0].kartId;
        }
        Pulsar::MissionMode::PopulateMissionCPUs(scenario);
        return true;
    }

    bool levelSelected;
    const u8* missionUiFile;
    u32 missionUiSize;
    const u8* missionKmtFile;
    u32 missionKmtSize;
    PushButton levelButtons[BUTTON_COUNT];
    PushButton stageButtons[BUTTON_COUNT];
    wchar_t buttonNames[BUTTON_COUNT * 2][32];
    PtmfHolder_2A<MissionSelectPage, void, PushButton&, u32> onButtonClickHandler;
    PtmfHolder_2A<MissionSelectPage, void, PushButton&, u32> onButtonSelectHandler;
    PtmfHolder_1A<MissionSelectPage, void, u32> onBackPressHandler;
    PtmfHolder_2A<MissionSelectPage, void, PushButton&, u32> onBackButtonClickHandler;
};

static void InstallMissionPage(ExpSection& section, PageId id, Page* page) {
    if (section.pages[id] != nullptr) {
        section.pages[id]->Dispose();
        delete section.pages[id];
    }
    section.Set(page, id);
    page->Init(id);
}

}

Page* CreateMissionPausePage() { return new MissionPausePage(); }

void PrepareMissionStageSelectReturn() {
    returnToStageSelect = true;
}

void ConfigureMissionInformationPage(Page& page) {
    if (page.pageId != PAGE_MISSION_INFORMATION_PROMPT) return;

    PushButton* buttons[2] = {};
    u32 buttonCount = 0;
    for (u32 i = 0; i < page.controlGroup.controlCount; ++i) {
        UIControl* control = page.controlGroup.GetControl(i);
        if (control == nullptr || strcmp(control->GetClassName(), "PushButton") != 0) continue;
        if (buttonCount < sizeof(buttons) / sizeof(buttons[0]))
            buttons[buttonCount++] = static_cast<PushButton*>(control);
    }

    if (buttonCount == 0) return;

    PushButton* okButton = buttons[0];
    PushButton* tutorialButton = nullptr;
    if (buttonCount > 1) {
        if (buttons[1]->positionAndscale[0].position.y > okButton->positionAndscale[0].position.y) {
            tutorialButton = okButton;
            okButton = buttons[1];
        } else {
            tutorialButton = buttons[1];
        }
    }

    okButton->SetMessage(BMG_OK);
    okButton->isHidden = false;
    okButton->manipulator.inaccessible = false;
    okButton->Select(0);

    if (tutorialButton != nullptr) {
        tutorialButton->isHidden = true;
        tutorialButton->manipulator.inaccessible = true;
    }
}

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

    MissionModel::CreateModelPage(section);

    InstallMissionPage(section, PAGE_MISSION_LEVEL_SELECT_UNUSED, new MissionSelectPage());

    const u32 pages[] = {PAGE_MISSION_INFORMATION_PROMPT, PAGE_MISSION_TUTORIAL};
    for (u32 i = 0; i < sizeof(pages) / sizeof(pages[0]); ++i) section.CreateAndInitPage(section, pages[i]);

    InstallMissionPage(section, PAGE_DRIFT_SELECT, MissionModel::CreateDriftSelectPage());
    InstallMissionPage(section, PAGE_DRIFT_SELECT_WITH_ONE_OPTION, MissionModel::CreateDriftSelectPage());
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

    selectedLevel = 0;
    selectedMission = 0;
    MissionModel::Reset();
    Pulsar::MissionMode::PrepareMenuScenario();
    page->LoadNextPageById(PAGE_MISSION_LEVEL_SELECT_UNUSED, button);
    return true;
}

}
}
}
