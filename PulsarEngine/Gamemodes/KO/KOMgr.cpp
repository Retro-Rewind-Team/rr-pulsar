#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <MarioKartWii/UI/Page/Page.hpp>
#include <MarioKartWii/Input/InputManager.hpp>
#include <GameModes/KO/KOMgr.hpp>
#include <Network/PacketExpansion.hpp>
#include <Gamemodes/KO/KORaceEndPage.hpp>
#include <CustomCharacters/CustomCharacters.hpp>
#include <Settings/Settings.hpp>
#include <Settings/SettingsParam.hpp>

namespace Pulsar {
namespace KO {

static const u8 offlineVSRaceCount = 255;

Mgr::Mgr() : winnerPlayerId(0xFF), isSpectating(false), hasSwapped(false), isOfflineVS(false), offlineRaceNumber(0) {
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    const RKNet::ControllerSub& sub = controller->subs[controller->currentSub];
    this->baseLocPlayerCount = sub.localPlayerCount;
    this->isOfflineVS = controller->roomType == RKNet::ROOMTYPE_NONE && Racedata::sInstance->menusScenario.settings.gamemode == MODE_VS_RACE;
    if (this->isOfflineVS) {
        const Settings::Mgr& settings = Settings::Mgr::Get();
        this->koPerRace = settings.GetUserSettingValue(Settings::SETTINGSTYPE_KO, SCROLLER_KOPERRACE) + 1;
        this->racesPerKO = settings.GetUserSettingValue(Settings::SETTINGSTYPE_KO, SCROLLER_RACESPERKO) + 1;
        this->alwaysFinal = settings.GetUserSettingValue(Settings::SETTINGSTYPE_KO, RADIO_KOFINAL) == KOSETTING_FINAL_ALWAYS;
        this->singleRace1v1Final = settings.GetUserSettingValue(Settings::SETTINGSTYPE_KO, RADIO_KO1V1FINALE) == KOSETTING_1V1FINALE_SINGLE;
        const u8 elimThreshold = settings.GetUserSettingValue(Settings::SETTINGSTYPE_KO, SCROLLER_KOELIMTHRESHOLD);
        this->elimThresholdPlayers = elimThreshold == KOSETTING_ELIMTHRESHOLD_DISABLED ? 0 : elimThreshold + 2;
        this->elimChangeCount = settings.GetUserSettingValue(Settings::SETTINGSTYPE_KO, SCROLLER_KOELIMCHANGE) + 1;
        this->baseLocPlayerCount = Racedata::sInstance->menusScenario.localPlayerCount;
        this->ForceOfflineVSRaceCount();
    }
    for (int aid = 0; aid < 12; ++aid) {
        this->status[aid][0] = NORMAL;
        this->status[aid][1] = NORMAL;
    }
    this->ResetRace();
}

bool Mgr::IsOfflineVS() const {
    return this->isOfflineVS;
}

void Mgr::ForceOfflineVSRaceCount() const {
    if (this->isOfflineVS) SectionMgr::sInstance->sectionParams->vsRaceCount = offlineVSRaceCount;
}

u32 Mgr::GetCurrentRaceNumber() const {
    const SectionParams* params = SectionMgr::sInstance->sectionParams;
    return this->isOfflineVS ? this->offlineRaceNumber + 1 : params->onlineParams.currentRaceNumber + 1;
}

void Mgr::AdvanceOfflineRaceNumber() {
    if (this->isOfflineVS) ++this->offlineRaceNumber;
}

void Mgr::FinishOfflineVSIfAllLocalPlayersAreOut() {
    if (!this->isOfflineVS) return;

    const RacedataScenario& scenario = Racedata::sInstance->racesScenario;
    bool hasLocalPlayer = false;
    for (u8 playerId = 0; playerId < scenario.playerCount; ++playerId) {
        if (scenario.players[playerId].playerType != PLAYER_REAL_LOCAL) continue;
        hasLocalPlayer = true;
        if (!this->IsKOdPlayerId(playerId)) return;
    }

    if (hasLocalPlayer) {
        SectionParams* params = SectionMgr::sInstance->sectionParams;
        params->vsRaceNumber = params->vsRaceCount;
    }
}

void Mgr::PrepareOfflineVSNextRace() {
    if (!this->isOfflineVS || this->winnerPlayerId != 0xFF) return;

    RacedataScenario& scenario = Racedata::sInstance->menusScenario;
    u8 remainingCount = 0;
    for (u8 playerId = 0; playerId < scenario.playerCount; ++playerId) {
        if (!this->IsKOdPlayerId(playerId)) {
            if (remainingCount != playerId) {
                CustomCharacters::CompactOfflineCpuSkinTable(remainingCount, playerId);
                memcpy(&scenario.players[remainingCount], &scenario.players[playerId], sizeof(RacedataPlayer));
            }
            ++remainingCount;
        }
    }
    if (remainingCount == scenario.playerCount) return;
    for (u8 playerId = remainingCount; playerId < 12; ++playerId) {
        scenario.players[playerId].playerType = PLAYER_NONE;
    }
    for (u8 playerId = 0; playerId < remainingCount; ++playerId) {
        scenario.players[playerId].SetPrevFinishPos(playerId + 1);
    }
    scenario.ComputePlayerCounts(&scenario.playerCount, &scenario.screenCount, &scenario.localPlayerCount);
    for (u8 playerId = 0; playerId < 12; ++playerId) {
        this->status[playerId][0] = NORMAL;
        this->status[playerId][1] = NORMAL;
    }
}
Mgr::~Mgr() {
    RKNet::Controller* controller = RKNet::Controller::sInstance;
    controller->subs[0].localPlayerCount = this->baseLocPlayerCount;
    controller->subs[1].localPlayerCount = this->baseLocPlayerCount;
    if (this->GetIsSwapped()) this->SwapControllersAndUI();
}

bool Mgr::Is1v1KoRace(u32 currentRaceNumber) const {
    if (this->singleRace1v1Final) return true;
    return currentRaceNumber % this->racesPerKO == 0;
}

u8 Mgr::GetRoundKoCount(u8 playerCount) const {
    u8 koCount = this->koPerRace;

    if (this->elimThresholdPlayers != 0 && playerCount <= this->elimThresholdPlayers) {
        koCount = this->elimChangeCount;
    }

    if (playerCount - koCount < 2 && this->alwaysFinal) {
        koCount = playerCount - 2;
    }

    if (this->koPerRace >= 2 && this->alwaysFinal && playerCount > 2) {
        if (playerCount == 3) {
            koCount = 1;
        } else if (playerCount == 4 && this->koPerRace >= 2) {
            koCount = 2;
        }
    } else {
        if (playerCount == 3 && this->koPerRace >= 3) {
            koCount = 2;
        } else if (playerCount == 4 && this->koPerRace >= 4) {
            koCount = 3;
        }
    }

    return koCount;
}

void Mgr::AddRaceStats() {
    const RacedataScenario& scenario = Racedata::sInstance->racesScenario;
    const u8 localPlayerCount = this->isOfflineVS ? 0 : scenario.localPlayerCount;
    for (int hudSlot = 0; hudSlot < localPlayerCount; ++hudSlot) {
        Stats& stats = this->stats[hudSlot];
        if (stats.boolCountArray >= arbitraryAlmostDied) ++stats.final.almostKOdCounter;
        const u8 pos = Raceinfo::sInstance->players[scenario.settings.hudPlayerIds[hudSlot]]->position;
        stats.percentageSum += static_cast<float>(pos) / static_cast<float>(System::sInstance->nonTTGhostPlayersCount);  // this allows higher precision across multiple races
        stats.final.finalPercentageSum = static_cast<u8>(stats.percentageSum * 100);
    }

    this->ResetRace();
}

void Mgr::CalcWouldBeKnockedOut() {
    const Raceinfo* raceInfo = Raceinfo::sInstance;
    const u8 playerCount = System::sInstance->nonTTGhostPlayersCount;
    const RacedataScenario& scenario = Racedata::sInstance->menusScenario;
    const u8* pointsArray = &Racedata::pointsRoom[playerCount - 1][0];

    PlayerPosition players[12];
    for (u8 curPlayerId = 0; curPlayerId < playerCount; ++curPlayerId) {
        this->wouldBeOut[curPlayerId] = false;
        players[curPlayerId].playerId = curPlayerId;

        if (this->racesPerKO > 1) {
            const u8 wouldBePoints = pointsArray[raceInfo->players[curPlayerId]->position - 1];
            players[curPlayerId].position = scenario.players[curPlayerId].score + wouldBePoints;
        } else {
            players[curPlayerId].position = raceInfo->players[curPlayerId]->position;
        }
    }

    const u32 currentRaceCount = this->GetCurrentRaceNumber();

    if (playerCount == 2) {
        if (!this->Is1v1KoRace(currentRaceCount)) return;

        if (this->racesPerKO > 1) {
            this->wouldBeOut[players[0].playerId] = players[0].position < players[1].position;
            this->wouldBeOut[players[1].playerId] = players[1].position < players[0].position;
        } else {
            for (int i = 0; i < playerCount; ++i) {
                this->wouldBeOut[i] = (raceInfo->players[i]->position != 1);
            }
        }
        return;
    }

    const bool isKoRace = currentRaceCount % this->racesPerKO == 0;
    if (!isKoRace) return;

    s32 roundKOs = this->GetRoundKoCount(playerCount);

    qsort(players, playerCount, sizeof(PlayerPosition),
          reinterpret_cast<int (*)(const void*, const void*)>(SortPlayersByPosition));

    s32 assignedKOs = 0;
    if (racesPerKO > 1) {
        for (s32 idx = 0; idx < playerCount && assignedKOs < roundKOs; ++idx) {
            if (this->racesPerKO > 1 && idx == playerCount - 1) {
                continue;
            }
            this->wouldBeOut[players[idx].playerId] = true;
            ++assignedKOs;
        }
    } else if (racesPerKO == 1) {
        for (s32 idx = playerCount - 1; idx >= 0 && assignedKOs < roundKOs; --idx) {
            if (this->racesPerKO == 1 && raceInfo->players[players[idx].playerId]->position == 1) {
                continue;
            }
            this->wouldBeOut[players[idx].playerId] = true;
            ++assignedKOs;
        }
    }
}

void Mgr::ProcessKOs(Pages::GPVSLeaderboardUpdate::Player* playerArr, size_t nitems, size_t size, int (*compar)(const void*, const void*)) {
    qsort(playerArr, nitems, size, compar);

    const System* system = System::sInstance;
    if (!system->IsContext(PULSAR_MODE_KO)) {
        return;
    }

    Mgr* self = system->koMgr;
    RacedataScenario& scenario = Racedata::sInstance->menusScenario;
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    const RKNet::ControllerSub& sub = controller->subs[controller->currentSub];
    const Raceinfo* raceinfo = Raceinfo::sInstance;
    const u8 playerCount = system->nonTTGhostPlayersCount;
    self->alwaysFinal = System::sInstance->IsContext(PULSAR_KOFINAL);
    SectionParams* sectionParams = SectionMgr::sInstance->sectionParams;
    const u32 currentRaceNumber = self->GetCurrentRaceNumber();
    bool hasTies = false;

    u8 disconnectedKOs = 0;
    for (int playerId = 0; !self->IsOfflineVS() && playerId < playerCount; ++playerId) {
        const u8 aid = controller->aidsBelongingToPlayerIds[playerId];
        if (aid >= 12) continue;

        if (((1 << aid) & sub.availableAids) == 0) {
            if (!self->IsDisconnectedPlayerId(playerId)) {
                self->SetDisconnected(playerId);
            }
            ++disconnectedKOs;
        }
    }

    u8 koCount = self->GetRoundKoCount(playerCount);

    const bool isKoRace = currentRaceNumber % self->racesPerKO == 0;
    const bool is1v1KoRace = playerCount == 2 && self->Is1v1KoRace(currentRaceNumber);
    const bool isCompletedKoRace = isKoRace && koCount > 0;
    if (isKoRace) {
        if (disconnectedKOs >= koCount)
            koCount = 0;
        else
            koCount -= disconnectedKOs;
    }

    if (is1v1KoRace || (playerCount - disconnectedKOs) == 1) {
        if (is1v1KoRace && self->racesPerKO > 1) {
            if (playerArr[0].totalScore == playerArr[1].totalScore) {
                self->SetTie(playerArr[0].playerId, playerArr[1].playerId);
                if (self->IsOfflineVS())
                    --sectionParams->vsRaceNumber;
                else
                    --sectionParams->onlineParams.currentRaceNumber;
                self->AddRaceStats();
                return;
            }
            self->winnerPlayerId = playerArr[0].playerId;
            self->SetKOd(playerArr[1].playerId);
        } else {
            self->winnerPlayerId = raceinfo->playerIdInEachPosition[0];
            self->SetKOd(raceinfo->playerIdInEachPosition[1]);
        }
        self->AddRaceStats();
        self->FinishOfflineVSIfAllLocalPlayersAreOut();
        if (self->winnerPlayerId != 0xFF && self->IsOfflineVS()) sectionParams->vsRaceNumber = sectionParams->vsRaceCount;
        self->AdvanceOfflineRaceNumber();
        return;
    }

    if (!isCompletedKoRace) {
        self->AddRaceStats();
        self->AdvanceOfflineRaceNumber();
        return;
    }

    if (self->racesPerKO > 1) {
        u32 koThresholdPosition = playerCount - koCount;
        u32 tieScore = playerArr[koThresholdPosition].totalScore;

        int tiedPlayersCount = 0;
        int playersInKOPosition = 0;
        int playersNotInKOPosition = 0;

        for (int position = 0; position < playerCount; ++position) {
            if (playerArr[position].totalScore == tieScore) {
                ++tiedPlayersCount;
                if (position >= koThresholdPosition) {
                    ++playersInKOPosition;
                } else {
                    ++playersNotInKOPosition;
                }
            }
        }

        if (playersInKOPosition > 0 && playersNotInKOPosition > 0) {
            for (int position = 0; position < playerCount; ++position) {
                if (playerArr[position].totalScore == tieScore) {
                    self->SetTie(playerArr[position].playerId, playerArr[koThresholdPosition].playerId);
                    hasTies = true;
                }
            }
            if (hasTies) {
                if (self->IsOfflineVS())
                    --sectionParams->vsRaceNumber;
                else
                    --sectionParams->onlineParams.currentRaceNumber;
                koCount = 0;
            }
        } else if (tiedPlayersCount == koCount) {
            for (int position = 0; position < playerCount; ++position) {
                if (playerArr[position].totalScore == tieScore) {
                    self->SetKOd(playerArr[position].playerId);
                }
            }
        }

        if (!hasTies && isKoRace) {
            for (int idx = 0; idx < 12; ++idx) {
                scenario.players[idx].score = 0;
                scenario.players[idx].previousScore = 0;
            }
        }
    }

    if (koCount > 0) {
        for (int idx = 0; idx < koCount; ++idx) {
            u8 playerId;
            u32 position = (playerCount - 1) - idx;

            if (self->racesPerKO == 1) {
                playerId = raceinfo->playerIdInEachPosition[position];
            } else {
                playerId = playerArr[position].playerId;

                if (playerCount > 2 && playerId == self->winnerPlayerId) {
                    continue;
                }
            }

            if (self->IsKOdPlayerId(playerId) || self->IsDisconnectedPlayerId(playerId)) {
                continue;
            }

            self->SetKOd(playerId);
        }
    }

    int notKOdCount = 0;
    u8 potentialWinner = 0xFF;
    for (int playerId = 0; playerId < playerCount; ++playerId) {
        if (!self->IsKOdPlayerId(playerId) && !self->IsDisconnectedPlayerId(playerId)) {
            ++notKOdCount;
            potentialWinner = playerId;
        }
    }

    if (notKOdCount == 1) {
        self->winnerPlayerId = potentialWinner;
    }

    self->AddRaceStats();
    self->FinishOfflineVSIfAllLocalPlayersAreOut();
    if (self->winnerPlayerId != 0xFF && self->IsOfflineVS()) sectionParams->vsRaceNumber = sectionParams->vsRaceCount;
    if (!hasTies) self->AdvanceOfflineRaceNumber();
}
kmCall(0x8085cb94, Mgr::ProcessKOs);

void Mgr::Update() {
    const System* system = System::sInstance;
    if (system->IsContext(PULSAR_MODE_KO)) {
        Mgr* self = System::sInstance->koMgr;
        self->CalcWouldBeKnockedOut();
        const RacedataScenario& scenario = Racedata::sInstance->racesScenario;
        const u8 localPlayerCount = self->IsOfflineVS() ? 0 : scenario.localPlayerCount;
        for (int hudSlot = 0; hudSlot < localPlayerCount; ++hudSlot) {
            const bool wouldBeOut = self->GetWouldBeKnockedOut(scenario.settings.hudPlayerIds[hudSlot]);
            const u32 idx = Raceinfo::sInstance->raceFrames % 300;

            Stats& stats = self->stats[hudSlot];
            if (wouldBeOut) ++stats.final.timeInDanger;
            if (!stats.isInDangerFrames[idx] && wouldBeOut)
                ++stats.boolCountArray;
            else if (stats.isInDangerFrames[idx] && !wouldBeOut)
                --stats.boolCountArray;
            stats.isInDangerFrames[idx] = wouldBeOut;
        }

        const u8 winnerPlayerId = self->winnerPlayerId;
        if (winnerPlayerId != 0xFF && !self->IsOfflineVS()) {
            const RKNet::Controller* controller = RKNet::Controller::sInstance;
            const RKNet::ControllerSub& sub = controller->subs[controller->currentSub];
            if (controller->aidsBelongingToPlayerIds[winnerPlayerId] == sub.localAid) {
                for (int aid = 0; aid < 12; ++aid) {
                    if (((1 << aid) & sub.availableAids) == 0 || aid == sub.localAid) continue;

                    Stats& stats = self->stats[0];
                    RKNet::PacketHolder<Network::PulRH1>* holder = controller->GetSendPacketHolder<Network::PulRH1>(aid);

                    Network::PulRH1* dest = holder->packet;
                    dest->timeInDanger = stats.final.timeInDanger;
                    dest->almostKOdCounter = stats.final.almostKOdCounter;
                    dest->finalPercentageSum = stats.final.finalPercentageSum;
                }
            }
        }
    }
}
RaceFrameHook koUpdate(Mgr::Update);

void Mgr::PatchAids(RKNet::ControllerSub& sub) const {
    u32 availableAids = sub.availableAids;
    const u8 localAid = sub.localAid;
    for (u8 aid = 0; aid < 12; ++aid) {
        bool isConsoleOut = false;
        const bool isMainOut = this->IsKOdAid(aid, 0) || this->IsDisconnectedAid(aid, 0);
        u8 aidPlayerCount = aid == localAid ? sub.localPlayerCount : sub.connectionUserDatas[aid].playersAtConsole;

        if (aidPlayerCount <= 1)
            isConsoleOut = isMainOut;
        else if (aidPlayerCount == 2) {
            const bool isGuestOut = this->IsKOdAid(aid, 1) || this->IsDisconnectedAid(aid, 1);
            if (isMainOut && isGuestOut)
                isConsoleOut = true;
            else if (isMainOut != isGuestOut) {
                aidPlayerCount = 1;
            }
        }
        if (isConsoleOut) {
            availableAids = availableAids & ~(1 << aid);
            aidPlayerCount = 0;
        }

        if (aid == localAid)
            sub.localPlayerCount = aidPlayerCount;
        else
            sub.connectionUserDatas[aid].playersAtConsole = aidPlayerCount;
    }
    sub.availableAids = availableAids;
}

u32 Mgr::GetAidAndSlotFromPlayerId(u8 playerId) const {
    if (this->isOfflineVS) return playerId;
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    const RKNet::ControllerSub& sub = controller->subs[controller->currentSub];
    const u8 aid = controller->aidsBelongingToPlayerIds[playerId];
    const u8 localAid = sub.localAid;
    u8 slot = 0;

    if ((aid == localAid && sub.localPlayerCount == 2) || (aid != localAid && sub.connectionUserDatas[aid].playersAtConsole == 2)) {
        if (playerId > 0 && controller->aidsBelongingToPlayerIds[playerId - 1] == aid)
            slot = 1;
        else if (playerId < 11 && this->status[aid][0] == KOD && controller->aidsBelongingToPlayerIds[playerId + 1] != aid)
            slot = 1;
    }
    return (slot << 16) | aid;  // 10001
}

SectionId Mgr::GetSectionAfterKO(SectionId defaultId) const {
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    const RKNet::ControllerSub& sub = controller->subs[controller->currentSub];
    if (this->baseLocPlayerCount == 2) {
        if (this->IsKOdAid(sub.localAid, 0) != this->IsKOdAid(sub.localAid, 1)) {
            if (defaultId == SECTION_P2_WIFI_FROOM_VS_VOTING)
                defaultId = SECTION_P1_WIFI_FROOM_VS_VOTING;  // select section
            else if (defaultId == SECTION_P1_WIFI_FROM_FROOM_RACE || defaultId == SECTION_P2_WIFI_FROM_FROOM_RACE) {
                defaultId = SECTION_P2_WIFI_FROM_FROOM_RACE;  // after room section
                SectionMgr::sInstance->sectionParams->localPlayerCount = 2;
            }
        }
    }
    return defaultId;
}

void OnDisconnectKO(SectionMgr* sectionMgr, SectionId id) {
    const System* system = System::sInstance;
    if (system->IsContext(PULSAR_MODE_KO)) id = system->koMgr->GetSectionAfterKO(id);
    sectionMgr->SetNextSection(id, 0);
}
kmCall(0x80651814, OnDisconnectKO);

PageId Mgr::KickPlayersOut(PageId defaultId) {  // only called if KOMode

    PageId ret = defaultId;
    const System* system = System::sInstance;

    Mgr* mgr = system->koMgr;
    RacedataScenario& scenario = Racedata::sInstance->racesScenario;
    const bool isMainOut = mgr->IsKOdPlayerId(scenario.settings.hudPlayerIds[0]) || mgr->IsDisconnectedPlayerId(scenario.settings.hudPlayerIds[0]);
    if (system->nonTTGhostPlayersCount > 2) {
        if (scenario.localPlayerCount == 1) {
            const RKNet::Controller* controller = RKNet::Controller::sInstance;
            const RKNet::ControllerSub& sub = controller->subs[controller->currentSub];
            if (isMainOut) {
                if (sub.localAid == sub.hostAid)
                    mgr->isSpectating = true;  // force the host to spectate, they should not be allowed to quit
                else
                    ret = static_cast<PageId>(RaceEndPage::id);
            }
        } else {
            const bool isGuestOut = mgr->IsKOdPlayerId(scenario.settings.hudPlayerIds[1]) || mgr->IsDisconnectedPlayerId(scenario.settings.hudPlayerIds[1]);
            if (isMainOut != isGuestOut) SectionMgr::sInstance->sectionParams->localPlayerCount = 1;
            if (isMainOut && !isGuestOut) {
                memcpy(&mgr->stats[0], &mgr->stats[1], sizeof(Mgr::Stats));
            } else if (isMainOut && isGuestOut)
                ret = static_cast<PageId>(RaceEndPage::id);
        }
    }
    return ret;
}

void Mgr::SwapControllersAndUI() {
    Input::Manager* input = Input::Manager::sInstance;

    char mainController[sizeof(Input::RealControllerHolder)];
    memcpy(&mainController, &input->realControllerHolders[0], sizeof(Input::RealControllerHolder));
    memcpy(&input->realControllerHolders[0], &input->realControllerHolders[1], sizeof(Input::RealControllerHolder));
    memcpy(&input->realControllerHolders[1], &mainController, sizeof(Input::RealControllerHolder));

    SectionMgr* sectionMgr = SectionMgr::sInstance;
    SectionPad& pad = sectionMgr->pad;
    PadInfo& main = pad.padInfos[0];
    PadInfo& guest = pad.padInfos[1];
    u32 old = main.controllerID;
    u32 oldg = guest.controllerID;
    main.controllerID = oldg;
    guest.controllerID = old;
    main.controllerIDActive = main.controllerID;
    guest.controllerIDActive = guest.controllerID;

    SectionParams* params = sectionMgr->sectionParams;
    CharacterId mainChar = params->characters[0];
    KartId mainKart = params->karts[0];
    params->characters[0] = params->characters[1];
    params->karts[0] = params->karts[1];
    params->characters[1] = mainChar;
    params->karts[1] = mainKart;
    memcpy(&params->combos[0], &params->combos[1], sizeof(PlayerCombo));
    this->hasSwapped = !this->hasSwapped;
}
}  // namespace KO
}  // namespace Pulsar
