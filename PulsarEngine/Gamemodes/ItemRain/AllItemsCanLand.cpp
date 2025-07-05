#include <kamek.hpp>
#include <MarioKartWii/Kart/KartCollision.hpp>
#include <MarioKartWii/Item/ItemManager.hpp>
#include <MarioKartWii/Item/ItemBehaviour.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/Item/Obj/ObjProperties.hpp>
#include <PulsarSystem.hpp>

namespace Pulsar {
namespace Race {

// originally developed by Brawlboxgaming, now adapted for rr's item rain.
int UseItem(Kart::Collision *kartCollision, ItemId id){
    u8 playerId = kartCollision->GetPlayerIdx();
    Item::Manager::sInstance->players[playerId].inventory.currentItemCount++;
    Item::Behavior::behaviourTable[id].useFunction(Item::Manager::sInstance->players[playerId]);
    return -1;
}

int AllShocksCanLand(Kart::Collision *kartCollision){
    if ((System::sInstance->IsContext(PULSAR_ITEMMODERAIN) || System::sInstance->IsContext(PULSAR_ITEMMODESTORM)) && (RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_HOST || RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_NONHOST || 
            RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_NONE)) return UseItem(kartCollision, LIGHTNING);
    return -1;
}

int AllMegasCanLand(Kart::Collision *kartCollision){
    if ((System::sInstance->IsContext(PULSAR_ITEMMODERAIN) || System::sInstance->IsContext(PULSAR_ITEMMODESTORM)) && (RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_HOST || RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_NONHOST || 
            RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_NONE)) return UseItem(kartCollision, MEGA_MUSHROOM);
    return -1;
}

int AllFeathersCanLand(Kart::Collision *kartCollision){
    if ((System::sInstance->IsContext(PULSAR_ITEMMODERAIN) || System::sInstance->IsContext(PULSAR_ITEMMODESTORM)) && (RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_HOST || RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_NONHOST || 
            RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_NONE)) return UseItem(kartCollision, BLOOPER);
    return -1;
}

int AllPOWsCanLand(Kart::Collision *kartCollision){
    if ((System::sInstance->IsContext(PULSAR_ITEMMODERAIN) || System::sInstance->IsContext(PULSAR_ITEMMODESTORM)) && (RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_HOST || RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_NONHOST || 
            RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_NONE)) return UseItem(kartCollision, POW_BLOCK);
    return -1;
}

int AllGoldensCanLand(Kart::Collision *kartCollision){
    if ((System::sInstance->IsContext(PULSAR_ITEMMODERAIN) || System::sInstance->IsContext(PULSAR_ITEMMODESTORM)) && (RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_HOST || RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_NONHOST || 
            RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_NONE)) return UseItem(kartCollision, MUSHROOM);
    return -1;
}

int AllBulletsCanLand(Kart::Collision *kartCollision){
    if ((System::sInstance->IsContext(PULSAR_ITEMMODERAIN) || System::sInstance->IsContext(PULSAR_ITEMMODESTORM)) && (RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_HOST || RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_NONHOST || 
            RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_NONE)) return UseItem(kartCollision, BULLET_BILL);
    return -1;
}

void AllowDroppedItems() {
    if (Pulsar::System::sInstance->IsContext(PULSAR_ITEMMODERAIN) || Pulsar::System::sInstance->IsContext(PULSAR_ITEMMODESTORM)) {
        if (RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_HOST || RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_NONHOST || 
            RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_NONE) {
            for (int i = 0; i < 15; i++) {
                Item::ObjProperties::objProperties[i].canFallOnTheGround = true;
            }
        }
    }
}
kmBranch(0x80790af8, AllowDroppedItems);

kmWritePointer(0x808b54b8, AllShocksCanLand);
kmWritePointer(0x808b54d0, AllMegasCanLand);
kmWritePointer(0x808b54f4, AllPOWsCanLand);
kmWritePointer(0x808b5500, AllGoldensCanLand);
kmWritePointer(0x808b550c, AllBulletsCanLand);

} // namespace Race
} // namespace Pulsar