#include <kamek.hpp>
#include <MarioKartWii/Kart/KartCollision.hpp>
#include <MarioKartWii/Item/ItemManager.hpp>
#include <MarioKartWii/Item/ItemBehaviour.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/Item/Obj/ObjProperties.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <Settings/Settings.hpp>
#include <PulsarSystem.hpp>

namespace Pulsar {
namespace Race {

static bool IsItemStormForcedEnabled() {
    const RKNet::RoomType roomType = RKNet::Controller::sInstance->roomType;
    if (roomType == RKNet::ROOMTYPE_FROOM_HOST ||
        roomType == RKNet::ROOMTYPE_FROOM_NONHOST ||
        roomType == RKNet::ROOMTYPE_NONE) {
        return Pulsar::System::sInstance->IsContext(PULSAR_ITEMMODESTORM);
    }
    return false;
}

static bool IsItemRainForcedEnabled() {
    const RKNet::RoomType roomType = RKNet::Controller::sInstance->roomType;
    if (roomType == RKNet::ROOMTYPE_FROOM_HOST ||
        roomType == RKNet::ROOMTYPE_FROOM_NONHOST ||
        roomType == RKNet::ROOMTYPE_NONE ||
        roomType == RKNet::ROOMTYPE_VS_REGIONAL ||
        roomType == RKNet::ROOMTYPE_JOINING_REGIONAL) {
        return Pulsar::System::sInstance->IsContext(PULSAR_ITEMMODERAIN);
    }
    return false;
}

static bool IsOfflineRaceOrBattle() {
    if (RKNet::Controller::sInstance->roomType != RKNet::ROOMTYPE_NONE) {
        return false;
    }

    const GameMode mode = Racedata::sInstance->racesScenario.settings.gamemode;
    return mode == MODE_VS_RACE || mode == MODE_PRIVATE_VS || mode == MODE_BATTLE || mode == MODE_PRIVATE_BATTLE;
}

static bool IsAllItemsCanLandSettingEnabled() {
    const u8 setting = Settings::Mgr::Get().GetUserSettingValue(Settings::SETTINGSTYPE_FROOM2, RADIO_ALLITEMSCANLAND);
    if (setting != ALLITEMSCANLAND_ENABLED) {
        return false;
    }

    const RKNet::RoomType roomType = RKNet::Controller::sInstance->roomType;
    const bool isFroom = roomType == RKNet::ROOMTYPE_FROOM_HOST || roomType == RKNet::ROOMTYPE_FROOM_NONHOST;
    return isFroom || IsOfflineRaceOrBattle();
}

static bool IsAllItemsCanLandActive() {
    return IsItemStormForcedEnabled() || IsItemRainForcedEnabled() || IsAllItemsCanLandSettingEnabled();
}

// originally developed by Brawlboxgaming, now adapted for rr's item rain.
int UseItem(Kart::Collision *kartCollision, ItemId id) {
    u8 playerId = kartCollision->GetPlayerIdx();
    Item::Manager::sInstance->players[playerId].inventory.currentItemCount++;
    Item::Behavior::behaviourTable[id].useFunction(Item::Manager::sInstance->players[playerId]);
    return -1;
}

int AllShocksCanLand(Kart::Collision *kartCollision) {
    if (IsAllItemsCanLandActive()) {
        return UseItem(kartCollision, LIGHTNING);
    }
    return -1;
}

int AllMegasCanLand(Kart::Collision *kartCollision) {
    if (IsAllItemsCanLandActive()) {
        return UseItem(kartCollision, MEGA_MUSHROOM);
    }
    return -1;
}

int AllFeathersCanLand(Kart::Collision *kartCollision) {
    if (IsAllItemsCanLandActive()) {
        return UseItem(kartCollision, BLOOPER);
    }
    return -1;
}

int AllPOWsCanLand(Kart::Collision *kartCollision) {
    if (IsAllItemsCanLandActive()) {
        return UseItem(kartCollision, POW_BLOCK);
    }
    return -1;
}

int AllGoldensCanLand(Kart::Collision *kartCollision) {
    if (IsAllItemsCanLandActive()) {
        return UseItem(kartCollision, MUSHROOM);
    }
    return -1;
}

int AllBulletsCanLand(Kart::Collision *kartCollision) {
    if (IsAllItemsCanLandActive()) {
        return UseItem(kartCollision, BULLET_BILL);
    }
    return -1;
}

void AllowDroppedItems() {
    if (IsAllItemsCanLandActive()) {
        for (int i = 0; i < 15; i++) {
            Item::ObjProperties::objProperties[i].canFallOnTheGround = true;
        }
    }
}
kmBranch(0x80790af8, AllowDroppedItems);

kmWritePointer(0x808b54b8, AllShocksCanLand);
kmWritePointer(0x808b54d0, AllMegasCanLand);
kmWritePointer(0x808b54f4, AllPOWsCanLand);
kmWritePointer(0x808b5500, AllGoldensCanLand);
kmWritePointer(0x808b550c, AllBulletsCanLand);

}  // namespace Race
}  // namespace Pulsar
