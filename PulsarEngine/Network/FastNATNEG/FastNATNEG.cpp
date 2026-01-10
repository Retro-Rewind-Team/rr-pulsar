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
#include <core/rvl/OS/OS.hpp>
#include <runtimeWrite.hpp>

/*
    Fast NATNEG Implementation - Ported from OpenPayload/Wiimmfi

    This module improves mesh connection establishment by:
    1. Attempting direct GT2 connections to other clients
    2. Accepting incoming connections in more states
    3. Improving the next NATNEG node selection algorithm
    4. Preventing repeated NATNEG failures from disconnecting hosts

    Original credits: Wiimmfi, CLF78
*/

namespace Pulsar {
namespace Network {
namespace FastNATNEG {

u16 sTimers[12];  // One timer per AID

// Maximum direct connection attempts before considering host routing
static const u8 MAX_DIRECT_ATTEMPTS = 3;

// Connect to a node directly
static void ConnectToNode(s32 nodeIdx) {
    DWC::MatchControl* matchCnt = *stpMatchCnt;
    if (!matchCnt) return;

    DWCNodeInfo* nodes = reinterpret_cast<DWCNodeInfo*>(reinterpret_cast<u8*>(matchCnt) + 0x38 + 8);
    DWCNodeInfo* node = &nodes[nodeIdx];
    u8 aid = node->aid;

    // Check if we should use host routing for this AID due to poor connection quality
    if (ConnectionQuality::ShouldUseHostRouting(aid)) {
        // Mark that we need host routing for this player
        ConnectionQuality::SetHostRoutingRequired(aid, true);
        return;
    }

    const char* ipAddr = gt2AddressToString(node->publicip, node->publicport, nullptr);

    char buffer[24];
    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "%u%s", matchCnt->profileID, "L");

    // Use adaptive timeout based on connection quality
    u32 timeout = ConnectionQuality::GetAdaptiveTimeout(aid);

    GT2::Connection* conn = nullptr;
    GT2::Result ret = gt2Connect(*matchCnt->gt2Socket, &conn, ipAddr, buffer,
                                 -1, timeout, matchCnt->gt2Callbacks, 0);

    // Record the connection attempt result
    ConnectionQuality::RecordConnectionAttempt(aid, ret == GT2::GT2_RESULT_SUCCESS);

