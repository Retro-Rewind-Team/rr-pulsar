#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <MarioKartWii/UI/Page/Page.hpp>
#include <MarioKartWii/Input/InputManager.hpp>
#include <GameModes/KO/KOMgr.hpp>
#include <Network/PacketExpansion.hpp>
#include <Gamemodes/KO/KORaceEndPage.hpp>
#include <Settings/SettingsParam.hpp>

namespace Pulsar
{
    namespace KO
    {

        Mgr::Mgr() : winnerPlayerId(0xFF), isSpectating(false), hasSwapped(false) /*, stillInCount(playerCount)*/
        {
            const RKNet::Controller *controller = RKNet::Controller::sInstance;
            const RKNet::ControllerSub &sub = controller->subs[controller->currentSub];
            this->baseLocPlayerCount = sub.localPlayerCount;
            for (int aid = 0; aid < 12; ++aid)
            {
                this->status[aid][0] = NORMAL;
                this->status[aid][1] = NORMAL;
            }
            this->ResetRace();
        }
        Mgr::~Mgr()
        {
            RKNet::Controller *controller = RKNet::Controller::sInstance;
            controller->subs[0].localPlayerCount = this->baseLocPlayerCount;
            controller->subs[1].localPlayerCount = this->baseLocPlayerCount;
            if (this->GetIsSwapped())
                this->SwapControllersAndUI();
        }

        void Mgr::AddRaceStats()
        { // SHOULD ONLY BE CALLED AFTER PROCESSKOS
            // CALC THE STATS
            const RacedataScenario &scenario = Racedata::sInstance->racesScenario;
            for (int hudSlot = 0; hudSlot < scenario.localPlayerCount; ++hudSlot)
            {
                Stats &stats = this->stats[hudSlot];
                if (stats.boolCountArray >= arbitraryAlmostDied)
                    ++stats.final.almostKOdCounter;
                const u8 pos = Raceinfo::sInstance->players[scenario.settings.hudPlayerIds[hudSlot]]->position;
                stats.percentageSum += static_cast<float>(pos) / static_cast<float>(System::sInstance->nonTTGhostPlayersCount); // this allows higher precision across multiple races
                stats.final.finalPercentageSum = static_cast<u8>(stats.percentageSum * 100);
            }

            this->ResetRace();
        }

        void Mgr::CalcWouldBeKnockedOut()
        {
            const Raceinfo *raceInfo = Raceinfo::sInstance;
            const u8 playerCount = System::sInstance->nonTTGhostPlayersCount;
            const RacedataScenario &scenario = Racedata::sInstance->menusScenario;
            const u8 *pointsArray = &Racedata::pointsRoom[playerCount - 1][0];

            // Initialize player positions array
            PlayerPosition players[12];
            for (u8 curPlayerId = 0; curPlayerId < playerCount; ++curPlayerId)
            {
                this->wouldBeOut[curPlayerId] = false;
                players[curPlayerId].playerId = curPlayerId;

                // Calculate position/points based on race mode
                if (this->racesPerKO > 1)
                {
                    const u8 wouldBePoints = pointsArray[raceInfo->players[curPlayerId]->position - 1];
                    players[curPlayerId].position = scenario.players[curPlayerId].score + wouldBePoints;
                }
                else
                {
                    players[curPlayerId].position = raceInfo->players[curPlayerId]->position;
                }
            }

            // Special handling for 1v1 finals
            if (playerCount == 2)
            {
                for (int i = 0; i < playerCount; ++i)
                {
                    this->wouldBeOut[i] = (raceInfo->players[i]->position != 1);
                }
                return;
            }

            // Check if this is a KO race
            const bool force1v1Final = System::sInstance->IsContext(Pulsar::PULSAR_KOFINAL) == KOSETTING_FINAL_ALWAYS;
            const u32 currentRaceCount = SectionMgr::sInstance->sectionParams->onlineParams.currentRaceNumber + 1;
            const bool isKoRace = currentRaceCount % this->racesPerKO == 0;

            if (isKoRace)
            {
                // Calculate how many players to KO
                s32 roundKOs = this->koPerRace;
                const s32 remainingPlayersAfter = playerCount - roundKOs;

                if (remainingPlayersAfter < 2 && force1v1Final)
                {
                    roundKOs = playerCount - 2;
                }

                // Sort players by score/position (ascending)
                qsort(players, playerCount, sizeof(PlayerPosition),
                      reinterpret_cast<int (*)(const void *, const void *)>(SortPlayersByPosition));

                // Mark players for KO starting with lowest scores
                s32 assignedKOs = 0;
                if (racesPerKO > 1)
                {
                    for (s32 idx = 0; idx < playerCount && assignedKOs < roundKOs; ++idx)
                    {
                        if (this->racesPerKO > 1 && idx == playerCount - 1)
                        {
                            continue;
                        }
                        this->wouldBeOut[players[idx].playerId] = true;
                        ++assignedKOs;
                    }
                }
                else if (racesPerKO == 1)
                {
                    for (s32 idx = playerCount - 1; idx >= 0 && assignedKOs < roundKOs; --idx)
                    {
                        if (this->racesPerKO == 1 && raceInfo->players[players[idx].playerId]->position == 1)
                        {
                            continue;
                        }
                        this->wouldBeOut[players[idx].playerId] = true;
                        ++assignedKOs;
                    }
                }
            }
        }

