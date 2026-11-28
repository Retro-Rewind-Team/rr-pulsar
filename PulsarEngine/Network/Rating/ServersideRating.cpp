#include <Network/Rating/ServersideRating.hpp>
#include <IO/RRRating.hpp>
#include <MarioKartWii/RKSYS/RKSYSMgr.hpp>
#include <MarioKartWii/UI/Page/Leaderboard/Leaderboard.hpp>
#include <Network/GPReport.hpp>
#include <Network/Json.hpp>
#include <Network/NHTTPHelper.hpp>
#include <Network/WiiLink.hpp>
#include <core/nw4r/ut/Misc.hpp>
#include <core/rvl/DWC/NHTTP.hpp>
#include <core/rvl/NHTTP/NHTTP.hpp>
#include <core/rvl/OS/OS.hpp>
#include <core/rvl/VI/VI.hpp>
#include <include/c_stdio.h>

namespace Pulsar {
namespace Network {

void ReportRating(u32 vr, u32 br) {
    char report[sizeof("vr=4294967295|br=4294967295")];
    snprintf(report, sizeof(report), "vr=%u|br=%u", vr, br);
    Report("wl:mkw_vrbr", report);
}

static void SaveAndReportRatings() {
    const RKSYS::Mgr &save = *RKSYS::Mgr::sInstance;
    const RKSYS::LicenseMgr &license = save.licenses[save.curLicenseId];
    const u32 oldVR = license.vr.points;
    const u32 oldBR = license.br.points;
    Pages::Leaderboard::SaveRatingsAndStats();
    if (license.vr.points != oldVR || license.br.points != oldBR)
        ReportRating(license.vr.points, license.br.points);
}
kmCall(0x8064f354, SaveAndReportRatings);
kmCall(0x8085d520, SaveAndReportRatings);

static void OnRatingReceived(s32 result, void *response, void *userdata) {
    RatingRequest *request = static_cast<RatingRequest *>(userdata);
    NHTTP::Res *res = static_cast<NHTTP::Res *>(response);
    int status = -1;
    u32 vr = 0, br = 0;
    if (result == 0 && res != nullptr) {
        const int code = NHTTP::GetResultCode(res);
        if (code == 404) {
            status = code;
        } else if (code == 200) {
            char *body = nullptr;
            const int length = NHTTP::GetBodyAll(res, &body);
            Json::Value root, rating;
            if (length > 0 && Json::Parse(body, length, root) && Json::Find(root, "Rating", rating) && Json::Get(rating, "VR", vr) && Json::Get(rating, "BR", br) && vr <= RRRating::maxRating
              && br <= RRRating::maxRating)
                status = code;
        }
    }
    if (res != nullptr)
        NHTTPDestroyResponse(res);
    FinishNHTTPRequest();

    {
        nw4r::ut::AutoInterruptLock lock;
        if (request->response != nullptr) {
            request->response->vr = vr;
            request->response->br = br;
            request->response->status = status;
        }
    }
    NHTTPFree(request);
}

void StoreRatingUponConnection(int profileId) {
    RKSYS::Mgr &save = *RKSYS::Mgr::sInstance;
    if (profileId <= 0 || save.curLicenseId >= 4 || !PrepareNHTTPRequest())
        return;

    RKSYS::LicenseMgr &license = save.licenses[save.curLicenseId];
    RatingResponse response = {};
    RatingRequest *request = static_cast<RatingRequest *>(NHTTPAlloc(sizeof(RatingRequest), 0x20));
    if (request == nullptr)
        return;
    request->response = &response;
    char url[sizeof("http://" WWFC_DOMAIN "/api/mkw_rating?pid=2147483647")];
    snprintf(url, sizeof(url), "http://" WWFC_DOMAIN "/api/mkw_rating?pid=%d", profileId);
    void *httpRequest = NHTTPCreateRequest(url, NHTTP::NHTTP_REQMETHOD_GET, request->buffer, sizeof(request->buffer), reinterpret_cast<void *>(&OnRatingReceived), request);
    if (httpRequest == nullptr) {
        NHTTPFree(request);
        return;
    }
    MarkNHTTPRequestActive();
    if (NHTTPSendRequestAsync(httpRequest) < 0) {
        FinishNHTTPRequest();
        NHTTPFree(request);
        return;
    }

    // Keep the login callback on the network thread so it can report through GameSpy.
    const u64 start = OS::GetTime();
    while (response.status == 0 && OS::TicksToMilliseconds(OS::GetTime() - start) < 5000) VI::WaitForRetrace();
    {
        nw4r::ut::AutoInterruptLock lock;
        if (response.status == 0) {
            // A late completion still owns the HTTP buffer.
            request->response = nullptr;
            return;
        }
    }
    if (response.status < 0)
        return;

    const s32 difference = static_cast<s32>(response.vr) - static_cast<s32>(license.vr.points);
    // The server returns this pair before the first rating report.
    if (response.status == 200 && (response.vr != 5000 || response.br != 5000) && (difference < -500 || difference > 500)) {
        if (!RRRating::StoreRating(*save.rawRKSYS, profileId, response.vr, response.br))
            return;
        license.vr.points = response.vr;
        license.br.points = response.br;
    }
    ReportRating(license.vr.points, license.br.points);
}

}  // namespace Network
}  // namespace Pulsar
