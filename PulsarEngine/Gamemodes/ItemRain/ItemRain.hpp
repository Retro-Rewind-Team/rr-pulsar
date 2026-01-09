#ifndef _PULSAR_ITEMRAIN_
#define _PULSAR_ITEMRAIN_
#include <kamek.hpp>
#include <MarioKartWii/System/Identifiers.hpp>

namespace Pulsar {
namespace ItemRain {

static const u32 MAX_RAIN_ITEMS_PER_PACKET = 4;

#pragma pack(push, 1)
struct RainItemEntry {
    u8 itemObjId;  // ItemObjId of the item to spawn
    u8 targetPlayer;  // Player index to spawn near
    s16 forwardOffset;  // Forward offset from player (scaled by 10)
    s16 rightOffset;  // Right offset from player (scaled by 10)
};

struct ItemRainSyncData {
    u8 itemCount;  // Number of valid items in this packet (0-4)
    u8 syncFrame;  // Frame counter for ordering (wraps at 256)
    RainItemEntry items[MAX_RAIN_ITEMS_PER_PACKET];
};
#pragma pack(pop)

bool IsItemRainEnabled();
void PackItemData(ItemRainSyncData* dest);
void UnpackAndSpawn(const ItemRainSyncData* src, u8 senderAid);
bool IsHost();
bool IsOnline();

}  // namespace ItemRain
}  // namespace Pulsar

#endif
