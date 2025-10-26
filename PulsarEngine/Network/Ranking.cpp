#include <RetroRewind.hpp>
#include <Network/Ranking.hpp>
#include <MarioKartWii/Race/Racedata.hpp>
#include <MarioKartWii/Race/Raceinfo/Raceinfo.hpp>
#include <MarioKartWii/RKSYS/LicenseMgr.hpp>
#include <MarioKartWii/System/Identifiers.hpp>
#include <MarioKartWii/RKSYS/RKSYSMgr.hpp>
#include <MarioKartWii/GlobalFunctions.hpp>
#include <MarioKartWii/System/Rating.hpp>
#include <MarioKartWii/RKNet/USER.hpp>
#include <runtimeWrite.hpp>
#include <core/rvl/OS/OS.hpp>

namespace Pulsar {
namespace Ranking {

// Friend Code override list: players in this list get the priority badge (value 10)
// Leafstruck Tournament Winners
static const u64 PRIORITY_BADGE_FC_LIST[] = {
    227777272227ULL,  // Roshi
    434334343434ULL  // Empex
};

static bool IsPriorityBadgeFC(u64 fc) {
    if (fc == 0) return false;
    for (size_t i = 0; PRIORITY_BADGE_FC_LIST[i] != 0ULL; ++i) {
        if (PRIORITY_BADGE_FC_LIST[i] == fc) return true;
    }
    return false;
}

// Compute final numeric score (0..100) and return both score and rank via out params
static float ComputeVsScoreFromLicense(const RKSYS::LicenseMgr& license) {
    // Collect raw license statistics
    const Rating& vr = license.GetVR();
    u32 vsWins = license.GetWFCVSWins();
    u32 vsLosses = license.GetWFCVSLosses();
    u32 totalVs = vsWins + vsLosses;

    // Additional metrics
    u32 times1st = license.GetTimes1stPlaceAchieved();
    float distTravelled = license.GetDistanceTravelled();
    float distInFirst = license.GetDistancetravelledwhilein1stplace();

    // Compute win percentages (0-100). If insufficient data (<1 match), treat as 50% neutral baseline.
    float racingWinPct = (totalVs > 0) ? (100.0f * (float)vsWins / (float)totalVs) : 45.0f;

    // Normalize ratings (expected range 1-30000). Clamp to [0,30000] just in case.
    float vrClamped = (float)(vr.points > 30000 ? 30000 : vr.points);
    if (vrClamped < 0) vrClamped = 0;  // safety
    float vrNorm = (vrClamped / 30000.0f) * 100.0f;

    // Normalize added metrics to 0-100
    float firstsNorm = (times1st >= 1500) ? 100.0f : (100.0f * (float)times1st / 1250.0f);
    float distNorm = (distTravelled >= 75000.0f) ? 100.0f : (100.0f * distTravelled / 50000.0f);
    float distFirstNorm = (distInFirst >= 20000.0f) ? 100.0f : (100.0f * distInFirst / 10000.0f);

    // Weighted composite emphasizing VR and VS performance; battle stats removed.
    // Base weights (sum = 1.0): VR 0.60, RWin 0.20, Firsts 0.10, Dist 0.05, DistInFirst 0.05
    // Then apply affine scaling so that high/low anchors map to 100/10.
    const float W_VR = 0.60f;
    const float W_RWIN = 0.20f;
    const float W_FIRSTS = 0.10f;
    const float W_DIST = 0.05f;
    const float W_DIST1ST = 0.05f;

    float baseM = (W_VR * vrNorm) + (W_RWIN * racingWinPct) + (W_FIRSTS * firstsNorm) + (W_DIST * distNorm) + (W_DIST1ST * distFirstNorm);

    // Anchors
    const float AH_VR = 100.0f, AH_RWIN = 65.0f, AH_FIRSTS = 100.0f, AH_DIST = 100.0f, AH_DIST1ST = 100.0f;  // -> 100
    const float AL_VR = 16.6667f, AL_RWIN = 50.0f, AL_FIRSTS = 0.0f, AL_DIST = 0.0f, AL_DIST1ST = 0.0f;  // -> 10
    float M1 = W_VR * AH_VR + W_RWIN * AH_RWIN + W_FIRSTS * AH_FIRSTS + W_DIST * AH_DIST + W_DIST1ST * AH_DIST1ST;  // ~93.0
    float M2 = W_VR * AL_VR + W_RWIN * AL_RWIN + W_FIRSTS * AL_FIRSTS + W_DIST * AL_DIST + W_DIST1ST * AL_DIST1ST;  // ~20.0
    float alpha = 90.0f / (M1 - M2);  // 100-10 over delta
    float beta = 100.0f - alpha * M1;

    float finalScore = alpha * baseM + beta;
    if (finalScore < 0.0f)
        finalScore = 0.0f;
    else if (finalScore > 100.0f)
        finalScore = 100.0f;
    return finalScore;
}

static int ScoreToRank(float finalScore) {
    if (finalScore >= 100.0f) return 9;
    if (finalScore >= 94.0f) return 8;
    if (finalScore >= 84.0f) return 7;
    if (finalScore >= 72.0f) return 6;
    if (finalScore >= 60.0f) return 5;
    if (finalScore >= 48.0f) return 4;
    if (finalScore >= 36.0f) return 3;
    if (finalScore >= 24.0f) return 2;
    if (finalScore >= 12.0f) return 1;
    return 0;
}

int GetCurrentLicenseRankVS() {
    const RKSYS::Mgr* rksysMgr = RKSYS::Mgr::sInstance;
    if (rksysMgr == nullptr || rksysMgr->curLicenseId < 0) return -1;
    const RKSYS::LicenseMgr& license = rksysMgr->licenses[rksysMgr->curLicenseId];
    float score = ComputeVsScoreFromLicense(license);
    return ScoreToRank(score);
}

int GetCurrentLicenseScore() {
    const RKSYS::Mgr* rksysMgr = RKSYS::Mgr::sInstance;
    if (rksysMgr == nullptr || rksysMgr->curLicenseId < 0) return -1;
    const RKSYS::LicenseMgr& license = rksysMgr->licenses[rksysMgr->curLicenseId];
    float score = ComputeVsScoreFromLicense(license);
    return static_cast<int>(score + 0.5f);
}

int FormatRankMessage(wchar_t* dst, size_t dstLen) {
    if (dst == nullptr || dstLen == 0) return -1;
    int rank = GetCurrentLicenseRankVS();
    int score = GetCurrentLicenseScore();
    if (rank < 0) rank = 0;  // default if unavailable
    if (score < 0) score = 0;
    // Map rank numbers to requested display characters.
    // Rank: 0 -> "0"
    // Rank: 1..9 -> U+F07D .. U+F085 respectively
    const wchar_t* rankLabel = L"0";
    switch (rank) {
        case 1:
            rankLabel = L"\uF07D";
            break;
        case 2:
            rankLabel = L"\uF07E";
            break;
        case 3:
            rankLabel = L"\uF07F";
            break;
        case 4:
            rankLabel = L"\uF080";
            break;
        case 5:
            rankLabel = L"\uF081";
            break;
        case 6:
            rankLabel = L"\uF082";
            break;
        case 7:
            rankLabel = L"\uF083";
            break;
        case 8:
            rankLabel = L"\uF084";
            break;
        case 9:
            rankLabel = L"\uF085";
            break;
        default:
            rankLabel = L"0";
            break;
    }

    // Format two-line message: Rank: <label>\nScore: <score>
    return ::swprintf(dst, dstLen, L"Rank: %ls\nScore: %d", rankLabel, score);
}

// Address found by B_squo, original idea by Zeraora, developed by ZPL
kmRuntimeUse(0x806436a0);
static void DisplayOnlineRanking() {
    // Default to rank 0
    kmRuntimeWrite32A(0x806436a0, 0x38600000);  // li r3,0

    // Priority badge override for specific friend codes, takes precedence over any ranking
    // Source for friend code: RKNet::USERHandler::toSendPacket.fc (local player's FC)
    // Provenance: structure defined in GameSource/MarioKartWii/RKNet/USER.hpp
    if (RKNet::USERHandler::sInstance != nullptr && RKNet::USERHandler::sInstance->isInitialized) {
        const u64 myFc = RKNet::USERHandler::sInstance->toSendPacket.fc;
        if (IsPriorityBadgeFC(myFc)) {
            kmRuntimeWrite32A(0x806436a0, 0x3860000A);  // li r3,10
            return;
        }
    }

    const RacedataSettings& racedataSettings = Racedata::sInstance->menusScenario.settings;
    const GameMode mode = racedataSettings.gamemode;
    if (mode != MODE_PUBLIC_VS) return;
    int rank = GetCurrentLicenseRankVS();
    if (rank < 0) rank = 0;

    // Write rank back by patching target instruction (li r3, rank)
    // Base opcode for li r3,imm is 0x3860000X where immediate fits 16 bits (rank 0-8 fits easily)
    u32 opcode = 0x38600000 | (rank & 0xFFFF);
    kmRuntimeWrite32A(0x806436a0, opcode);
}
static SectionLoadHook HookRankIcon(DisplayOnlineRanking);

}  // namespace Ranking
}  // namespace Pulsar