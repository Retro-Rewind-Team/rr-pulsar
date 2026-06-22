#include <kamek.hpp>
#include <MarioKartWii/Kart/KartLink.hpp>
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

// Send RACE packets to up to 2 aids per network tick [ZPL]
kmWrite32(0x80657F5C, 0x7F9C1A14);  // add r28, r28, r3
kmWrite32(0x80657FB4, 0x93590008);  // stw r26, 0x8(r25)
kmWrite32(0x80657FB8, 0x2C1C0002);  // cmpwi r28, 2

// Reduce remote kart forward prediction from received RACE packets to 0.1x [ZPL]
static float GetReducedRemotePredictionSpeed(const Kart::Link* kartLink) {
    return kartLink->GetEngineSpeed() * 0.1f;
}
kmCall(0x8058B5E8, GetReducedRemotePredictionSpeed);

}  // namespace Network
}  // namespace Pulsar
