#include <kamek.hpp>
#include <runtimeWrite.hpp>
#include <Gamemodes/TTPractice.hpp>
#include <PulsarSystem.hpp>
#include <MarioKartWii/Item/ItemPlayer.hpp>
#include <MarioKartWii/Input/ControllerHolder.hpp>
#include <MarioKartWii/File/RKG.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/RKSYS/RKSYSMgr.hpp>
#include <MarioKartWii/UI/Page/Menu/CourseSelect.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>

namespace Pulsar {
namespace TTPractice {

static const u32 ITEM_COUNT = 7;
static const u16 INPUT_LEFT = 0x20;
static const u16 INPUT_RIGHT = 0x40;
static const float MODE_BUTTON_X_OFFSET = -110.0f;
static const ItemId ITEM_WHEEL_ITEMS[ITEM_COUNT] = {
    STAR, GOLDEN_MUSHROOM, TRIPLE_MUSHROOM, MUSHROOM, MEGA_MUSHROOM, BULLET_BILL, THUNDER_CLOUD,
};

static bool isPracticeMode = false;
static u32 selectedItemIndexes[4] = {0, 0, 0, 0};

kmRuntimeUse(0x808d9da0);  // DriftSelect button variant table

void SetPracticeMode(bool enabled) {
    isPracticeMode = enabled;
}

bool IsPracticeMode() {
    return isPracticeMode;
}

static bool IsEnabled() {
    const Racedata* racedata = Racedata::sInstance;
    return isPracticeMode && racedata != nullptr && racedata->racesScenario.settings.gamemode == MODE_TIME_TRIAL;
}

static void CycleItem(u32 hudSlotId, s32 direction) {
    u32& selected = selectedItemIndexes[hudSlotId];
    s32 next = static_cast<s32>(selected) + direction;
    if (next < 0) next = ITEM_COUNT - 1;
    if (next >= static_cast<s32>(ITEM_COUNT)) next = 0;
    selected = static_cast<u32>(next);
}

static void GiveSelectedItem(Item::Player& player, ItemId item) {
    player.inventory.SetItem(item, false);
    player.bitfield |= 0x2;
}

static void UpdateItemWheel(Item::PlayerInventory& inventory) {
    inventory.Update();

    if (!IsEnabled()) return;

    Item::Player& player = *reinterpret_cast<Item::Player*>(reinterpret_cast<u8*>(&inventory) - 0x88);
    if (!player.isHuman || player.isRemote || player.hudSlotId >= 4) return;

    Input::ControllerHolder& holder = player.GetControllerHolder();
    const u16 held = holder.uiinputStates[0].buttonActions;
    const u16 prevHeld = holder.uiinputStates[1].buttonActions;
    const u16 pressed = held & ~prevHeld;
    bool changed = false;

    if ((pressed & INPUT_LEFT) != 0) {
        CycleItem(player.hudSlotId, -1);
        changed = true;
    } else if ((pressed & INPUT_RIGHT) != 0) {
        CycleItem(player.hudSlotId, 1);
        changed = true;
    }

    if (changed || inventory.currentItemId == ITEM_NONE) {
        GiveSelectedItem(player, ITEM_WHEEL_ITEMS[selectedItemIndexes[player.hudSlotId]]);
    }
}
kmCall(0x80797fb4, UpdateItemWheel);

static void LoadGhostSelectOrPracticeRace(Pages::Menu& page, PageId id, PushButton& button) {
    Racedata* racedata = Racedata::sInstance;
    if (!isPracticeMode || racedata == nullptr || racedata->menusScenario.settings.gamemode != MODE_TIME_TRIAL) {
        page.LoadNextPageById(id, button);
        return;
    }

    SectionMgr* sectionMgr = SectionMgr::sInstance;
    if (sectionMgr == nullptr || sectionMgr->sectionParams == nullptr || RKSYS::Mgr::sInstance == nullptr) {
        page.LoadNextPageById(id, button);
        return;
    }

    SectionParams* params = sectionMgr->sectionParams;
    const CourseId courseId = racedata->menusScenario.settings.courseId;
    params->ghostType = BEST_TIME;
    params->courseId = courseId;
    params->licenseId = RKSYS::Mgr::sInstance->curLicenseId;
    params->lastSelectedCourse = courseId;

    racedata->menusScenario.players[1].playerType = PLAYER_NONE;
    racedata->menusScenario.players[2].playerType = PLAYER_NONE;
    racedata->menusScenario.players[3].playerType = PLAYER_NONE;
    page.ChangeSectionById(SECTION_TT, button);
}
kmCall(0x80840a00, LoadGhostSelectOrPracticeRace);

SelectPage::SelectPage() {
    this->onButtonClickHandler.subject = this;
    this->onButtonClickHandler.ptmf = &SelectPage::OnButtonClick;
    this->onButtonSelectHandler.subject = this;
    this->onButtonSelectHandler.ptmf = &SelectPage::OnButtonSelect;
    this->onButtonDeselectHandler.subject = this;
    this->onButtonDeselectHandler.ptmf = &SelectPage::OnButtonDeselect;
    this->onBackPressHandler.subject = this;
    this->onBackPressHandler.ptmf = &SelectPage::OnBackPress;

    this->externControlCount = 2;
    this->internControlCount = 0;
    this->hasBackButton = true;
    this->activePlayerBitfield = 1;
    this->playerBitfield = 1;
    this->controlSources = 2;
    this->prevPageId = PAGE_SINGLE_PLAYER_MENU;
    this->titleBmg = UI::BMG_TIME_TRIALS;

    this->controlsManipulatorManager.Init(1, false);
    this->SetManipulatorManager(this->controlsManipulatorManager);
    this->controlsManipulatorManager.SetGlobalHandler(BACK_PRESS, this->onBackPressHandler, false, false);
}

SelectPage::~SelectPage() {}

void SelectPage::OnInit() {
    Pages::Menu::OnInit();
    this->Pages::Menu::titleText = &this->title;
    this->Pages::Menu::bottomText = &this->bottom;
    this->AddControl(2, this->title, 0);
    this->AddControl(3, this->bottom, 0);
    this->title.Load(0);
    this->bottom.Load();
}

UIControl* SelectPage::CreateExternalControl(u32 controlId) {
    if (controlId >= 2) return nullptr;

    PushButton& button = this->buttons[controlId];
    this->AddControl(this->controlCount++, button, 0);
    const char** driftButtonVariants = reinterpret_cast<const char**>(kmRuntimeAddr(0x808d9da0));
    const u32 layoutId = 1 - controlId;
    button.Load(UI::buttonFolder, "GlobePadEasy", driftButtonVariants[layoutId], this->activePlayerBitfield, 0, false);
    for (int i = 0; i < 4; ++i) {
        button.positionAndscale[i].position.x += MODE_BUTTON_X_OFFSET;
    }
    button.SetPosition(0.0f);
    button.buttonId = controlId;
    button.SetOnClickHandler(this->onButtonClickHandler, 0);
    button.SetOnSelectHandler(this->onButtonSelectHandler);
    button.SetOnDeselectHandler(this->onButtonDeselectHandler);

    button.SetMessage(controlId == 0 ? UI::BMG_TT_NORMAL_BUTTON : UI::BMG_TT_PRACTICE_BUTTON);
    return &button;
}

UIControl* SelectPage::CreateControl(u32 controlId) {
    return nullptr;
}

void SelectPage::OnActivate() {
    Pages::Menu::OnActivate();
    this->title.SetMessage(this->titleBmg);
    this->SelectButton(this->buttons[0]);
    this->OnButtonSelect(this->buttons[0], 0);
}

void SelectPage::OnDeactivate() {
    Pages::Menu::OnDeactivate();
}

void SelectPage::BeforeEntranceAnimations() {
    Pages::Menu::BeforeEntranceAnimations();
    this->OnButtonSelect(this->buttons[0], 0);
}

void SelectPage::OnButtonClick(PushButton& button, u32 hudSlotId) {
    SetPracticeMode(button.buttonId == 1);
    this->LoadNextPageById(PAGE_CHARACTER_SELECT, button);
}

void SelectPage::OnButtonSelect(PushButton& button, u32 hudSlotId) {
    this->bottom.SetMessage(button.buttonId == 0 ? UI::BMG_TT_NORMAL_BOTTOM : UI::BMG_TT_PRACTICE_BOTTOM);
}

void SelectPage::OnButtonDeselect(PushButton& button, u32 hudSlotId) {}

void SelectPage::OnBackPress(u32 hudSlotId) {
    SetPracticeMode(false);
    this->LoadPrevPageWithDelayById(PAGE_SINGLE_PLAYER_MENU, 0.0f);
}

}  // namespace TTPractice
}  // namespace Pulsar