        void Mgr::ProcessKOs(Pages::GPVSLeaderboardUpdate::Player *playerArr, size_t nitems, size_t size, int (*compar)(const void *, const void *))
        {
            // Sort players by default sorting function
            qsort(playerArr, nitems, size, compar);

            // Only process if we're in KO mode
            const System *system = System::sInstance;
            if (!system->IsContext(PULSAR_MODE_KO))
            {
                return;
            }

            Mgr *self = system->koMgr;
            RacedataScenario &scenario = Racedata::sInstance->menusScenario;
            const RKNet::Controller *controller = RKNet::Controller::sInstance;
            const RKNet::ControllerSub &sub = controller->subs[controller->currentSub];
            const Raceinfo *raceinfo = Raceinfo::sInstance;
            const u8 playerCount = system->nonTTGhostPlayersCount;
            self->alwaysFinal = System::sInstance->IsContext(PULSAR_KOFINAL);
            SectionParams *sectionParams = SectionMgr::sInstance->sectionParams;
            const u32 currentRaceNumber = sectionParams->onlineParams.currentRaceNumber + 1;
            bool hasTies = false;

            // Handle disconnected players first
            u8 disconnectedKOs = 0;
            for (int playerId = 0; playerId < playerCount; ++playerId)
            {
                const u8 aid = controller->aidsBelongingToPlayerIds[playerId];
                if (aid >= 12)
                    continue;

                if ((1 << aid & sub.availableAids) == 0)
                {
                    self->SetDisconnected(playerId);
                    ++disconnectedKOs;
                }
            }

            // Calculate how many players to eliminate
            u8 koCount = self->koPerRace;

            // Adjust KO count if we'd end up with fewer than 2 players and alwaysFinal is on
            if (playerCount - koCount < 2 && self->alwaysFinal)
            {
                koCount = playerCount - 2;
            }

            // Special cases for small player counts
            if (self->koPerRace >= 2 && self->alwaysFinal && playerCount > 2)
            {
                if (playerCount == 3)
                {
                    koCount = 1;
                }
                else if (playerCount == 4 && self->koPerRace >= 2)
                {
                    koCount = 2;
                }
            }
            else
            {
                if (playerCount == 3 && self->koPerRace >= 3)
                {
                    koCount = 2;
                }
                else if (playerCount == 4 && self->koPerRace >= 4)
                {
                    koCount = 3;
                }
            }

            // Handle 1v1 finals or near-end scenarios
            if (playerCount == 2 || (playerCount - disconnectedKOs) == 1)
            {
                // In finals or when only one player remains due to disconnects
                self->winnerPlayerId = raceinfo->playerIdInEachPosition[0];
                self->SetKOd(raceinfo->playerIdInEachPosition[1]);
                self->AddRaceStats();
                return;
            }

            // Check if this is a KO race
            const bool isKoRace = currentRaceNumber % self->racesPerKO == 0;
            if (!isKoRace || koCount <= 0)
            {
                self->AddRaceStats();
                return;
            }

            // Handle KO logic based on race mode
            if (self->racesPerKO > 1)
            {
                // Multi-race KO: Eliminate based on total score

                // Check for ties at the KO threshold
                u32 koThresholdPosition = playerCount - koCount;
                u32 tieScore = playerArr[koThresholdPosition].totalScore;

                // Count players involved in the tie
                int tiedPlayersCount = 0;
                int playersInKOPosition = 0;
                int playersNotInKOPosition = 0;

                for (int position = 0; position < playerCount; ++position)
                {
                    if (playerArr[position].totalScore == tieScore)
                    {
                        ++tiedPlayersCount;
                        if (position >= koThresholdPosition)
                        {
                            ++playersInKOPosition;
                        }
                        else
                        {
                            ++playersNotInKOPosition;
                        }
                    }
                }

                // Handle tie resolution
                if (playersInKOPosition > 0 && playersNotInKOPosition > 0)
                {
                    // Tie spans across the KO threshold - no eliminations, repeat race
                    for (int position = 0; position < playerCount; ++position)
                    {
                        if (playerArr[position].totalScore == tieScore)
                        {
                            self->SetTie(playerArr[position].playerId, playerArr[koThresholdPosition].playerId);
                            hasTies = true;
                        }
                    }
                    if (hasTies)
                    {
                        // Decrement race number to repeat
                        --sectionParams->onlineParams.currentRaceNumber;
                        koCount = 0; // Don't eliminate anyone this round
                    }
                }
                else if (tiedPlayersCount == koCount)
                {
                    // All tied players would be KO'd - eliminate them all
                    for (int position = 0; position < playerCount; ++position)
                    {
                        if (playerArr[position].totalScore == tieScore)
                        {
                            self->SetKOd(playerArr[position].playerId);
                        }
                    }
                }

                // Reset scores after KO round (if no ties and multi-race KO)
                if (!hasTies && isKoRace)
                {
                    for (int idx = 0; idx < 12; ++idx)
                    {
                        scenario.players[idx].score = 0;
                        scenario.players[idx].previousScore = 0;
                    }
                }
            }

            // Eliminate players based on position or score
            if (koCount > 0)
            {
                for (int idx = 0; idx < koCount; ++idx)
                {
                    u8 playerId;
                    u32 position = (playerCount - 1) - idx;

                    if (self->racesPerKO == 1)
                    {
                        // Single race KO: Eliminate based on race position
                        playerId = raceinfo->playerIdInEachPosition[position];
                    }
                    else
                    {
                        // Multi-race KO: Eliminate based on total score
                        playerId = playerArr[position].playerId;

                        // Don't eliminate the winner in multi-race mode with more than 2 players
                        if (playerCount > 2 && playerId == self->winnerPlayerId)
                        {
                            continue;
                        }
                    }

                    // Skip already KO'd players
                    if (self->IsKOdPlayerId(playerId))
                    {
                        continue;
                    }

                    self->SetKOd(playerId);
                }
            }

            // Check for overall winner
            int notKOdCount = 0;
            u8 potentialWinner = 0xFF;
            for (int playerId = 0; playerId < playerCount; ++playerId)
            {
                if (!self->IsKOdPlayerId(playerId))
                {
                    ++notKOdCount;
                    potentialWinner = playerId;
                }
            }

            if (notKOdCount == 1)
            {
                self->winnerPlayerId = potentialWinner;
            }

            self->AddRaceStats();
        }
        kmCall(0x8085cb94, Mgr::ProcessKOs);

