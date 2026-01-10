#include <Network/FastNATNEG/FastNATNEG.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <core/rvl/OS/OS.hpp>

namespace Pulsar {
namespace Network {
namespace FastNATNEG {
namespace ConnectionQuality {

// Connection statistics array, one per AID (max 12 players)
ConnectionStats sStats[12];

// RTT thresholds in milliseconds for quality levels
static const u32 RTT_EXCELLENT = 50;
static const u32 RTT_GOOD = 100;
static const u32 RTT_MODERATE = 200;
static const u32 RTT_POOR = 400;

// Packet loss thresholds (percentage * 100, so 500 = 5%)
static const u32 LOSS_EXCELLENT = 200;   // 2%
static const u32 LOSS_GOOD = 500;        // 5%
static const u32 LOSS_MODERATE = 1000;   // 10%
static const u32 LOSS_POOR = 2000;       // 20%

// Maximum failed connection attempts before suggesting host routing
static const u8 MAX_FAILED_ATTEMPTS_FOR_HOST_ROUTING = 3;

// Base timeout in milliseconds
static const u32 BASE_TIMEOUT_MS = 2000;

// Smoothing factor for RTT average (1/4 weight for new sample)
static const u32 RTT_SMOOTHING_FACTOR = 4;

void Reset() {
    for (int i = 0; i < 12; i++) {
        ResetForAID(static_cast<u8>(i));
    }
}

void ResetForAID(u8 aid) {
    if (aid >= 12) return;

    ConnectionStats& stats = sStats[aid];
    stats.rtt = 0;
    stats.rttAvg = 0;
    stats.packetsSent = 0;
    stats.packetsLost = 0;
    stats.failedAttempts = 0;
    stats.quality = QUALITY_GOOD;  // Start with good assumption
    stats.useHostRouting = false;
}

// Update RTT measurement for an AID using exponential smoothing
void UpdateRTT(u8 aid, u32 rttMs) {
    if (aid >= 12) return;

    ConnectionStats& stats = sStats[aid];
    stats.rtt = rttMs;

    // Exponential smoothing: avg = (avg * (n-1) + new) / n
    if (stats.rttAvg == 0) {
        stats.rttAvg = rttMs;
    } else {
        stats.rttAvg = ((stats.rttAvg * (RTT_SMOOTHING_FACTOR - 1)) + rttMs) / RTT_SMOOTHING_FACTOR;
    }

    // Recalculate quality level
    stats.quality = static_cast<u8>(GetQuality(aid));
}

void RecordPacketSent(u8 aid) {
    if (aid >= 12) return;
    sStats[aid].packetsSent++;
}

void RecordPacketLost(u8 aid) {
    if (aid >= 12) return;
    sStats[aid].packetsLost++;

    // Recalculate quality level
    sStats[aid].quality = static_cast<u8>(GetQuality(aid));
}

void RecordConnectionAttempt(u8 aid, bool success) {
    if (aid >= 12) return;

    ConnectionStats& stats = sStats[aid];

    if (success) {
        // Reset failed attempts on success
        stats.failedAttempts = 0;
        stats.useHostRouting = false;
    } else {
        // Increment failed attempts, capped at 255
        if (stats.failedAttempts < 255) {
            stats.failedAttempts++;
        }

        // After too many failures, suggest host routing
        if (stats.failedAttempts >= MAX_FAILED_ATTEMPTS_FOR_HOST_ROUTING) {
            stats.useHostRouting = true;
        }
    }
}

// Calculate quality level based on RTT and packet loss
QualityLevel GetQuality(u8 aid) {
    if (aid >= 12) return QUALITY_MODERATE;

    const ConnectionStats& stats = sStats[aid];

    // Calculate packet loss percentage * 100 (to avoid floats)
    u32 lossPercent = 0;
    if (stats.packetsSent > 0) {
        lossPercent = (stats.packetsLost * 10000) / stats.packetsSent;
    }

    u32 rtt = stats.rttAvg;

    // Determine quality based on both RTT and packet loss
    // Use the worse of the two metrics
    QualityLevel rttQuality;
    if (rtt < RTT_EXCELLENT) rttQuality = QUALITY_EXCELLENT;
    else if (rtt < RTT_GOOD) rttQuality = QUALITY_GOOD;
    else if (rtt < RTT_MODERATE) rttQuality = QUALITY_MODERATE;
    else if (rtt < RTT_POOR) rttQuality = QUALITY_POOR;
    else rttQuality = QUALITY_CRITICAL;

    QualityLevel lossQuality;
    if (lossPercent < LOSS_EXCELLENT) lossQuality = QUALITY_EXCELLENT;
    else if (lossPercent < LOSS_GOOD) lossQuality = QUALITY_GOOD;
    else if (lossPercent < LOSS_MODERATE) lossQuality = QUALITY_MODERATE;
    else if (lossPercent < LOSS_POOR) lossQuality = QUALITY_POOR;
    else lossQuality = QUALITY_CRITICAL;

    // Return the worse quality level
    return rttQuality > lossQuality ? rttQuality : lossQuality;
}

// Get adaptive timeout based on connection quality
// Poor connections get longer timeouts to prevent premature disconnections
u32 GetAdaptiveTimeout(u8 aid) {
    if (aid >= 12) return BASE_TIMEOUT_MS;

    const ConnectionStats& stats = sStats[aid];
    QualityLevel quality = static_cast<QualityLevel>(stats.quality);

    // Base timeout multiplied by quality factor
    u32 timeout = BASE_TIMEOUT_MS;

    switch (quality) {
        case QUALITY_EXCELLENT:
            timeout = BASE_TIMEOUT_MS;
            break;
        case QUALITY_GOOD:
            timeout = BASE_TIMEOUT_MS + 500;  // 2.5s
            break;
        case QUALITY_MODERATE:
            timeout = BASE_TIMEOUT_MS + 1000;  // 3s
            break;
        case QUALITY_POOR:
            timeout = BASE_TIMEOUT_MS + 2000;  // 4s
            break;
        case QUALITY_CRITICAL:
            timeout = BASE_TIMEOUT_MS + 4000;  // 6s
            break;
    }

    // Add RTT-based padding (2x average RTT)
    if (stats.rttAvg > 0) {
        timeout += stats.rttAvg * 2;
    }

    // Cap at 10 seconds
    if (timeout > 10000) timeout = 10000;

    return timeout;
}

// Get adaptive retry delay in frames (at 60fps)
// Failed connections get exponential backoff
u32 GetAdaptiveRetryDelay(u8 aid) {
    if (aid >= 12) return 300;  // 5 seconds default

    const ConnectionStats& stats = sStats[aid];
    QualityLevel quality = static_cast<QualityLevel>(stats.quality);

    // Base delay in frames (60fps)
    u32 baseDelay = 180;  // 3 seconds

    // Quality-based multiplier
    switch (quality) {
        case QUALITY_EXCELLENT:
        case QUALITY_GOOD:
            baseDelay = 180;
            break;
        case QUALITY_MODERATE:
            baseDelay = 240;
            break;
        case QUALITY_POOR:
            baseDelay = 300;
            break;
        case QUALITY_CRITICAL:
            baseDelay = 420;
            break;
    }

    // Exponential backoff based on failed attempts
    if (stats.failedAttempts > 0) {
        u32 backoff = 1 << (stats.failedAttempts > 3 ? 3 : stats.failedAttempts);
        baseDelay *= backoff;
    }

    // Cap at 30 seconds
    if (baseDelay > 1800) baseDelay = 1800;

    return baseDelay;
}

bool ShouldUseHostRouting(u8 aid) {
    if (aid >= 12) return false;
    return sStats[aid].useHostRouting;
}

void SetHostRoutingRequired(u8 aid, bool required) {
    if (aid >= 12) return;
    sStats[aid].useHostRouting = required;
}

// Hook into connection establishment to track RTT
// Uses GT2 ping latency when available
static void UpdateRTTFromPing(u8 aid, u32 latencyMs) {
    ConnectionQuality::UpdateRTT(aid, latencyMs);
}

// Reset connection quality tracking when entering online mode
static void ResetConnectionQualityOnMatch() {
    ConnectionQuality::Reset();
}
// This will be called via BootHook or other initialization

}  // namespace ConnectionQuality
}  // namespace FastNATNEG
}  // namespace Network
}  // namespace Pulsar