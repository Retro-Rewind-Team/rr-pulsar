#include <RetroRewind.hpp>
#include <Gamemodes/Battle/BattleElimination.hpp>
#include <core/RK/RKSystem.hpp>
#include <core/egg/mem/Heap.hpp>
#include <core/System/SystemManager.hpp>
#include <core/rvl/DWC/NHTTP.hpp>
#include <core/rvl/NHTTP/NHTTP.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <Network/NHTTPHelper.hpp>
#include <Network/Rating/PlayerRating.hpp>
#include <Network/ServerDateTime.hpp>
#include <PulsarSystem.hpp>
#include <Gamemodes/ItemRain/ItemRain.hpp>
#include <include/c_string.h>

namespace Pulsar {
namespace PointRating {

#ifdef BETA
static const char* MULTIPLIER_URL = "http://update.rwfc.net/RetroRewind/multiplierBeta.txt";
#else
static const char* MULTIPLIER_URL = "https://update.rwfc.net/RetroRewind/multiplier.txt";
#endif
static const u32 MULTIPLIER_REQUEST_WORK_BUF_SIZE = 0x1000;

static void* s_multiplierRequestWorkBuf = nullptr;
static bool s_multiplierRequestActive = false;
static bool s_multiplierRequestDone = false;
static bool s_multiplierRequestPending = false;
static bool s_remoteMultiplierValid = false;
static float s_remoteMultiplier = 1.0f;

static const char* SkipWhitespace(const char* p) {
    while (p != nullptr && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) ++p;
    return p;
}

static bool ParseRemoteMultiplier(const char* body, int bodyLen, float& out) {
    if (body == nullptr || bodyLen <= 0) return false;

    const int maxLen = bodyLen < 31 ? bodyLen : 31;
    char text[32];
    memcpy(text, body, maxLen);
    text[maxLen] = '\0';

    const char* start = SkipWhitespace(text);
    const char* p = start;
    if (*p == '+') ++p;

    bool hasDigit = false;
    float value = 0.0f;
    while (*p >= '0' && *p <= '9') {
        hasDigit = true;
        value = value * 10.0f + (float)(*p - '0');
        ++p;
    }
    if (*p == '.') {
        ++p;
        float scale = 0.1f;
        while (*p >= '0' && *p <= '9') {
            hasDigit = true;
            value += (float)(*p - '0') * scale;
            scale *= 0.1f;
            ++p;
        }
    }
    if (!hasDigit) return false;

    p = SkipWhitespace(p);
    if (*p != '\0') return false;

    out = value;
    return true;
}

static bool CanStartMultiplierDownload() {
    RKNet::Controller* controller = RKNet::Controller::sInstance;
    if (controller == nullptr) return false;

    const RKNet::ConnectionState state = controller->GetConnectionState();
    return state == RKNet::CONNECTIONSTATE_IDLE || state == RKNet::CONNECTIONSTATE_ROOM;
}

static void OnMultiplierDownloaded(s32 result, void* response, void* /*userdata*/) {
    Network::FinishNHTTPRequest();
    s_multiplierRequestActive = false;
    s_multiplierRequestDone = true;

    if (response == nullptr) return;

    if (result == 0) {
        char* body = nullptr;
        const int bodyLen = NHTTP::GetBodyAll(reinterpret_cast<NHTTP::Res*>(response), &body);
        float multiplier = 1.0f;
        if (ParseRemoteMultiplier(body, bodyLen, multiplier)) {
            s_remoteMultiplier = multiplier;
            s_remoteMultiplierValid = true;
        }
    }

    NHTTPDestroyResponse(response);
}

static void TryStartMultiplierDownload() {
    if (!s_multiplierRequestPending || s_multiplierRequestActive || s_multiplierRequestDone || !CanStartMultiplierDownload()) return;

    if (!Network::PrepareNHTTPRequest()) return;

    if (s_multiplierRequestWorkBuf == nullptr) {
        s_multiplierRequestWorkBuf = Network::NHTTPAlloc(MULTIPLIER_REQUEST_WORK_BUF_SIZE, 0x20);
        if (s_multiplierRequestWorkBuf == nullptr) return;
    }
    memset(s_multiplierRequestWorkBuf, 0, MULTIPLIER_REQUEST_WORK_BUF_SIZE);

    void* request = NHTTPCreateRequest(MULTIPLIER_URL, 0, s_multiplierRequestWorkBuf, MULTIPLIER_REQUEST_WORK_BUF_SIZE,
                                       reinterpret_cast<void*>(&OnMultiplierDownloaded),
                                       nullptr);
    if (request == nullptr) {
        s_multiplierRequestDone = true;
        return;
    }

    const s32 sendRet = NHTTPSendRequestAsync(request);
    if (sendRet < 0) {
        s_multiplierRequestDone = true;
        return;
    }
    Network::MarkNHTTPRequestActive();
    s_multiplierRequestActive = true;
    s_multiplierRequestPending = false;
}

static void StartMultiplierDownloadForRace() {
    s_multiplierRequestDone = false;
    s_multiplierRequestPending = true;
    s_remoteMultiplierValid = false;
    s_remoteMultiplier = 1.0f;

    TryStartMultiplierDownload();
}

static RaceLoadHook raceMultiplierHook(StartMultiplierDownloadForRace);
static FrameLoadHook remoteMultiplierHook(TryStartMultiplierDownload);

static bool IsEventDay(unsigned m, unsigned d) {
    return (m == 12 && d >= 23) ||  // Christmas
           (m == 1 && d <= 3) ||  // New Year
           (m == 10 && d >= 25) ||  // Halloween
           (m == 6 && d >= 5 && d <= 8) ||  // Start of Summer
           (m == 3 && d >= 13 && d <= 17) ||  // St. Patrick's Day
           (m == 4 && d >= 10 && d <= 14) ||  // MKWii Birthday
           (m == 8 && d >= 23 && d <= 29);  // End of Summer
}

static float GetBattleBonus() {
    if (!BattleElim::ShouldApplyBattleElimination()) return 0.0f;
    const RKNet::Controller* ctrl = RKNet::Controller::sInstance;
    int count = ctrl->subs[ctrl->currentSub].playerCount;
    return (count > 5) ? (float)(count - 5) * 0.166f : 0.0f;
}

bool IsWeekendMultiplierActive() {
    ServerDateTime* sdt = ServerDateTime::sInstance;
    if (sdt == nullptr || !sdt->isValid) return false;

    // Use SERVER time for the actual check (this is the authoritative time from login)
    u8 dow = ServerDateTime::GetDayOfWeek(sdt->year, sdt->month, sdt->day);
    bool isWeekend = (dow == 0 || dow == 6);  // Sunday or Saturday
    if (!isWeekend) return false;

    u32 weekNum = sdt->GetWeekNumber();
    return (weekNum % 2) == 1;  // Even weeks get the multiplier
}

bool IsWeekendMultiplierActiveForRegion(u8 region) {
    if (!IsWeekendMultiplierActive()) return false;
    ServerDateTime* sdt = ServerDateTime::sInstance;
    return sdt->GetCurrentVRMultiplierRegion() == region;
}

bool IsItemRainEventActive() {
    unsigned year = 0, month = 0, day = 0;
    bool valid = false;

    ServerDateTime* sdt = ServerDateTime::sInstance;
    if (sdt && sdt->isValid) {
        year = sdt->year;
        month = sdt->month;
        day = sdt->day;
        valid = true;
    } else {
        SystemManager* sm = SystemManager::sInstance;
        if (sm && sm->isValidDate) {
            year = sm->year + 2000;
            month = sm->month;
            day = sm->day;
            valid = true;
        }
    }

    if (valid) {
        if (year == 2026 && month == 1 && day >= 26 && day <= 29) {
            return true;
        }
    }
    return false;
}

float GetMultiplier() {
    unsigned month = 0, day = 0;
    bool valid = false;

    ServerDateTime* sdt = ServerDateTime::sInstance;
    if (sdt && sdt->isValid) {
        month = sdt->month;
        day = sdt->day;
        valid = true;
    } else {
        SystemManager* sm = SystemManager::sInstance;
        if (sm && sm->isValidDate) {
            month = sm->month;
            day = sm->day;
            valid = true;
        }
    }

    float base = (valid && IsEventDay(month, day)) ? 2.0f : 1.0f;

    // Weekend VR multiplier (1.5x) for the active region
    u8 currentRegion = System::sInstance->netMgr.region;
    if (IsWeekendMultiplierActiveForRegion(currentRegion) || (valid && month == 4 && day == 1)) {
        base *= 1.5f;
    }
    float multiplier = base + GetBattleBonus();
    if (s_remoteMultiplierValid) multiplier *= s_remoteMultiplier;
#ifdef BETA
    return multiplier * 1.25f;
#endif

    return multiplier;
}

}  // namespace PointRating
}  // namespace Pulsar