        void Mgr::Update()
        {
            const System *system = System::sInstance;
            if (system->IsContext(PULSAR_MODE_KO))
            {
                Mgr *self = System::sInstance->koMgr;
                self->CalcWouldBeKnockedOut();
                const RacedataScenario &scenario = Racedata::sInstance->racesScenario;
                for (int hudSlot = 0; hudSlot < scenario.localPlayerCount; ++hudSlot)
                {
                    const bool wouldBeOut = self->GetWouldBeKnockedOut(scenario.settings.hudPlayerIds[hudSlot]);
                    const u32 idx = Raceinfo::sInstance->raceFrames % 300;

                    Stats &stats = self->stats[hudSlot];
                    if (wouldBeOut)
                        ++stats.final.timeInDanger;
                    if (stats.isInDangerFrames[idx] == false && wouldBeOut == true)
                        ++stats.boolCountArray;
                    else if (stats.isInDangerFrames[idx] == true && wouldBeOut == false)
                        --stats.boolCountArray;
                    stats.isInDangerFrames[idx] = wouldBeOut;
                }

                const u8 winnerPlayerId = self->winnerPlayerId;
                if (winnerPlayerId != 0xFF)
                { // if the if is taken, ProcessKOs and therefore AddRaceStats are guaranteed to have been called
                    const RKNet::Controller *controller = RKNet::Controller::sInstance;
                    const RKNet::ControllerSub &sub = controller->subs[controller->currentSub];
                    if (controller->aidsBelongingToPlayerIds[winnerPlayerId] == sub.localAid)
                    { // only send the data if needed
                        for (int aid = 0; aid < 12; ++aid)
                        {
                            if ((1 << aid & sub.availableAids) == 0 || aid == sub.localAid)
                                continue;

                            Stats &stats = self->stats[0];
                            RKNet::PacketHolder<Network::PulRH1> *holder = controller->GetSendPacketHolder<Network::PulRH1>(aid);

                            /*
                            this shouldn't be needed because the "ExportRH1ToPulRH1" always does it
                            const u32 curSize = holder->packetSize;/
                            u32 addedSize = 0;
                            if(curSize == sizeof(RKNet::RACEHEADER1Packet)) addedSize = sizeof(Network::PulRH1) - sizeof(RKNet::RACEHEADER1Packet);
                            else if(holder->packetSize == 0) addedSize = sizeof(Network::PulRH1);
                            holder->packetSize += addedSize;
                            */
                            Network::PulRH1 *dest = holder->packet;
                            dest->timeInDanger = stats.final.timeInDanger;
                            dest->almostKOdCounter = stats.final.almostKOdCounter;
                            dest->finalPercentageSum = stats.final.finalPercentageSum;
                        }
                    }
                }
            }
        }
        RaceFrameHook koUpdate(Mgr::Update);

