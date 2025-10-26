#include <kamek.hpp>
#include <MarioKartWii/Item/ItemManager.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/Item/Obj/ItemObj.hpp>
#include <MarioKartWii/Kart/KartPlayer.hpp>
#include <PulsarSystem.hpp>

namespace Pulsar {
namespace ItemRain {

static s32 sRaceInfoFrameCounter = 0;

static int ITEMS_PER_SPAWN = 1;
static int SPAWN_HEIGHT = 1500;
static int SPAWN_RADIUS = 8000;
static int MAX_ITEM_LIFETIME = 600;
static int DESPAWN_CHECK_INTERVAL = 2;

void ItemModeCheck() {
    if (RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_HOST ||
        RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_NONHOST ||
        RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_NONE) {
        if (Pulsar::System::sInstance->IsContext(PULSAR_ITEMMODESTORM)) {
            ITEMS_PER_SPAWN = 5;
            MAX_ITEM_LIFETIME = 180;
        } else {
            ITEMS_PER_SPAWN = 1;
            MAX_ITEM_LIFETIME = 570;
        }
    } else if ((RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_VS_REGIONAL || RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_JOINING_REGIONAL) && (System::sInstance->netMgr.region == 0x0D)) {
        ITEMS_PER_SPAWN = 1;
        MAX_ITEM_LIFETIME = 570;
    }
}
static RaceLoadHook ItemModeCheckHook(ItemModeCheck);

static int GetSpawnInterval(u8 playerCount) {
    return 6;
}

static u32 GetRandom() {
    static u32 counter = 0;
    counter++;
    u32 value = (sRaceInfoFrameCounter * 1103515245 + counter * 12345) ^ (counter << 8);
    return value * 1664525 + 1013904223;
}

static float GetRandomRange(float min, float max) {
    return min + ((GetRandom() & 0xFFFF) / 65535.0f) * (max - min);
}

static ItemObjId GetRandomItem() {
    struct ItemWeight {
        ItemObjId id;
        u32 weight;
    };

    const RacedataScenario& scenario = Racedata::sInstance->racesScenario;
    const GameMode mode = scenario.settings.gamemode;
    static const ItemWeight weightsVS[] = {
        {OBJ_MUSHROOM, 20},
        {OBJ_GREEN_SHELL, 10},
        {OBJ_BANANA, 15},
        {OBJ_RED_SHELL, 8},
        {OBJ_FAKE_ITEM_BOX, 8},
        {OBJ_BOBOMB, 1},
        {OBJ_STAR, 10},
        {OBJ_BLUE_SHELL, 3},
        {OBJ_GOLDEN_MUSHROOM, 7},
        {OBJ_MEGA_MUSHROOM, 12},
        {OBJ_BULLET_BILL, 5},
    };
    static const ItemWeight weightsBattle[] = {
        {OBJ_MUSHROOM, 20},
        {OBJ_GREEN_SHELL, 10},
        {OBJ_BANANA, 15},
        {OBJ_RED_SHELL, 8},
        {OBJ_FAKE_ITEM_BOX, 8},
        {OBJ_BOBOMB, 1},
        {OBJ_STAR, 10},
        {OBJ_BLUE_SHELL, 8},
        {OBJ_GOLDEN_MUSHROOM, 7},
        {OBJ_MEGA_MUSHROOM, 12},
        {OBJ_BULLET_BILL, 0},
    };

    const ItemWeight* weights = weightsVS;
    u32 count = sizeof(weightsVS) / sizeof(weightsVS[0]);
    if (mode == MODE_BATTLE || mode == MODE_PRIVATE_BATTLE) {
        weights = weightsBattle;
        count = sizeof(weightsBattle) / sizeof(weightsBattle[0]);
    }

    u32 totalWeight = 0;
    for (u32 i = 0; i < count; i++) totalWeight += weights[i].weight;
    if (totalWeight == 0) return OBJ_MUSHROOM;

    u32 roll = GetRandom() % totalWeight;
    u32 cumulative = 0;
    for (u32 i = 0; i < count; i++) {
        cumulative += weights[i].weight;
        if (roll < cumulative) return weights[i].id;
    }
    return OBJ_MUSHROOM;
}

void DespawnItems(bool checkDistance = false) {
    if (!Item::Manager::sInstance) return;
    u8 playerCount = 0;
    Vec3 playerPositions[12];
    if (checkDistance) {
        playerCount = Pulsar::System::sInstance ? Pulsar::System::sInstance->nonTTGhostPlayersCount : 0;
        for (int i = 0; i < playerCount && i < 12; i++) {
            playerPositions[i] = Item::Manager::sInstance->players[i].GetPosition();
        }
    }
    for (int i = 0; i < 0xF; i++) {
        Item::ObjHolder& holder = Item::Manager::sInstance->itemObjHolders[i];
        if (holder.itemObjId == OBJ_NONE || holder.bodyCount == 0 || holder.itemObjId == OBJ_THUNDER_CLOUD) continue;
        for (u32 j = 0; j < holder.capacity; j++) {
            Item::Obj* obj = holder.itemObj[j];
            if (!obj || (obj->bitfield74 & 0x1)) continue;
            bool shouldDespawn = obj->duration > MAX_ITEM_LIFETIME;
            if (checkDistance && obj->duration >= 300) {
                bool farFromAll = true;
                for (int k = 0; k < playerCount && k < 12 && farFromAll; k++) {
                    Vec3 diff;
                    diff.x = obj->position.x - playerPositions[k].x;
                    diff.y = obj->position.y - playerPositions[k].y;
                    diff.z = obj->position.z - playerPositions[k].z;
                    if (diff.x * diff.x + diff.y * diff.y + diff.z * diff.z < 225000000.0f) {
                        farFromAll = false;
                    }
                }
                shouldDespawn |= farFromAll;
            }
            if (shouldDespawn) {
                obj->DisappearDueToExcess(true);
            }
        }
    }
}

void SpawnItemRain() {
    const RacedataScenario& scenario = Racedata::sInstance->racesScenario;
    const GameMode mode = scenario.settings.gamemode;
    if (!Pulsar::System::sInstance->IsContext(PULSAR_ITEMMODERAIN) && !Pulsar::System::sInstance->IsContext(PULSAR_ITEMMODESTORM)) return;
    if (Pulsar::System::sInstance->IsContext(PULSAR_MODE_OTT)) return;
    if (RKNet::Controller::sInstance->roomType != RKNet::ROOMTYPE_FROOM_HOST && RKNet::Controller::sInstance->roomType != RKNet::ROOMTYPE_FROOM_NONHOST &&
        RKNet::Controller::sInstance->roomType != RKNet::ROOMTYPE_NONE && RKNet::Controller::sInstance->roomType != RKNet::ROOMTYPE_VS_REGIONAL &&
        RKNet::Controller::sInstance->roomType != RKNet::ROOMTYPE_JOINING_REGIONAL) return;
    if (mode == MODE_TIME_TRIAL) return;
    if (!Racedata::sInstance || !Raceinfo::sInstance || !Item::Manager::sInstance) return;
    if (!Raceinfo::sInstance->IsAtLeastStage(RACESTAGE_RACE)) return;

    sRaceInfoFrameCounter++;

    if ((sRaceInfoFrameCounter % DESPAWN_CHECK_INTERVAL) == 0) {
        DespawnItems();
    }
    if ((sRaceInfoFrameCounter % (DESPAWN_CHECK_INTERVAL * 2)) == 0) {
        DespawnItems(true);
    }
    u8 playerCount = Pulsar::System::sInstance->nonTTGhostPlayersCount;
    if (playerCount == 0) return;
    if ((sRaceInfoFrameCounter % GetSpawnInterval(playerCount)) != 0) return;

    Vec3 dummyDirection;
    dummyDirection.x = 0.0f;
    dummyDirection.y = 0.0f;
    dummyDirection.z = 0.0f;

    Vec3 positions[12];
    for (int i = 0; i < playerCount && i < 12; i++) positions[i] = Item::Manager::sInstance->players[i].GetPosition();

    for (int i = 0; i < ITEMS_PER_SPAWN; i++) {
        for (int playerIdx = 0; playerIdx < playerCount; playerIdx++) {
            Item::Player& player = Item::Manager::sInstance->players[playerIdx];
            bool isLocal = false;
            if (player.kartPlayer && player.kartPlayer->IsLocal()) {
                isLocal = true;
            } else if (scenario.players[playerIdx].playerType == PLAYER_REAL_LOCAL) {
                isLocal = true;
            }
            if (!isLocal) continue;

            Vec3 playerPos = positions[playerIdx];
            Vec3 forwardDir;
            if (player.kartPlayer) {
                forwardDir = player.kartPlayer->GetMovement().dir;
            } else {
                forwardDir.x = 0.0f;
                forwardDir.y = 0.0f;
                forwardDir.z = 1.0f;
            }
            Vec3 rightDir;
            rightDir.x = forwardDir.z;
            rightDir.y = 0.0f;
            rightDir.z = -forwardDir.x;

            float forward = GetRandomRange(1000.0f, 12000.0f);
            float side = GetRandomRange(-SPAWN_RADIUS, SPAWN_RADIUS);

            Vec3 spawnPos;
            spawnPos.x = playerPos.x + forwardDir.x * forward + rightDir.x * side;
            spawnPos.y = playerPos.y + SPAWN_HEIGHT;
            spawnPos.z = playerPos.z + forwardDir.z * forward + rightDir.z * side;

            ItemObjId selectedItem = GetRandomItem();
            for (int attempt = 0; attempt < 3; attempt++) {
                if (attempt > 0) {
                    float newForward = GetRandomRange(1000.0f, 12000.0f);
                    float newSide = GetRandomRange(-SPAWN_RADIUS, SPAWN_RADIUS);
                    spawnPos.x = playerPos.x + forwardDir.x * newForward + rightDir.x * newSide;
                    spawnPos.z = playerPos.z + forwardDir.z * newForward + rightDir.z * newSide;
                }

                Item::Manager::sInstance->CreateItemDirect(selectedItem, &spawnPos, &dummyDirection, playerIdx);
            }
        }
    }
}
RaceFrameHook ItemRainHook(SpawnItemRain);

}  // namespace ItemRain
}  // namespace Pulsar
