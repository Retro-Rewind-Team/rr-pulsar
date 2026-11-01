#include <RetroRewind.hpp>
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

static void OnMultiplierDownloaded(s32 result, void *response, void * /*userdata*/) {
    Network::FinishNHTTPRequest();
    s_multiplierRequestActive = false;
    s_multiplierRequestDone = true;

    if (response == nullptr)
        return;

    if (result == 0) {
        char *body = nullptr;
        const int bodyLen = NHTTP::GetBodyAll(reinterpret_cast<NHTTP::Res *>(response), &body);
        if (body != nullptr && bodyLen > 0) {
            const int length = bodyLen < 31 ? bodyLen : 31;
            char text[32];
            memcpy(text, body, length);
            text[length] = '\0';
            const char *start = text;
            while (*start == ' ' || *start == '\n' || *start == '\r' || *start == '\t') ++start;
            const char *cursor = start;
            if (*cursor == '+')
                ++cursor;
            bool hasDigit = false;
            while (*cursor >= '0' && *cursor <= '9') {
                hasDigit = true;
                ++cursor;
            }
            if (*cursor == '.') {
                ++cursor;
                while (*cursor >= '0' && *cursor <= '9') {
                    hasDigit = true;
                    ++cursor;
                }
            }
            while (*cursor == ' ' || *cursor == '\n' || *cursor == '\r' || *cursor == '\t') ++cursor;
            float multiplier = 1.0f;
            if (hasDigit && *cursor == '\0' && sscanf(start, "%f", &multiplier) == 1) {
                s_remoteMultiplier = multiplier;
                s_remoteMultiplierValid = true;
            }
        }
    }

    NHTTPDestroyResponse(response);
}

static void UpdateMultiplierDownloadForWfcConnection() {
    RKNet::Controller *controller = RKNet::Controller::sInstance;
    const bool isConnectedToWfc = controller != nullptr && controller->connectionState != RKNet::CONNECTIONSTATE_SHUTDOWN;

    if (isConnectedToWfc && !s_wasConnectedToWfc) {
        s_multiplierRequestDone = false;
        s_remoteMultiplierValid = false;
        s_remoteMultiplier = 1.0f;
    }
    s_wasConnectedToWfc = isConnectedToWfc;

    if (s_multiplierRequestActive || s_multiplierRequestDone || controller == nullptr || controller->GetConnectionState() != RKNet::CONNECTIONSTATE_IDLE)
        return;

    if (!Network::PrepareNHTTPRequest())
        return;

    if (s_multiplierRequestWorkBuf == nullptr) {
        s_multiplierRequestWorkBuf = Network::NHTTPAlloc(MULTIPLIER_REQUEST_WORK_BUF_SIZE, 0x20);
        if (s_multiplierRequestWorkBuf == nullptr)
            return;
    }
    memset(s_multiplierRequestWorkBuf, 0, MULTIPLIER_REQUEST_WORK_BUF_SIZE);

    void *request = NHTTPCreateRequest(MULTIPLIER_URL, 0, s_multiplierRequestWorkBuf, MULTIPLIER_REQUEST_WORK_BUF_SIZE, reinterpret_cast<void *>(&OnMultiplierDownloaded), nullptr);
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

static FrameLoadHook remoteMultiplierHook(UpdateMultiplierDownloadForWfcConnection);

bool IsWeekendMultiplierActiveForRegion(u8 region) {
    ServerDateTime *sdt = ServerDateTime::sInstance;
    if (sdt == nullptr || !sdt->isValid)
        return false;
    sdt->Update();
    return sdt->IsVRMultiplierWeekend() && sdt->GetCurrentVRMultiplierRegion() == region;
}

float GetMultiplier() {
    float multiplier = 1.0f;

    // Weekend VR multiplier (1.5x) for the active region
    u8 currentRegion = System::sInstance->netMgr.region;
    if (IsWeekendMultiplierActiveForRegion(currentRegion)) {
        multiplier *= 1.5f;
    }
    if ((System::sInstance->IsContext(PULSAR_ELIMINATION) && System::sInstance->IsContext(PULSAR_FFA))) {
        const RKNet::Controller *controller = RKNet::Controller::sInstance;
        const int playerCount = controller->subs[controller->currentSub].playerCount;
        if (playerCount > 5)
            multiplier += (float)(playerCount - 5) * 0.166f;
    }
    if (s_remoteMultiplierValid)
        multiplier *= s_remoteMultiplier;
#ifdef BETA
    multiplier *= 1.25f;
#endif

    if (multiplier < 1.0f)
        multiplier = 1.0f;
    if (multiplier > 2.5f)
        multiplier = 2.5f;

    return multiplier;
}

}  // namespace PointRating
}  // namespace Pulsar
