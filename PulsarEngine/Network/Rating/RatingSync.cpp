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
#include <Network/Rating/PlayerRating.hpp>
#include <Network/Rating/RatingSync.hpp>
#include <Network/WiiLink.hpp>

namespace Pulsar {
namespace PointRating {

static bool s_syncReportingSuppressed = false;
static const u32 s_nhttpWorkBufSize = 0x1000;
static u32 s_requestGeneration = 0;
static float s_requestStartVr = 0.0f;
static float s_requestStartBr = 0.0f;
static void* s_requestWorkBuf = nullptr;
static char s_requestUrl[160];

struct RequestCtx {
    u32 generation;
    s32 profileId;
    u32 licenseId;
};

static RequestCtx s_requestCtx;

static void* NHTTPAllocFromEggHeap(u32 size, s32 align) {
    EGG::Heap* heap = RKSystem::mInstance.EGGSystem;
    if (heap == nullptr) return nullptr;
    if (align < 4) align = 4;
    return EGG::Heap::alloc(size, align, heap);
}

static void NHTTPFreeFromEggHeap(void* ptr) {
    if (ptr == nullptr) return;
    EGG::Heap* heap = RKSystem::mInstance.EGGSystem;
    if (heap != nullptr) EGG::Heap::free(ptr, heap);
}

static int ClampRatingForSync(float rating) {
    int scaled = (int)(rating * 100.0f + 0.5f);
    if (scaled < 1) return 1;
    if (scaled > 1000000) return 1000000;
    return scaled;
}

static const char* SkipWhitespace(const char* p) {
    while (p != nullptr && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) ++p;
    return p;
}

static bool ParseJsonScaledValue(const char* json, const char* key, int& out) {
    if (json == nullptr || key == nullptr) return false;

    const char* pos = strstr(json, key);
    if (pos == nullptr) return false;

    const char* colon = strchr(pos, ':');
    if (colon == nullptr) return false;

    char* end = nullptr;
    long value = strtol(SkipWhitespace(colon + 1), &end, 10);
    if (end == nullptr || end == colon + 1) return false;
    out = (int)value;
    return true;
}

static bool ParseJsonFoundFlag(const char* json) {
    if (json == nullptr) return false;

    const char* pos = strstr(json, "\"found\"");
    if (pos == nullptr) return false;

    const char* colon = strchr(pos, ':');
    if (colon == nullptr) return false;

    colon = SkipWhitespace(colon + 1);
    return colon != nullptr && *colon == '1';
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

static bool IsRequestStillRelevant(const RequestCtx& ctx) {
    if (ctx.generation != s_requestGeneration) return false;
    if (ctx.profileId <= 0) return false;

    RKSYS::Mgr* rksys = RKSYS::Mgr::sInstance;
    if (rksys == nullptr || ctx.licenseId >= 4) return false;

    const RKSYS::LicenseMgr& license = rksys->licenses[ctx.licenseId];
    if ((s32)license.dwcAccUserData.gsProfileId != ctx.profileId) return false;

    const float currentVr = GetUserVR(ctx.licenseId);
    const float currentBr = GetUserBR(ctx.licenseId);
    if (currentVr != s_requestStartVr || currentBr != s_requestStartBr) return false;

    return true;
}

static void OnRatingsDownloaded(s32 result, void* response, void* userdata) {
    RequestCtx* ctx = reinterpret_cast<RequestCtx*>(userdata);
    if (ctx == nullptr || response == nullptr) return;

    if (ctx->generation != s_requestGeneration) {
        NHTTPDestroyResponse(response);
        return;
    }

    if (result != 0) {
        NHTTPDestroyResponse(response);
        return;
    }

    char* body = nullptr;
    const int bodyLen = NHTTP::GetBodyAll(reinterpret_cast<NHTTP::Res*>(response), &body);
    if (body == nullptr || bodyLen <= 0) {
        NHTTPDestroyResponse(response);
        return;
    }

    const int maxLen = bodyLen < 255 ? bodyLen : 255;
    char json[256];
    memcpy(json, body, maxLen);
    json[maxLen] = '\0';

    NHTTPDestroyResponse(response);

    if (!ParseJsonFoundFlag(json)) return;
    if (!IsRequestStillRelevant(*ctx)) return;

    int vrScaled = 0;
    int brScaled = 0;
    if (!ParseJsonScaledValue(json, "\"vr\"", vrScaled)) return;
    if (!ParseJsonScaledValue(json, "\"br\"", brScaled)) return;

    SetSyncReportingSuppressed(true);
    SaveProfileVR(ctx->profileId, (float)vrScaled / 100.0f);
    SaveProfileBR(ctx->profileId, (float)brScaled / 100.0f);
    SetSyncReportingSuppressed(false);
}

void StartLoginRatingDownload(s32 profileId, u32 licenseId) {
    if (profileId <= 0) return;

    RKSYS::Mgr* rksys = RKSYS::Mgr::sInstance;
    if (rksys == nullptr || licenseId >= 4) return;
    BindLicenseProfileId(licenseId, profileId);

    const s32 startupRet = NHTTPStartup(reinterpret_cast<void*>(&NHTTPAllocFromEggHeap),
                                        reinterpret_cast<void*>(&NHTTPFreeFromEggHeap),
                                        0x11);
    if (startupRet < 0) return;

    if (s_requestWorkBuf == nullptr) {
        s_requestWorkBuf = NHTTPAllocFromEggHeap(s_nhttpWorkBufSize, 0x20);
        if (s_requestWorkBuf == nullptr) return;
    }
    memset(s_requestWorkBuf, 0, s_nhttpWorkBufSize);

    ++s_requestGeneration;
    s_requestStartVr = GetUserVR(licenseId);
    s_requestStartBr = GetUserBR(licenseId);

    s_requestCtx.generation = s_requestGeneration;
    s_requestCtx.profileId = profileId;
    s_requestCtx.licenseId = licenseId;

    if (snprintf(s_requestUrl, sizeof(s_requestUrl), "http://nas.%s/api/mkw_rr_ratings?pid=%ld",
                 WWFC_DOMAIN, (long)profileId) < 0) {
        return;
    }

    void* request = NHTTPCreateRequest(s_requestUrl, 0, s_requestWorkBuf, s_nhttpWorkBufSize,
                                       reinterpret_cast<void*>(&OnRatingsDownloaded),
                                       reinterpret_cast<void*>(&s_requestCtx));
    if (request == nullptr) return;

    const s32 sendRet = NHTTPSendRequestAsync(request);
    if (sendRet < 0) {
        ++s_requestGeneration;
    }
}

}  // namespace PointRating
}  // namespace Pulsar