#include <kamek.hpp>
#include <core/rvl/DWC/NHTTP.hpp>
#include <core/rvl/NHTTP/NHTTP.hpp>
#include <core/rvl/OS/OS.hpp>
#include <core/rvl/VI/VI.hpp>
#include <include/c_string.h>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/RKSYS/RKSYSMgr.hpp>
#include <Network/Json.hpp>
#include <Network/NHTTPHelper.hpp>
#include <Network/Rating/PlayerRating.hpp>
#include <Network/Rating/RatingSync.hpp>

namespace Pulsar {
namespace Network {

extern "C" void DWC_LoginAsync(wchar_t *miiName, int unk, void *callback, RKNet::Controller *self);
static DWC::LoginCallback s_originalLoginCallback = nullptr;
static void *s_leaderboardWorkBuf = nullptr;
static volatile bool s_leaderboardRequestActive = false;
static volatile u32 s_topVR = 0;

static void OnLeaderboardDownloaded(s32 result, void *response, void *) {
    u32 topVR = 0;
    if (result == 0 && response != nullptr) {
        char *body = nullptr;
        const int bodyLen = NHTTP::GetBodyAll(reinterpret_cast<NHTTP::Res *>(response), &body);
        if (body != nullptr && bodyLen > 0) {
            char *json = reinterpret_cast<char *>(NHTTPAlloc(bodyLen + 1, 4));
            if (json != nullptr) {
                memcpy(json, body, bodyLen);
                json[bodyLen] = '\0';
                Json::Value array;
                Json::Value entry;
                const char *cursor = nullptr;
                if (Json::FindArray(json, array) && Json::Next(array, cursor, entry))
                    Json::Get(entry, "vr", topVR);
                NHTTPFree(json);
            }
        }
    }
    if (response != nullptr)
        NHTTPDestroyResponse(response);
    FinishNHTTPRequest();
    s_topVR = topVR;
    s_leaderboardRequestActive = false;
}

static void LoginCallbackAndQueueRating(DWC::Error error, int profileId, void *param) {
    if (s_originalLoginCallback != nullptr)
        s_originalLoginCallback(error, profileId, param);
    RKSYS::Mgr *rksys = RKSYS::Mgr::sInstance;
    const u32 licenseId = rksys->curLicenseId;
    PointRating::StartLoginRatingDownload(profileId, licenseId);
}

void CheckVRAndLogin(wchar_t *miiName, int unk, void *callback, RKNet::Controller *self) {
    bool requested = false;
    if (!s_leaderboardRequestActive && PrepareNHTTPRequest()) {
        s_topVR = 0;
        if (s_leaderboardWorkBuf == nullptr)
            s_leaderboardWorkBuf = NHTTPAlloc(0x20000, 0x20);
        if (s_leaderboardWorkBuf != nullptr) {
            memset(s_leaderboardWorkBuf, 0, 0x20000);
            void *request = NHTTPCreateRequest("http://rwfc.net/api/leaderboard/in-game?page=1", 0, s_leaderboardWorkBuf, 0x20000, reinterpret_cast<void *>(&OnLeaderboardDownloaded), nullptr);
            if (request != nullptr) {
                s_leaderboardRequestActive = true;
                MarkNHTTPRequestActive();
                if (NHTTPSendRequestAsync(request) < 0) {
                    s_leaderboardRequestActive = false;
                    FinishNHTTPRequest();
                } else {
                    requested = true;
                    const u64 startTime = OS::GetTime();
                    while (s_leaderboardRequestActive && !self->shutdownScheduled && OS::TicksToMilliseconds(OS::GetTime() - startTime) < 10000) VI::WaitForRetrace();
                }
            }
        }
    }

    if (self->shutdownScheduled)
        return;

    RKSYS::Mgr *rksys = RKSYS::Mgr::sInstance;
    if (requested && !s_leaderboardRequestActive && s_topVR != 0 && rksys != nullptr && static_cast<u32>(PointRating::GetUserVR(rksys->curLicenseId) * 100.0f + 0.5f) >= s_topVR + 2000) {
        self->errorParams.errorParam1 = 1;
        self->errorParams.dwcErrorCode = 42069;
        self->connectionState = static_cast<RKNet::ConnectionState>(7);
        return;
    }

    s_originalLoginCallback = reinterpret_cast<DWC::LoginCallback>(callback);
    DWC_LoginAsync(miiName, unk, reinterpret_cast<void *>(&LoginCallbackAndQueueRating), self);
}
kmCall(0x80658cdc, CheckVRAndLogin);

}  // namespace Network
}  // namespace Pulsar