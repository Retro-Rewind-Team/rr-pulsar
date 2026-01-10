#include <Network/FastNATNEG/FastNATNEG.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <core/rvl/OS/OS.hpp>

/*
    Room Stall Prevention - Ported from OpenPayload/Wiimmfi

    This module detects stalled rooms where a player hasn't received
    data from another player for too long, and schedules a kick.

    The base threshold is 60*60 frames (approximately 60 seconds at 60fps).

    Enhanced with connection quality awareness:
    - Players with poor connection quality get extended thresholds
    - This prevents premature kicks for players with slower/unstable connections

    Original credits: Wiimmfi, CLF78
*/

namespace Pulsar {
namespace Network {
namespace FastNATNEG {
namespace RoomStall {

// Last frame when we received data from each AID
static u64 sLastRecvFrame[12] = {0};

// Get adaptive kick threshold based on connection quality
// Players with poor connections get more time before being kicked
static u64 GetAdaptiveKickThreshold(u8 aid) {
    u64 baseThreshold = KICK_THRESHOLD_TIME;

    // Apply multiplier based on connection quality
    ConnectionQuality::QualityLevel quality = ConnectionQuality::GetQuality(aid);
    switch (quality) {
        case ConnectionQuality::QUALITY_EXCELLENT:
        case ConnectionQuality::QUALITY_GOOD:
            return baseThreshold;
        case ConnectionQuality::QUALITY_MODERATE:
            return (baseThreshold * 5) / 4;  // 1.25x (75 seconds)
        case ConnectionQuality::QUALITY_POOR:
            return (baseThreshold * 3) / 2;  // 1.5x (90 seconds)
        case ConnectionQuality::QUALITY_CRITICAL:
            return baseThreshold * 2;  // 2x (120 seconds)
    }

    return baseThreshold;
}

void Reset() {
    for (int i = 0; i < 12; i++) {
        sLastRecvFrame[i] = 0;
    }
}

void ReceivedFromAID(u8 aid) {
    if (aid >= 12) return;

    // Record current frame
    sLastRecvFrame[aid] = *sMatchStateTick;
}

void CalcStall() {
    RKNet::Controller* rkNetController = RKNet::Controller::sInstance;
    if (!rkNetController) return;

    // Get current AIDs that we should be connected to
    u32 aidsConnectedToHost = Natneg::GetAIDsConnectedToHost();
    if (aidsConnectedToHost == 0) return;

    u64 currentTick = *sMatchStateTick;
    u8 myAid = DWC_GetMyAid();

    // Check each connected AID
    for (u8 aid = 0; aid < 12; aid++) {
        // Skip if not in connected mask
        if (!((aidsConnectedToHost >> aid) & 1)) continue;

        // Skip ourselves
        if (aid == myAid) continue;

        // Skip if we have no record (first time seeing this AID)
        if (sLastRecvFrame[aid] == 0) {
            sLastRecvFrame[aid] = currentTick;
            continue;
        }

        // Get adaptive threshold based on connection quality
        u64 kickThreshold = GetAdaptiveKickThreshold(aid);

        // Check if enough time has passed
        u64 elapsedTime = currentTick - sLastRecvFrame[aid];
        if (elapsedTime >= kickThreshold) {
            // Before kicking, check if this might be a routing issue
            // If they need host routing, give them extra time
            if (ConnectionQuality::ShouldUseHostRouting(aid) && elapsedTime < kickThreshold * 2) {
                continue;  // Don't kick yet, they may need host routing
            }

            // Schedule kick
            Kick::ScheduleForAID(aid);

            // Reset the timer (prevent repeated kicks)
            sLastRecvFrame[aid] = currentTick;

            // Also record this as packet loss for quality tracking
            ConnectionQuality::RecordPacketLost(aid);
        }
    }
}

// Track when data is received from a player
// This is called when RKNet receives data from a player
static void OnRecvDataHook(u8 srcAid) {
    RoomStall::ReceivedFromAID(srcAid);

    // Also record successful packet receipt for quality tracking
    ConnectionQuality::RecordPacketSent(srcAid);
}

// Hook into frame update to check for stalls
static void CalcStallHook() {
    RoomStall::CalcStall();

    // Also update host routing state periodically
    HostRouting::UpdateRoutingState();
}
kmBranch(0x806579A0, CalcStallHook);

}  // namespace RoomStall
}  // namespace FastNATNEG
}  // namespace Network
}  // namespace Pulsar