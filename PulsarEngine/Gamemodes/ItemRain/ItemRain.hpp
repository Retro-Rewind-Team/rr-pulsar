#ifndef _PULSAR_ITEMRAIN_
#define _PULSAR_ITEMRAIN_
#include <kamek.hpp>
#include <MarioKartWii/System/Identifiers.hpp>

namespace Pulsar {
namespace ItemRain {

// Unused native Obj::bitfield7c bit; cleared by Obj::Spawn before pool reuse.
static const u32 spawnedItemFlag = 0x80000000;

bool IsItemRainEnabled();

}  // namespace ItemRain
}  // namespace Pulsar

#endif
