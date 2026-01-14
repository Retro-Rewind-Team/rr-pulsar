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

#ifndef _PUL_FASTNATNEG_
#define _PUL_FASTNATNEG_

#include <kamek.hpp>
#include <core/rvl/DWC/DWCMatch.hpp>

/*
    Delay Compensation Module - Ported from OpenPayload/Wiimmfi

    This module calculates network frame delays during online races
    to help with synchronization.

    Original credits: Wiimmfi, CLF78
*/

namespace Pulsar {
namespace Network {
namespace FastNATNEG {

// External functions
extern "C" {
u32 current_time();
void VIWaitForRetrace();
}

// Pointers to DWC structures
extern DWC::MatchControl** stpMatchCnt;

// Delay Module functions
namespace Delay {
void Reset();
void Calc(u32 frameCount);
u32 Apply(u32 timer);

extern u32 sMatchStartTime;
extern u32 sCumulativeDelay;
extern u32 sCurrentDelay;
}  // namespace Delay

}  // namespace FastNATNEG
}  // namespace Network
}  // namespace Pulsar

#endif
