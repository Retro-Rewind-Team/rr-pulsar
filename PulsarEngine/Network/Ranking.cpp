#include <RetroRewind.hpp>
#include <Network/Json.hpp>
#include <Network/Ranking.hpp>
#include <Network/NHTTPHelper.hpp>
#include <Network/WiiLink.hpp>
#include <MarioKartWii/Race/Racedata.hpp>
#include <MarioKartWii/Race/Raceinfo/Raceinfo.hpp>
#include <MarioKartWii/RKSYS/LicenseMgr.hpp>
#include <MarioKartWii/System/Identifiers.hpp>
#include <MarioKartWii/RKSYS/RKSYSMgr.hpp>
#include <MarioKartWii/GlobalFunctions.hpp>
#include <MarioKartWii/System/Rating.hpp>
#include <Network/Rating/PlayerRating.hpp>
#include <core/rvl/DWC/NHTTP.hpp>
#include <core/rvl/NHTTP/NHTTP.hpp>
#include <MarioKartWii/RKNet/USER.hpp>
#include <Settings/Settings.hpp>
#include <Settings/SettingsParam.hpp>
#include <hooks.hpp>
#include <core/rvl/OS/OS.hpp>
#include <include/c_string.h>

namespace Pulsar {
namespace Ranking {

struct BadgeRequestCtx {
    u32 generation;
    u32 pid;
};

static BadgeRequestCtx s_badgeRequestCtx;
static void* s_badgeRequestWorkBuf = nullptr;
static u32 s_badgeRequestGeneration = 0;
static u32 s_badgePid = 0;
static u32 s_badgeMask = 0;
static bool s_badgeRequestActive = false;
static bool s_badgeRefreshPending = false;
static u32 s_pendingBadgePid = 0;

static s32 BadgeTypeToIcon(u32 badgeType) {
    // These are the existing ranking icon slots. Multiple API badge types map
    // to the same slot so the server can expose more specific badge names.
    switch (static_cast<BadgeType>(badgeType)) {
        case BADGE_RETRO_REWIND_DEVELOPER:
        case BADGE_WHEEL_WIZARD_DEVELOPER:
            return 10;
        case BADGE_RWFC_MODERATOR:
            return 11;
        case BADGE_MAJOR_CONTRIBUTOR:
            return 12;
        case BADGE_CONTRIBUTOR:
            return 13;
        case BADGE_SUPPORTER:
        case BADGE_HEART:
            return 14;
        case BADGE_TRANSLATOR:
            return 15;
        case BADGE_DISCORD_STAFF:
            return 16;
        case BADGE_BETA_TESTER:
            return 17;
        default:
            return -1;
    }
}

static u32 ParseBadgeJson(const char* body, int bodyLen, u32 pid) {
    if (body == nullptr || bodyLen <= 0 || pid == 0) return 0;

    const char* p = body;
    const char* end = body + bodyLen;
    while (p < end) {
        if (*p != '"') {
            ++p;
            continue;
        }

        ++p;
        u32 key = 0;
        bool hasKeyDigits = false;
        bool validKey = true;
        while (p < end && *p != '"') {
            if (*p >= '0' && *p <= '9') {
                const u32 digit = static_cast<u32>(*p - '0');
                if (key > (0xffffffffu - digit) / 10) validKey = false;
                key = key * 10 + digit;
                hasKeyDigits = true;
            } else {
                validKey = false;
            }
            ++p;
        }
        if (p >= end) break;
        ++p;
        p = Network::Json::SkipWhitespace(p, end);
        if (p >= end || *p != ':') continue;
        ++p;
        p = Network::Json::SkipWhitespace(p, end);
        if (!validKey || !hasKeyDigits || key != pid || p >= end || *p != '[') continue;

        ++p;
        u32 badgeMask = 0;
        while (p < end && *p != ']') {
            p = Network::Json::SkipWhitespace(p, end);
            if (p < end && *p == ',') {
                ++p;
                continue;
            }

            u32 badgeType = 0;
            if (!Network::Json::ParseU32(p, end, badgeType)) break;
            const s32 icon = BadgeTypeToIcon(badgeType);
            if (icon >= SPECIAL_BADGE_FIRST && icon <= SPECIAL_BADGE_LAST) badgeMask |= 1u << icon;

            p = Network::Json::SkipWhitespace(p, end);
            if (p < end && *p == ',') ++p;
        }
        return p < end && *p == ']' ? badgeMask : 0;
    }
    return 0;
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

    const RKSYS::Mgr* rksys = RKSYS::Mgr::sInstance;
    const float userVr = rksys != nullptr ? PointRating::GetUserVR(rksys->curLicenseId) : static_cast<float>(vr.points);
    float vrClamped = userVr > 1000.0f ? 1000.0f : userVr;
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
    return 1;
}

struct RankText {
    const wchar_t* summaryFormat;
    const wchar_t* noLicenseLoaded;
    const wchar_t* detailsFormat;
};

static Language GetCurrentLanguage() {
    return static_cast<Language>(
        Settings::Mgr::Get().GetSettingValue(Pulsar::Settings::SETTING_LANGUAGE));
}

static const RankText& GetRankText() {
    static const RankText english = {
        L"Rank: %ls\nScore: %d",
        L"No license loaded.",
        L"Retro Rewind Rank:\n"
        L"VR: %u / 100000\n"
        L"Win Rate: %.1f%% / 65%%\n"
        L"1st Places: %u / 2250\n"
        L"Distance: %.1f km / 40000 km\n"
        L"1st Distance: %.1f km / 10000 km\n"
        L"Score: %.2f points (need +%.2f for Rank %ls)\n"};

    static const RankText japanese = {
        L"\u30E9\u30F3\u30AF: %ls\n\u30B9\u30B3\u30A2: %d",
        L"\u30E9\u30A4\u30BB\u30F3\u30B9\u304C\u8AAD\u307F\u8FBC\u307E\u308C\u3066\u3044\u307E\u305B\u3093\u3002",
        L"Retro Rewind\u30E9\u30F3\u30AF:\n"
        L"VR: %u / 100000\n"
        L"\u52DD\u7387: %.1f%% / 65%%\n"
        L"1\u4F4D\u56DE\u6570: %u / 2250\n"
        L"\u8D70\u884C\u8DDD\u96E2: %.1f km / 40000 km\n"
        L"1\u4F4D\u8DDD\u96E2: %.1f km / 10000 km\n"
        L"\u30B9\u30B3\u30A2: %.2f (\u5FC5\u8981: %.2f / \u30E9\u30F3\u30AF %ls)\n"};

    static const RankText french = {
        L"Rang : %ls\nScore : %d",
        L"Aucune licence charg\u00E9e.",
        L"Rang Retro Rewind :\n"
        L"VR : %u / 100000\n"
        L"Taux de victoire : %.1f%% / 65%%\n"
        L"1res places : %u / 2250\n"
        L"Distance : %.1f km / 40000 km\n"
        L"Distance en 1re : %.1f km / 10000 km\n"
        L"Score : %.2f points (il faut %.2f pour le rang %ls)\n"};

    static const RankText german = {
        L"Rang: %ls\nPunktzahl: %d",
        L"Keine Lizenz geladen.",
        L"Retro Rewind-Rang:\n"
        L"VR: %u / 100000\n"
        L"Siegrate: %.1f%% / 65%%\n"
        L"1. Pl\u00E4tze: %u / 2250\n"
        L"Distanz: %.1f km / 40000 km\n"
        L"Distanz auf Platz 1: %.1f km / 10000 km\n"
        L"Punktzahl: %.2f Punkte (%.2f ben\u00F6tigt f\u00FCr Rang %ls)\n"};

    static const RankText dutch = {
        L"Rang: %ls\nScore: %d",
        L"Geen licentie geladen.",
        L"Retro Rewind-rang:\n"
        L"VR: %u / 100000\n"
        L"Winstpercentage: %.1f%% / 65%%\n"
        L"1e plaatsen: %u / 2250\n"
        L"Afstand: %.1f km / 40000 km\n"
        L"Afstand op plek 1: %.1f km / 10000 km\n"
        L"Score: %.2f punten (%.2f nodig voor rang %ls)\n"};

    static const RankText spanish = {
        L"Rango: %ls\nPuntuaci\u00F3n: %d",
        L"No hay licencia cargada.",
        L"Rango de Retro Rewind:\n"
        L"VR: %u / 100000\n"
        L"Tasa de victorias: %.1f%% / 65%%\n"
        L"Primeros puestos: %u / 2250\n"
        L"Distancia: %.1f km / 40000 km\n"
        L"Distancia en 1.er lugar: %.1f km / 10000 km\n"
        L"Puntuaci\u00F3n: %.2f puntos (faltan %.2f para el rango %ls)\n"};

    static const RankText finnish = {
        L"Sijoitus: %ls\nPisteet: %d",
        L"Lisenssi\u00E4 ei ole ladattu.",
        L"Retro Rewind -sijoitus:\n"
        L"VR: %u / 100000\n"
        L"Voittoprosentti: %.1f%% / 65%%\n"
        L"1. sijat: %u / 2250\n"
        L"Matka: %.1f km / 40000 km\n"
        L"Matka 1. sijalla: %.1f km / 10000 km\n"
        L"Pisteet: %.2f (tarvitaan %.2f sijoitukseen %ls)\n"};

    static const RankText italian = {
        L"Grado: %ls\nPunteggio: %d",
        L"Nessuna licenza caricata.",
        L"Grado Retro Rewind:\n"
        L"VR: %u / 100000\n"
        L"Tasso di vittoria: %.1f%% / 65%%\n"
        L"Primi posti: %u / 2250\n"
        L"Distanza: %.1f km / 40000 km\n"
        L"Distanza in 1a posizione: %.1f km / 10000 km\n"
        L"Punteggio: %.2f punti (ne servono %.2f per il grado %ls)\n"};

    static const RankText korean = {
        L"\uB7AD\uD06C: %ls\n\uC810\uC218: %d",
        L"\uB85C\uB4DC\uB41C \uB77C\uC774\uC13C\uC2A4\uAC00 \uC5C6\uC2B5\uB2C8\uB2E4.",
        L"Retro Rewind \uB7AD\uD06C:\n"
        L"VR: %u / 100000\n"
        L"\uC2B9\uB960: %.1f%% / 65%%\n"
        L"1\uC704 \uD69F\uC218: %u / 2250\n"
        L"\uC8FC\uD589 \uAC70\uB9AC: %.1f km / 40000 km\n"
        L"1\uC704 \uC8FC\uD589 \uAC70\uB9AC: %.1f km / 10000 km\n"
        L"\uC810\uC218: %.2f (\uD544\uC694: %.2f / \uB7AD\uD06C %ls)\n"};

    static const RankText russian = {
        L"\u0420\u0430\u043D\u0433: %ls\n\u041E\u0447\u043A\u0438: %d",
        L"\u041B\u0438\u0446\u0435\u043D\u0437\u0438\u044F \u043D\u0435 \u0437\u0430\u0433\u0440\u0443\u0436\u0435\u043D\u0430.",
        L"\u0420\u0430\u043D\u0433 Retro Rewind:\n"
        L"VR: %u / 100000\n"
        L"\u041F\u0440\u043E\u0446\u0435\u043D\u0442 \u043F\u043E\u0431\u0435\u0434: %.1f%% / 65%%\n"
        L"\u041F\u0435\u0440\u0432\u044B\u0445 \u043C\u0435\u0441\u0442: %u / 2250\n"
        L"\u0414\u0438\u0441\u0442\u0430\u043D\u0446\u0438\u044F: %.1f km / 40000 km\n"
        L"\u0414\u0438\u0441\u0442\u0430\u043D\u0446\u0438\u044F \u043D\u0430 1-\u043C \u043C\u0435\u0441\u0442\u0435: %.1f km / 10000 km\n"
        L"\u041E\u0447\u043A\u0438: %.2f (\u043D\u0443\u0436\u043D\u043E %.2f \u0434\u043B\u044F \u0440\u0430\u043D\u0433\u0430 %ls)\n"};

    static const RankText turkish = {
        L"R\u00FCtbe: %ls\nPuan: %d",
        L"Y\u00FCkl\u00FC lisans yok.",
        L"Retro Rewind r\u00FCtbesi:\n"
        L"VR: %u / 100000\n"
        L"Kazanma oran\u0131: %.1f%% / 65%%\n"
        L"Birincilik say\u0131s\u0131: %u / 2250\n"
        L"Mesafe: %.1f km / 40000 km\n"
        L"Birincilik mesafesi: %.1f km / 10000 km\n"
        L"Puan: %.2f (gerekli: %.2f / r\u00FCtbe %ls)\n"};

    static const RankText czech = {
        L"Hodnost: %ls\nSk\u00F3re: %d",
        L"Nen\u00ED na\u010Dten\u00E1 \u017E\u00E1dn\u00E1 licence.",
        L"Hodnost Retro Rewind:\n"
        L"VR: %u / 100000\n"
        L"M\u00EDra v\u00FDher: %.1f%% / 65%%\n"
        L"1. m\u00EDsta: %u / 2250\n"
        L"Vzd\u00E1lenost: %.1f km / 40000 km\n"
        L"Vzd\u00E1lenost na 1. m\u00EDst\u011B: %.1f km / 10000 km\n"
        L"Sk\u00F3re: %.2f bod\u016F (pot\u0159eba %.2f pro hodnost %ls)\n"};

    switch (GetCurrentLanguage()) {
        case LANGUAGE_JAPANESE:
            return japanese;
        case LANGUAGE_FRENCH:
            return french;
        case LANGUAGE_GERMAN:
            return german;
        case LANGUAGE_DUTCH:
            return dutch;
        case LANGUAGE_SPANISHUS:
        case LANGUAGE_SPANISHEU:
            return spanish;
        case LANGUAGE_FINNISH:
            return finnish;
        case LANGUAGE_ITALIAN:
            return italian;
        case LANGUAGE_KOREAN:
            return korean;
        case LANGUAGE_RUSSIAN:
            return russian;
        case LANGUAGE_TURKISH:
            return turkish;
        case LANGUAGE_CZECH:
            return czech;
        case LANGUAGE_ENGLISH:
        default:
            return english;
    }
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
    const RankText& text = GetRankText();
    int rank = GetCurrentLicenseRankVS();
    int score = GetCurrentLicenseScore();
    if (rank < 0) rank = 0;
    if (score < 0) score = 0;
    const wchar_t* rankLabel = RankToLabel(rank);

    return ::swprintf(dst, dstLen, text.summaryFormat, rankLabel, score);
}

int FormatRankDetailsMessage(wchar_t* dst, size_t dstLen) {
    if (dst == nullptr || dstLen == 0) return -1;
    const RankText& text = GetRankText();
    const RKSYS::Mgr* rksysMgr = RKSYS::Mgr::sInstance;
    if (rksysMgr == nullptr || rksysMgr->curLicenseId < 0) {
        return ::swprintf(dst, dstLen, text.noLicenseLoaded);
    }

    const RKSYS::LicenseMgr& license = rksysMgr->licenses[rksysMgr->curLicenseId];
    const u32 vsWins = license.GetWFCVSWins();
    const u32 vsLosses = license.GetWFCVSLosses();
    const u32 totalVs = vsWins + vsLosses;
    const u32 MIN_VS_MATCHES = 100;

    float winPct = (totalVs > 0) ? (100.0f * (float)vsWins / (float)totalVs) : 45.0f;

    float vr = PointRating::GetUserVR(rksysMgr->curLicenseId);
    if (vr < 0.0f) vr = 0.0f;
    u32 vrClamped = static_cast<u32>(vr * 100.0f + 0.5f);

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
        text.detailsFormat,
        vrClamped, winPct, times1st, distTravelled, distInFirst, score, scoreNeededForNextRank, nextRankLabel);
}

static u32 GetCurrentLicensePID() {
    RKSYS::Mgr* rksysMgr = RKSYS::Mgr::sInstance;
    if (rksysMgr == nullptr || rksysMgr->curLicenseId < 0 || rksysMgr->curLicenseId >= 4) return 0;
    RKSYS::LicenseMgr& license = rksysMgr->licenses[rksysMgr->curLicenseId];
    if (license.dwcAccUserData.gsProfileId <= 0) return 0;
    return static_cast<u32>(license.dwcAccUserData.gsProfileId);
}

bool IsSpecialBadgeAvailable(u8 badge) {
    return badge >= SPECIAL_BADGE_FIRST && badge <= SPECIAL_BADGE_LAST && (s_badgeMask & (1u << badge)) != 0;
}

bool HasSpecialBadges() {
    return s_badgePid != 0 && s_badgeMask != 0;
}

u32 GetSpecialBadgeCount() {
    u32 count = 0;
    for (u8 badge = SPECIAL_BADGE_FIRST; badge <= SPECIAL_BADGE_LAST; ++badge) {
        if (IsSpecialBadgeAvailable(badge)) ++count;
    }
    return count;
}

u8 GetSpecialBadgeAt(u32 index) {
    u32 current = 0;
    for (u8 badge = SPECIAL_BADGE_FIRST; badge <= SPECIAL_BADGE_LAST; ++badge) {
        if (!IsSpecialBadgeAvailable(badge)) continue;
        if (current++ == index) return badge;
    }
    return NORMAL_RANKING_BADGE;
}

static s32 GetFetchedBadgeForPID(u32 pid) {
    if (pid == 0 || pid != s_badgePid || !Settings::Mgr::IsCreated()) return -1;

    const Settings::Mgr& settings = Settings::Mgr::Get();
    if (settings.GetSettingValue(Pulsar::Settings::SETTING_STREAMERMODE) != STREAMERMODE_DISABLED) {
        return -1;
    }
    const u8 selectedBadge = settings.GetRankingBadge();
    return IsSpecialBadgeAvailable(selectedBadge) ? selectedBadge : -1;
}

static void OnBadgeResponse(s32 result, void* response, void* userdata) {
    Network::FinishNHTTPRequest();
    BadgeRequestCtx* ctx = reinterpret_cast<BadgeRequestCtx*>(userdata);
    if (ctx == nullptr || ctx->generation != s_badgeRequestGeneration || response == nullptr) {
        if (response != nullptr) NHTTPDestroyResponse(response);
        s_badgeRequestActive = false;
        s_badgeMask = 0;
        return;
    }

    if (result == 0) {
        char* body = nullptr;
        const int bodyLen = NHTTP::GetBodyAll(reinterpret_cast<NHTTP::Res*>(response), &body);
        s_badgeMask = ParseBadgeJson(body, bodyLen, ctx->pid);
        if (Settings::Mgr::IsCreated()) {
            Settings::Mgr& settings = Settings::Mgr::Get();
            const u8 selectedBadge = settings.GetRankingBadge();
            if (selectedBadge != NORMAL_RANKING_BADGE && !IsSpecialBadgeAvailable(selectedBadge)) {
                settings.SetRankingBadge(NORMAL_RANKING_BADGE);
            }
        }
    } else {
        s_badgeMask = 0;
    }

    NHTTPDestroyResponse(response);
    s_badgeRequestActive = false;
}

static bool StartBadgeRequest(u32 pid) {
    if (pid == 0) return false;
    if (s_badgeRequestActive) return false;

    if (!Network::PrepareNHTTPRequest()) return false;

    if (s_badgeRequestWorkBuf == nullptr) {
        s_badgeRequestWorkBuf = Network::NHTTPAlloc(BADGE_REQUEST_WORK_BUF_SIZE, 0x20);
        if (s_badgeRequestWorkBuf == nullptr) return false;
    }
    memset(s_badgeRequestWorkBuf, 0, BADGE_REQUEST_WORK_BUF_SIZE);

    s_badgeRequestCtx.generation = s_badgeRequestGeneration;
    s_badgeRequestCtx.pid = pid;

    void* request = NHTTPCreateRequest(BADGE_URL, 0, s_badgeRequestWorkBuf, BADGE_REQUEST_WORK_BUF_SIZE,
                                       reinterpret_cast<void*>(&OnBadgeResponse),
                                       reinterpret_cast<void*>(&s_badgeRequestCtx));
    if (request == nullptr) return false;

    const s32 sendRet = NHTTPSendRequestAsync(request);
    if (sendRet >= 0) {
        Network::MarkNHTTPRequestActive();
        s_badgeRequestActive = true;
    }
    return sendRet >= 0;
}

static void StartBadgeRefresh(u32 pid) {
    s_badgeRefreshPending = false;
    s_pendingBadgePid = 0;

    if (pid == 0) {
        s_badgePid = 0;
        s_badgeMask = 0;
        return;
    }

    ++s_badgeRequestGeneration;
    s_badgePid = pid;
    s_badgeMask = 0;
    if (!StartBadgeRequest(s_badgePid)) s_badgeMask = 0;
}

static void BeginBadgeDownloads() {
    const u32 pid = GetCurrentLicensePID();
    if (pid == 0) return;

    if (s_badgeRequestActive) {
        s_badgeRefreshPending = true;
        s_pendingBadgePid = pid;
        return;
    }

    StartBadgeRefresh(pid);
}

static void ProcessPendingBadgeRequests() {
    if (s_badgeRequestActive) return;

    if (s_badgeRefreshPending) {
        StartBadgeRefresh(s_pendingBadgePid);
    }
}
static FrameLoadHook BadgeRequestFrameHook(ProcessPendingBadgeRequests);

asmFunc AsmHook_WFCMainOnActivateBadgeRefresh() {
    ASM(
        nofralloc;
        stwu r1, -0x20(r1);
        mflr r0;
        stw r0, 0x24(r1);
        stw r31, 0x8(r1);

        bl BeginBadgeDownloads;

        lwz r31, 0x8(r1);
        li r0, -1;
        stw r0, 0xf30(r31);

        lwz r0, 0x24(r1);
        mtlr r0;
        addi r1, r1, 0x20;
        blr;);
}
kmCall(0x8064bcd0, AsmHook_WFCMainOnActivateBadgeRefresh);

static u8 GetOnlineRankingIcon(u8, u8) {
    if (RKNet::USERHandler::sInstance != nullptr && RKNet::USERHandler::sInstance->isInitialized) {
        const u32 pid = GetCurrentLicensePID();
        const s32 badge = GetFetchedBadgeForPID(pid);
        if (badge >= 0) return static_cast<u8>(badge);
    }

#ifdef BETA
    return 17;  // Beta tester badge
#endif

    const RacedataSettings& racedataSettings = Racedata::sInstance->menusScenario.settings;
    const GameMode mode = racedataSettings.gamemode;
    if (mode != MODE_PUBLIC_VS && !System::sInstance->IsContext(PULSAR_RANKING)) return 0;
    int rank = GetCurrentLicenseRankVS();
    if (rank < 0) rank = 0;
    const RKSYS::LicenseMgr& license = RKSYS::Mgr::sInstance->licenses[RKSYS::Mgr::sInstance->curLicenseId];
    const u32 MIN_VS_MATCHES = 100;
    const u32 totalVs = license.GetWFCVSWins() + license.GetWFCVSLosses();
    if (totalVs <= MIN_VS_MATCHES) {
        rank = 0;
    }

    return static_cast<u8>(rank);
}
kmCall(0x806436a0, GetOnlineRankingIcon);

}  // namespace Ranking
}  // namespace Pulsar
