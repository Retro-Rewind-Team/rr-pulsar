#include <kamek.hpp>
#include <MarioKartWii/Kart/KartLink.hpp>
#include <MarioKartWii/RKNet/RH1.hpp>
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

// Send RACE packets to up to 3 aids per network tick [ZPL]
kmWrite32(0x80657F5C, 0x7F9C1A14);  // add r28, r28, r3
kmWrite32(0x80657FB4, 0x93590008);  // stw r26, 0x8(r25)
kmWrite32(0x80657FB8, 0x2C1C0003);  // cmpwi r28, 3

static float GetRemotePredictionMultiplier(u8 playerIdx) {
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    if (!controller) return 0.1f;

    const RKNet::ControllerSub& sub = controller->subs[controller->currentSub];
    float multiplier = 0.1f;
    if (sub.playerCount <= 6) {
        multiplier = 0.12f;
    } else if (sub.playerCount >= 10) {
        multiplier = 0.08f;
    }

    const u8 aid = controller->aidsBelongingToPlayerIds[playerIdx];
    if (aid >= 12 || aid == sub.localAid) return multiplier;

    const RKNet::PacketHolder<RKNet::RACEHEADER1Packet>* holder =
            controller->GetReceivedPacketHolder<RKNet::RACEHEADER1Packet>(aid);
    if (holder && holder->packet && holder->packetSize >= 0xe) {
        const u16 lagFrames = holder->packet->lagFrames;
        if (lagFrames >= 20) {
            multiplier += 0.06f;
        } else if (lagFrames >= 10) {
            multiplier += 0.03f;
        }
    }
    return multiplier;
}

// Reduce remote kart forward prediction from received RACE packets based on lobby size and lag [ZPL]
static float GetReducedRemotePredictionSpeed(const Kart::Link* kartLink) {
    return kartLink->GetEngineSpeed() * GetRemotePredictionMultiplier(kartLink->GetPlayerIdx());
}
kmCall(0x8058B5E8, GetReducedRemotePredictionSpeed);

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
        if (sUserPacketRefreshCounter == 300) {
            handler->CreateSendPacket();
        }
    }
}
kmCall(0x806579ac, UserUpdateWithMiiRefresh);

}  // namespace Network
}  // namespace Pulsar
