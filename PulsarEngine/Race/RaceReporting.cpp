#include <MarioKartWii/Scene/RaceScene.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <MarioKartWii/Driver/DriverManager.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/RKSYS/RKSYSMgr.hpp>
#include <Network/GPReport.hpp>
#include <Network/Rating/RatingSync.hpp>

namespace Pulsar {

RaceStage sLastRaceStage = RACESTAGE_RACE;

static bool HasOnlineControllerRoom() {
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    return controller != nullptr && controller->roomType != RKNet::ROOMTYPE_NONE;
}

void UpdateRaceInstances() {
    RaceScene::UpdateRaceInstances();
    if (!DriverMgr::isOnlineRace && !HasOnlineControllerRoom())
        return;

    Raceinfo* raceInfo = Raceinfo::sInstance;
    if (!raceInfo)
        return;

    if (raceInfo->stage != sLastRaceStage) {
        sLastRaceStage = raceInfo->stage;
        if (sLastRaceStage == RACESTAGE_FINISHED) {
            Network::ReportU32("wl:mkw_race_stage", sLastRaceStage);

            RKSYS::Mgr* rksys = RKSYS::Mgr::sInstance;
            if (rksys != nullptr && rksys->curLicenseId < 4) {
                PointRating::ReportCurrentRatings(rksys->curLicenseId);
            }
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

}  // namespace Pulsar
