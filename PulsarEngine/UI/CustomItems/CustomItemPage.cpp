#include <UI/CustomItems/CustomItemPage.hpp>
#include <Settings/Settings.hpp>
#include <Settings/SettingsParam.hpp>
#include <PulsarSystem.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/UI/Page/Other/FriendRoom.hpp>
#include <core/rvl/OS/OS.hpp>

namespace Pulsar {
namespace UI {

// Indexed by ItemId.
static const char *const itemTpls[Race::CUSTOM_ITEM_COUNT] = {
    "tt_item_kame_green.tpl",  // GREEN_SHELL
    "tt_item_kame_red.tpl",  // RED_SHELL
    "tt_item_banana.tpl",  // BANANA
    "tt_item_dummybox.tpl",  // FAKE_ITEM_BOX
    "tt_item_kinoko.tpl",  // MUSHROOM
    "tt_item_kinoko_3.tpl",  // TRIPLE_MUSHROOM
    "tt_item_bomb_hei.tpl",  // BOBOMB
    "tt_item_kame_wing.tpl",  // BLUE_SHELL
    "tt_item_thunder.tpl",  // LIGHTNING
    "tt_item_star.tpl",  // STAR
    "tt_item_GoldenKinoko.tpl",  // GOLDEN_MUSHROOM
    "fm_item_kinoko_l.tpl",  // MEGA_MUSHROOM
    "fm_item_gesso.tpl",  // BLOOPER
    "fm_item_pow.tpl",  // POW_BLOCK
    "fm_item_pikakumo.tpl",  // THUNDER_CLOUD
    "tt_item_killer.tpl",  // BULLET_BILL
    "tt_item_kame_green_3.tpl",  // TRIPLE_GREEN_SHELL
    "tt_item_kame_red_3.tpl",  // TRIPLE_RED_SHELL
    "item_banana_3.tpl"  // TRIPLE_BANANA
};

bool CustomItemPage::ShouldSkipFriendRoomPreview() {
    const System *system = System::sInstance;
    if (system == nullptr)
        return false;

    return system->IsContext(PULSAR_STARTRETROS) || system->IsContext(PULSAR_STARTCTS) || system->IsContext(PULSAR_STARTREGS) || system->IsContext(PULSAR_START200)
      || system->IsContext(PULSAR_STARTOTT) || system->IsContext(PULSAR_STARTITEMRAIN) || system->IsContext(PULSAR_MODE_OTT) || (system->netMgr.hostContext & (1 << PULSAR_MODE_OTT));
}

CustomItemPage::CustomItemPage() {
    this->onButtonClickHandler.subject = this;
    this->onButtonClickHandler.ptmf = &CustomItemPage::OnButtonClick;
    this->onBackPressHandler.subject = this;
    this->onBackPressHandler.ptmf = &CustomItemPage::OnBackPress;

    this->internControlCount = BUTTON_COUNT;
    this->externControlCount = 0;
    this->hasBackButton = true;
    this->activePlayerBitfield = 1;
    this->controlSources = 2;
    this->prevPageId = static_cast<PageId>(PULPAGE_SETTINGSPAGESELECT);
    this->titleBmg = Settings::Params::GetPageDef(Settings::SETTINGS_PAGE_ITEMS).nameBmg;
    this->friendRoomPreviewNextPageId = PAGE_CHARACTER_SELECT;
    this->isFriendRoomPreview = false;

    this->controlsManipulatorManager.Init(1, false);
    this->SetManipulatorManager(controlsManipulatorManager);
    this->controlsManipulatorManager.SetGlobalHandler(BACK_PRESS, onBackPressHandler, false, false);
}

CustomItemPage::~CustomItemPage() {
}

UIControl *CustomItemPage::CreateControl(u32 controlId) {
    if (controlId >= BUTTON_COUNT)
        return nullptr;

    PushButton &button = this->buttons[controlId];
    this->AddControl(controlId, button, 0);
    char variant[32];
    snprintf(variant, sizeof(variant), "CustomItem_%d", controlId);
    if (controlId < ENABLE_ALL) {
        button.Load(UI::buttonFolder, "item_window_new", variant, 1, 0, false);
    } else {
        button.Load(UI::buttonFolder, "SettingsPageSelect", variant, 1, 0, false);
        button.SetMessage(BMG_CUSTOM_ITEMS_ALL_ENABLED + controlId - ENABLE_ALL);
    }
    button.buttonId = controlId;
    button.SetOnClickHandler(this->onButtonClickHandler, 0);

    if (controlId < ENABLE_ALL) {
        ChangeImage(button, "icon", itemTpls[controlId]);
        ChangeImage(button, "icon_light_01", itemTpls[controlId]);
        ChangeImage(button, "icon_light_02", itemTpls[controlId]);
    }
    return &button;
}

void CustomItemPage::OnActivate() {
    if (this->isFriendRoomPreview && ShouldSkipFriendRoomPreview()) {
        this->isFriendRoomPreview = false;
        ExpSection *section = ExpSection::GetSection();
        section->RemovePageLayers(section->layerCount - 1);
        ExpSection::AddPageLayer(*section, this->friendRoomPreviewNextPageId);
        return;
    }

    ::Pages::Menu::OnActivate();
    if (this->titleText != nullptr)
        this->titleText->SetMessage(this->titleBmg);
    this->UpdateButtonVisuals();
    this->backButton.isHidden = this->isFriendRoomPreview;
    this->backButton.manipulator.inaccessible = this->isFriendRoomPreview;
    for (u32 i = ENABLE_ALL; i < BUTTON_COUNT; ++i) this->buttons[i].isHidden = this->isFriendRoomPreview;
    for (u32 i = 0; i < BUTTON_COUNT; ++i) this->buttons[i].manipulator.inaccessible = this->isFriendRoomPreview;
    if (!this->isFriendRoomPreview)
        this->SelectButton(this->buttons[0]);
}

void CustomItemPage::AfterControlUpdate() {
    Pages::MenuInteractable::AfterControlUpdate();
    if (this->isFriendRoomPreview) {
        this->previewTimer.Update();
        if (this->previewTimer.countdown <= 0.0f) {
            this->isFriendRoomPreview = false;
            ExpSection *section = ExpSection::GetSection();
            section->RemovePageLayers(section->layerCount - 1);
            ExpSection::AddPageLayer(*section, this->friendRoomPreviewNextPageId);
        }
        return;
    }

    const Pages::FriendRoomManager *mgr = SectionMgr::sInstance->curSection->Get<Pages::FriendRoomManager>();
    if (mgr && mgr->startedGameMode < 4 && (RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_HOST || RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_NONHOST)) {
        this->LoadPrevPageById(static_cast<PageId>(PULPAGE_SETTINGSPAGESELECT), this->buttons[0]);
    }
}

void CustomItemPage::OnButtonClick(PushButton &button, u32 hudSlotId) {
    if (this->isFriendRoomPreview)
        return;

    Settings::Mgr &settings = Settings::Mgr::Get();
    u32 bitfield;
    if (button.buttonId == ENABLE_ALL) {
        bitfield = Race::ALL_CUSTOM_ITEMS;
    } else if (button.buttonId == DISABLE_ALL) {
        bitfield = 0;
    } else if (button.buttonId == RANDOMIZE) {
        static u32 seed = 0;
        if (seed == 0)
            seed = OS::GetTick();
        seed = seed * 1103515245 + 12345;
        bitfield = (seed >> 13) & Race::ALL_CUSTOM_ITEMS;
    } else {
        bitfield = settings.GetCustomItems() ^ (1 << button.buttonId);
    }
    settings.SetCustomItems(bitfield);
    this->UpdateButtonVisuals();
}

void CustomItemPage::OnBackPress(u32 hudSlotId) {
    if (this->isFriendRoomPreview)
        return;

    this->nextPageId = static_cast<PageId>(PULPAGE_SETTINGSPAGESELECT);
    this->EndStateAnimated(0, 0.0f);
}

void CustomItemPage::StartFriendRoomPreview(PageId nextPageId) {
    this->friendRoomPreviewNextPageId = nextPageId;
    this->isFriendRoomPreview = true;
    this->previewTimer.SetInitial(3.0f);
    this->previewTimer.isActive = true;
}

void CustomItemPage::UpdateButtonVisuals() {
    const u32 bitfield = this->isFriendRoomPreview ? Race::GetEffectiveCustomItemsBitfield() : Settings::Mgr::Get().GetCustomItems();

    for (u32 i = 0; i < ENABLE_ALL; ++i) {
        const u32 color = ((bitfield >> i) & 1) ? 0xFFFFFFFF : 0x60606080;
        lyt::Pane *pane = this->buttons[i].layout.GetPaneByName("icon");
        for (u32 vertex = 0; vertex < 4; ++vertex) pane->SetVtxColor(vertex, color);
    }
}

}  // namespace UI
}  // namespace Pulsar
