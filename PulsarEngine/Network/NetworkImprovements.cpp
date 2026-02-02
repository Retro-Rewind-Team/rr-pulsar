#include <kamek.hpp>
#include <MarioKartWii/System/Identifiers.hpp>
#include <MarioKartWii/Item/ItemManager.hpp>
#include <MarioKartWii/Item/ItemBehaviour.hpp>
#include <MarioKartWii/Item/Obj/ItemObjHolder.hpp>
#include <MarioKartWii/Item/ItemPlayer.hpp>
#include <MarioKartWii/Item/ItemSlot.hpp>
#include <MarioKartWii/RKNet/ITEM.hpp>
#include <runtimeWrite.hpp>

namespace Pulsar {
namespace Network {

// Reduce PING retry time from 700 to 80 [Wiimmfi]
kmWrite16(0x8011B47A, 80);

// Do not wait the retry time in case of successful NATNEG [Wiimmfi]
kmWrite32(0x8011B4B0, 0x60000000);

// Do not wait the idle time after a successful NATNEG [WiiLink24]
kmWrite16(0x8011BC3A, 0);

// Change the SYN-ACK timeout to 7 seconds instead of 5 seconds per node [Wiimmfi]
kmWrite32(0x800E1A58, 0x38C00000 | 7000);

// Fix the "suspend bug" where DWC stalls suspending due to ongoing NATNEG [WiiLink24, MrBean35000vr]
kmWrite32(0x800E77F8, 0x60000000);
kmWrite32(0x800E77FC, 0x60000000);

// Slower High Data Rate [MrBean35000vr, Chadderz]
kmWrite32(0x80657EA8, 0x2804000C);

// Pulsar Network Optimizations [ZPL]
// Reduce GT2 keep-alive timeout from 30000ms to 20000ms (cmplwi r0, 0x7530 -> 0x4e20)
kmWrite16(0x8010A666, 20000);

// Reduce GT2 ACK delay from 100ms to 50ms (cmplwi r0, 0x64 -> 0x32)
kmWrite16(0x8010A722, 50);

// Reduce server polling interval from 15000ms to 10000ms (li r6, 0x3a98 -> 0x2710)
kmWrite16(0x800E6E1E, 10000);

// Reduce match state 1 timeout from 3000ms to 2000ms (li r6, 0xbb8 -> 0x7d0)
kmWrite16(0x800D69AA, 2000);

// Reduce connection check delay from 5000ms to 3000ms (li r6, 0x1388 -> 0xbb8)
kmWrite16(0x800D771E, 3000);

// Reduce NN phase timeout from 6000ms to 4000ms (li r6, 0x1770 -> 0xfa0)
kmWrite16(0x800D6C96, 4000);

// Reduce NATNEG report retry delay from 1000ms to 500ms (addi r0, r3, 0x3e8 -> 0x1f4)
kmWrite16(0x8011B6F6, 500);

}  // namespace Network
}  // namespace Pulsar