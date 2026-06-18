#include <kamek.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/RKNet/User.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <runtimeWrite.hpp>

namespace Pulsar {
namespace Network {

// Reduce PING retry time from 700 to 120 [Wiimmfi]
kmWrite16(0x8011B47A, 120);

// Do not wait the retry time in case of successful NATNEG [Wiimmfi]
kmWrite32(0x8011B4B0, 0x60000000);

// Change the SYN-ACK timeout to 7 seconds instead of 5 seconds per node [Wiimmfi]
kmWrite32(0x800E1A58, 0x38C00000 | 7000);

// Fix the "suspend bug" where DWC stalls suspending due to ongoing NATNEG [WiiLink24, MrBean35000vr]
kmWrite32(0x800E77F8, 0x60000000);
kmWrite32(0x800E77FC, 0x60000000);

static void TrySendAllRACEPackets(RKNet::Controller* controller) {
    const RKNet::ControllerSub& sub = controller->subs[controller->currentSub];
    static u8 nextAid = 0;
    const Raceinfo* raceInfo = Raceinfo::sInstance;
    const bool hasRaceStarted = raceInfo != nullptr && raceInfo->timerMgr != nullptr && raceInfo->timerMgr->hasRaceStarted;
    const u8 maxAttempts = hasRaceStarted ? 12 : 1;
    u8 attempts = 0;
    u8 failedAttempts = 0;

    for (u8 i = 0; i < 12; ++i) {
        const u8 aid = (nextAid + i) % 12;
        if (aid == sub.localAid) continue;
        if ((sub.availableAids & (1 << aid)) == 0) continue;
        if (attempts >= maxAttempts) break;
        ++attempts;
        if (!controller->SendAidNextRACEPacket(aid)) {
            if (++failedAttempts >= 2) {
                nextAid = (aid + 1) % 12;
                return;
            }
        }
    }
    nextAid = (nextAid + 1) % 12;
}
kmBranch(0x80657e30, TrySendAllRACEPackets);

// Fix Ghost Player Bug [ImZeaora]
kmWrite32(0x80662f5c, 0x60000000);

static u32 sUserPacketRefreshCounter = 0;
static void UserUpdateWithMiiRefresh(RKNet::USERHandler* handler) {
    // Call the original Update implementation.
    handler->Update();

    // Once initialised, rebuild the send packet shortly after to pick up
    // any Mii data that was not yet ready during Prepare().
    // 300 frames @ 60 fps ≈ 5 seconds.
    if (handler->isInitialized) {
        sUserPacketRefreshCounter++;
        if (sUserPacketRefreshCounter >= 300) {
            handler->CreateSendPacket();
            sUserPacketRefreshCounter = 0;
        }
    } else {
        sUserPacketRefreshCounter = 0;
    }
}
kmCall(0x806579ac, UserUpdateWithMiiRefresh);

}  // namespace Network
}  // namespace Pulsar
