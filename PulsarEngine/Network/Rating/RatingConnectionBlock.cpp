#include <kamek.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <Network/Rating/PlayerRating.hpp>
#include <MarioKartWii/RKSYS/RKSYSMgr.hpp>

namespace Pulsar {
namespace Network {

extern "C" void DWC_LoginAsync(wchar_t* miiName, int unk, void* callback, RKNet::Controller* self);

void CheckVRAndLogin(wchar_t* miiName, int unk, void* callback, RKNet::Controller* self) {
    bool block = false;
    RKSYS::Mgr* rksys = RKSYS::Mgr::sInstance;
    if (rksys) {
        float vr = PointRating::GetUserVR(rksys->curLicenseId);
        if (vr > 1837.67f) {
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

    // Call original DWC_LoginAsync
    DWC_LoginAsync(miiName, unk, callback, self);
}
kmCall(0x80658cdc, CheckVRAndLogin);

}  // namespace Network
}  // namespace Pulsar
