#include <UI/CustomItems/CustomItemPage.hpp>
#include <Settings/Settings.hpp>
#include <Settings/SettingsBinary.hpp>
#include <MarioKartWii/System/Identifiers.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/UI/Page/Other/FriendRoom.hpp>
#include <core/RK/RKSystem.hpp>
#include <core/rvl/OS/OS.hpp>

namespace Pulsar {
namespace UI {

static const char* itemTpls[] = {
    "tt_item_kame_green.tpl",  // 0: GREEN_SHELL
    "tt_item_kame_red.tpl",  // 1: RED_SHELL
    "tt_item_banana.tpl",  // 2: BANANA
    "tt_item_dummybox.tpl",  // 3: FAKE_ITEM_BOX
    "tt_item_kinoko.tpl",  // 4: MUSHROOM
    "tt_item_kinoko_3.tpl",  // 5: TRIPLE_MUSHROOM
    "tt_item_bomb_hei.tpl",  // 6: BOBOMB
    "tt_item_kame_wing.tpl",  // 7: BLUE_SHELL
    "tt_item_thunder.tpl",  // 8: LIGHTNING
    "tt_item_star.tpl",  // 9: STAR
    "tt_item_GoldenKinoko.tpl",  // 10: GOLDEN_MUSHROOM
    "fm_item_kinoko_l.tpl",  // 11: MEGA_MUSHROOM
    "fm_item_gesso.tpl",  // 12: BLOOPER
    "fm_item_pow.tpl",  // 13: POW_BLOCK
    "fm_item_pikakumo.tpl",  // 14: THUNDER_CLOUD
    "tt_item_killer.tpl",  // 15: BULLET_BILL
    "tt_item_kame_green_3.tpl",  // 16: TRIPLE_GREEN_SHELL
    "tt_item_kame_red_3.tpl",  // 17: TRIPLE_RED_SHELL
    "item_banana_3.tpl",  // 18: TRIPLE_BANANA
    "tt_item_random.tpl"  // 19: RANDOMIZE
};

CustomItemPage::CustomItemPage() {
    this->onButtonClickHandler.subject = this;
    this->onButtonClickHandler.ptmf = &CustomItemPage::OnButtonClick;
    this->onButtonSelectHandler.subject = this;
    this->onButtonSelectHandler.ptmf = &CustomItemPage::OnButtonSelect;
    this->onButtonDeselectHandler.subject = this;
    this->onButtonDeselectHandler.ptmf = &CustomItemPage::OnButtonDeselect;
    this->onBackPressHandler.subject = this;
    this->onBackPressHandler.ptmf = &CustomItemPage::OnBackPress;

    this->internControlCount = 20;
    this->externControlCount = 0;
    this->hasBackButton = true;
    this->activePlayerBitfield = 1;
    this->controlSources = 2;
    this->prevPageId = static_cast<PageId>(PULPAGE_SETTINGSPAGESELECT);
    this->titleBmg = BMG_USERSETTINGSOFFSET + BMG_SETTINGS_TITLE + 12;

    this->buttons = new (RKSystem::mInstance.EGGSystem) PushButton[20];

    this->controlsManipulatorManager.Init(1, false);
    this->SetManipulatorManager(controlsManipulatorManager);
    this->controlsManipulatorManager.SetGlobalHandler(BACK_PRESS, onBackPressHandler, false, false);
}

CustomItemPage::~CustomItemPage() {
    delete[] buttons;
}

void CustomItemPage::OnInit() {
    ::Pages::Menu::OnInit();
    this->Pages::Menu::titleText = &this->titleText;
    this->AddControl(21, this->titleText, 0);
    this->titleText.Load(0);
}

UIControl* CustomItemPage::CreateControl(u32 controlId) {
    if (controlId < 20 && buttons != nullptr) {
        this->AddControl(controlId, buttons[controlId], 0);
        char variant[32];
        snprintf(variant, 32, "CustomItem_%d", controlId);
        buttons[controlId].Load(UI::buttonFolder, "item_window_new", variant, 1, 0, false);
        buttons[controlId].buttonId = controlId;
        buttons[controlId].SetOnClickHandler(this->onButtonClickHandler, 0);
        buttons[controlId].SetOnSelectHandler(this->onButtonSelectHandler);
        buttons[controlId].SetOnDeselectHandler(this->onButtonDeselectHandler);

        // Fix for missing "touch" pane in the brlyt. PushButton expects it for its manipulator.
        nw4r::lyt::Pane* touchPane = buttons[controlId].layout.GetPaneByName("race_null");
        if (touchPane) {
            buttons[controlId].manipulator.boundingBox.touchPane = touchPane;
        }

        // Hide highlight initially
        buttons[controlId].SetPaneVisibility("hilight_curr", false);

        this->SetButtonIcon(buttons[controlId], controlId);
        return &buttons[controlId];
    }
    return nullptr;
}

void CustomItemPage::OnActivate() {
    ::Pages::Menu::OnActivate();
    this->titleText.SetMessage(this->titleBmg);
    this->UpdateButtonVisuals();
    this->SelectButton(buttons[0]);
    this->OnButtonSelect(buttons[0], 0);
}

void CustomItemPage::AfterControlUpdate() {
    Pages::MenuInteractable::AfterControlUpdate();
    const Pages::FriendRoomManager* mgr = SectionMgr::sInstance->curSection->Get<Pages::FriendRoomManager>();
    if (mgr && mgr->startedGameMode < 4 && (RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_HOST || RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_NONHOST)) {
        this->LoadPrevPageById(static_cast<PageId>(PULPAGE_SETTINGSPAGESELECT), buttons[0]);
    }
}

void CustomItemPage::OnDeactivate() {
    ::Pages::Menu::OnDeactivate();
    for (int i = 0; i < 20; ++i) {
        buttons[i].SetPaneVisibility("hilight_curr", false);
    }
}

void CustomItemPage::BeforeEntranceAnimations() {
    ::Pages::Menu::BeforeEntranceAnimations();
    this->OnButtonSelect(buttons[0], 0);
}

void CustomItemPage::OnButtonClick(PushButton& button, u32 hudSlotId) {
    u32 bitfield = Settings::Mgr::Get().GetCustomItems();
    if (button.buttonId == 19) {
        // Randomize - using a LCG
        static u32 seed = 0;
        if (seed == 0) seed = OS::GetTick();
        seed = seed * 1103515245 + 12345;
        bitfield = (seed >> 13) & 0x7FFFF;  // 19 bits
        if (bitfield == 0) bitfield = 0x7FFFF;  // Re-enable all if all are disabled
    } else {
        // Toggle bit
        bitfield ^= (1 << button.buttonId);
        if (bitfield == 0) bitfield = 0x7FFFF;  // Safe fallback
    }
    Settings::Mgr::Get().SetCustomItems(bitfield);
    this->UpdateButtonVisuals();
}

void CustomItemPage::OnButtonSelect(PushButton& button, u32 hudSlotId) {
    button.SetPaneVisibility("hilight_curr", true);
}

void CustomItemPage::OnButtonDeselect(PushButton& button, u32 hudSlotId) {
    button.SetPaneVisibility("hilight_curr", false);
}

void CustomItemPage::OnBackPress(u32 hudSlotId) {
    this->nextPageId = static_cast<PageId>(PULPAGE_SETTINGSPAGESELECT);
    this->EndStateAnimated(0, 0.0f);
}

void CustomItemPage::UpdateButtonVisuals() {
    u32 bitfield = Settings::Mgr::Get().GetCustomItems();
    if (bitfield == 0) bitfield = 0x7FFFF;  // Should have been handled but just in case

    for (int i = 0; i < 19; ++i) {
        bool enabled = (bitfield >> i) & 1;
        // Enabled: No tint (fully transparent), Disabled: red tint
        u32 color = enabled ? 0x00000000 : 0xA0000080;
        lyt::Pane* pane = buttons[i].layout.GetPaneByName("item_curr");
        if (pane) ResetMatColor(pane, color);
    }
    // Button 19 (Randomize) is always fully transparent
    lyt::Pane* randPane = buttons[19].layout.GetPaneByName("item_curr");
    if (randPane) ResetMatColor(randPane, 0x00000000);
}

void CustomItemPage::SetButtonIcon(PushButton& button, u32 itemId) {
    if (itemId < 20) {
        ChangeImage(button, "item_curr", itemTpls[itemId]);
    }
}

}  // namespace UI
}  // namespace Pulsar