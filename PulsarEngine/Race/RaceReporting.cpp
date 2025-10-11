#include <MarioKartWii/Scene/RaceScene.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <MarioKartWii/Driver/DriverManager.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <Network/GPReport.hpp>

namespace Pulsar {

RaceStage sLastRaceStage = RACESTAGE_RACE;

void UpdateRaceInstances() {
    RaceScene::UpdateRaceInstances();
    if (!DriverMgr::isOnlineRace)
        return;

    Raceinfo* raceInfo = Raceinfo::sInstance;
    if (!raceInfo)
        return;

    if (raceInfo->stage != sLastRaceStage) {
        sLastRaceStage = raceInfo->stage;
        // OS::Report("================================\n");
        // OS::Report("Race stage: %d\n", sLastRaceStage);
        // OS::Report("================================\n");
        if (sLastRaceStage == RACESTAGE_FINISHED) {
            Network::ReportU32("wl:mkw_race_stage", sLastRaceStage);
        }
    }
}

void EndPlayerRaceHook(Raceinfo* _this, u8 playerIdx) {
    _this->EndPlayerRace(playerIdx);
    if (!DriverMgr::isOnlineRace)
        return;

    Racedata* raceData = Racedata::sInstance;
    if (!raceData)
        return;

    RacedataPlayer* racePlayer = &raceData->racesScenario.players[playerIdx];
    if (racePlayer->playerType == PLAYER_REAL_LOCAL) {
        RKNet::Controller* netController = RKNet::Controller::sInstance;
        RKNet::ControllerSub& sub = netController->subs[netController->currentSub];

        s8 hostPlayerIdx = -1;
        for (int i = 0; i < raceData->racesScenario.playerCount; i++) {
            if (netController->aidsBelongingToPlayerIds[i] == sub.hostAid) {
                hostPlayerIdx = i;
                break;
            }
        }

        // If the host could not be found, return immediately
        if (hostPlayerIdx == -1)
            return;

        // You have dc'd from the host (or the host dc'd), crying cat emoji.
        //
        // If less than half the room finished, do not report. It's likely a win dc.
        // If 3 or more people disconnected in a single race, do not report.
        if (_this->players[hostPlayerIdx]->stateFlags & 0x10) {
            u8 finishedCount = 0;
            u8 disconnectCount = 0;
            for (int i = 0; i < raceData->racesScenario.playerCount; i++) {
                if (_this->players[i]->stateFlags & 0x10) {
                    disconnectCount++;
                } else if (_this->players[i]->stateFlags & 0x02) {
                    finishedCount++;
                }
            }

            if (finishedCount <= (raceData->racesScenario.playerCount / 2))
                return;

            if (disconnectCount >= 3)
                return;
        }

        Timer* finishTime = _this->players[playerIdx]->raceFinishTime;
        float time = (finishTime->minutes * 60.0f) + (finishTime->seconds) + (finishTime->milliseconds / 1000.0f);

        char buffer[128];
        snprintf(buffer,
                 sizeof(buffer),
                 "hi=%d|ch=%d|ve=%d|ft=%u|fp=%d|f1=%u|pc=%d",
                 racePlayer->hudSlotId,
                 racePlayer->characterId,
                 racePlayer->kartId,
                 *(u32*)&time,
                 racePlayer->finishPos,
                 _this->players[playerIdx]->framesInFirst,
                 raceData->racesScenario.playerCount);

        Network::Report("wl:mkw_race_result", buffer);
    }
}

kmCall(0x80554eec, UpdateRaceInstances);
kmCall(0x8053491c, EndPlayerRaceHook);

// No sit DC [Bully]
//
// For testing purposes
/*
04521408 38000000
0453EC94 38000000
0453EF6C 38000000
0453F0B4 38000000
0453F124 38000000
*/
// kmWrite32(0x80521408, 0x38000000);
// kmWrite32(0x8053ec94, 0x38000000);
// kmWrite32(0x8053ef6c, 0x38000000);
// kmWrite32(0x8053f0b4, 0x38000000);
// kmWrite32(0x8053f124, 0x38000000);

}  // namespace Pulsar