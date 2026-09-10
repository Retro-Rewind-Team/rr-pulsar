#include <RetroRewind.hpp>
#include <Gamemodes/Battle/BattleElimination.hpp>
#include <core/RK/RKSystem.hpp>
#include <core/egg/mem/Heap.hpp>
#include <core/rvl/DWC/NHTTP.hpp>
#include <core/rvl/NHTTP/NHTTP.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <Network/NHTTPHelper.hpp>
#include <Network/Rating/PlayerRating.hpp>
#include <Network/WiiLink.hpp>
#include <Network/ServerDateTime.hpp>
#include <PulsarSystem.hpp>
#include <include/c_stdio.h>
#include <include/c_string.h>

namespace Pulsar {
namespace PointRating {

#ifdef BETA
static const char *MULTIPLIER_URL = "http://rwfc.net/api/multiplier?channel=beta";
#else
static const char *MULTIPLIER_URL = "http://rwfc.net/api/multiplier?channel=stable";
#endif
static const u32 MULTIPLIER_REQUEST_WORK_BUF_SIZE = 0x1000;

static void *s_multiplierRequestWorkBuf = nullptr;
static bool s_multiplierRequestActive = false;
static bool s_multiplierRequestDone = false;
static bool s_wasConnectedToWfc = false;
static bool s_remoteMultiplierValid = false;
static float s_remoteMultiplier = 1.0f;

static const char *SkipWhitespace(const char *p) {
    while (p != nullptr && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) ++p;
    return p;
}

static bool ParseRemoteMultiplier(const char *body, int bodyLen, float &out) {
    if (body == nullptr || bodyLen <= 0) return false;

    const int maxLen = bodyLen < 31 ? bodyLen : 31;
    char text[32];
    memcpy(text, body, maxLen);
    text[maxLen] = '\0';

    const char *start = SkipWhitespace(text);
    const char *p = start;
    if (*p == '+') ++p;

    bool hasDigit = false;
    while (*p >= '0' && *p <= '9') {
        hasDigit = true;
        ++p;
    }
    if (*p == '.') {
        ++p;
        while (*p >= '0' && *p <= '9') {
            hasDigit = true;
            ++p;
        }
    }
    if (!hasDigit) return false;

    p = SkipWhitespace(p);
    if (*p != '\0') return false;

    return sscanf(start, "%f", &out) == 1;
}

static bool CanStartMultiplierDownload() {
    RKNet::Controller *controller = RKNet::Controller::sInstance;
    return controller != nullptr && controller->GetConnectionState() == RKNet::CONNECTIONSTATE_IDLE;
}

static void OnMultiplierDownloaded(s32 result, void *response, void * /*userdata*/) {
    Network::FinishNHTTPRequest();
    s_multiplierRequestActive = false;
    s_multiplierRequestDone = true;

    if (response == nullptr) return;

    if (result == 0) {
        char *body = nullptr;
        const int bodyLen = NHTTP::GetBodyAll(reinterpret_cast<NHTTP::Res *>(response), &body);
        float multiplier = 1.0f;
        if (ParseRemoteMultiplier(body, bodyLen, multiplier)) {
            s_remoteMultiplier = multiplier;
            s_remoteMultiplierValid = true;
        }
    }

    NHTTPDestroyResponse(response);
}

static void TryStartMultiplierDownload() {
    if (s_multiplierRequestActive || s_multiplierRequestDone || !CanStartMultiplierDownload()) return;

    if (!Network::PrepareNHTTPRequest()) return;

    if (s_multiplierRequestWorkBuf == nullptr) {
        s_multiplierRequestWorkBuf = Network::NHTTPAlloc(MULTIPLIER_REQUEST_WORK_BUF_SIZE, 0x20);
        if (s_multiplierRequestWorkBuf == nullptr) return;
    }
    memset(s_multiplierRequestWorkBuf, 0, MULTIPLIER_REQUEST_WORK_BUF_SIZE);

    void *request = NHTTPCreateRequest(MULTIPLIER_URL, 0, s_multiplierRequestWorkBuf, MULTIPLIER_REQUEST_WORK_BUF_SIZE,
                                       reinterpret_cast<void *>(&OnMultiplierDownloaded),
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
}

static void UpdateMultiplierDownloadForWfcConnection() {
    RKNet::Controller *controller = RKNet::Controller::sInstance;
    const bool isConnectedToWfc =
        controller != nullptr && controller->connectionState != RKNet::CONNECTIONSTATE_SHUTDOWN;

    if (isConnectedToWfc && !s_wasConnectedToWfc) {
        s_multiplierRequestDone = false;
        s_remoteMultiplierValid = false;
        s_remoteMultiplier = 1.0f;
    }
    s_wasConnectedToWfc = isConnectedToWfc;

    TryStartMultiplierDownload();
}

static FrameLoadHook remoteMultiplierHook(UpdateMultiplierDownloadForWfcConnection);

static float GetBattleBonus() {
    if (!BattleElim::ShouldApplyBattleElimination()) return 0.0f;
    const RKNet::Controller *ctrl = RKNet::Controller::sInstance;
    int count = ctrl->subs[ctrl->currentSub].playerCount;
    return (count > 5) ? (float)(count - 5) * 0.166f : 0.0f;
}

bool IsWeekendMultiplierActive() {
    ServerDateTime *sdt = ServerDateTime::sInstance;
    if (sdt == nullptr || !sdt->isValid) return false;
    sdt->Update();
    return sdt->IsVRMultiplierWeekend();
}

bool IsWeekendMultiplierActiveForRegion(u8 region) {
    if (!IsWeekendMultiplierActive()) return false;
    ServerDateTime *sdt = ServerDateTime::sInstance;
    return sdt->GetCurrentVRMultiplierRegion() == region;
}

float GetMultiplier() {
    float base = 1.0f;

    // Weekend VR multiplier (1.5x) for the active region
    u8 currentRegion = System::sInstance->netMgr.region;
    if (IsWeekendMultiplierActiveForRegion(currentRegion)) {
        base *= 1.5f;
    }
    float multiplier = base + GetBattleBonus();
    if (s_remoteMultiplierValid) multiplier *= s_remoteMultiplier;
#ifdef BETA
    multiplier *= 1.25f;
#endif

    // Clamp the multiplier to a reasonable range to prevent stacked multipliers causing unintentional flags
    if (multiplier < 1.0f) multiplier = 1.0f;
    if (multiplier > 2.5f) multiplier = 2.5f;

    return multiplier;
}

}  // namespace PointRating
}  // namespace Pulsar
