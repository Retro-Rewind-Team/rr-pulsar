#ifndef _CUSTOMITEMS_
#define _CUSTOMITEMS_

#include <kamek.hpp>
#include <MarioKartWii/System/Identifiers.hpp>

namespace Pulsar {
namespace Race {

static const u32 CUSTOM_ITEM_COUNT = TRIPLE_BANANA + 1;
static const u32 ALL_CUSTOM_ITEMS = (1u << CUSTOM_ITEM_COUNT) - 1;

u32 GetEffectiveCustomItemsBitfield();

}  // namespace Race
}  // namespace Pulsar

#endif  // _CUSTOMITEMS_