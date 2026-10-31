#include <kamek.hpp>
#include <core/egg/System.hpp>
#include <include/c_stdio.h>
#include <include/c_stdlib.h>
#include <include/c_string.h>
#include <MarioKartWii/RKSYS/RKSYSMgr.hpp>
#include <core/RK/RKSystem.hpp>
#include <core/rvl/DWC/NHTTP.hpp>
#include <core/rvl/NHTTP/NHTTP.hpp>
#include <Network/GPReport.hpp>
#include <Network/Json.hpp>
#include <Network/NHTTPHelper.hpp>
#include <Network/Rating/PlayerRating.hpp>
#include <Network/Rating/RatingSync.hpp>
#include <Network/WiiLink.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>

namespace Pulsar {
namespace PointRating {

static const u32 s_nhttpWorkBufSize = 0x1000;
static u32 s_requestGeneration = 0;
static bool s_requestInFlight = false;
static void *s_requestWorkBuf = nullptr;
static char s_requestUrl[160];
static s32 s_pendingProfileId = 0;
static u32 s_pendingLicenseId = 0;

struct RatingRequest {
    u32 generation;
    s32 profileId;
    u32 licenseId;
};

static RatingRequest s_requestCtx;

void ReportCurrentRatings(u32 licenseId) {
    const int vr = (int)(GetUserVR(licenseId) + 0.5f);
    const int br = (int)(GetUserBR(licenseId) + 0.5f);

    char buffer[64];
    if (snprintf(buffer, sizeof(buffer), "vr=%d|br=%d", vr < 1 ? 1 : vr > MAX_RATING ? MAX_RATING : vr, br < 1 ? 1 : br > MAX_RATING ? MAX_RATING : br) < 0)
        return;
    Network::Report("wl:mkw_vrbr", buffer);
}

static void OnRatingsDownloaded(s32 result, void *response, void *userdata) {
    Network::FinishNHTTPRequest();
    s_requestInFlight = false;
    RatingRequest *ctx = reinterpret_cast<RatingRequest *>(userdata);
    if (ctx == nullptr || response == nullptr)
        return;

    if (ctx->generation != s_requestGeneration) {
        NHTTPDestroyResponse(response);
        return;
    }

    if (result != 0) {
        NHTTPDestroyResponse(response);
        return;
    }

    char *body = nullptr;
    const int bodyLen = NHTTP::GetBodyAll(reinterpret_cast<NHTTP::Res *>(response), &body);
    if (body == nullptr || bodyLen <= 0) {
        NHTTPDestroyResponse(response);
        return;
    }

    const int maxLen = bodyLen < 255 ? bodyLen : 255;
    char json[256];
    memcpy(json, body, maxLen);
    json[maxLen] = '\0';

    NHTTPDestroyResponse(response);

    const RKSYS::Mgr *rksys = RKSYS::Mgr::sInstance;
    if (ctx->generation != s_requestGeneration || rksys == nullptr || ctx->licenseId >= 4 || rksys->curLicenseId != ctx->licenseId)
        return;

    Network::Json::Value root;
    u32 found = 0;
    if (!Network::Json::Parse(json, root) || !Network::Json::Get(root, "found", found) || found != 1) {
        ReportCurrentRatings(ctx->licenseId);
        return;
    }

    int vr = 0;
    int br = 0;
    if (!Network::Json::Get(root, "vr", vr))
        return;
    if (!Network::Json::Get(root, "br", br))
        return;

    SaveProfileVR(ctx->profileId, (float)vr);
    SaveProfileBR(ctx->profileId, (float)br);
}

static void TryStartPendingLoginRatingDownload() {
    if (s_pendingProfileId == 0 || s_requestInFlight)
        return;
    if (!Network::PrepareNHTTPRequest())
        return;

    RKSYS::Mgr *rksys = RKSYS::Mgr::sInstance;
    if (rksys == nullptr || s_pendingLicenseId >= 4)
        return;
    BindLicenseProfileId(s_pendingLicenseId, s_pendingProfileId);

    if (s_requestWorkBuf == nullptr) {
        s_requestWorkBuf = Network::NHTTPAlloc(s_nhttpWorkBufSize, 0x20);
        if (s_requestWorkBuf == nullptr)
            return;
    }
    memset(s_requestWorkBuf, 0, s_nhttpWorkBufSize);

    s_requestCtx.generation = s_requestGeneration;
    s_requestCtx.profileId = s_pendingProfileId;
    s_requestCtx.licenseId = s_pendingLicenseId;

    if (snprintf(s_requestUrl, sizeof(s_requestUrl), "http://nas.%s/api/mkw_rr_ratings?pid=%ld", WWFC_DOMAIN, (long)s_pendingProfileId) < 0) {
        return;
    }

    void *request = NHTTPCreateRequest(s_requestUrl, 0, s_requestWorkBuf, s_nhttpWorkBufSize, reinterpret_cast<void *>(&OnRatingsDownloaded), reinterpret_cast<void *>(&s_requestCtx));
    if (request == nullptr)
        return;

    const s32 sendRet = NHTTPSendRequestAsync(request);
    if (sendRet < 0)
        return;
    Network::MarkNHTTPRequestActive();
    s_requestInFlight = true;
    s_pendingProfileId = 0;
    s_pendingLicenseId = 0;
}

static FrameLoadHook startPendingLoginRatingDownload(TryStartPendingLoginRatingDownload);

void StartLoginRatingDownload(s32 profileId, u32 licenseId) {
    if (profileId <= 0)
        return;

    RKSYS::Mgr *rksys = RKSYS::Mgr::sInstance;
    if (rksys == nullptr || licenseId >= 4)
        return;
    BindLicenseProfileId(licenseId, profileId);
    ++s_requestGeneration;
    s_pendingProfileId = profileId;
    s_pendingLicenseId = licenseId;
}

}  // namespace PointRating
}  // namespace Pulsar