    if (ret == GT2::GT2_RESULT_SUCCESS && conn != nullptr) {
        u8* connBytes = reinterpret_cast<u8*>(conn);
        connBytes[6] = static_cast<u8>(aid + 1);
    }
}

// Calculate adaptive timer based on player count and connection quality
static u16 GetAdaptiveTimer(s32 nodeCount) {
    // Base timer values (frames at 60fps)
    // 1-4 players: 300 frames (5 seconds)
    // 5-8 players: 200 frames (3.3 seconds)
    // 9-12 players: 120 frames (2 seconds)
    u16 baseTimer;
    if (nodeCount >= 9)
        baseTimer = 120;
    else if (nodeCount >= 5)
        baseTimer = 200;
    else
        baseTimer = 300;

    return baseTimer;
}

// Calculate adaptive timer for a specific AID, accounting for connection quality
static u16 GetAdaptiveTimerForAID(s32 nodeCount, u8 aid) {
    u16 baseTimer = GetAdaptiveTimer(nodeCount);

    // Apply connection quality multiplier for poor connections
    ConnectionQuality::QualityLevel quality = ConnectionQuality::GetQuality(aid);
    switch (quality) {
        case ConnectionQuality::QUALITY_POOR:
            baseTimer = (baseTimer * 3) / 2;  // 1.5x for poor connections
            break;
        case ConnectionQuality::QUALITY_CRITICAL:
            baseTimer *= 2;  // 2x for critical connections
            break;
        default:
            break;
    }

    return baseTimer;
}

// Calculate adaptive initial delay based on player count and connection quality
static u16 GetAdaptiveInitialDelay(s32 nodeCount, u8 aid) {
    // Base delay with staggering to prevent simultaneous connection storms
    u16 baseDelay = 100;
    u16 stagger = (aid % 4) * 30;

    // Apply exponential backoff based on failed attempts
    u8 failedAttempts = ConnectionQuality::sStats[aid].failedAttempts;
    if (failedAttempts > 0) {
        // Exponential backoff: base * 2^attempts, capped at 4x
        u16 backoffMultiplier = 1 << (failedAttempts > 2 ? 2 : failedAttempts);
        baseDelay *= backoffMultiplier;
    }

    // Additional delay for poor connection quality
    ConnectionQuality::QualityLevel quality = ConnectionQuality::GetQuality(aid);
    if (quality >= ConnectionQuality::QUALITY_POOR) {
        baseDelay += 60;  // Extra second for poor connections
    }

    return baseDelay + stagger;
}

void CalcTimers(bool connectedToHost) {
    DWC::MatchControl* matchCnt = *stpMatchCnt;
    if (!matchCnt) return;

    // Get node count from structure (offset 0x38)
    s32 nodeCount = *reinterpret_cast<s32*>(reinterpret_cast<u8*>(matchCnt) + 0x38);
    if (nodeCount < 1) return;

    DWCNodeInfo* nodes = reinterpret_cast<DWCNodeInfo*>(reinterpret_cast<u8*>(matchCnt) + 0x38 + 8);

    // Calculate adaptive timers based on current lobby size
    u16 retryTimer = GetAdaptiveTimer(nodeCount);

    for (s32 i = 0; i < nodeCount; i++) {
        u8 aid = nodes[i].aid;

        // If I am host, do not use fast NATNEG
        if (DWC_IsServerMyself()) {
            sTimers[aid] = 0;
            continue;
        }

        // Check for invalid AID
        if (aid > 11) continue;

        // If connected to host, set adaptive initial delay with staggering
        if (connectedToHost) {
            sTimers[aid] = GetAdaptiveInitialDelay(nodeCount, aid);
            continue;
        }

        // Wait for timer to reach zero
        if (sTimers[aid] > 0) {
            sTimers[aid]--;
            continue;
        }

        // Skip if already connected
        if (DWCi_GetGT2Connection(aid)) continue;

        // Check for empty PID or my own PID
        u32 pid = nodes[i].profileId;
        if (pid == 0 || pid == static_cast<u32>(matchCnt->profileID)) continue;

        // Get match state
        DWC::MatchState matchState = static_cast<DWC::MatchState>(matchCnt->state);

        // If we are not in forbidden states, try to connect
        if (matchState != DWC::DWC_MATCH_STATE_CL_WAIT_RESV &&
            matchState != DWC::DWC_MATCH_STATE_CL_NN &&
            matchState != DWC::DWC_MATCH_STATE_CL_GT2 &&
            matchState != DWC::DWC_MATCH_STATE_SV_OWN_NN &&
            matchState != DWC::DWC_MATCH_STATE_SV_OWN_GT2) {
            ConnectToNode(i);
        }

        // Reset timer with adaptive value based on connection quality for this specific AID
        sTimers[aid] = GetAdaptiveTimerForAID(nodeCount, aid);
    }
}

static void StopMeshMaking() {
    DWCi_StopMeshMaking();
    DWCi_SetMatchStatus(DWC_IsServerMyself() ? DWC::DWC_MATCH_STATE_SV_WAITING : DWC::DWC_MATCH_STATE_CL_WAITING);
}

static void StoreConnectionAndInfo(s32 connIdx, GT2::Connection* conn, DWCNodeInfo* node) {
    // Get connection pointer slot and store the new connection
    GT2::Connection** connPtr = DWCi_GetGT2ConnectionByIdx(connIdx);
    *connPtr = conn;

    // Get connection info and update it
    DWCConnectionInfo* connInfo = DWCi_GetConnectionInfoByIdx(connIdx);
    connInfo->index = static_cast<u8>(connIdx);
    connInfo->aid = node->aid;
    connInfo->profileId = node->profileId;
    gt2SetConnectionData(conn, connInfo);

    // Reset data receive time and update connection matrix
    DWCi_ResetLastSomeDataRecvTimeByAid(node->aid);
    ConnectionMatrix::Update();
}

void ConnectAttemptCallback(GT2::Socket* socket, GT2::Connection* conn, u32 ip, u16 port,
                            s32 latency, const char* msg, s32 msgLen) {
    DWC::MatchControl* matchCnt = *stpMatchCnt;

    // Obtain PID from message
    char* msgBuffer;
    u32 pid = strtoul(msg, &msgBuffer, 10);

    // If message was not generated from our ConnectToNode function, fall back to original
    if (*msgBuffer != 'L') {
        DWCi_GT2ConnectAttemptCallback(socket, conn, ip, port, latency, msg, msgLen);
        return;
    }

    // If no match control structure, bail
    if (!matchCnt) return;

    // If still in INIT state, reject
    if (matchCnt->state == DWC::DWC_MATCH_STATE_INIT) {
        gt2Reject(conn, "wait1", -1);
        return;
    }

    // If PID is not found, reject
    DWCNodeInfo* node = DWCi_NodeInfoList_GetNodeInfoForProfileId(pid);
    if (!node) {
        gt2Reject(conn, "wait2", -1);
        return;
    }

    // If connection already exists, bail
    if (DWCi_GetGT2Connection(node->aid)) return;

    // Stop mesh making if stuck doing NATNEG
    DWC::MatchState state = static_cast<DWC::MatchState>(matchCnt->state);

    // Get tempNewNodeInfo profileId (offset 0x638 in MatchControl)
    u32 tempNewPid = *reinterpret_cast<u32*>(reinterpret_cast<u8*>(matchCnt) + 0x638);

    switch (state) {
        case DWC::DWC_MATCH_STATE_CL_WAIT_RESV:
        case DWC::DWC_MATCH_STATE_CL_NN:
        case DWC::DWC_MATCH_STATE_SV_OWN_NN:
        case DWC::DWC_MATCH_STATE_SV_OWN_GT2:
            if (pid == tempNewPid) StopMeshMaking();
            break;

        case DWC::DWC_MATCH_STATE_CL_GT2:
            if (pid == tempNewPid) {
                DWCi_GT2ConnectAttemptCallback(socket, conn, ip, port, latency, msg, msgLen);
                return;
            }
            break;

        default:
            break;
    }

    // If server is full, bail
    s32 connIdx = DWCi_GT2GetConnectionListIdx();
    if (connIdx == -1) return;

    // Store IP and port
    node->gt2Ip = ip;
    node->gt2Port = port;

    // Accept the connection
    if (!gt2Accept(conn, matchCnt->gt2Callbacks)) return;

    // Store connection and info
    StoreConnectionAndInfo(connIdx, conn, node);
}

void ConnectedCallback(GT2::Connection* conn, GT2::Result result, const char* msg, s32 msgLen) {
    DWC::MatchControl* matchCnt = *stpMatchCnt;

    // Check if custom AID field was set (at offset 6)
    u8 customAid = reinterpret_cast<u8*>(conn)[6];
    if (customAid == 0) {
        DWCi_GT2ConnectedCallback(conn, result, msg, msgLen);
        return;
    }

    // Get actual AID
    u8 aid = customAid - 1;

    // If negotiation error, try again with adaptive delay
    if (result == GT2::GT2_RESULT_NEGOTIATIONERROR) {
        // Record the failed connection attempt for quality tracking
        ConnectionQuality::RecordConnectionAttempt(aid, false);

        DWCNodeInfo* node = DWCi_NodeInfoList_GetNodeInfoForAid(aid);
        if (node && node->profileId <= static_cast<u32>(matchCnt->profileID)) {
            // Get node count for adaptive timing with quality-aware backoff
            s32 nodeCount = *reinterpret_cast<s32*>(reinterpret_cast<u8*>(matchCnt) + 0x38);
            sTimers[aid] = GetAdaptiveInitialDelay(nodeCount, aid);
        }
        return;
    }

    // For non-success results, check for wait message
    if (result != GT2::GT2_RESULT_SUCCESS) {
        // Record failed attempt
        ConnectionQuality::RecordConnectionAttempt(aid, false);

        if (msg && (!strcmp(msg, "wait1") || !strcmp(msg, "wait2"))) {
            sTimers[aid] = 0;
        }
        return;
    }

    // Connection succeeded - record for quality tracking
    ConnectionQuality::RecordConnectionAttempt(aid, true);

    // If still in INIT state, bail
    if (matchCnt->state == DWC::DWC_MATCH_STATE_INIT) return;

    // If AID not found, bail
    DWCNodeInfo* node = DWCi_NodeInfoList_GetNodeInfoForAid(aid);
    if (!node) return;

    // If connection already exists, bail
    if (DWCi_GetGT2Connection(node->aid)) return;

    // Stop mesh making if stuck doing NATNEG
    DWC::MatchState state = static_cast<DWC::MatchState>(matchCnt->state);
    u32 tempNewPid = *reinterpret_cast<u32*>(reinterpret_cast<u8*>(matchCnt) + 0x638);

    switch (state) {
        case DWC::DWC_MATCH_STATE_CL_WAIT_RESV:
        case DWC::DWC_MATCH_STATE_CL_NN:
        case DWC::DWC_MATCH_STATE_SV_OWN_NN:
        case DWC::DWC_MATCH_STATE_SV_OWN_GT2:
            if (node->profileId == tempNewPid) StopMeshMaking();
            break;

        case DWC::DWC_MATCH_STATE_CL_GT2:
            if (node->profileId == tempNewPid) {
                DWCi_GT2ConnectedCallback(conn, result, msg, msgLen);
                return;
            }
            break;

        default:
            break;
    }

    // If server is full, bail
    s32 connIdx = DWCi_GT2GetConnectionListIdx();
    if (connIdx == -1) return;

    // Store IP, port and connection
    node->gt2Ip = *reinterpret_cast<u32*>(conn);  // IP at offset 0
    node->gt2Port = *reinterpret_cast<u16*>(reinterpret_cast<u8*>(conn) + 4);  // Port at offset 4
    StoreConnectionAndInfo(connIdx, conn, node);
}

DWCNodeInfo* GetNextMeshMakingNode() {
    DWC::MatchControl* matchCnt = *stpMatchCnt;
    if (!matchCnt) return nullptr;

    s64 minNextTryTime = 0x7FFFFFFFFFFFFFFFLL;
    DWCNodeInfo* minNextTryTimeNode = nullptr;

    s32 nodeCount = *reinterpret_cast<s32*>(reinterpret_cast<u8*>(matchCnt) + 0x38);
    DWCNodeInfo* nodes = reinterpret_cast<DWCNodeInfo*>(reinterpret_cast<u8*>(matchCnt) + 0x38 + 8);

    for (s32 i = 0; i < nodeCount; i++) {
        DWCNodeInfo* node = &nodes[i];

        // Skip my own node
        if (node->profileId == static_cast<u32>(matchCnt->profileID)) continue;

        // Skip nodes we are already connected to
        if (DWCi_GetGT2Connection(node->aid)) continue;

        // If retry time is empty, fill it
        if (node->nextMeshMakeTryTick == 0) {
            node->nextMeshMakeTryTick = DWCi_GetNextMeshMakeTryTick();
        }

        // Check if time is less than current minimum
        if (node->nextMeshMakeTryTick < minNextTryTime) {
            minNextTryTime = node->nextMeshMakeTryTick;
            minNextTryTimeNode = node;
        }
    }

    // Check if minimum time has been reached
    if (OS::GetTime() <= minNextTryTime) return nullptr;

    return minNextTryTimeNode;
}

bool PreventRepeatNATNEGFail(u32 failedPid) {
    static u32 sFailedPids[10];
    static u32 sFailedPidsIdx = 0;

    // Only run check for host
    if (!DWC_IsServerMyself()) return false;

    // Check if PID is valid
    if (failedPid == 0) return false;

    // If PID is already in list, do not count the failed attempt
    for (u32 i = 0; i < 10; i++) {
        if (sFailedPids[i] == failedPid) {
            return false;
        }
    }

    // Store PID in list through rolling counter
    if (sFailedPidsIdx >= 10) sFailedPidsIdx = 0;
    sFailedPids[sFailedPidsIdx++] = failedPid;
    return true;
}

void RecoverSynAckTimeout() {
    static u32 sSynAckTimer = 0;

    DWC::MatchControl* matchCnt = *stpMatchCnt;
    if (!matchCnt) return;

    // If not host or not in SYN state, bail
    if (matchCnt->state != DWC::DWC_MATCH_STATE_SV_SYN) {
        sSynAckTimer = 0;
        return;
    }

    // Update timer and run code every 150 frames
    if (++sSynAckTimer % 150) return;

    // If no nodes connected, bail
    s32 nodeCount = *reinterpret_cast<s32*>(reinterpret_cast<u8*>(matchCnt) + 0x38);
    if (nodeCount == 0) return;

    DWCNodeInfo* nodes = reinterpret_cast<DWCNodeInfo*>(reinterpret_cast<u8*>(matchCnt) + 0x38 + 8);

    // Get connected AIDs, insert newly connected one
    u32 noSynAckAids = DWC_GetAidBitmap();

    // Get tempNewNodeInfo aid (offset 0x64E in MatchControl)
    u8 tempNewAid = *reinterpret_cast<u8*>(reinterpret_cast<u8*>(matchCnt) + 0x64E);
    noSynAckAids |= (1 << tempNewAid);

    // Get synAckBit (offset 0x778 in MatchControl) - actually this varies, using known offset
    // Remove AIDs who have completed SYN-ACK and my own
    // Note: synAckBit location may need verification
    noSynAckAids &= ~(1 << DWC_GetMyAid());

    // Send SYN command periodically
    for (s32 i = 0; i < nodeCount; i++) {
        if ((noSynAckAids >> i) & 1) {
            DWCi_SendMatchSynPacket(static_cast<u8>(i), 1);  // DWC_MATCH_SYN_CMD_SYN
        }
    }
}

void StopNATNEGAfterTime() {
    DWC::MatchControl* matchCnt = *stpMatchCnt;
    if (!matchCnt) return;

    // If at least 11 seconds haven't passed since last state change, bail
    if (OS::TicksToSeconds(OS::GetTime() - *sMatchStateTick) <= 11) return;

    // If we are host, bail
    if (DWC_IsServerMyself()) return;

    // If not in NATNEG state, bail
    DWC::MatchState state = static_cast<DWC::MatchState>(matchCnt->state);
    if (state != DWC::DWC_MATCH_STATE_CL_WAIT_RESV &&
        state != DWC::DWC_MATCH_STATE_CL_NN &&
        state != DWC::DWC_MATCH_STATE_CL_GT2) {
        return;
    }

    // Stop NATNEG and change state
    BOOL ret = DWCi_StopMeshMaking();
    if (ret) {
        DWCi_SetMatchStatus(DWC::DWC_MATCH_STATE_CL_WAITING);
    }
}

u32 Natneg::GetAIDsConnectedToHost() {
    // Return the AID bitmap from DWC
    return DWC_GetAidBitmap();
}

// RKNetController::mainNetworkLoop() - Update NATNEG timers
static void UpdateNatnegTimers(RKNet::Controller* self) {
    self->UpdateSubsAndVR();
    CalcTimers(false);
}
kmCall(0x80657990, UpdateNatnegTimers);

// DWC_InitFriendsMatch() - Replace ConnectedCallback
static void ReplaceConnectedCallback() {
    void* dwcCnt = *stpDwcCnt;
    if (dwcCnt) {
        // cbConnected is at offset 0x4 in DWCContext
        void** cbConnectedPtr = reinterpret_cast<void**>(reinterpret_cast<u8*>(dwcCnt) + 0x4);
        *cbConnectedPtr = reinterpret_cast<void*>(ConnectedCallback);
    }
}
kmBranch(0x800D0FE8, ReplaceConnectedCallback);

// DWCi_MatchedCallback() - Update NATNEG timers with self value
static void UpdateNatnegOnMatched(DWC::Error error, BOOL cancel, BOOL self, BOOL isServer, s32 index, void* param) {
    // Get the callback from match control and call it
    DWC::MatchControl* matchCnt = *stpMatchCnt;
    if (matchCnt && matchCnt->matchedCallback) {
        matchCnt->matchedCallback(error, cancel, self, isServer, index, param);
    }
    CalcTimers(self != 0);
}
kmCall(0x800D3188, UpdateNatnegOnMatched);

// DWCi_PostProcessConnection() - Update connection matrix when new connection is made
kmBranch(0x800E09A8, ConnectionMatrix::Update);

// DWCi_ProcessMatchSynTimeout() - Change SYN-ACK timeout to 7 seconds
kmWrite32(0x800E1A58, 0x38C00000 | 7000);  // li r6, 7000

// NegotiateThink() patch
// Reduce PING retry time from 700 to 80
kmWrite16(0x8011B47A, 80);

// NegotiateThink() patch
// Do not wait the retry time in case of successful NATNEG
kmWrite32(0x8011B4B0, 0x60000000);

// ProcessPingPacket() patch
// Do not wait the idle time after a successful NATNEG
kmWrite16(0x8011BC3A, 0);

}  // namespace FastNATNEG
}  // namespace Network
}  // namespace Pulsar