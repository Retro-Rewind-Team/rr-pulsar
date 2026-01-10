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
#include <core/GS/GT2/GT2.hpp>

/*
    FastNATNEG - Network lag stability improvements ported from OpenPayload/Wiimmfi

    This module provides:
    - Fast NATNEG: Improves mesh connection establishment by attempting direct GT2 connections
    - Delay compensation: Calculates and compensates for network frame delays
    - Connection matrix: Tracks connection status between clients
    - Room stall prevention: Kicks stalled players to prevent room freezes

    Original credits: Wiimmfi, WiiLink24, MrBean35000vr, CLF78
*/

namespace Pulsar {
namespace Network {
namespace FastNATNEG {

// Use DWC::MatchState from DWCMatch.hpp
// Additional states not in the original header
static const int DWC_MATCH_STATE_CL_SVDOWN_1 = 8;
static const int DWC_MATCH_STATE_CL_SVDOWN_2 = 9;
static const int DWC_MATCH_STATE_CL_SVDOWN_3 = 10;
static const int DWC_MATCH_STATE_CL_SEARCH_GROUPID_HOST = 11;

// Custom match commands for NATNEG
enum DWCMatchCommandCustom {
    DWC_MATCH_CMD_CONN_FAIL_MTX = 0xDC,
    DWC_MATCH_CMD_CONN_MTX = 0xDD,
};

// DWC Node Info structure - extended version with more fields
// Note: DWC::NodeInfo in DWCMatch.hpp is 0x30 bytes, this adds more details
struct DWCNodeInfo {
    u32 profileId;  // 0x00 Player's profile ID
    u32 publicip;  // 0x04 Public IP address
    u32 localIp;  // 0x08 Local IP address
    u16 publicport;  // 0x0C Public port
    u16 localPort;  // 0x0E Local port
    u32 gt2Ip;  // 0x10 GT2 IP address
    u16 gt2Port;  // 0x14 GT2 port
    u8 aid;  // 0x16 Player's AID
    u8 hasPrivateAddress;  // 0x17
    s32 nnTryCount;  // 0x18 NATNEG retry count
    u8 padding[4];  // 0x1C
    s64 nextMeshMakeTryTick;  // 0x20 Next mesh making attempt time
    u8 connectionUserData[4];  // 0x28
    u32 pad2;  // 0x2C
};  // size 0x30

// DWC Node Info List
struct DWCNodeInfoList {
    s32 nodeCount;
    u32 pad;
    DWCNodeInfo nodeInfos[32];
};  // 0x608

// Custom match command data for connection fail matrix
struct DWCMatchCommandConnFailMtx {
    u32 connFailMtx;
};

// Custom match command data for connection matrix
struct DWCMatchCommandConnMtx {
    u32 connMtx;
};

// Custom match command for NEW_PID_AID (byte-swapped)
struct DWCMatchCommandNewPidAid {
    u32 pid;
    u32 aid;
};

// DWC Connection Info
struct DWCConnectionInfo {
    u8 index;
    u8 aid;
    u16 pad;
    u32 profileId;
};

// GT2 Connection with custom AID field
struct GT2ConnectionCustom : GT2::Connection {
    // Note: The aid field is added at offset 0x6 in the original struct
    // We override it in our callbacks
};

// External DWC functions
extern "C" {
// DWC functions
BOOL DWC_IsServerMyself();
u8 DWC_GetMyAid();
u8 DWC_GetServerAid();
u32 DWC_GetAidBitmap();
s32 DWC_CloseConnectionHard(u8 aid);
s32 DWC_CloseAllConnectionsHard();

// DWCi internal functions
GT2::Connection* DWCi_GetGT2Connection(u8 aid);
GT2::Connection** DWCi_GetGT2ConnectionByIdx(s32 idx);
s32 DWCi_GT2GetConnectionListIdx();
DWCConnectionInfo* DWCi_GetConnectionInfoByIdx(s32 idx);
DWCNodeInfo* DWCi_NodeInfoList_GetNodeInfoForAid(u8 aid);
DWCNodeInfo* DWCi_NodeInfoList_GetNodeInfoForProfileId(u32 pid);
DWCNodeInfo* DWCi_NodeInfoList_GetServerNodeInfo();
void DWCi_ResetLastSomeDataRecvTimeByAid(u8 aid);
void DWCi_PostProcessConnection(s32 type);
BOOL DWCi_StopMeshMaking();
void DWCi_SetMatchStatus(DWC::MatchState status);
void DWCi_SendMatchSynPacket(u8 aid, u16 type);
s32 DWCi_SendMatchCommand(u8 cmd, u32 pid, u32 ip, u16 port, void* data, u32 dataSize);
s64 DWCi_GetNextMeshMakeTryTick();
BOOL DWCi_ProcessRecvMatchCommand(u8 cmd, u32 pid, u32 ip, u16 port, void* data, u32 dataSize);

// GT2 functions
GT2::Result gt2Connect(GT2::Socket socket, GT2::Connection** connection, const char* remoteAddress,
                       const char* msg, s32 msgLen, s32 timeout, GT2::ConnectionCallbacks* callbacks, s32 blocking);
BOOL gt2Accept(GT2::Connection* connection, GT2::ConnectionCallbacks* callbacks);
void gt2Reject(GT2::Connection* connection, const char* msg, s32 msgLen);
void gt2Listen(GT2::Socket socket, GT2::ConnectAttemptCallback callback);
const char* gt2AddressToString(u32 ip, u16 port, char* string);
void gt2SetConnectionData(GT2::Connection* connection, void* data);
void* gt2GetConnectionData(GT2::Connection* connection);

// OS functions
u32 current_time();

// VI functions
void VIWaitForRetrace();

// String functions
char* strstr(const char* haystack, const char* needle);
s32 strcmp(const char* s1, const char* s2);
u32 strtoul(const char* str, char** endptr, s32 base);

// DWCi_SendGPBuddyMsgCommand - sends match commands via GP or GT2
s32 DWCi_SendGPBuddyMsgCommand(u8 cmd, u32 pid, u32 ip, u16 port, void* data, u32 dataSize);

// Original callbacks (for fallback)
void DWCi_GT2ConnectAttemptCallback(GT2::Socket* socket, GT2::Connection* conn, u32 ip, u16 port,
                                    s32 latency, const char* msg, s32 msgLen);
void DWCi_GT2ConnectedCallback(GT2::Connection* conn, GT2::Result result, const char* msg, s32 msgLen);
}

// Pointers to DWC structures
extern DWC::MatchControl** stpMatchCnt;
extern void** stpDwcCnt;  // Address of pointer to DWCContext (callbacks at offset 0x4 of DWCContext)
extern s64* sMatchStateTick;

// NATNEG Module functions
namespace Natneg {
void CalcTimers(bool connectedToHost);
void ConnectAttemptCallback(GT2::Socket* socket, GT2::Connection* conn, u32 ip, u16 port,
                            s32 latency, const char* msg, s32 msgLen);
void ConnectedCallback(GT2::Connection* conn, GT2::Result result, const char* msg, s32 msgLen);
DWCNodeInfo* GetNextMeshMakingNode();
bool PreventRepeatNATNEGFail(u32 failedPid);
void RecoverSynAckTimeout();
void StopNATNEGAfterTime();
u32 GetAIDsConnectedToHost();

extern u16 sTimers[12];
}  // namespace Natneg

// Delay Module functions
namespace Delay {
void Reset();
void Calc(u32 frameCount);
u32 Apply(u32 timer);

extern u32 sMatchStartTime;
extern u32 sCumulativeDelay;
extern u32 sCurrentDelay;
}  // namespace Delay

// Connection Matrix Module functions
namespace ConnectionMatrix {
void ResetRecv();
void Update();

extern u32 sRecvConnMtx[12];
}  // namespace ConnectionMatrix

// Match Command Module functions
namespace MatchCommand {
bool ProcessRecvMatchCommand(u8 cmd, u32 pid, u32 ip, u16 port, void* data, u32 dataSize);
void SendConnFailMtxCommand(u32 aidsConnectedToHost, u32 aidsConnectedToMe);
void SendConnMtxCommand(u32 aidsConnectedToMe);
}  // namespace MatchCommand

// Room Stall Module functions
namespace RoomStall {
static const u32 KICK_THRESHOLD_TIME = 60 * 60;  // 60 seconds at 60fps

void Reset();
void ReceivedFromAID(u8 aid);
void CalcStall();

extern u64 sLastRecvFrame[12];
}  // namespace RoomStall

// Kick Module functions
namespace Kick {
void ScheduleForAID(u8 aid);
void ScheduleForAIDs(u32 aidMask);
void Reset();
u32 GetScheduled();
void CalcKick();

extern u32 sScheduledKicks;
}  // namespace Kick

// Connection Quality Module - Tracks connection health for stability improvements
namespace ConnectionQuality {

// Quality levels for adaptive behavior
enum QualityLevel {
    QUALITY_EXCELLENT = 0,  // RTT < 50ms, no packet loss
    QUALITY_GOOD = 1,  // RTT < 100ms, < 5% packet loss
    QUALITY_MODERATE = 2,  // RTT < 200ms, < 10% packet loss
    QUALITY_POOR = 3,  // RTT < 400ms, < 20% packet loss
    QUALITY_CRITICAL = 4  // RTT >= 400ms or >= 20% packet loss
};

// Per-AID connection quality data
struct ConnectionStats {
    u32 rtt;  // Current RTT in milliseconds
    u32 rttAvg;  // Average RTT (smoothed)
    u32 packetsSent;  // Total packets sent
    u32 packetsLost;  // Packets lost/not acknowledged
    u8 failedAttempts;  // Consecutive failed connection attempts
    u8 quality;  // Current quality level
    bool useHostRouting;  // Whether to route through host
};

void Reset();
void ResetForAID(u8 aid);
void UpdateRTT(u8 aid, u32 rttMs);
void RecordPacketSent(u8 aid);
void RecordPacketLost(u8 aid);
void RecordConnectionAttempt(u8 aid, bool success);
QualityLevel GetQuality(u8 aid);
u32 GetAdaptiveTimeout(u8 aid);
u32 GetAdaptiveRetryDelay(u8 aid);
bool ShouldUseHostRouting(u8 aid);
void SetHostRoutingRequired(u8 aid, bool required);

extern ConnectionStats sStats[12];
}  // namespace ConnectionQuality

// Host Routing Module - Fallback for players who cannot establish direct connections
namespace HostRouting {

// Custom match command for host-routed data
static const u8 DWC_MATCH_CMD_HOST_RELAY = 0xDE;

// Relay packet header
struct RelayHeader {
    u8 srcAid;  // Source AID
    u8 dstAid;  // Destination AID
    u16 dataLen;  // Payload length
};

void Reset();
void SendViaHost(u8 dstAid, const void* data, u32 dataLen);
bool ProcessRelayPacket(u8 srcAid, const void* data, u32 dataLen);
void UpdateRoutingState();

extern u32 sHostRoutedAids;  // Bitmask of AIDs that need host routing
}  // namespace HostRouting

}  // namespace FastNATNEG
}  // namespace Network
}  // namespace Pulsar

#endif