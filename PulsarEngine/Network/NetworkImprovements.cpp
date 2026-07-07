#include <kamek.hpp>
#include <MarioKartWii/Kart/KartLink.hpp>
#include <MarioKartWii/RKNet/RH1.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/RKNet/User.hpp>
#include <core/rvl/DWC/DWCCore.hpp>
#include <core/rvl/OS/OS.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>
#include <runtimeWrite.hpp>

extern "C" u32 DWC_GetAidBitmap();
extern "C" BOOL DWC_IsValidAid(u8 aid);

namespace Pulsar {
namespace Network {

static const u32 PHANTOM_AID_TIMEOUT_SECONDS = 2;
static u32 s_phantomAids = 0;

void MarkPhantomAid(u32 aid) {
    if (aid >= 12) return;
    s_phantomAids |= 1 << aid;
}

void ClearPhantomAid(u32 aid) {
    if (aid >= 12) return;
    s_phantomAids &= ~(1 << aid);
}

bool ShouldPreservePhantomAid(u32 aid) {
    if (aid >= 12) return false;

    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    if (controller == nullptr) return false;

    const RKNet::ControllerSub& sub = controller->subs[controller->currentSub];
    if (sub.localAid == sub.hostAid || aid == sub.hostAid) return false;

    return (s_phantomAids & (1 << aid)) != 0;
}

static void ExpireStalePhantomAids(RKNet::Controller* controller) {
    const u32 phantomAids = s_phantomAids;
    if (phantomAids == 0) return;

    const u64 now = OS::GetTime();
    const u64 timeout = static_cast<u64>(OS::GetTimerClock()) * PHANTOM_AID_TIMEOUT_SECONDS;
    for (u32 aid = 0; aid < 12; ++aid) {
        const u32 aidBit = 1 << aid;
        if ((phantomAids & aidBit) == 0) continue;

        const u64 lastReceived = controller->lastRACERecivedTimes[aid];
        if (lastReceived != 0 && now - lastReceived <= timeout) continue;

        s_phantomAids &= ~aidBit;
        controller->ProcessPlayerDisconnect(aid);
    }
}

static u32 GetAidBitmapWithPhantomAids() {
    u32 aidBitmap = DWC_GetAidBitmap();

    RKNet::Controller* controller = RKNet::Controller::sInstance;
    if (controller == nullptr) {
        s_phantomAids = 0;
        return aidBitmap;
    }

    const RKNet::ControllerSub& sub = controller->subs[controller->currentSub];
    const u32 localAidBit = 1 << sub.localAid;
    const u32 hostAidBit = 1 << sub.hostAid;
    if ((aidBitmap & localAidBit) == 0 || (aidBitmap & hostAidBit) == 0) {
        s_phantomAids = 0;
        return aidBitmap;
    }

    s_phantomAids &= ~(localAidBit | hostAidBit);
    ExpireStalePhantomAids(controller);
    return aidBitmap | s_phantomAids;
}
kmCall(0x80658e10, GetAidBitmapWithPhantomAids);

static bool IsValidAidOrPhantom(u32 aid) {
    return DWC_IsValidAid(aid) || ShouldPreservePhantomAid(aid);
}
kmCall(0x800e8690, IsValidAidOrPhantom);

// Reduce PING retry time from 700 to 80 [Wiimmfi]
kmWrite16(0x8011B47A, 80);

// Do not wait the retry time in case of successful NATNEG [Wiimmfi]
kmWrite32(0x8011B4B0, 0x60000000);

// Change the SYN-ACK timeout to 7 seconds instead of 5 seconds per node [Wiimmfi]
kmWrite32(0x800E1A58, 0x38C00000 | 7000);

// Fix the "suspend bug" where DWC stalls suspending due to ongoing NATNEG [WiiLink24, MrBean35000vr]
kmWrite32(0x800E77F8, 0x60000000);
kmWrite32(0x800E77FC, 0x60000000);

// Slower High Data Rate [MrBean35000vr, Chadderz]
kmWrite32(0x80657EA8, 0x2804000C);

}  // namespace Network
}  // namespace Pulsar
