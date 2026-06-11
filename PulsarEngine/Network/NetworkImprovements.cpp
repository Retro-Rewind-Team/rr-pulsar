#include <kamek.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/RKNet/User.hpp>
#include <runtimeWrite.hpp>

namespace Pulsar {
namespace Network {

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

static void TrySendAllRACEPackets(RKNet::Controller* controller) {
    const RKNet::ControllerSub& sub = controller->subs[controller->currentSub];
    for (u8 aid = 0; aid < 12; ++aid) {
        if (aid == sub.localAid) continue;
        if ((sub.availableAids & (1 << aid)) == 0) continue;
        controller->SendAidNextRACEPacket(aid);
    }
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
