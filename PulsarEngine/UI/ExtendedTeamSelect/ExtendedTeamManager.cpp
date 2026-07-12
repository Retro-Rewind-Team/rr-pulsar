#include <UI/ExtendedTeamSelect/ExtendedTeamManager.hpp>
#include <UI/ExtendedTeamSelect/ExtendedTeamSelect.hpp>
#include <Settings/Settings.hpp>
#include <MarioKartWii/System/random.hpp>
#include <MarioKartWii/UI/Page/Other/FriendRoom.hpp>
#include <MarioKartWii/UI/Page/Other/SELECTStageMgr.hpp>
#include <Network/Mogi.hpp>

namespace Pulsar {
namespace UI {

ExtendedTeamManager* ExtendedTeamManager::sInstance = nullptr;

static void ResetTeamPlayer(ExtendedTeamPlayer& player, u32 index) {
    player.playerIdx = 0xFF;
    player.miiIdx = 0xFF;
    player.aid = 0xFF;
    player.playerIdOnConsole = 0xFF;
    player.active = 0;
    player.done = 0;
    player.team = static_cast<ExtendedTeamID>(index % TEAM_COUNT);
}

ExtendedTeamManager::ExtendedTeamManager() {
    this->ResetPlayers();
    this->isHost = false;
    this->status = STATUS_NONE;
    this->hasFriendRoomStarted = false;
}

void ExtendedTeamManager::CreateInstance(ExtendedTeamManager* obj) {
    sInstance = obj;
}

void ExtendedTeamManager::DestroyInstance() {
    if (sInstance) {
        sInstance = nullptr;
    }
}

void ExtendedTeamManager::SendStartRacePacket() {
    Pages::FriendRoomManager* friendRoomManager = SectionMgr::sInstance->curSection->Get<Pages::FriendRoomManager>();
    RKNet::Controller* controller = RKNet::Controller::sInstance;
    RKNet::ControllerSub* sub = &controller->subs[controller->currentSub];

    if (sub->localAid == sub->hostAid) {
        friendRoomManager->lastMessageId++;

        RKNet::ROOMPacket packet;
        packet.messageType = MSG_TYPE_START_RACE;
        packet.message = 0;
        packet.unknown_0x3 = friendRoomManager->lastMessageId;

        for (int i = 0; i < 12; ++i) {
            if (i != sub->localAid) {
                RKNet::ROOMHandler::sInstance->toSendPackets[i] = packet;
            }
        }

        friendRoomManager->networkManager.lastSentPacket = packet;
        friendRoomManager->networkManager.localAid = sub->localAid;

        if (this->status == STATUS_SELECTING) {
            this->waitingTimer.SetInitial(2.5f);
            this->waitingTimer.isActive = true;

            this->status = STATUS_WAITING_POST;
        }
    }
}

void ExtendedTeamManager::SendUpdateTeamsPacket() {
    Pages::FriendRoomManager* friendRoomManager = SectionMgr::sInstance->curSection->Get<Pages::FriendRoomManager>();
    RKNet::Controller* controller = RKNet::Controller::sInstance;
    RKNet::ControllerSub* sub = &controller->subs[controller->currentSub];

    if (sub->localAid == sub->hostAid) {
        friendRoomManager->lastMessageId++;

        RKNet::ROOMPacket packet;
        packet.messageType = MSG_TYPE_UPDATE_TEAMS;
        packet.message = 0;
        packet.unknown_0x3 = friendRoomManager->lastMessageId;

        for (int i = 0; i < 12; ++i) {
            if (i != sub->localAid) {
                RKNet::ROOMHandler::sInstance->toSendPackets[i] = packet;
            }
        }

        friendRoomManager->networkManager.lastSentPacket = packet;
        friendRoomManager->networkManager.localAid = sub->localAid;
    }
}

void ExtendedTeamManager::SendPingPacket() {
    Pages::FriendRoomManager* friendRoomManager = SectionMgr::sInstance->curSection->Get<Pages::FriendRoomManager>();
    RKNet::Controller* controller = RKNet::Controller::sInstance;
    RKNet::ControllerSub* sub = &controller->subs[controller->currentSub];

    friendRoomManager->lastMessageId++;

    RKNet::ROOMPacket packet;
    packet.messageType = MSG_TYPE_PING;
    packet.message = 0;
    packet.unknown_0x3 = friendRoomManager->lastMessageId;

    RKNet::ROOMHandler::sInstance->toSendPackets[sub->hostAid] = packet;

    friendRoomManager->networkManager.lastSentPacket = packet;
    friendRoomManager->networkManager.localAid = sub->localAid;
}

void ExtendedTeamManager::SendAckStartRacePacket() {
    Pages::FriendRoomManager* friendRoomManager = SectionMgr::sInstance->curSection->Get<Pages::FriendRoomManager>();
    RKNet::Controller* controller = RKNet::Controller::sInstance;
    RKNet::ControllerSub* sub = &controller->subs[controller->currentSub];

    friendRoomManager->lastMessageId++;

    RKNet::ROOMPacket packet;
    packet.messageType = MSG_TYPE_ACK_START_RACE;
    packet.message = 0;
    packet.unknown_0x3 = friendRoomManager->lastMessageId;

    RKNet::ROOMHandler::sInstance->toSendPackets[sub->hostAid] = packet;

    friendRoomManager->networkManager.lastSentPacket = packet;
    friendRoomManager->networkManager.localAid = sub->localAid;
}

bool ExtendedTeamManager::AreAllOtherPlayersActive(u8 localAid) {
    for (int i = 0; i < 12; i++) {
        if (this->players[i].playerIdx != 0xFF && this->players[i].aid != localAid && !this->players[i].active) {
            return false;
        }
    }
    return true;
}

bool ExtendedTeamManager::AreAllOtherPlayersDone(u8 localAid) {
    for (int i = 0; i < 12; i++) {
        if (this->players[i].playerIdx != 0xFF && this->players[i].aid != localAid && !this->players[i].done) {
            return false;
        }
    }
    return true;
}

void ExtendedTeamManager::Update() {
    Pages::FriendRoomManager* friendRoomManager = SectionMgr::sInstance->curSection->Get<Pages::FriendRoomManager>();
    RKNet::Controller* controller = RKNet::Controller::sInstance;
    const RKNet::ControllerSub& sub = controller->subs[controller->currentSub];
    this->isHost = controller->roomType == RKNet::ROOMTYPE_FROOM_HOST ||
                   (Mogi::IsActive() && Mogi::IsTeamFormat() && sub.localAid == sub.hostAid);
    u8 localAid = sub.localAid;
    if (!this->isHost) {
        if (this->status == STATUS_NONE) {
            if (this->hasFriendRoomStarted) {
                this->status = STATUS_SELECTING;
            }
        } else if (this->status == STATUS_SELECTING) {
            if (!this->lastUpdateTimer.isActive) {
                this->lastUpdateTimer.SetInitial(0.666f);
                this->lastUpdateTimer.isActive = true;

                this->SendPingPacket();
            }
        } else if (this->status == STATUS_DONE) {
            if (!this->lastUpdateTimer.isActive) {
                this->lastUpdateTimer.SetInitial(0.666f);
                this->lastUpdateTimer.isActive = true;

                this->SendAckStartRacePacket();
            }
        }
    } else {
        if (this->status == STATUS_NONE) {
            if (this->hasFriendRoomStarted) {
                this->status = STATUS_WAITING_PRE;
                this->waitingTimer.SetInitial(5.0f);
                this->waitingTimer.isActive = true;
            }
        } else if (this->status == STATUS_WAITING_PRE) {
            if (!this->waitingTimer.isActive) {
                this->status = STATUS_SELECTING;
            } else {
                if (this->AreAllOtherPlayersActive(localAid)) {
                    this->status = STATUS_SELECTING;
                    this->waitingTimer.isActive = false;
                }
            }
        } else if (this->status == STATUS_SELECTING) {
            // ...
        } else if (this->status == STATUS_WAITING_POST) {
            if (!this->waitingTimer.isActive) {
                this->status = STATUS_DONE;
            } else {
                if (this->AreAllOtherPlayersDone(localAid)) {
                    this->status = STATUS_DONE;
                    this->waitingTimer.isActive = false;
                }
            }

            if (!this->lastUpdateTimer.isActive) {
                this->lastUpdateTimer.SetInitial(0.666f);
                this->lastUpdateTimer.isActive = true;

                this->SendStartRacePacket();
            }
        }
    }

    this->lastUpdateTimer.Update();
    this->waitingTimer.Update();
}

void ExtendedTeamManager::VotePageSync() {
    ExtendedTeamPlayer newPlayers[12];
    for (int i = 0; i < 12; i++) {
        ResetTeamPlayer(newPlayers[i], i);
    }

    Pages::SELECTStageMgr* voteMgr = SectionMgr::sInstance->curSection->Get<Pages::SELECTStageMgr>();
    RKNet::Controller* controller = RKNet::Controller::sInstance;

    for (int i = 0; i < 12; i++) {
        u8 aid = controller->aidsBelongingToPlayerIds[i];
        if (aid == 0xFF) {
            continue;
        }

        u8 localPlayerId;
        if (i < 1) {
            localPlayerId = 0;
        } else {
            u8 aid2P = controller->aidsBelongingToPlayerIds[i - 1];
            if (aid != aid2P) {
                localPlayerId = 0;
            } else {
                localPlayerId = 1;
            }
        }

        for (int j = 0; j < 12; j++) {
            if (voteMgr->infos[j].aid == aid && voteMgr->infos[j].hudSlotid == localPlayerId) {
                newPlayers[i].playerIdx = i;
                newPlayers[i].miiIdx = aid * 2 + localPlayerId;
                newPlayers[i].aid = aid;
                newPlayers[i].playerIdOnConsole = localPlayerId;
                newPlayers[i].team = Mogi::IsTeamFormat() ? static_cast<ExtendedTeamID>(Mogi::GetTeamForPlayer(i)) :
                                                            this->GetPlayerTeamByAID(aid, localPlayerId);
                break;
            }
        }
    }

    memcpy(this->players, newPlayers, sizeof(ExtendedTeamPlayer) * 12);
}

void ExtendedTeamManager::ConfigureMogiTeams() {
    RKNet::Controller* controller = RKNet::Controller::sInstance;
    if (controller == nullptr) return;

    const RKNet::ControllerSub& sub = controller->subs[controller->currentSub];
    const u8 playerCount = sub.playerCount < 12 ? sub.playerCount : 12;
    bool hasPlayerIdMapping = true;
    for (u8 i = 0; i < playerCount; ++i) {
        if (controller->aidsBelongingToPlayerIds[i] == 0xFF) {
            hasPlayerIdMapping = false;
            break;
        }
    }

    this->ResetPlayers();
    if (hasPlayerIdMapping) {
        for (u8 i = 0; i < playerCount; ++i) {
            const u8 aid = controller->aidsBelongingToPlayerIds[i];
            const u8 playerIdOnConsole = i > 0 && controller->aidsBelongingToPlayerIds[i - 1] == aid ? 1 : 0;
            this->SetPlayerIndexes(i, aid * 2 + playerIdOnConsole, aid, playerIdOnConsole);
            this->SetPlayerTeam(i, static_cast<ExtendedTeamID>(Mogi::GetTeamForPlayer(i)));
        }
    } else {
        u8 playerIdx = 0;
        for (u8 aid = 0; aid < 12 && playerIdx < playerCount; ++aid) {
            if ((sub.availableAids & (1 << aid)) == 0) continue;

            u8 playersOnAid = sub.connectionUserDatas[aid].playersAtConsole;
            if (aid == sub.localAid && playersOnAid == 0) playersOnAid = sub.localPlayerCount;
            if (playersOnAid == 0) playersOnAid = 1;

            for (u8 playerIdOnConsole = 0; playerIdOnConsole < playersOnAid && playerIdx < playerCount;
                 ++playerIdOnConsole, ++playerIdx) {
                this->SetPlayerIndexes(playerIdx, aid * 2 + playerIdOnConsole, aid, playerIdOnConsole);
                this->SetPlayerTeam(playerIdx, static_cast<ExtendedTeamID>(Mogi::GetTeamForPlayer(playerIdx)));
            }
        }
    }
    ExtendedTeamSelect::RandomizeTeamColors(Mogi::GetLobbySeed());
    this->hasFriendRoomStarted = true;
    OS::Report("[MogiTeams] ConfigureMogiTeams players=%u mapped=%u active=%u teamFormat=%u teams=%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",
               playerCount, hasPlayerIdMapping, Mogi::IsActive(), Mogi::IsTeamFormat(), this->players[0].team, this->players[1].team,
               this->players[2].team, this->players[3].team, this->players[4].team, this->players[5].team,
               this->players[6].team, this->players[7].team, this->players[8].team, this->players[9].team,
               this->players[10].team, this->players[11].team);
}

void ExtendedTeamManager::ConfigureOfflineTeams() {
    RacedataScenario& scenario = Racedata::sInstance->racesScenario;
    const u8 playersSetting = Settings::Mgr::Get().GetUserSettingValue(Settings::SETTINGSTYPE_EXTENDEDTEAMS, SCROLLER_EXTENDEDTEAMSPLAYERS);
    u32 playersPerTeam = 2;
    switch (playersSetting) {
        case EXTENDEDTEAMS_PLAYERS_3:
            playersPerTeam = 3;
            break;
        case EXTENDEDTEAMS_PLAYERS_4:
            playersPerTeam = 4;
            break;
        case EXTENDEDTEAMS_PLAYERS_6:
            playersPerTeam = 6;
            break;
    }

    this->ResetPlayers();

    u32 playerOrder[12];
    for (u32 i = 0; i < scenario.playerCount; ++i) {
        playerOrder[i] = i;
    }

    Random random;
    for (u32 i = scenario.playerCount; i > 1; --i) {
        const u32 other = random.NextLimited(i);
        const u32 player = playerOrder[i - 1];
        playerOrder[i - 1] = playerOrder[other];
        playerOrder[other] = player;
    }

    for (u32 i = 0; i < scenario.playerCount; ++i) {
        this->SetPlayerIndexes(i, playerOrder[i], 0xFF, 0xFF);
        this->SetPlayerTeam(i, static_cast<ExtendedTeamID>(i / playersPerTeam));
    }

    ExtendedTeamSelect::RandomizeTeamColors();
}

void ExtendedTeamManager::Reset() {
    this->status = STATUS_NONE;
    this->isHost = false;
    this->lastUpdateTimer.isActive = false;
    this->waitingTimer.isActive = false;
    this->hasFriendRoomStarted = false;
    ExtendedTeamSelect::ResetTeamColors();
}

void ExtendedTeamManager::ResetPlayers() {
    for (int i = 0; i < 12; i++) {
        ResetTeamPlayer(this->players[i], i);
    }
}

}  // namespace UI
}  // namespace Pulsar
