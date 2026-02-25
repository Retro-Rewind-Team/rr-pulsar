/*
    Ranking.cpp
    Copyright (C) 2025 ZPL

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <RetroRewind.hpp>
#include <Network/Ranking.hpp>
#include <MarioKartWii/Race/Racedata.hpp>
#include <MarioKartWii/Race/Raceinfo/Raceinfo.hpp>
#include <MarioKartWii/RKSYS/LicenseMgr.hpp>
#include <MarioKartWii/System/Identifiers.hpp>
#include <MarioKartWii/RKSYS/RKSYSMgr.hpp>
#include <MarioKartWii/GlobalFunctions.hpp>
#include <MarioKartWii/System/Rating.hpp>
#include <Network/Rating/PlayerRating.hpp>
#include <MarioKartWii/RKNet/USER.hpp>
#include <Settings/SettingsParam.hpp>
#include <runtimeWrite.hpp>
#include <core/rvl/OS/OS.hpp>

namespace Pulsar {
namespace Ranking {

// Developers
static const u64 PRIORITY_BADGE_FC_LIST[] = {
    8906072005ULL,  // ZPL
    100000011ULL,  // Dynohack
    348484848484ULL,  // Gab
    464464664446ULL,  // Weebo
    331616161616ULL,  // Noel
    464666644446ULL,  // Eppe
    121212121245ULL,  // Bodacious
    506980546757ULL,  // Cyrus
    421507127201ULL,  // Jacher
    800580ULL,  // Rambo
    81604380197ULL,  // Patchzy
    400032268799ULL,  // Wrkus
    0ULL};

// FOR THE COLONY
static const u64 ANT_BADGE_FC_LIST[] = {
    65400900000ULL,  // Fenixien
    116565656533ULL,  // ImZeraora
    524266609436ULL,  // TheBeefBai
    0ULL};

static bool IsPriorityBadgeFC(u64 fc) {
    if (fc == 0) return false;
    for (size_t i = 0; PRIORITY_BADGE_FC_LIST[i] != 0ULL; ++i) {
        if (PRIORITY_BADGE_FC_LIST[i] == fc) return true;
    }
    return false;
}

static bool IsAntBadgeFC(u64 fc) {
    if (fc == 0) return false;
    for (size_t i = 0; ANT_BADGE_FC_LIST[i] != 0ULL; ++i) {
        if (ANT_BADGE_FC_LIST[i] == fc) return true;
    }
    return false;
}

static float ComputeVsScoreFromLicense(const RKSYS::LicenseMgr& license) {
    const Rating& vr = license.GetVR();
    u32 vsWins = license.GetWFCVSWins();
    u32 vsLosses = license.GetWFCVSLosses();
    u32 totalVs = vsWins + vsLosses;

    u32 times1st = license.GetTimes1stPlaceAchieved();
    float distTravelled = license.GetDistanceTravelled();
    float distInFirst = license.GetDistancetravelledwhilein1stplace();

    float racingWinPct = (totalVs > 0) ? (100.0f * (float)vsWins / (float)totalVs) : 45.0f;

    float vrClamped = (float)(PointRating::GetUserVR(RKSYS::Mgr::sInstance->curLicenseId) > 1000.0f ? 1000.0f : PointRating::GetUserVR(RKSYS::Mgr::sInstance->curLicenseId));
    if (vrClamped < 0) vrClamped = 0;
    float vrNorm = (vrClamped / 1000.0f) * 100.0f;

    float firstsNorm = (times1st >= 2250.0f) ? 100.0f : (100.0f * times1st / 2250.0f);
    float distNorm = (distTravelled >= 40000.0f) ? 100.0f : (100.0f * distTravelled / 40000.0f);
    float distFirstNorm = (distInFirst >= 10000.0f) ? 100.0f : (100.0f * distInFirst / 10000.0f);

    const float W_VR = 0.60f;
    const float W_RWIN = 0.15f;
    const float W_FIRSTS = 0.15f;
    const float W_DIST = 0.05f;
    const float W_DIST1ST = 0.05f;

    float baseM = (W_VR * vrNorm) + (W_RWIN * racingWinPct) + (W_FIRSTS * firstsNorm) + (W_DIST * distNorm) + (W_DIST1ST * distFirstNorm);

    // Anchors
    const float AH_VR = 100.0f, AH_RWIN = 55.0f, AH_FIRSTS = 100.0f, AH_DIST = 100.0f, AH_DIST1ST = 100.0f;  // -> 100
    const float AL_VR = 5.0f, AL_RWIN = 50.0f, AL_FIRSTS = 0.0f, AL_DIST = 0.0f, AL_DIST1ST = 0.0f;  // -> 10
    float M1 = W_VR * AH_VR + W_RWIN * AH_RWIN + W_FIRSTS * AH_FIRSTS + W_DIST * AH_DIST + W_DIST1ST * AH_DIST1ST;  // ~100.0
    float M2 = W_VR * AL_VR + W_RWIN * AL_RWIN + W_FIRSTS * AL_FIRSTS + W_DIST * AL_DIST + W_DIST1ST * AL_DIST1ST;  // ~10.0
    float alpha = 90.0f / (M1 - M2);
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
    //if (finalScore >= 12.0f) return 1;
    return 1;
}

static const wchar_t* RankToLabel(int rank) {
    switch (rank) {
        case 1:
            return L"\uF07D";
        case 2:
            return L"\uF07E";
        case 3:
            return L"\uF07F";
        case 4:
            return L"\uF080";
        case 5:
            return L"\uF081";
        case 6:
            return L"\uF082";
        case 7:
            return L"\uF083";
        case 8:
            return L"\uF084";
        case 9:
            return L"\uF085";
        default:
            return L"0";
    }
}

int GetCurrentLicenseRankVS() {
    const RKSYS::Mgr* rksysMgr = RKSYS::Mgr::sInstance;
    if (rksysMgr == nullptr || rksysMgr->curLicenseId < 0) return -1;
    const RKSYS::LicenseMgr& license = rksysMgr->licenses[rksysMgr->curLicenseId];
    const u32 MIN_VS_MATCHES = 100;
    const u32 vsWins = license.GetWFCVSWins();
    const u32 vsLosses = license.GetWFCVSLosses();
    const u32 totalVs = vsWins + vsLosses;
    if (totalVs < MIN_VS_MATCHES) {
        return 0;
    }
    float score = ComputeVsScoreFromLicense(license);
    return ScoreToRank(score);
}

int GetCurrentLicenseScore() {
    const RKSYS::Mgr* rksysMgr = RKSYS::Mgr::sInstance;
    if (rksysMgr == nullptr || rksysMgr->curLicenseId < 0) return -1;
    const RKSYS::LicenseMgr& license = rksysMgr->licenses[rksysMgr->curLicenseId];
    const u32 MIN_VS_MATCHES = 100;
    const u32 vsWins = license.GetWFCVSWins();
    const u32 vsLosses = license.GetWFCVSLosses();
    const u32 totalVs = vsWins + vsLosses;
    if (totalVs < MIN_VS_MATCHES) {
        return 0;
    }
    float score = ComputeVsScoreFromLicense(license);
    return static_cast<int>(score);
}

int FormatRankMessage(wchar_t* dst, size_t dstLen) {
    if (dst == nullptr || dstLen == 0) return -1;
    int rank = GetCurrentLicenseRankVS();
    int score = GetCurrentLicenseScore();
    if (rank < 0) rank = 0;
    if (score < 0) score = 0;
    const wchar_t* rankLabel = RankToLabel(rank);

    return ::swprintf(dst, dstLen, L"Rank: %ls\nScore: %d", rankLabel, score);
}

int FormatRankDetailsMessage(wchar_t* dst, size_t dstLen) {
    if (dst == nullptr || dstLen == 0) return -1;
    const RKSYS::Mgr* rksysMgr = RKSYS::Mgr::sInstance;
    if (rksysMgr == nullptr || rksysMgr->curLicenseId < 0) {
        return ::swprintf(dst, dstLen, L"No license loaded.");
    }

    const RKSYS::LicenseMgr& license = rksysMgr->licenses[rksysMgr->curLicenseId];
    const u32 vsWins = license.GetWFCVSWins();
    const u32 vsLosses = license.GetWFCVSLosses();
    const u32 totalVs = vsWins + vsLosses;
    const u32 MIN_VS_MATCHES = 100;

    float winPct = (totalVs > 0) ? (100.0f * (float)vsWins / (float)totalVs) : 45.0f;

    float vr = PointRating::GetUserVR(rksysMgr->curLicenseId);
    if (vr < 0.0f) vr = 0.0f;
    u32 vrClamped = (vr > 1000.0f) ? 1000.0f : vr * 100.0f;

    u32 times1st = license.GetTimes1stPlaceAchieved();
    float distTravelled = license.GetDistanceTravelled();
    float distInFirst = license.GetDistancetravelledwhilein1stplace();

    int rank = GetCurrentLicenseRankVS();
    float score = 0.0f;
    if (totalVs >= MIN_VS_MATCHES) {
        score = ComputeVsScoreFromLicense(license);
    }

    if (rank < 0) rank = 0;
    static const float kRankThresholds[] = {12.0f, 24.0f, 36.0f, 48.0f, 60.0f, 72.0f, 84.0f, 94.0f, 100.0f};
    float nextThreshold = (rank >= 9) ? 100.0f : kRankThresholds[rank];
    float scoreNeededForNextRank = (rank >= 9) ? 0.0f : (nextThreshold - score);
    if (scoreNeededForNextRank < 0.0f) scoreNeededForNextRank = 0.0f;
    int nextRank = (rank >= 9) ? 9 : (rank + 1);
    const wchar_t* nextRankLabel = RankToLabel(nextRank);

    return ::swprintf(
        dst, dstLen,
        L"Retro Rewind Rank:\n"
        L"VR: %d VR out of 100000 VR\n"
        L"Win Rate: %.1f%% out of 65%% Needed\n"
        L"1st Places: %u Firsts out of 2250 Firsts\n"
        L"Distance: %.1f KM out of 40000 KM\n"
        L"1st Distance: %.1f KM out of 10000 KM\n"
        L"Score: %.2f Points (%.2f till Rank %ls)\n",
        vrClamped, winPct, times1st, distTravelled, distInFirst, score, scoreNeededForNextRank, nextRankLabel);
}

// Address found by B_squo, original idea by Zeraora, developed by ZPL
kmRuntimeUse(0x806436a0);
static void DisplayOnlineRanking() {
    kmRuntimeWrite32A(0x806436a0, 0x38600000);  // li r3,0
    const Settings::Mgr& settings = Settings::Mgr::Get();
    if (RKNet::USERHandler::sInstance != nullptr && RKNet::USERHandler::sInstance->isInitialized) {
        const u64 myFc = RKNet::USERHandler::sInstance->toSendPacket.fc;
        // Force "Ant Defined" badge for specific friend codes regardless of other settings
        if (IsAntBadgeFC(myFc) && settings.GetUserSettingValue(Settings::SETTINGSTYPE_ONLINE, RADIO_STREAMERMODE) == STREAMERMODE_DISABLED) {
            kmRuntimeWrite32A(0x806436a0, 0x3860000A);  // li r3,10 -> Ants
            return;
        }
        if (IsPriorityBadgeFC(myFc) && settings.GetUserSettingValue(Settings::SETTINGSTYPE_ONLINE, RADIO_STREAMERMODE) == STREAMERMODE_DISABLED) {
            kmRuntimeWrite32A(0x806436a0, 0x3860000B);  // li r3,11 -> Developers
            return;
        }
    }

#ifdef BETA
    kmRuntimeWrite32A(0x806436a0, 0x3860000A);  // li r3,10
    return;
#endif

    const RacedataSettings& racedataSettings = Racedata::sInstance->menusScenario.settings;
    const GameMode mode = racedataSettings.gamemode;
    if (mode != MODE_PUBLIC_VS && !System::sInstance->IsContext(PULSAR_RANKING)) return;
    int rank = GetCurrentLicenseRankVS();
    if (rank < 0) rank = 0;
    const RKSYS::LicenseMgr& license = RKSYS::Mgr::sInstance->licenses[RKSYS::Mgr::sInstance->curLicenseId];
    const u32 MIN_VS_MATCHES = 100;
    const u32 totalVs = license.GetWFCVSWins() + license.GetWFCVSLosses();
    if (totalVs <= MIN_VS_MATCHES) {
        rank = 0;
    }

    u32 opcode = 0x38600000 | (rank & 0xFFFF);
    kmRuntimeWrite32A(0x806436a0, opcode);
}
static SectionLoadHook HookRankIcon(DisplayOnlineRanking);

}  // namespace Ranking
}  // namespace Pulsar
