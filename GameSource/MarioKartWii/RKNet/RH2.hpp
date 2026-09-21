#ifndef _RKNETRH2_
#define _RKNETRH2_
#include <kamek.hpp>
#include <MarioKartWii/System/Identifiers.hpp>

namespace RKNet {
struct RACEHEADER2Packet {
    static const u32 idx = 2;
    u8 unknown_0x0[0x10];
    u8 player1Id;  // 0x10
    u8 player2Id;  // 0x11
    u8 unknown_0x12;
    u8 localPlayerCount;  // 0x13
    u32 timeElapsedFirstFinished;  // 0x14
    u32 minTimeBeforeTimeout;  // 0x18
    u8 disconnected;  // 0x1c
    u8 unknown_0x1d[0x28 - 0x1d];
};  // 0x28
static_assert(sizeof(RACEHEADER2Packet) == 0x28, "RACEHEADER2Packet layout changed");

}  // namespace RKNet
#endif
