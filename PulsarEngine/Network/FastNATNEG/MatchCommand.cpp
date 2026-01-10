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
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/RKNet/Select.hpp>
#include <core/rvl/OS/OS.hpp>

/*
    Match Command Processing - Ported from OpenPayload/Wiimmfi

    This module handles custom match commands for connection matrix sharing
    and failure reporting.

    Original credits: Wiimmfi, CLF78
*/

namespace Pulsar {
namespace Network {
namespace FastNATNEG {
namespace MatchCommand {

// Size conversion macro (DWC uses 32-bit word count)
#define DWC_MATCH_CMD_GET_ACTUAL_SIZE(size) ((size) * 4)
#define DWC_MATCH_CMD_GET_SIZE(size) ((size) / 4)

// Inline byte swap for big endian
static inline u32 ByteSwap32(u32 value) {
    return ((value >> 24) & 0x000000FF) |
           ((value >> 8) & 0x0000FF00) |
           ((value << 8) & 0x00FF0000) |
           ((value << 24) & 0xFF000000);
}

static void ProcessRecvConnFailMtxCommand(u32 clientAPid, u32 clientAIP, u16 clientAPort,
                                          DWCMatchCommandConnFailMtx* data, u32 dataLen) {
    DWC::MatchControl* matchCnt = *stpMatchCnt;
    if (!matchCnt) return;

    // Only process if we are waiting (host check)
    if (matchCnt->state != DWC::DWC_MATCH_STATE_SV_WAITING) return;

    // Ensure data size is correct
    if (DWC_MATCH_CMD_GET_ACTUAL_SIZE(dataLen) != sizeof(DWCMatchCommandConnFailMtx)) return;

    // Find a "client B" with connection failure (highest AID)
    s32 clientBAid = -1;
    for (s32 i = 11; i >= 0; i--) {
        if ((data->connFailMtx >> i) & 1) {
            clientBAid = i;
            break;
        }
    }

    // If no client B found, bail
    if (clientBAid == -1) return;

    // Get node info for client A and B
    DWCNodeInfo* clientAInfo = DWCi_NodeInfoList_GetNodeInfoForProfileId(clientAPid);
    DWCNodeInfo* clientBInfo = DWCi_NodeInfoList_GetNodeInfoForAid(static_cast<u8>(clientBAid));
    if (!clientAInfo || !clientBInfo) return;

    // Copy node info to temporary new node (offset 0x638)
    DWCNodeInfo* tempNewNode = reinterpret_cast<DWCNodeInfo*>(reinterpret_cast<u8*>(matchCnt) + 0x638);
    memcpy(tempNewNode, clientBInfo, sizeof(DWCNodeInfo));

    // Set up command for client B and send it
    DWCMatchCommandNewPidAid cmd;
    cmd.pid = ByteSwap32(tempNewNode->profileId);
    cmd.aid = ByteSwap32(tempNewNode->aid);
    DWCi_SendGPBuddyMsgCommand(0x07, clientAPid, clientAIP, clientAPort,
                               &cmd, DWC_MATCH_CMD_GET_SIZE(sizeof(cmd)));  // DWC_MATCH_CMD_NEW_PID_AID = 0x07

    // Send SYN packet to client A
    DWCi_SendMatchSynPacket(clientAInfo->aid, 1);  // DWC_MATCH_SYN_CMD_SYN

    // Reset node info
    memset(tempNewNode, 0, sizeof(DWCNodeInfo));
}

static void ProcessRecvConnMtxCommand(u32 srcPid, DWCMatchCommandConnMtx* data, u32 dataLen) {
    // Ensure data size is correct
    if (DWC_MATCH_CMD_GET_ACTUAL_SIZE(dataLen) != sizeof(DWCMatchCommandConnMtx)) return;

    // Update received connection matrix
    DWCNodeInfo* node = DWCi_NodeInfoList_GetNodeInfoForProfileId(srcPid);
    if (node) {
        ConnectionMatrix::sRecvConnMtx[node->aid] = data->connMtx;
    } else {
        ConnectionMatrix::ResetRecv();
    }
}

bool ProcessRecvMatchCommand(u8 cmd, u32 pid, u32 ip, u16 port, void* data, u32 dataSize) {
    switch (cmd) {
        case DWC_MATCH_CMD_CONN_FAIL_MTX:
            ProcessRecvConnFailMtxCommand(pid, ip, port,
                                          reinterpret_cast<DWCMatchCommandConnFailMtx*>(data), dataSize);
            return true;

        case DWC_MATCH_CMD_CONN_MTX:
            ProcessRecvConnMtxCommand(pid, reinterpret_cast<DWCMatchCommandConnMtx*>(data), dataSize);
            return true;

        default:
            return false;
    }
}

void SendConnFailMtxCommand(u32 aidsConnectedToHost, u32 aidsConnectedToMe) {
    DWC::MatchControl* matchCnt = *stpMatchCnt;
    if (!matchCnt) return;

    // Get AIDs who haven't connected to me
    DWCMatchCommandConnFailMtx cmd;
    cmd.connFailMtx = aidsConnectedToHost & ~aidsConnectedToMe;

    // If all AIDs connected or waiting, bail
    if (!cmd.connFailMtx || matchCnt->state == DWC::DWC_MATCH_STATE_CL_WAITING) return;

    // Get host's node info
    DWCNodeInfo* hostNodeInfo = DWCi_NodeInfoList_GetServerNodeInfo();
    if (!hostNodeInfo || hostNodeInfo->profileId == 0) return;

    // Send command
    DWCi_SendGPBuddyMsgCommand(DWC_MATCH_CMD_CONN_FAIL_MTX, hostNodeInfo->profileId,
                               hostNodeInfo->publicip, hostNodeInfo->publicport,
                               &cmd, DWC_MATCH_CMD_GET_SIZE(sizeof(cmd)));
}

void SendConnMtxCommand(u32 aidsConnectedToMe) {
    DWC::MatchControl* matchCnt = *stpMatchCnt;
    if (!matchCnt) return;

    s32 nodeCount = *reinterpret_cast<s32*>(reinterpret_cast<u8*>(matchCnt) + 0x38);
    if (nodeCount == 0) return;

    DWCNodeInfo* nodes = reinterpret_cast<DWCNodeInfo*>(reinterpret_cast<u8*>(matchCnt) + 0x38 + 8);

    // Set up command
    DWCMatchCommandConnMtx cmd;
    cmd.connMtx = aidsConnectedToMe;

    // Send to every node
    for (s32 i = 0; i < nodeCount; i++) {
        DWCNodeInfo* node = &nodes[i];

        // Skip if it's me
        if (node->profileId == static_cast<u32>(matchCnt->profileID)) continue;

        // Send command
        DWCi_SendGPBuddyMsgCommand(DWC_MATCH_CMD_CONN_MTX, node->profileId,
                                   node->publicip, node->publicport,
                                   &cmd, DWC_MATCH_CMD_GET_SIZE(sizeof(cmd)));
    }
}

// Parse custom match commands - GPCM
static s32 ProcessMatchCommandHook(u8 cmd, u32 pid, u32 ip, u16 port, void* data, u32 dataLen) {
    // Check if we can parse the command
    if (MatchCommand::ProcessRecvMatchCommand(cmd, pid, ip, port, data, dataLen)) {
        return 0;
    }

    // Fall back to game code
    return DWCi_ProcessRecvMatchCommand(cmd, pid, ip, port, data, dataLen);
}
kmCall(0x800D94F0, ProcessMatchCommandHook);

// Parse custom match commands - GT2
kmCall(0x800E5980, ProcessMatchCommandHook);

// Parse custom match commands - MASTER
kmCall(0x800E5B14, ProcessMatchCommandHook);

}  // namespace MatchCommand
}  // namespace FastNATNEG
}  // namespace Network
}  // namespace Pulsar