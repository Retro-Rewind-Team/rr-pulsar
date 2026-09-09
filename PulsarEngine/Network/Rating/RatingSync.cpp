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

static bool s_syncReportingSuppressed = false;
static const u32 s_nhttpWorkBufSize = 0x1000;
static u32 s_requestGeneration = 0;
static bool s_requestInFlight = false;
static void *s_requestWorkBuf = nullptr;
static char s_requestUrl[160];
static s32 s_pendingInitialReportProfileId = 0;
static u32 s_pendingInitialReportLicenseId = 0;
static bool s_pendingLoginDownload = false;
static s32 s_pendingProfileId = 0;
static u32 s_pendingLicenseId = 0;

struct RequestCtx {
    u32 generation;
    s32 profileId;
    u32 licenseId;
};

static RequestCtx s_requestCtx;

static int ClampRatingForSync(float rating) {
    int scaled = (int)(rating * 100.0f + 0.5f);
    if (scaled < 1) return 1;
    if (scaled > 1000000) return 1000000;
    return scaled;
}

void SetSyncReportingSuppressed(bool suppress) {
    s_syncReportingSuppressed = suppress;
}

void ReportCurrentRatings(u32 licenseId) {
    if (s_syncReportingSuppressed) return;

    const int vrScaled = ClampRatingForSync(GetUserVR(licenseId));
    const int brScaled = ClampRatingForSync(GetUserBR(licenseId));

    char buffer[64];
    if (snprintf(buffer, sizeof(buffer), "vr=%d|br=%d", vrScaled, brScaled) < 0) return;
    Network::Report("wl:mkw_vrbr", buffer);
}

static bool IsRequestStillRelevant(const RequestCtx &ctx) {
    if (ctx.generation != s_requestGeneration) return false;
    if (ctx.profileId <= 0) return false;

    RKSYS::Mgr *rksys = RKSYS::Mgr::sInstance;
    if (rksys == nullptr || ctx.licenseId >= 4) return false;
    if (rksys->curLicenseId != ctx.licenseId) return false;

    return true;
}

static void OnRatingsDownloaded(s32 result, void *response, void *userdata) {
    Network::FinishNHTTPRequest();
    s_requestInFlight = false;
    RequestCtx *ctx = reinterpret_cast<RequestCtx *>(userdata);
    if (ctx == nullptr || response == nullptr) return;

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

    if (!IsRequestStillRelevant(*ctx)) return;

    Network::Json::Value root;
    u32 found = 0;
    if (!Network::Json::Parse(json, root) || !Network::Json::Get(root, "found", found) || found != 1) {
        ReportCurrentRatings(ctx->licenseId);
        return;
    }

    int vrScaled = 0;
    int brScaled = 0;
    if (!Network::Json::Get(root, "vr", vrScaled)) return;
    if (!Network::Json::Get(root, "br", brScaled)) return;

    SetSyncReportingSuppressed(true);
    SaveProfileVR(ctx->profileId, (float)vrScaled / 100.0f);
    SaveProfileBR(ctx->profileId, (float)brScaled / 100.0f);
    SetSyncReportingSuppressed(false);
}

static bool BeginLoginRatingDownload(s32 profileId, u32 licenseId) {
    if (profileId <= 0) return false;

    RKSYS::Mgr *rksys = RKSYS::Mgr::sInstance;
    if (rksys == nullptr || licenseId >= 4) return false;
    BindLicenseProfileId(licenseId, profileId);

    if (s_requestWorkBuf == nullptr) {
        s_requestWorkBuf = Network::NHTTPAlloc(s_nhttpWorkBufSize, 0x20);
        if (s_requestWorkBuf == nullptr) return false;
    }
    memset(s_requestWorkBuf, 0, s_nhttpWorkBufSize);

    s_requestCtx.generation = s_requestGeneration;
    s_requestCtx.profileId = profileId;
    s_requestCtx.licenseId = licenseId;

    if (snprintf(s_requestUrl, sizeof(s_requestUrl), "http://nas.%s/api/mkw_rr_ratings?pid=%ld",
                 WWFC_DOMAIN, (long)profileId) < 0) {
        return false;
    }

    void *request = NHTTPCreateRequest(s_requestUrl, 0, s_requestWorkBuf, s_nhttpWorkBufSize,
                                       reinterpret_cast<void *>(&OnRatingsDownloaded),
                                       reinterpret_cast<void *>(&s_requestCtx));
    if (request == nullptr) return false;

    const s32 sendRet = NHTTPSendRequestAsync(request);
    if (sendRet < 0) return false;
    Network::MarkNHTTPRequestActive();
    s_requestInFlight = true;
    return true;
}

static void TryStartPendingLoginRatingDownload() {
    if (!s_pendingLoginDownload || s_requestInFlight) return;
    if (!Network::PrepareNHTTPRequest()) return;
    if (!BeginLoginRatingDownload(s_pendingProfileId, s_pendingLicenseId)) return;
    s_pendingLoginDownload = false;
    s_pendingProfileId = 0;
    s_pendingLicenseId = 0;
}

static FrameLoadHook startPendingLoginRatingDownload(TryStartPendingLoginRatingDownload);

void StartLoginRatingDownload(s32 profileId, u32 licenseId) {
    if (profileId <= 0) return;

    RKSYS::Mgr *rksys = RKSYS::Mgr::sInstance;
    if (rksys == nullptr || licenseId >= 4) return;
    BindLicenseProfileId(licenseId, profileId);
    ++s_requestGeneration;
    s_pendingLoginDownload = true;
    s_pendingProfileId = profileId;
    s_pendingLicenseId = licenseId;
}

}  // namespace PointRating
}  // namespace Pulsar
