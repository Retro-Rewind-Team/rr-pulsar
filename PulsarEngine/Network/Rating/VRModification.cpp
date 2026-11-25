#include <Network/Rating/VRModification.hpp>
#include <Network/Json.hpp>
#include <Network/NHTTPHelper.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/RKSYS/RKSYSMgr.hpp>
#include <core/rvl/DWC/NHTTP.hpp>
#include <core/rvl/NHTTP/NHTTP.hpp>
#include <core/rvl/OS/OS.hpp>
#include <core/rvl/VI/VI.hpp>
#include <include/c_string.h>

namespace Pulsar {
namespace Network {

volatile s32 topLeaderboardVR = 0;

static u32 ParseTopVR(char *body, int length) {
    char *json = static_cast<char *>(NHTTPAlloc(length + 1, 4));
    if (json == nullptr)
        return 0;
    memcpy(json, body, length);
    json[length] = '\0';

    u32 vr = 0;
    Json::Value array;
    Json::Value first;
    const char *cursor = nullptr;
    if (Json::FindArray(json, array) && Json::Next(array, cursor, first))
        Json::Get(first, "vr", vr);
    NHTTPFree(json);
    return vr;
}

static void OnLeaderboardReceived(s32 result, void *response, void *buffer) {
    FinishNHTTPRequest();
    NHTTP::Res *res = static_cast<NHTTP::Res *>(response);
    u32 vr = 0;
    if (result == 0 && res != nullptr && NHTTP::GetResultCode(res) == 200) {
        char *body = nullptr;
        const int length = NHTTP::GetBodyAll(res, &body);
        if (body != nullptr && length > 0)
            vr = ParseTopVR(body, length);
    }
    if (res != nullptr)
        NHTTPDestroyResponse(res);
    NHTTPFree(buffer);
    topLeaderboardVR = static_cast<s32>(vr);
}

// Runs on the network thread from Controller::ConnectToWFC, so blocking here does not stall the UI.
bool PreventVRModification() {
    const RKSYS::Mgr *save = RKSYS::Mgr::sInstance;
    if (save == nullptr || save->curLicenseId < 0 || !PrepareNHTTPRequest())
        return false;

    void *buffer = NHTTPAlloc(vrLeaderboardBufferSize, 0x20);
    if (buffer == nullptr)
        return false;
    void *request = NHTTPCreateRequest(vrLeaderboardTopUrl, 0, buffer, vrLeaderboardBufferSize, reinterpret_cast<void *>(&OnLeaderboardReceived), buffer);
    if (request == nullptr) {
        NHTTPFree(buffer);
        return false;
    }

    topLeaderboardVR = -1;
    MarkNHTTPRequestActive();
    if (NHTTPSendRequestAsync(request) < 0) {
        FinishNHTTPRequest();
        NHTTPFree(buffer);
        return false;
    }

    const u64 start = OS::GetTime();
    while (topLeaderboardVR < 0 && OS::TicksToMilliseconds(OS::GetTime() - start) < vrModificationTimeoutMs) VI::WaitForRetrace();

    const s32 topVR = topLeaderboardVR;
    const u32 userVR = save->licenses[save->curLicenseId].vr.points;
    if (topVR <= 0 || userVR < static_cast<u32>(topVR) + vrModificationMargin)
        return false;

    RKNet::Controller::sInstance->SetDisconnectInfo(1, VR_MODIFICATION_ERROR_CODE);
    return true;
}

}  // namespace Network
}  // namespace Pulsar
