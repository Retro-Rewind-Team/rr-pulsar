#include <Network/Rating/RatingMulti.hpp>
#include <Network/Rating/ServersideRating.hpp>
#include <Network/Rating/VRModification.hpp>
#include <Network/NHTTPHelper.hpp>
#include <Network/ServerDateTime.hpp>
#include <PulsarSystem.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <core/nw4r/ut/Misc.hpp>
#include <core/rvl/DWC/NHTTP.hpp>
#include <core/rvl/NHTTP/NHTTP.hpp>

namespace Pulsar {
namespace Network {

float ratingMultiplier = 1.0f;
u32 loginGeneration;
DWC::LoginCallback originalLoginCallback;

static void OnMultiplierReceived(s32 result, void *response, void *userdata) {
    FinishNHTTPRequest();
    MultiplierRequest *request = static_cast<MultiplierRequest *>(userdata);
    NHTTP::Res *res = static_cast<NHTTP::Res *>(response);
    if (result == 0 && res != nullptr && NHTTP::GetResultCode(res) == 200) {
        char *body = nullptr;
        const int length = NHTTP::GetBodyAll(res, &body);
        char value[32];
        if (body != nullptr && length > 0 && length < sizeof(value)) {
            memcpy(value, body, length);
            value[length] = '\0';
            float multiplier;
            char extra;
            if (sscanf(value, "%f %c", &multiplier, &extra) == 1) {
                nw4r::ut::AutoInterruptLock lock;
                if (request->generation == loginGeneration)
                    ratingMultiplier = multiplier;
            }
        }
    }
    if (res != nullptr)
        NHTTPDestroyResponse(res);
    NHTTPFree(request);
}

static void FetchMultiplier(DWC::Error error, int profileId, void *param) {
    if (error == DWC::DWC_ERROR_NONE)
        StoreRatingUponConnection(profileId);
    originalLoginCallback(error, profileId, param);
    if (error != DWC::DWC_ERROR_NONE || !PrepareNHTTPRequest())
        return;

    MultiplierRequest *request = static_cast<MultiplierRequest *>(NHTTPAlloc(sizeof(MultiplierRequest), 0x20));
    if (request == nullptr)
        return;
    request->generation = loginGeneration;
    void *httpRequest = NHTTPCreateRequest(multiplierUrl, 0, request->buffer, sizeof(request->buffer), reinterpret_cast<void *>(&OnMultiplierReceived), request);
    if (httpRequest == nullptr) {
        NHTTPFree(request);
        return;
    }
    MarkNHTTPRequestActive();
    if (NHTTPSendRequestAsync(httpRequest) < 0) {
        FinishNHTTPRequest();
        NHTTPFree(request);
    }
}

static BOOL LoginWithMultiplier(const wchar_t *name, const char *reserved, DWC::LoginCallback callback, void *param) {
    if (PreventVRModification())
        return false;
    {
        nw4r::ut::AutoInterruptLock lock;
        ++loginGeneration;
        ratingMultiplier = 1.0f;
    }
    originalLoginCallback = callback;
    return DWC_LoginAsync(name, reserved, FetchMultiplier, param);
}
kmCall(0x80658cdc, LoginWithMultiplier);

float ApplyRatingMultiplier(float change) {
    const GameMode mode = Racedata::sInstance->racesScenario.settings.gamemode;
    if (mode != MODE_PUBLIC_VS && mode != MODE_PUBLIC_BATTLE)
        return change;

    float multiplier = ratingMultiplier;
    const ServerDateTime &date = *ServerDateTime::sInstance;
    if (date.IsVRMultiplierWeekend() && date.GetCurrentVRMultiplierRegion() == System::sInstance->netMgr.region)
        multiplier *= 1.5f;
#ifdef BETA
    multiplier *= 1.25f;
#endif
    if (multiplier > 2.5f)
        multiplier = 2.5f;
    else if (!(multiplier >= 1.0f))
        multiplier = 1.0f;

    change *= multiplier;
    if (mode == MODE_PUBLIC_VS && change < 0.0f && change >= -1.01f)
        return 0.0f;
    if (change < -210.0f)
        return -210.0f;
    if (multiplier > 1.0f)
        return static_cast<s32>(change + (change < 0.0f ? -0.5 : 0.5));
    return change;
}

}  // namespace Network
}  // namespace Pulsar
