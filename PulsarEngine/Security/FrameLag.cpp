#include <PulsarSystem.hpp>
#include <Dolphin/DolphinIOS.hpp>
#include <core/System/SystemManager.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/System/ElineManager.hpp>

namespace Security {

extern "C" u32 current_time();

void getRTCTimeInMs(u32 &out) {
    u32 currentTimeStamp;
    bool isDolphin = Dolphin::IsEmulator();
    if (isDolphin) {
        Dolphin::GetElapsedTime(currentTimeStamp);
    }

    if (!isDolphin || currentTimeStamp == 0) {
        currentTimeStamp = current_time();
    }

    out = currentTimeStamp;
}

static u32 raceStartTime = 0;

void EveryFrame(ElineMgr *_this) {
    _this->Update();

    RKNet::Controller *controller = RKNet::Controller::sInstance;
    if (!controller || controller->roomType == RKNet::ROOMTYPE_NONE) {
        return;
    }

    if (Raceinfo::sInstance->timerMgr->raceFrameCounter == 0) {
        return;
    }

    if (raceStartTime == 0)
        getRTCTimeInMs(raceStartTime);

    u32 currentTime;
    getRTCTimeInMs(currentTime);

    u32 expectedNumberOfRaceFrames = (u32)((currentTime - raceStartTime) / SystemManager::sInstance->frameDuration);
    if (Raceinfo::sInstance->timerMgr->raceFrameCounter < expectedNumberOfRaceFrames) {
        OS::Report("Detected frame lag, applied a %d frame correction\n", expectedNumberOfRaceFrames - Raceinfo::sInstance->timerMgr->raceFrameCounter);
        Raceinfo::sInstance->timerMgr->raceFrameCounter = expectedNumberOfRaceFrames;
    }
}

void InitNewRace(RacedataScenario *scenario) {
    scenario->UpdateFromPrevRace();
    raceStartTime = 0;
    OS::Report("New race started\n");
}

kmCall(0x8052fbd8, InitNewRace);
kmCall(0x80533240, EveryFrame);

}  // namespace Security