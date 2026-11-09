#include <kamek.hpp>
#include <MarioKartWii/Kart/KartCollision.hpp>
#include <MarioKartWii/Item/ItemManager.hpp>
#include <MarioKartWii/Item/ItemBehaviour.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/Item/Obj/ObjProperties.hpp>
#include <PulsarSystem.hpp>

namespace Pulsar {
namespace Race {

// Originally developed by Brawlboxgaming
static bool IsAllItemsCanLandActive() {
    const System *system = System::sInstance;
    switch (RKNet::Controller::sInstance->roomType) {
        case RKNet::ROOMTYPE_FROOM_HOST:
        case RKNet::ROOMTYPE_FROOM_NONHOST:
        case RKNet::ROOMTYPE_NONE:
            return system->IsContext(PULSAR_ITEMMODESTORM) || system->IsContext(PULSAR_ITEMMODERAIN) || system->IsContext(PULSAR_ALLITEMSCANLAND);
        case RKNet::ROOMTYPE_VS_REGIONAL:
        case RKNet::ROOMTYPE_JOINING_REGIONAL:
            return system->IsContext(PULSAR_ITEMMODERAIN);
        default:
            return false;
    }
}

template <ItemId id>
static int UseDroppedItem(Kart::Collision *kartCollision) {
    if (IsAllItemsCanLandActive()) {
        Item::Player &player = Item::Manager::sInstance->players[kartCollision->GetPlayerIdx()];
        player.inventory.currentItemCount++;
        Item::Behavior::behaviourTable[id].useFunction(player);
    }
    return -1;
}
kmWritePointer(0x808b54b8, static_cast<int (*)(Kart::Collision *)>(UseDroppedItem<LIGHTNING>));
kmWritePointer(0x808b54d0, static_cast<int (*)(Kart::Collision *)>(UseDroppedItem<MEGA_MUSHROOM>));
kmWritePointer(0x808b54f4, static_cast<int (*)(Kart::Collision *)>(UseDroppedItem<POW_BLOCK>));
kmWritePointer(0x808b5500, static_cast<int (*)(Kart::Collision *)>(UseDroppedItem<MUSHROOM>));
kmWritePointer(0x808b550c, static_cast<int (*)(Kart::Collision *)>(UseDroppedItem<BULLET_BILL>));

void AllowDroppedItems() {
    if (IsAllItemsCanLandActive()) {
        for (int i = 0; i <= OBJ_THUNDER_CLOUD; i++) {
            Item::ObjProperties::objProperties[i].canFallOnTheGround = true;
        }
    }
}
kmBranch(0x80790af8, AllowDroppedItems);

}  // namespace Race
}  // namespace Pulsar