        /*
        void Mgr::UpdateStillInCount() {
            u32 stillIn = 0;
            const RKNet::Controller* controller = RKNet::Controller::sInstance;
            const u32 availableAids =  controller->subs[controller->currentSub].availableAids;
            for(int aid = 0; aid < 12; ++aid) {
                u8 aid = controller->aidsBelongingToPlayerIds[aid];
                if((1 << aid & availableAids) == 0) { //the playerId is disconnected
                    this->SetMgrd(aid);
                }
                if(this->IsMgrd(aid)) continue;
                ++stillIn;
            }
            this->stillInCount = stillIn;
        }
        */

        void Mgr::PatchAids(RKNet::ControllerSub &sub) const
        {
            u32 availableAids = sub.availableAids;
            const u8 localAid = sub.localAid;
            for (u8 aid = 0; aid < 12; ++aid)
            {

                bool isConsoleOut = false;
                const bool isMainOut = this->IsKOdAid(aid, 0) || this->IsDisconnectedAid(aid, 0);
                u8 aidPlayerCount = aid == localAid ? sub.localPlayerCount : sub.connectionUserDatas[aid].playersAtConsole;

                if (aidPlayerCount <= 1)
                    isConsoleOut = isMainOut;
                else if (aidPlayerCount == 2)
                {
                    const bool isGuestOut = this->IsKOdAid(aid, 1) || this->IsDisconnectedAid(aid, 1);
                    if (isMainOut && isGuestOut)
                        isConsoleOut = true;
                    else if (isMainOut != isGuestOut)
                    {
                        aidPlayerCount = 1;
                    }
                }
                if (isConsoleOut)
                {
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

        u32 Mgr::GetAidAndSlotFromPlayerId(u8 playerId) const
        {
            const RKNet::Controller *controller = RKNet::Controller::sInstance;
            const RKNet::ControllerSub &sub = controller->subs[controller->currentSub];
            const u8 aid = controller->aidsBelongingToPlayerIds[playerId];
            const u8 localAid = sub.localAid;
            u8 slot = 0;

            if ((aid == localAid && sub.localPlayerCount == 2) || (aid != localAid && sub.connectionUserDatas[aid].playersAtConsole == 2))
            {
                if (playerId > 0 && controller->aidsBelongingToPlayerIds[playerId - 1] == aid)
                    slot = 1;
                else if (playerId < 11 && this->status[aid][0] == KOD && controller->aidsBelongingToPlayerIds[playerId + 1] != aid)
                    slot = 1;
            }
            return (slot << 16) | aid; // 10001
        }

        SectionId Mgr::GetSectionAfterKO(SectionId defaultId) const
        {
            const RKNet::Controller *controller = RKNet::Controller::sInstance;
            const RKNet::ControllerSub &sub = controller->subs[controller->currentSub];
            if (this->baseLocPlayerCount == 2)
            {
                if (this->IsKOdAid(sub.localAid, 0) != this->IsKOdAid(sub.localAid, 1))
                {
                    if (defaultId == SECTION_P2_WIFI_FROOM_VS_VOTING)
                        defaultId = SECTION_P1_WIFI_FROOM_VS_VOTING; // select section
                    else if (defaultId == SECTION_P1_WIFI_FROM_FROOM_RACE || defaultId == SECTION_P2_WIFI_FROM_FROOM_RACE)
                    {
                        defaultId = SECTION_P2_WIFI_FROM_FROOM_RACE; // after room section
                        SectionMgr::sInstance->sectionParams->localPlayerCount = 2;
                    }
                }
            }
            return defaultId;
        }

        void OnDisconnectKO(SectionMgr *sectionMgr, SectionId id)
        {
            const System *system = System::sInstance;
            if (system->IsContext(PULSAR_MODE_KO))
                id = system->koMgr->GetSectionAfterKO(id);
            sectionMgr->SetNextSection(id, 0);
        }
        kmCall(0x80651814, OnDisconnectKO);

        PageId Mgr::KickPlayersOut(PageId defaultId)
        { // only called if KOMode

            PageId ret = defaultId;
            const System *system = System::sInstance;

            Mgr *mgr = system->koMgr;
            RacedataScenario &scenario = Racedata::sInstance->racesScenario;
            const bool isMainOut = mgr->IsKOdPlayerId(scenario.settings.hudPlayerIds[0]) || mgr->IsDisconnectedPlayerId(scenario.settings.hudPlayerIds[0]);
            if (system->nonTTGhostPlayersCount > 2)
            {
                if (scenario.localPlayerCount == 1)
                {
                    const RKNet::Controller *controller = RKNet::Controller::sInstance;
                    const RKNet::ControllerSub &sub = controller->subs[controller->currentSub];
                    if (isMainOut)
                    {
                        if (sub.localAid == sub.hostAid)
                            mgr->isSpectating = true; // force the host to spectate, they should not be allowed to quit
                        else
                            ret = static_cast<PageId>(RaceEndPage::id);
                    }
                }
                else
                {
                    const bool isGuestOut = mgr->IsKOdPlayerId(scenario.settings.hudPlayerIds[1]) || mgr->IsDisconnectedPlayerId(scenario.settings.hudPlayerIds[1]);
                    if (isMainOut != isGuestOut)
                        SectionMgr::sInstance->sectionParams->localPlayerCount = 1;
                    if (isMainOut && !isGuestOut)
                    {
                        memcpy(&mgr->stats[0], &mgr->stats[1], sizeof(Mgr::Stats));
                    }
                    else if (isMainOut && isGuestOut)
                        ret = static_cast<PageId>(RaceEndPage::id);
                }
            }
            return ret;
        }

        void Mgr::SwapControllersAndUI()
        {

            // Swap the controllers
            Input::Manager *input = Input::Manager::sInstance;

            char mainController[sizeof(Input::RealControllerHolder)];
            memcpy(&mainController, &input->realControllerHolders[0], sizeof(Input::RealControllerHolder));
            memcpy(&input->realControllerHolders[0], &input->realControllerHolders[1], sizeof(Input::RealControllerHolder));
            memcpy(&input->realControllerHolders[1], &mainController, sizeof(Input::RealControllerHolder));

            SectionMgr *sectionMgr = SectionMgr::sInstance;
            SectionPad &pad = sectionMgr->pad;
            PadInfo &main = pad.padInfos[0];
            PadInfo &guest = pad.padInfos[1];
            u32 old = main.controllerID;
            u32 oldg = guest.controllerID;
            main.controllerID = oldg;
            guest.controllerID = old;
            main.controllerIDActive = main.controllerID;
            guest.controllerIDActive = guest.controllerID;

            // Swap the characters on the UI
            SectionParams *params = sectionMgr->sectionParams;
            CharacterId mainChar = params->characters[0];
            KartId mainKart = params->karts[0];
            params->characters[0] = params->characters[1];
            params->karts[0] = params->karts[1];
            params->characters[1] = mainChar;
            params->karts[1] = mainKart;
            memcpy(&params->combos[0], &params->combos[1], sizeof(PlayerCombo));
            this->hasSwapped = !this->hasSwapped;
        }
    } // namespace KO
} // namespace Pulsar