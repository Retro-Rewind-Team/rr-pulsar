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
#include <core/rvl/OS/OS.hpp>

/*
    Connection Matrix - Ported from OpenPayload/Wiimmfi
    
    This module tracks the connection status between clients and shares this
    information to improve mesh networking.
    
    Original credits: Wiimmfi
*/

namespace Pulsar {
namespace Network {
namespace FastNATNEG {
namespace ConnectionMatrix {

u32 sRecvConnMtx[12];

void ResetRecv() {
    // Reset received matrix for AIDs without node info
    for (s32 aid = 0; aid < 12; aid++) {
        if (!DWCi_NodeInfoList_GetNodeInfoForAid(static_cast<u8>(aid))) {
            sRecvConnMtx[aid] = 0;
        }
    }
}

void Update() {
    DWC::MatchControl* matchCnt = *stpMatchCnt;
    if (!matchCnt) return;
    
    // Disable interrupts for thread safety
    BOOL enabled = OS::DisableInterrupts();
    
    // Compute connection matrix
    u32 aidsConnectedToMe = 0;
    s32 nodeCount = *reinterpret_cast<s32*>(reinterpret_cast<u8*>(matchCnt) + 0x38);
    DWCNodeInfo* nodes = reinterpret_cast<DWCNodeInfo*>(reinterpret_cast<u8*>(matchCnt) + 0x38 + 8);
    
    for (s32 i = 0; i < nodeCount; i++) {
        u8 aid = nodes[i].aid;
        if (DWCi_GetGT2Connection(aid)) {
            aidsConnectedToMe |= (1 << i);
        }
    }
    
    // Share connection matrix with other clients
    MatchCommand::SendConnMtxCommand(aidsConnectedToMe);
    
    OS::RestoreInterrupts(enabled);
}

} // namespace ConnectionMatrix
} // namespace FastNATNEG
} // namespace Network
} // namespace Pulsar