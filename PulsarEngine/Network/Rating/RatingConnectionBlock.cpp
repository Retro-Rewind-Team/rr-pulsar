#include <kamek.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <Network/Rating/PlayerRating.hpp>
#include <Network/Rating/RatingSync.hpp>
#include <MarioKartWii/RKSYS/RKSYSMgr.hpp>

namespace Pulsar {
namespace Network {

extern "C" void DWC_LoginAsync(wchar_t *miiName, int unk, void *callback, RKNet::Controller *self);
static DWC::LoginCallback s_originalLoginCallback = nullptr;

static void LoginCallbackAndQueueRating(DWC::Error error, int profileId, void *param) {
    if (s_originalLoginCallback != nullptr) s_originalLoginCallback(error, profileId, param);
    RKSYS::Mgr *rksys = RKSYS::Mgr::sInstance;
    const u32 licenseId = rksys->curLicenseId;
    PointRating::StartLoginRatingDownload(profileId, licenseId);
}

void CheckVRAndLogin(wchar_t *miiName, int unk, void *callback, RKNet::Controller *self) {
    bool block = false;
    RKSYS::Mgr *rksys = RKSYS::Mgr::sInstance;
    if (rksys) {
        float vr = PointRating::GetUserVR(rksys->curLicenseId);
        if (vr > 2587.67f) {
            block = true;
        }
    }

    if (block) {
        self->errorParams.errorParam1 = 1;  // Show error
        self->errorParams.dwcErrorCode = 42069;
        // State 7 is CONNECTIONSTATE_ERROR
        self->connectionState = static_cast<RKNet::ConnectionState>(7);
        return;
    }

    s_originalLoginCallback = reinterpret_cast<DWC::LoginCallback>(callback);
    DWC_LoginAsync(miiName, unk, reinterpret_cast<void *>(&LoginCallbackAndQueueRating), self);
}
kmCall(0x80658cdc, CheckVRAndLogin);

}  // namespace Network
}  // namespace Pulsar