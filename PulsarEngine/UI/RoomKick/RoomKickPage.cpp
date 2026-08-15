#include <UI/RoomKick/RoomKickPage.hpp>

namespace Pulsar {
namespace UI {

RoomKickPage::RoomKickPage() {
    hasBackButton = true;
    externControlCount = 0;
    internControlCount = 13;
    extraControlNumber = 0;
    controlSources = 2;
    titleBmg = BMG_KICK_BOTTOM;
    nextPageId = PAGE_NONE;
    prevPageId = PAGE_FRIEND_ROOM;
    nextSection = SECTION_NONE;
    movieStartFrame = -1;
    isLocked = false;
    activePlayerBitfield = 1;

    playerCount = 0;
    selectedIdx = -1;

    onButtonClickHandler.subject = this;
    onButtonClickHandler.ptmf = &RoomKickPage::OnButtonClick;
    onButtonSelectHandler.subject = this;
    onButtonSelectHandler.ptmf = &RoomKickPage::OnButtonSelect;
    onButtonDeselectHandler.subject = this;
    onButtonDeselectHandler.ptmf = &RoomKickPage::OnButtonDeselect;
    onBackPressHandler.subject = this;
    onBackPressHandler.ptmf = &RoomKickPage::OnBackPress;
    onYesNoClickHandler.subject = this;
    onYesNoClickHandler.ptmf = &RoomKickPage::OnYesNoClick;

    this->controlsManipulatorManager.Init(1, false);
    this->SetManipulatorManager(controlsManipulatorManager);
    this->controlsManipulatorManager.SetGlobalHandler(BACK_PRESS, onBackPressHandler, false, false);
    activePlayerBitfield = 1;
}

void RoomKickPage::OnInit() {
    this->miiGroup = &SectionMgr::sInstance->curSection->Get<Pages::FriendRoomManager>()->miiGroup;
    Menu::OnInit();
}

void RoomKickPage::BeforeEntranceAnimations() {
    MenuInteractable::BeforeEntranceAnimations();

    this->nextPageId = PAGE_NONE;
    this->prevPageId = PAGE_FRIEND_ROOM;
}

void RoomKickPage::BeforeControlUpdate() {
    const RKNet::Controller *controller = RKNet::Controller::sInstance;
    const RKNet::ControllerSub *sub = &controller->subs[0];
    if (sub->connectionUserDatas[sub->localAid].playersAtConsole == 0)
        sub = &controller->subs[1];

    int idx = 0;
    for (int aid = 0; aid < 12; ++aid) {
        if (sub->availableAids & (1 << aid)) {
            for (int player = 0; player < sub->connectionUserDatas[aid].playersAtConsole; ++player) {
                this->miiIdx[idx] = aid * 2 + player;
                this->aidIdx[idx] = aid;

                LayoutUIControl &mii = this->miis[idx];

                mii.isHidden = false;
                mii.SetMiiPane("chara", *this->miiGroup, this->miiIdx[idx], 2);
                mii.SetMiiPane("chara_shadow", *this->miiGroup, this->miiIdx[idx], 2);
                mii.SetMiiPane("chara_c_down", *this->miiGroup, this->miiIdx[idx], 2);
                mii.SetMiiPane("chara_light_01", *this->miiGroup, this->miiIdx[idx], 2);
                mii.SetMiiPane("chara_light_02", *this->miiGroup, this->miiIdx[idx], 2);

                if (aid == sub->localAid) {
                    this->miis[idx].animator.GetAnimationGroupById(4).PlayAnimationAtFrame(2, 0.0f);
                } else {
                    this->miis[idx].animator.GetAnimationGroupById(4).PlayAnimationAtFrame(1, 0.0f);
                }

                mii.SetPaneVisibility("w_null", true);

                ++idx;
            }
        }
    }

    this->playerCount = idx;

    for (int remIdx = idx; remIdx < 12; ++remIdx) {
        this->miis[remIdx].isHidden = false;  // Change to true if you don't want to display empty slots
        this->miis[remIdx].manipulator.inaccessible = false;
        this->miis[remIdx].SetPicturePane("chara", "no_linkmii");
        this->miis[remIdx].SetPicturePane("chara_shadow", "no_linkmii");
        this->miis[remIdx].SetPicturePane("chara_c_down", "no_linkmii");
        this->miis[remIdx].SetPicturePane("chara_light_01", "no_linkmii");
        this->miis[remIdx].SetPicturePane("chara_light_02", "no_linkmii");

        this->miis[remIdx].animator.GetAnimationGroupById(4).PlayAnimationAtFrame(0, 0.0f);

        this->miis[remIdx].SetPaneVisibility("w_null", false);
    }
}

UIControl *RoomKickPage::CreateControl(u32 id) {
    const u32 count = this->controlCount;
    char variant[0x40];
    if (id < 12) {
        this->AddControl(count, this->miis[id], 0);
        snprintf(variant, 0x40, "KickMii%d", id);

        const char *anims[] = {
            "State",
            "Offline",
            "Online",
            "RandomMatching",
            "FriendParent",
            nullptr,
            nullptr,
        };

        this->miis[id].LoadWithAnims(anims, "button", "KickMii", variant, 1, 0);

        this->miis[id].animator.GetAnimationGroupById(4).PlayAnimationAtFrame(1, 0.0f);
        this->miis[id].buttonId = id;
        this->miis[id].SetOnClickHandler(this->onButtonClickHandler, 0);
        this->miis[id].SetOnSelectHandler(this->onButtonSelectHandler);
        this->controlCount++;
        return &this->miis[id];
    } else if (id == 12) {
        this->AddControl(count, this->name, 0);
        ControlLoader loader(&this->name);
        const char *brctr = "TeamName";
        loader.Load(UI::controlFolder, brctr, brctr, nullptr);

        this->controlCount++;
        return &this->name;
    }

    return nullptr;
}

int RoomKickPage::GetActivePlayerBitfield() const {
    return this->activePlayerBitfield;
}

int RoomKickPage::GetPlayerBitfield() const {
    return this->playerBitfield;
}

ManipulatorManager &RoomKickPage::GetManipulatorManager() {
    return this->controlsManipulatorManager;
}

UIControl *RoomKickPage::CreateExternalControl(u32 id) {
    return nullptr;
}

void RoomKickPage::SetButtonHandlers(PushButton &button) {
    button.SetOnClickHandler(this->onButtonClickHandler, 0);
    button.SetOnSelectHandler(this->onButtonSelectHandler);
    button.SetOnDeselectHandler(this->onButtonDeselectHandler);
}

void RoomKickPage::OnBackPress(u32 hudSlotId) {
    this->nextPageId = PAGE_NONE;
    this->prevPageId = PAGE_FRIEND_ROOM;
    this->EndStateAnimated(1, 0.0f);
}

void RoomKickPage::OnYesNoClick(u32 choice, PushButton &button) {
    if (choice == 0) {
        DWC::NodeInfo *nodes = DWC::MatchControl::sInstance->nodes;
        for (int i = 0; i < 32; ++i) {
            if (nodes[i].aid == this->aidIdx[this->selectedIdx]) {
                this->kickedPIDs[this->kickedCount % 64] = nodes[i].pid;
                ++this->kickedCount;
                break;
            }
        }

        DWC::CloseConnectionHard(this->aidIdx[this->selectedIdx]);
    }
}

void RoomKickPage::ClearKickHistory() {
    this->kickedCount = 0;
}

u32 *RoomKickPage::GetKickHistory(u32 &outCount) {
    outCount = this->kickedCount;
    return this->kickedPIDs;
}

void RoomKickPage::OnButtonClick(PushButton &button, u32 hudSlotId) {
    RKNet::Controller *controller = RKNet::Controller::sInstance;
    RKNet::ControllerSub *sub = &controller->subs[controller->currentSub];

    this->nextPageId = PAGE_NONE;
    this->prevPageId = PAGE_FRIEND_ROOM;

    const u32 btnIdx = button.buttonId;
    if (btnIdx < this->playerCount) {
        if (sub->localAid != this->aidIdx[btnIdx]) {
            Pages::YesNoPopUp *msgBox = SectionMgr::sInstance->curSection->Get<Pages::YesNoPopUp>();

            Text::Info info;
            info.miis[0] = this->miiGroup->GetMii(this->miiIdx[this->selectedIdx]);
            msgBox->Reset();
            msgBox->SetMessageBoxMsg(BMG_KICK_CONFIRM, &info);
            msgBox->PrepareButton(0, BMG_YES, nullptr, 0, this->onYesNoClickHandler);
            msgBox->PrepareButton(1, BMG_NO, nullptr, 0, this->onYesNoClickHandler);

            msgBox->initialButtonIdx = 1;

            this->nextPageId = PAGE_VOTERANDOM_MESSAGE_BOX;

            this->EndStateAnimated(1, 0.0f);
        }
    }
}

void RoomKickPage::OnButtonSelect(PushButton &button, u32 hudSlotId) {
    const u32 btnIdx = button.buttonId;
    if (btnIdx < this->playerCount) {
        Text::Info info;
        info.miis[0] = this->miiGroup->GetMii(this->miiIdx[btnIdx]);
        this->name.SetMessage(BMG_MII_NAME, &info);
        this->selectedIdx = btnIdx;
    }
}

}  // namespace UI
}  // namespace Pulsar