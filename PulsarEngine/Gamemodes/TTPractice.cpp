#include <kamek.hpp>
#include <runtimeWrite.hpp>
#include <Gamemodes/TTPractice.hpp>
#include <PulsarSystem.hpp>
#include <MarioKartWii/Item/ItemPlayer.hpp>
#include <MarioKartWii/Item/ItemManager.hpp>
#include <MarioKartWii/Item/Obj/Kumo.hpp>
#include <MarioKartWii/Input/ControllerHolder.hpp>
#include <MarioKartWii/File/RKG.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/RKSYS/RKSYSMgr.hpp>
#include <MarioKartWii/UI/Page/Menu/CourseSelect.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>

namespace Pulsar {
namespace TTPractice {

static const u32 ITEM_COUNT = 7;
static const u16 INPUT_ITEM_USE = 0x4;
static const float MODE_BUTTON_X_OFFSET = -110.0f;
static const float STICK_WHEEL_THRESHOLD = 0.5f;
static const u32 TC_TIMER_OFFSET = 0x1d8;
static const u32 TC_NATURAL_STRIKE_TIMER = 600;
static const u32 TC_STRIKE_TIMER = 599;
static const u32 ITEM_OBJ_KILLED = 0x1;
static const u32 ITEM_OBJ_UNAVAILABLE = 0xc0;
static const ItemId ITEM_WHEEL_ITEMS[ITEM_COUNT] = {
    MUSHROOM, TRIPLE_MUSHROOM, GOLDEN_MUSHROOM, MEGA_MUSHROOM, STAR, BULLET_BILL, THUNDER_CLOUD,
};

static bool isPracticeMode = false;
static u32 selectedItemIndexes[4] = {0, 0, 0, 0};
static s8 stickWheelDirections[4] = {0, 0, 0, 0};
static bool hasGrantedItem[4] = {false, false, false, false};
static bool canRefillOnUse[4] = {false, false, false, false};

kmRuntimeUse(0x808d9da0);  // DriftSelect button variant table

static Item::ObjKumo* FindActiveThunderCloud(Item::Player& player);

void SetPracticeMode(bool enabled) {
    isPracticeMode = enabled;
    for (u32 i = 0; i < 4; ++i) {
        selectedItemIndexes[i] = 0;
        stickWheelDirections[i] = 0;
        hasGrantedItem[i] = false;
        canRefillOnUse[i] = false;
    }
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
    hasGrantedItem[player.hudSlotId] = true;
}

static bool ShouldAutoRefill(ItemId item) {
    return item == STAR || item == MUSHROOM || item == MEGA_MUSHROOM || item == BULLET_BILL || item == THUNDER_CLOUD;
}

static bool ShouldAutoRefill(Item::Player& player, ItemId item) {
    if (!ShouldAutoRefill(item)) return false;
    if (item == THUNDER_CLOUD && FindActiveThunderCloud(player) != nullptr) return false;
    return true;
}

static bool ShouldRefillOnItemUse(ItemId item) {
    return item == GOLDEN_MUSHROOM || item == TRIPLE_MUSHROOM;
}

static bool CanGrantSelectedItem(Item::Player& player, ItemId item) {
    return item != THUNDER_CLOUD || FindActiveThunderCloud(player) == nullptr;
}

static Item::ObjKumo* FindActiveThunderCloud(Item::Player& player) {
    Item::Manager* manager = Item::Manager::sInstance;
    if (manager == nullptr) return nullptr;

    Item::ObjHolder& holder = manager->itemObjHolders[OBJ_THUNDER_CLOUD];
    if (holder.itemObj == nullptr) return nullptr;

    for (u32 i = 0; i < holder.capacity; ++i) {
        Item::Obj* obj = holder.itemObj[i];
        if (obj == nullptr || (obj->bitfield74 & ITEM_OBJ_KILLED) != 0) continue;
        if ((obj->bitfield78 & ITEM_OBJ_UNAVAILABLE) != 0) continue;
        if (obj->itemObjId != OBJ_THUNDER_CLOUD) continue;
        if (*reinterpret_cast<u32*>(reinterpret_cast<u8*>(obj) + TC_TIMER_OFFSET) >= TC_NATURAL_STRIKE_TIMER) continue;

        void* carrier = *reinterpret_cast<void**>(reinterpret_cast<u8*>(obj) + 0x1a0);
        if (carrier == &player) return static_cast<Item::ObjKumo*>(obj);
    }

    return nullptr;
}

static bool IsItemUsePressed(Item::Player& player) {
    Input::ControllerHolder& holder = player.GetControllerHolder();
    const u16 held = holder.inputStates[0].buttonActions;
    const u16 prevHeld = holder.inputStates[1].buttonActions;
    return (held & ~prevHeld & INPUT_ITEM_USE) != 0;
}

static bool IsItemUseHeld(Item::Player& player) {
    return (player.GetControllerHolder().inputStates[0].buttonActions & INPUT_ITEM_USE) != 0;
}

static void RequestThunderCloudStrike(Item::Player& player, bool hadThunderCloudBeforeUpdate) {
    if (!hadThunderCloudBeforeUpdate || !IsEnabled()) return;
    if (!player.isHuman || player.isRemote || player.hudSlotId >= 4) return;
    if (!IsItemUsePressed(player)) return;

    Item::ObjKumo* thunderCloud = FindActiveThunderCloud(player);
    if (thunderCloud == nullptr) return;

    *reinterpret_cast<u32*>(reinterpret_cast<u8*>(thunderCloud) + TC_TIMER_OFFSET) = TC_STRIKE_TIMER;
}

static s8 GetAnalogWheelDirection(u32 hudSlotId, float stickX) {
    s8 direction = 0;
    if (stickX <= -STICK_WHEEL_THRESHOLD) {
        direction = -1;
    } else if (stickX >= STICK_WHEEL_THRESHOLD) {
        direction = 1;
    }

    s8& previousDirection = stickWheelDirections[hudSlotId];
    if (direction == 0) {
        previousDirection = 0;
        return 0;
    }
    if (previousDirection == direction) return 0;

    previousDirection = direction;
    return direction;
}

static s8 GetWheelDirection(Item::Player& player) {
    Input::ControllerHolder& holder = player.GetControllerHolder();
    if (holder.curController == nullptr) return 0;

    const u32 hudSlotId = player.hudSlotId;
    const ControllerType type = holder.curController->GetType();
    switch (type) {
        case NUNCHUCK: {
            stickWheelDirections[hudSlotId] = 0;
            const u16 pressed = holder.uiinputStates[0].rawButtons & ~holder.uiinputStates[1].rawButtons;
            if ((pressed & WPAD::WPAD_BUTTON_LEFT) != 0) return -1;
            if ((pressed & WPAD::WPAD_BUTTON_RIGHT) != 0) return 1;
            return 0;
        }
        case CLASSIC: {
            Input::WiiController* controller = static_cast<Input::WiiController*>(holder.curController);
            return GetAnalogWheelDirection(hudSlotId, controller->kpadStatus[0].extStatus.cl.stickR.x);
        }
        case GCN: {
            Input::GCNController* controller = static_cast<Input::GCNController*>(holder.curController);
            return GetAnalogWheelDirection(hudSlotId, controller->cStickHorizontal);
        }
        default:
            stickWheelDirections[hudSlotId] = 0;
            return 0;
    }
}

static void UpdatePracticeWheel(Item::Player& player) {
    if (!IsEnabled()) return;
    if (!player.isHuman || player.isRemote || player.hudSlotId >= 4) return;

    Item::PlayerInventory& inventory = player.inventory;
    if (inventory.currentItemId == ITEM_NONE) stickWheelDirections[player.hudSlotId] = 0;

    const s8 direction = GetWheelDirection(player);
    bool changed = false;

    if (direction != 0) {
        CycleItem(player.hudSlotId, direction);
        changed = true;
    }

    const ItemId selectedItem = ITEM_WHEEL_ITEMS[selectedItemIndexes[player.hudSlotId]];
    const bool inventoryEmpty = inventory.currentItemId == ITEM_NONE;
    const bool refillableOnUse = ShouldRefillOnItemUse(selectedItem);
    if (!inventoryEmpty || !refillableOnUse) {
        canRefillOnUse[player.hudSlotId] = false;
    } else if (!IsItemUseHeld(player)) {
        canRefillOnUse[player.hudSlotId] = true;
    }

    const bool refillOnUse = inventoryEmpty && refillableOnUse && canRefillOnUse[player.hudSlotId] && IsItemUsePressed(player);
    if (CanGrantSelectedItem(player, selectedItem) &&
        (changed || refillOnUse ||
         (inventory.currentItemId == ITEM_NONE && (!hasGrantedItem[player.hudSlotId] || ShouldAutoRefill(player, selectedItem))))) {
        GiveSelectedItem(player, selectedItem);
        canRefillOnUse[player.hudSlotId] = false;
    }
}

static void UpdatePlayerAndPracticeWheel(Item::Player& player) {
    const bool hadThunderCloudBeforeUpdate = FindActiveThunderCloud(player) != nullptr;
    player.Update();
    RequestThunderCloudStrike(player, hadThunderCloudBeforeUpdate);
    UpdatePracticeWheel(player);
}
kmCall(0x8079994c, UpdatePlayerAndPracticeWheel);

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
