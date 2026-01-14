/*
MIT License

Copyright (c) 2024 CLF78

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <Network/FastNATNEG/FastNATNEG.hpp>
#include <MarioKartWii/Driver/DriverManager.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>

/*
    Delay Compensation - Ported from OpenPayload/Wiimmfi

    This module calculates and compensates for network frame delays to improve
    online race synchronization.
    Original credits: Wiimmfi
*/

namespace Pulsar {
namespace Network {
namespace FastNATNEG {
namespace Delay {

u32 sMatchStartTime = 0;
u32 sCumulativeDelay = 0;
u32 sCurrentDelay = 0;

static u32 GetTime() {
    return current_time();
}

void Reset() {
    sMatchStartTime = 0;
    sCumulativeDelay = 0;
    sCurrentDelay = 0;
}

void Calc(u32 frameCount) {
    if (frameCount == 0) return;

    if (sMatchStartTime == 0) {
        sMatchStartTime = GetTime();
    }

    u32 timeElapsed = GetTime() - sMatchStartTime;
    float framesElapsed = timeElapsed / (1000.0f / 60.0f);
    u32 framesElapsed32 = static_cast<u32>(framesElapsed);

    s32 delay = static_cast<s32>(framesElapsed32) - static_cast<s32>(frameCount) - static_cast<s32>(sCumulativeDelay);
    if (delay > 0) {
        sCurrentDelay = static_cast<u32>(delay);
    }
}

u32 Apply(u32 timer) {
    u32 currDelay = sCurrentDelay;
    sCumulativeDelay += currDelay;
    sCurrentDelay = 0;
    return timer + currDelay + 1;
}

// Reset delay values when race scene is created
static void ResetDelayOnRaceStart() {
    Delay::Reset();
}
RaceLoadHook raceLoadResetDelay(ResetDelayOnRaceStart);

// mainNetworkLoop patch
// Calculate delay during online races (hooked into VIWaitForRetrace call)
static void CalcDelayInNetworkLoop() {
    DWC::MatchControl* matchCnt = *stpMatchCnt;
    if (matchCnt != nullptr) {
        Raceinfo* raceInfo = Raceinfo::sInstance;
        if (DriverMgr::isOnlineRace && raceInfo != nullptr) {
            Delay::Calc(raceInfo->raceFrames);
        }
    }
    VIWaitForRetrace();
}
kmCall(0x806579B0, CalcDelayInNetworkLoop);

}  // namespace Delay
}  // namespace FastNATNEG
}  // namespace Network
}  // namespace Pulsar