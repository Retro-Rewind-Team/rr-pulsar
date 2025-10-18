#include <UI/ExtendedTeamSelect/ExtendedTeamSelect.hpp>
#include <MarioKartWii/UI/Page/Other/FriendRoom.hpp>
#include <MarioKartWii/UI/Page/Other/Message.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <Network/GPReport.hpp>
#include <core/nw4r/ut/Misc.hpp>

namespace Pulsar {
namespace UI {

ExtendedTeamSelect::ExtendedTeamSelect() {
    hasBackButton = true;
    externControlCount = 0;
    internControlCount = CONTROL_COUNT;
    extraControlNumber = 0;
    controlSources = 2;
    titleBmg = 0;
    nextPageId = PAGE_CHARACTER_SELECT;
    prevPageId = PAGE_FRIEND_ROOM;
    nextSection = SECTION_NONE;
    movieStartFrame = -1;
    isLocked = false;
    activePlayerBitfield = 1;

    this->shouldDisconnect = false;
    this->isHost = false;
    this->onBackPressHandler.subject = this;
    this->onBackPressHandler.ptmf = &ExtendedTeamSelect::OnBackPress;

    this->onFrontPressHandler.subject = this;
    this->onFrontPressHandler.ptmf = &ExtendedTeamSelect::OnFrontPress;

    this->onBackClickHandler.subject = this;
    this->onBackClickHandler.ptmf = &ExtendedTeamSelect::OnBackButtonClick;

    this->onStartRaceClickHandler.subject = this;
    this->onStartRaceClickHandler.ptmf = &ExtendedTeamSelect::OnStartRaceClick;

    this->onStartRaceSelectHandler.subject = this;
    this->onStartRaceSelectHandler.ptmf = &ExtendedTeamSelect::OnStartRaceSelect;

    this->onArrowClickHandler.subject = this;
    this->onArrowClickHandler.ptmf = &ExtendedTeamSelect::OnArrowClick;

    this->onArrowSelectHandler.subject = this;
    this->onArrowSelectHandler.ptmf = &ExtendedTeamSelect::OnArrowSelect;

    this->controlsManipulatorManager.Init(1, false);
    this->SetManipulatorManager(this->controlsManipulatorManager);

    this->controlsManipulatorManager.SetGlobalHandler(FORWARD_PRESS, onFrontPressHandler, false, false);
    this->controlsManipulatorManager.SetGlobalHandler(BACK_PRESS, onBackPressHandler, false, false);

    this->manager = ExtendedTeamManager::sInstance;
}

void ExtendedTeamSelect::OnInit() {
    MenuInteractable::OnInit();

    this->backButton.SetOnClickHandler(this->onBackClickHandler, 0);
}

void ExtendedTeamSelect::BeforeEntranceAnimations() {
    MenuInteractable::BeforeEntranceAnimations();

    Pages::FriendRoomManager* friendRoomManager = SectionMgr::sInstance->curSection->Get<Pages::FriendRoomManager>();

    this->isHost = RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_HOST;
    this->instructionText.SetMessage(this->isHost ? BMG_TEAM_SELECT : BMG_EXTENDEDTEAMS_NONHOST_TITLE);

    this->backButton.isHidden = this->manager->hasFriendRoomStarted;
    this->backButton.manipulator.inaccessible = this->manager->hasFriendRoomStarted;

    this->miiGroup = &friendRoomManager->miiGroup;

    this->shouldDisconnect = false;

    this->teamPlayerArrows[0].SelectInitial(0);
}

void ExtendedTeamSelect::BeforeExitAnimations() {
    MenuInteractable::BeforeExitAnimations();

    // Report team info to GPCM
    if (this->manager->hasFriendRoomStarted) {
        RKNet::Controller* controller = RKNet::Controller::sInstance;
        RKNet::ControllerSub& sub = controller->subs[controller->currentSub];

        u8 numPlayersOnAid = sub.connectionUserDatas[sub.localAid].playersAtConsole;
        for (int playerOnAid = 0; playerOnAid < numPlayersOnAid; playerOnAid++) {
            char buffer[64];
            snprintf(buffer, sizeof(buffer), "hi=%d|tm=%d", playerOnAid, this->manager->GetPlayerTeamByAID(sub.localAid, playerOnAid));
            Network::Report("wl:mkw_extended_teams", buffer);
        }
    }
}

void ExtendedTeamSelect::OnResume() {
    MenuInteractable::OnResume();

    RKNet::Controller* controller = RKNet::Controller::sInstance;
    RKNet::ControllerSub& sub = controller->subs[controller->currentSub];

    if (sub.localPlayerCount > 1) {
        SectionMgr::sInstance->SetNextSection(SECTION_P2_WIFI_FROM_FROOM_RACE, 0);
    } else {
        SectionMgr::sInstance->SetNextSection(SECTION_P1_WIFI_FROM_FROOM_RACE, 0);
    }

    SectionMgr::sInstance->RequestSceneChange(0.0, 0xff);
}

void ExtendedTeamSelect::BeforeControlUpdate() {
    Pages::FriendRoomManager* friendRoomManager = SectionMgr::sInstance->curSection->Get<Pages::FriendRoomManager>();
    friendRoomManager->networkManager.Update();
    this->manager->Update();

    this->busySymbol.isHidden = this->isHost && !this->manager->IsWaitingStatus();
    this->startRaceButton.isHidden = !this->busySymbol.isHidden || !this->manager->hasFriendRoomStarted;
    this->startRaceButton.manipulator.inaccessible = !this->busySymbol.isHidden || !this->manager->hasFriendRoomStarted;

    if (this->manager->IsDoneStatus())
        this->OnStartRaceClick(this->startRaceButton, 0);

    RKNet::Controller* controller = RKNet::Controller::sInstance;
    RKNet::ControllerSub& sub = controller->subs[0];
    if (sub.connectionUserDatas[sub.localAid].playersAtConsole == 0)
        sub = controller->subs[1];

    ExtendedTeamID savedTeams[12][2];  // [aid][playerOnAid]
    for (int i = 0; i < 12; i++) {
        savedTeams[i][0] = this->manager->GetPlayerTeamByAID(i, 0);
        savedTeams[i][1] = this->manager->GetPlayerTeamByAID(i, 1);

        if (savedTeams[i][0] == TEAM_COUNT) {
            savedTeams[i][0] = (ExtendedTeamID)(i % TEAM_COUNT);
        }

        if (savedTeams[i][1] == TEAM_COUNT) {
            savedTeams[i][1] = (ExtendedTeamID)(i % TEAM_COUNT);
        }
    }

    int count = 0;
    for (u8 aid = 0; aid < 12; aid++) {
        if (sub.availableAids & (1 << aid)) {
            u8 numPlayersOnAid = sub.connectionUserDatas[aid].playersAtConsole;
            for (int playerOnAid = 0; playerOnAid < numPlayersOnAid; playerOnAid++) {
                const u8 id = aid * 2 + playerOnAid;
                Text::Info textInfo;
                textInfo.miis[0] = this->miiGroup->GetMii(id);

                this->teamPlayerControl[count].SetMiiPane("chara_icon", *this->miiGroup, id, 2);
                this->teamPlayerControl[count].SetMiiPane("chara_icon_sha", *this->miiGroup, id, 2);
                this->teamPlayerControl[count].SetTextBoxMessage("mii_name", BMG_MII_NAME, &textInfo);

                this->UpdatePlayerTeam(count, savedTeams[aid][playerOnAid]);
                this->manager->SetPlayerIndexes(count, id, aid, playerOnAid);

                this->teamPlayerControl[count].isHidden = false;
                this->teamPlayerArrows[count].isHidden = !this->isHost;
                this->teamPlayerArrows[count].manipulator.inaccessible = !this->isHost;

                if (sub.localAid == aid) {
                    this->teamPlayerControl[count].animator.GetAnimationGroupById(1).PlayAnimationAtFrame(0, 0.0f);  // Local::Local
                    this->teamPlayerControl[count].animator.GetAnimationGroupById(2).PlayAnimationAtFrame(0, 0.0f);  // FlashLocal::FlashLocal
                } else {
                    this->teamPlayerControl[count].animator.GetAnimationGroupById(1).PlayAnimationAtFrame(1, 0.0f);  // Local::NetOther
                    this->teamPlayerControl[count].animator.GetAnimationGroupById(2).PlayAnimationAtFrame(1, 0.0f);  // FlashLocal::FlashNetOther
                }

                count++;
            }
        }
    }

    for (int i = count; i < 12; i++) {
        this->teamPlayerControl[count].isHidden = true;
        this->teamPlayerArrows[count].isHidden = true;
        this->teamPlayerArrows[count].manipulator.inaccessible = true;
    }

    this->playerCount = count;

    if (this->playerCount <= 1 && (this->manager->IsSelectingStatus() || this->manager->IsWaitingStatus()) && !this->shouldDisconnect) {
        Pages::MessageBox* messageBox = SectionMgr::sInstance->curSection->Get<Pages::MessageBox>();
        messageBox->Reset();
        messageBox->SetMessageWindowText(BMG_DISCONNECTED_FROM_OTHER_PLAYERS);
        this->AddPageLayer(Pages::MessageBox::id, 0);
        this->shouldDisconnect = true;
    }
}

void ExtendedTeamSelect::AfterControlUpdate() {
    if (this->isHost && this->manager->IsDoneStatus()) {
        this->nextPageId = PAGE_CHARACTER_SELECT;
        this->EndStateAnimated(0, this->startRaceButton.GetAnimationFrameSize());
    }
}

int ExtendedTeamSelect::GetActivePlayerBitfield() const {
    return this->activePlayerBitfield;
}

int ExtendedTeamSelect::GetPlayerBitfield() const {
    return this->playerBitfield;
}

ManipulatorManager& ExtendedTeamSelect::GetManipulatorManager() {
    return this->controlsManipulatorManager;
}

UIControl* ExtendedTeamSelect::CreateExternalControl(u32 id) {
    return nullptr;
}

UIControl* ExtendedTeamSelect::CreateControl(u32 id) {
    const u32 count = this->controlCount;
    this->controlCount++;
    if (id == INSTRUCTION_TEXT) {
        this->AddControl(count, this->instructionText, 1);
        this->instructionText.Load();
        this->instructionText.SetMessage(BMG_TEAM_SELECT);
        this->instructionText.layout.GetPaneByName("message_nul")->trans.y -= 12.0f;
        this->instructionText.layout.GetPaneByName("text_null")->trans.y -= 12.0f;
        this->instructionText.layout.GetPaneByName("text")->trans.y -= 12.0f;

        return &this->instructionText;
    } else if (id == BUSY_SYMBOL) {
        this->AddControl(count, this->busySymbol, 0);
        this->busySymbol.Load("message_window", "FriendRoomBusySymbol", "BusySymbol");
        return &this->busySymbol;
    } else if (id == START_RACE_BUTTON) {
        this->AddControl(count, this->startRaceButton, 0);
        this->startRaceButton.Load("button", "ExpandedTeamButton", "ExpandedTeamButton", 1, 0, false);
        this->startRaceButton.buttonId = 1;
        this->startRaceButton.SetOnClickHandler(this->onStartRaceClickHandler, 0);
        this->startRaceButton.SetOnSelectHandler(this->onStartRaceSelectHandler);
        return &this->startRaceButton;
    } else if (id >= TEAM_ENTRY_0 && id <= TEAM_ENTRY_11) {
        const u32 idx = id - TEAM_ENTRY_0;
        this->AddControl(count, this->teamPlayerControl[idx], 0);

        const char* anims[] = {
            "Loop", "Loop", nullptr,
            "Local", "Local", "NetOther", nullptr,
            "LocalFlash", "FlashLocal", "FlashNetOther", nullptr,
            nullptr};

        char variant[16];
        snprintf(variant, 16, "Member%02d", idx);

        ControlLoader loader(&this->teamPlayerControl[idx]);
        loader.Load("button", "ExpandedTeamPlayerEntry", variant, anims);

        this->teamPlayerControl[idx].animator.GetAnimationGroupById(0).PlayAnimationAtFrame(0, 0.0f);
        this->teamPlayerControl[idx].animator.GetAnimationGroupById(1).PlayAnimationAtFrame(1, 0.0f);
        this->teamPlayerControl[idx].animator.GetAnimationGroupById(2).PlayAnimationAtFrame(1, 0.0f);

        this->teamPlayerControl[idx].isHidden = true;
        return &this->teamPlayerControl[idx];
    } else if (id >= TEAM_ENTRY_ARROW_0 && id <= TEAM_ENTRY_ARROW_11) {
        const u32 idx = id - TEAM_ENTRY_ARROW_0;
        this->AddControl(count, this->teamPlayerArrows[idx], 0);

        char variant[16];
        snprintf(variant, 16, "player%d", idx + 1);

        this->teamPlayerArrows[idx].Load(UI::buttonFolder, "ExpandedTeamPlayerArrow", variant, 1, 0, true);
        this->teamPlayerArrows[idx].buttonId = 2 + idx;
        this->teamPlayerArrows[idx].SetOnClickHandler(this->onArrowClickHandler, 0);
        this->teamPlayerArrows[idx].SetOnSelectHandler(this->onArrowSelectHandler);

        if (id == TEAM_ENTRY_ARROW_0) {
            this->teamPlayerArrows[idx].SelectInitial(0);
        }

        // Rotate left-side arrows
        if (idx % 2 == 0) {
            this->teamPlayerArrows[idx].layout.layout.rootPane->rotate.z = -180.0f;
        }

        this->teamPlayerArrows[idx].isHidden = true;
        return &this->teamPlayerArrows[idx];
    }

    return nullptr;
}

void ExtendedTeamSelect::OnBackPress(u32 hudSlotId) {
    if (!this->backButton.isHidden) {
        this->nextPageId = PAGE_NONE;
        this->EndStateAnimated(1, this->backButton.GetAnimationFrameSize());
    }
}

void ExtendedTeamSelect::OnStartRaceClick(PushButton& button, u32 hudSlotId) {
    Pages::FriendRoomWaiting* friendRoomWaiting = SectionMgr::sInstance->curSection->Get<Pages::FriendRoomWaiting>();
    Pages::FriendRoomManager* friendRoomManager = SectionMgr::sInstance->curSection->Get<Pages::FriendRoomManager>();

    if (!this->manager->IsInactiveStatus()) {
        if (this->isHost) {
            this->manager->SendStartRacePacket();
            if (this->manager->IsSelectingStatus()) {
                this->manager->SetStatusExternal(ExtendedTeamManager::STATUS_WAITING_POST);
            }
        } else {
            this->manager->SendAckStartRacePacket();
            this->nextPageId = PAGE_CHARACTER_SELECT;
            this->EndStateAnimated(0, this->startRaceButton.GetAnimationFrameSize());
        }

        friendRoomWaiting->countdown.SetInitial(20.0f);
        friendRoomWaiting->countdown.isActive = true;
    }
}

void ExtendedTeamSelect::OnArrowClick(PushButton& button, u32 hudSlotId) {
    u32 id = button.buttonId - 2;
    this->UpdatePlayerTeam(id, static_cast<ExtendedTeamID>((this->manager->GetPlayerTeam(id) + 1) % TEAM_COUNT));
    this->manager->SendUpdateTeamsPacket();
}

void ExtendedTeamSelect::UpdatePlayerTeam(u32 idx, ExtendedTeamID team) {
    this->manager->SetPlayerTeam(idx, team);
    ExtendedTeamSelect::ChangeVRButtonColors(this->teamPlayerControl[idx], team);

    u8 nr, ng, nb;
    ExtendedTeamSelect::GetTeamColor(static_cast<ExtendedTeamID>((team + 1) % TEAM_COUNT), nr, ng, nb);

    nw4r::lyt::Material* arrowBorder = this->teamPlayerArrows[idx].layout.GetPaneByName("border")->GetMaterial();
    arrowBorder->tevColours[0].r = nr;
    arrowBorder->tevColours[0].g = ng;
    arrowBorder->tevColours[0].b = nb;
    arrowBorder->tevColours[0].a = 255;
    arrowBorder->tevColours[1].r = nr;
    arrowBorder->tevColours[1].g = ng;
    arrowBorder->tevColours[1].b = nb;
    arrowBorder->tevColours[1].a = 255;
}

void ExtendedTeamSelect::UpdatePlayerTeamByAID(u8 aid, u8 playerIdOnConsole, ExtendedTeamID team) {
    for (int i = 0; i < 12; ++i) {
        if (this->manager->GetPlayerAID(i, playerIdOnConsole) == aid) {
            this->UpdatePlayerTeam(i, team);
        }
    }
}

const void ExtendedTeamSelect::ChangeVRButtonColors(LayoutUIControl& button, ExtendedTeamID team) {
    button.SetPaneVisibility("blue_null", false);
    button.SetPaneVisibility("red_null", true);

    nw4r::lyt::Pane* teamColorPane1 = button.layout.GetPaneByName("black_parts_r_l");
    nw4r::lyt::Pane* teamColorPane2 = button.layout.GetPaneByName("black_parts_r_r");
    nw4r::lyt::Material* teamColor1 = teamColorPane1->GetMaterial();
    nw4r::lyt::Material* teamColor2 = teamColorPane2->GetMaterial();
    teamColorPane1->alpha = teamColorPane2->alpha = 255;

    u8 r, g, b;
    ExtendedTeamSelect::GetTeamColor(team, r, g, b);

    for (int i = 0; i < 2; i++) {
        teamColor1->tevColours[i].r = teamColor2->tevColours[i].r = r;
        teamColor1->tevColours[i].g = teamColor2->tevColours[i].g = g;
        teamColor1->tevColours[i].b = teamColor2->tevColours[i].b = b;
        teamColor1->tevColours[i].a = teamColor2->tevColours[i].a = 255;
    }
}

struct InternalTeamColor {
    u8 color[3];
    u8 text1[3];  // Lighter color
    u8 text2[3];  // Darker color
};

static const InternalTeamColor TEAM_COLORS[TEAM_COUNT] = {
    {// TEAM_RED
     {255, 0, 0},
     {230, 70, 0},
     {180, 30, 0}},
    {// TEAM_ORANGE
     {255, 165, 0},
     {255, 165, 0},
     {255, 165, 0}},
    {// TEAM_YELLOW
     {255, 255, 0},
     {255, 255, 0},
     {255, 255, 0}},
    {// TEAM_GREEN
     {0, 255, 0},
     {0, 255, 0},
     {0, 255, 0}},
    {// TEAM_BLUE
     {0, 64, 255},
     {0, 180, 255},
     {80, 80, 255}},
    {// TEAM_PURPLE
     {128, 0, 128},
     {128, 0, 128},
     {128, 0, 128}

    }};

void ExtendedTeamSelect::GetTeamColor(ExtendedTeamID team, u8& r, u8& g, u8& b) {
    if (team >= TEAM_COUNT) {
        r = 0;
        g = 0;
        b = 0;
    } else {
        r = TEAM_COLORS[team].color[0];
        g = TEAM_COLORS[team].color[1];
        b = TEAM_COLORS[team].color[2];
    }
}

}  // namespace UI
}  // namespace Pulsar