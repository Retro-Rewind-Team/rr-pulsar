#include <Network/FastNATNEG/FastNATNEG.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <core/rvl/OS/OS.hpp>

/*
    Host Routing Module - Fallback for poor direct connections

    This module provides a fallback mechanism for players who cannot establish
    stable direct connections with other players. When a player has repeatedly
    failed to connect directly (tracked by ConnectionQuality), their packets
    can be routed through the host.

    How it works:
    1. ConnectionQuality tracks failed connection attempts per AID
    2. After MAX_FAILED_ATTEMPTS (3), the player is marked for host routing
    3. When sending data to that player, packets are wrapped with RelayHeader
       and sent to the host using a custom match command (DWC_MATCH_CMD_HOST_RELAY)
    4. The host unwraps and forwards the packet to the destination

    This is particularly helpful for:
    - Players behind strict NATs (Symmetric NAT)
    - Players with unstable connections that cause NATNEG timeouts
    - Scenarios where direct P2P hole-punching repeatedly fails

    Note: Host routing adds latency (data goes client->host->client instead of
    client->client) but provides a stable fallback for otherwise unplayable
    connections.

    Original concept inspired by Wiimmfi's connection handling.
*/

namespace Pulsar {
namespace Network {
namespace FastNATNEG {
namespace HostRouting {

// Bitmask of AIDs that need host routing
u32 sHostRoutedAids = 0;

// Maximum relay packet size (headers + payload)
static const u32 MAX_RELAY_PACKET_SIZE = 512;

void Reset() {
    sHostRoutedAids = 0;
}

// Check if an AID should use host routing
static bool NeedsHostRouting(u8 aid) {
    if (aid >= 12) return false;
    return (sHostRoutedAids >> aid) & 1;
}

// Mark an AID as needing host routing
static void SetNeedsHostRouting(u8 aid, bool needs) {
    if (aid >= 12) return;

    if (needs) {
        sHostRoutedAids |= (1 << aid);
    } else {
        sHostRoutedAids &= ~(1 << aid);
    }
}

// Send data to another player via the host
// This is used when direct connections repeatedly fail
void SendViaHost(u8 dstAid, const void* data, u32 dataLen) {
    DWC::MatchControl* matchCnt = *stpMatchCnt;
    if (!matchCnt) return;

    // Validate destination
    if (dstAid >= 12 || dataLen == 0 || data == nullptr) return;

    // Don't route if we are the host
    if (DWC_IsServerMyself()) return;

    // Don't route if data is too large
    if (dataLen + sizeof(RelayHeader) > MAX_RELAY_PACKET_SIZE) return;

    // Get host node info
    DWCNodeInfo* hostNode = DWCi_NodeInfoList_GetServerNodeInfo();
    if (!hostNode) return;

    // Build relay packet: RelayHeader + payload
    u8 relayBuffer[MAX_RELAY_PACKET_SIZE];
    RelayHeader* header = reinterpret_cast<RelayHeader*>(relayBuffer);

    header->srcAid = DWC_GetMyAid();
    header->dstAid = dstAid;
    header->dataLen = static_cast<u16>(dataLen);

    // Copy payload after header
    memcpy(relayBuffer + sizeof(RelayHeader), data, dataLen);

    // Send via match command to host
    u32 totalLen = sizeof(RelayHeader) + dataLen;
    DWCi_SendMatchCommand(DWC_MATCH_CMD_HOST_RELAY, hostNode->profileId,
                          hostNode->gt2Ip, hostNode->gt2Port,
                          relayBuffer, totalLen);
}

// Process an incoming relay packet (host only)
// Returns true if packet was processed as a relay, false otherwise
bool ProcessRelayPacket(u8 srcAid, const void* data, u32 dataLen) {
    // Only the host processes relay packets
    if (!DWC_IsServerMyself()) return false;

    // Validate minimum size
    if (dataLen < sizeof(RelayHeader)) return false;

    const RelayHeader* header = reinterpret_cast<const RelayHeader*>(data);

    // Validate header fields
    if (header->srcAid >= 12 || header->dstAid >= 12) return false;
    if (header->dataLen == 0 || header->dataLen + sizeof(RelayHeader) > dataLen) return false;

    // Get the actual payload
    const u8* payload = reinterpret_cast<const u8*>(data) + sizeof(RelayHeader);
    u32 payloadLen = header->dataLen;

    // Get destination node info
    DWCNodeInfo* dstNode = DWCi_NodeInfoList_GetNodeInfoForAid(header->dstAid);
    if (!dstNode) return false;

    // Forward the packet to the destination, marking the original source
    u8 forwardBuffer[MAX_RELAY_PACKET_SIZE];
    RelayHeader* fwdHeader = reinterpret_cast<RelayHeader*>(forwardBuffer);

    fwdHeader->srcAid = header->srcAid;  // Preserve original source
    fwdHeader->dstAid = header->dstAid;
    fwdHeader->dataLen = static_cast<u16>(payloadLen);

    memcpy(forwardBuffer + sizeof(RelayHeader), payload, payloadLen);

    u32 totalLen = sizeof(RelayHeader) + payloadLen;
    DWCi_SendMatchCommand(DWC_MATCH_CMD_HOST_RELAY, dstNode->profileId,
                          dstNode->gt2Ip, dstNode->gt2Port,
                          forwardBuffer, totalLen);

    return true;
}

// Update routing state based on connection quality
// Called periodically to check if routing needs have changed
void UpdateRoutingState() {
    u32 connectedAids = DWC_GetAidBitmap();
    u8 myAid = DWC_GetMyAid();

    for (u8 aid = 0; aid < 12; aid++) {
        // Skip if not connected or is ourselves
        if (!((connectedAids >> aid) & 1)) continue;
        if (aid == myAid) continue;

        // Check if this AID needs host routing based on connection quality
        bool needsRouting = ConnectionQuality::ShouldUseHostRouting(aid);
        SetNeedsHostRouting(aid, needsRouting);
    }
}

// Hook into match command processing for relay packets
// This extends DWCi_ProcessRecvMatchCommand to handle our custom relay command
static bool ProcessHostRelayCommand(u8 cmd, u32 pid, u32 ip, u16 port, void* data, u32 dataLen) {
    if (cmd == HostRouting::DWC_MATCH_CMD_HOST_RELAY) {
        // Get source AID from PID
        DWCNodeInfo* srcNode = DWCi_NodeInfoList_GetNodeInfoForProfileId(pid);
        if (srcNode) {
            return HostRouting::ProcessRelayPacket(srcNode->aid, data, dataLen);
        }
        return false;
    }

    // Not our command, let the original handler process it
    return DWCi_ProcessRecvMatchCommand(cmd, pid, ip, port, data, dataLen);
}

// Periodic update hook for routing state
static void UpdateHostRoutingHook() {
    HostRouting::UpdateRoutingState();
}

// Reset host routing when connection state changes
static void ResetHostRoutingOnMatchStart() {
    HostRouting::Reset();
    ConnectionQuality::Reset();
}

}  // namespace HostRouting
}  // namespace FastNATNEG
}  // namespace Network
}  // namespace Pulsar