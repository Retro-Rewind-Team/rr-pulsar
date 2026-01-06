#include <MarioKartWii/System/Identifiers.hpp>
#include <hooks.hpp>
#include <kamek.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/Item/ItemManager.hpp>
#include <MarioKartWii/Item/Obj/ItemObjHolder.hpp>
#include <MarioKartWii/Item/Obj/ObjProperties.hpp>
#include <MarioKartWii/Item/Obj/Bomb.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>
#include <MarioKartWii/Kart/KartManager.hpp>
#include <MarioKartWii/Kart/KartBody.hpp>
#include <MarioKartWii/Kart/KartPhysics.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <PulsarSystem.hpp>
#include <Network/PacketExpansion.hpp>
#include <Gamemodes/ItemRain/ItemRain.hpp>
#include <runtimeWrite.hpp>

namespace Pulsar {
namespace ItemRain {

static const float SPAWN_HEIGHT = 2500.0f;
static const float XZ_RANGE = 8000.0f;
static const float MIN_FORWARD_OFFSET = 3500.0f;
static const u32 BOBOMB_DURATION_EXTRA = 20;
static const float PLAYER_PROXIMITY_SQ = 10000.0f * 10000.0f;
static const u32 ITEMS_PER_PLAYER_PER_FRAME = 1;
static const u32 LIGHTNING_MIN_FRAME = 1800;
static const float OFFSET_SCALE = 10.0f;

struct ItemWeight {
    u32 threshold;
    ItemObjId id;
};
static const ItemWeight ITEM_WEIGHTS[] = {
    {0x199A, OBJ_MUSHROOM},
    {0x2B86, OBJ_BANANA},
    {0x3ADA, OBJ_MEGA_MUSHROOM},
    {0x47AF, OBJ_GREEN_SHELL},
    {0x5334, OBJ_STAR},
    {0x5D71, OBJ_FAKE_ITEM_BOX},
    {0x6667, OBJ_GOLDEN_MUSHROOM},
    {0x7005, OBJ_RED_SHELL},
    {0x747B, OBJ_BULLET_BILL},
    {0x7852, OBJ_BOBOMB},
    {0x7C29, OBJ_POW_BLOCK},
    {0x7EB8, OBJ_BLUE_SHELL},
    {0x8000, OBJ_LIGHTNING},
};

struct State {
    u32 lastFrame;
    u32 seed;
    s32 playerIdx;
    u32 trackPlayerCount;
    bool bobombSurvive;
    u8 syncFrame;
    ItemRainSyncData pendingSpawns;
    u8 lastReceivedSyncFrame[12];
};
static State sState;

static ItemObjId GetRandomItem(u32 rnd) {
    for (u32 i = 0; i < sizeof(ITEM_WEIGHTS) / sizeof(ITEM_WEIGHTS[0]); i++) {
        if (rnd < ITEM_WEIGHTS[i].threshold) return ITEM_WEIGHTS[i].id;
    }
    return OBJ_LIGHTNING;
}

static float RandomOffset(Random& rng, float range) {
    return ((rng.NextLimited(0x8000) / 32767.0f) - 0.5f) * 2.0f * range;
}

bool IsItemRainEnabled() {
    Pulsar::System* sys = Pulsar::System::sInstance;
    if (!sys->IsContext(PULSAR_ITEMMODERAIN) && !sys->IsContext(PULSAR_ITEMMODESTORM)) return false;
    if (sys->IsContext(PULSAR_MODE_OTT)) return false;

    RKNet::RoomType rt = RKNet::Controller::sInstance->roomType;
    if (rt != RKNet::ROOMTYPE_FROOM_HOST && rt != RKNet::ROOMTYPE_FROOM_NONHOST &&
        rt != RKNet::ROOMTYPE_NONE && rt != RKNet::ROOMTYPE_VS_REGIONAL &&
        rt != RKNet::ROOMTYPE_JOINING_REGIONAL) return false;

    GameMode mode = Racedata::sInstance->racesScenario.settings.gamemode;
    return mode == MODE_VS_RACE || mode == MODE_GRAND_PRIX ||
           mode == MODE_PUBLIC_VS || mode == MODE_PRIVATE_VS;
}

bool IsOnline() {
    RKNet::Controller* controller = RKNet::Controller::sInstance;
    if (!controller) return false;
    RKNet::RoomType rt = controller->roomType;
    return rt == RKNet::ROOMTYPE_FROOM_HOST || rt == RKNet::ROOMTYPE_FROOM_NONHOST ||
           rt == RKNet::ROOMTYPE_VS_REGIONAL || rt == RKNet::ROOMTYPE_JOINING_REGIONAL;
}

bool IsHost() {
    RKNet::Controller* controller = RKNet::Controller::sInstance;
    if (!controller) return true;
    RKNet::RoomType rt = controller->roomType;
    if (rt == RKNet::ROOMTYPE_NONE) return true;
    if (rt == RKNet::ROOMTYPE_FROOM_HOST) return true;
    if (rt == RKNet::ROOMTYPE_VS_REGIONAL || rt == RKNet::ROOMTYPE_JOINING_REGIONAL) {
        const RKNet::ControllerSub& sub = controller->subs[controller->currentSub];
        return sub.localAid == sub.hostAid;
    }
    return false;
}

static bool HasLocalLead(s32 idx, const Vec3& pos, u8 rank, Kart::Manager* km, bool& isolated) {
    isolated = true;
    for (s32 j = 0; j < km->playerCount; ++j) {
        if (j == idx) continue;
        Kart::Player* other = km->players[j];
        if (!other) continue;
        const Vec3& op = other->pointers.kartBody->kartPhysicsHolder->position;
        float dx = pos.x - op.x, dy = pos.y - op.y, dz = pos.z - op.z;
        if (dx * dx + dy * dy + dz * dz < PLAYER_PROXIMITY_SQ) {
            isolated = false;
            if (Raceinfo::sInstance->players[j]->position < rank) return false;
        }
    }
    return true;
}

kmRuntimeUse(0x807961f0);
kmRuntimeUse(0x807a6f74);
kmRuntimeUse(0x8079ee30);
kmRuntimeUse(0x8079c3c4);

static void DoSpawnItem(ItemObjId itemId, s32 playerIdx, float fOff, float rOff, bool isStorm) {
    Kart::Manager* km = Kart::Manager::sInstance;
    if (!km) return;
    Kart::Player* player = km->players[playerIdx];
    if (!player) return;

    Item::Manager* im = Item::Manager::sInstance;
    if (!im) return;
    if (itemId >= 0xF) return;

    Item::ObjHolder* holder = &im->itemObjHolders[itemId];
    if (!holder) return;

    const Vec3& pos = player->pointers.kartBody->kartPhysicsHolder->position;
    const Mtx34& mtx = player->pointers.kartBody->kartPhysicsHolder->transforMtx;

    Vec3 spawnPos;
    spawnPos.x = pos.x + fOff * mtx.mtx[0][2] + rOff * mtx.mtx[0][0];
    spawnPos.y = pos.y + SPAWN_HEIGHT;
    spawnPos.z = pos.z + fOff * mtx.mtx[2][2] + rOff * mtx.mtx[2][0];

    Item::Obj* obj = nullptr;
    u8 ownerId = IsOnline() ? 0 : static_cast<u8>(playerIdx);  // Use host (player 0) as owner when online
    holder->Spawn(1u, &obj, ownerId, spawnPos, false);
    if (!obj) return;

    if (!obj->entity) reinterpret_cast<void (*)(Item::Obj*, bool)>(kmRuntimeAddr(0x8079ee30))(obj, false);

    obj->bitfield78 &= ~0x20000;

    obj->playerUsedItemId = 0;  // Set owner to host (player 0) for online Item Rain
    obj->bitfield7c &= ~0x20;

    reinterpret_cast<void (*)(Item::ObjHolder*, Item::Obj*)>(kmRuntimeAddr(0x807961f0))(holder, obj);
    Vec3 dir(0.0f, 0.0f, -1.0f), zero(0.0f, 0.0f, 0.0f);
    reinterpret_cast<void (*)(Item::Obj*, u32, Vec3*, Vec3*, Vec3*)>(kmRuntimeAddr(0x807a6f74))(obj, 0, &dir, &zero, &zero);

    if (obj->itemObjId == OBJ_BOBOMB && !isStorm) obj->duration += BOBOMB_DURATION_EXTRA;
    if (isStorm) obj->duration = static_cast<u32>(obj->duration * 0.01f);

    if (obj->itemObjId == OBJ_BOBOMB) {
        Item::ObjBomb* bomb = reinterpret_cast<Item::ObjBomb*>(obj);
        bomb->timer = 90;
        *reinterpret_cast<u16*>(reinterpret_cast<u8*>(obj) + 0x1A8) = Item::ObjBomb::STATE_TICKING;
    }

    if (Raceinfo::sInstance && Raceinfo::sInstance->timerMgr)
        *reinterpret_cast<u32*>(reinterpret_cast<u8*>(obj) + 0x164) = Raceinfo::sInstance->timerMgr->raceFrameCounter;
}

static bool TryGenerateItemSpawn(s32 idx, RaceTimerMgr* tm, Kart::Manager* km, bool isStorm, RainItemEntry* outEntry) {
    Kart::Player* player = km->players[idx];
    if (!player) return false;

    const Vec3& pos = player->pointers.kartBody->kartPhysicsHolder->position;
    u8 rank = Raceinfo::sInstance->players[idx]->position;
    bool isolated;
    if (!HasLocalLead(idx, pos, rank, km, isolated)) return false;

    u32 frame = tm->raceFrameCounter;
    Item::Manager* im = Item::Manager::sInstance;
    Item::ObjHolder* holder = nullptr;
    ItemObjId itemId;
    bool found = false;

    for (int r = 0; r < 5; r++) {
        itemId = GetRandomItem(tm->random.NextLimited(0x8000));
        if (itemId == OBJ_LIGHTNING && frame < LIGHTNING_MIN_FRAME) continue;
        holder = &im->itemObjHolders[itemId];
        if (holder->bodyCount < holder->capacity) {
            found = true;
            break;
        }
        if (holder->spawnedCount > 0) {
            u32 spawnFrame = *reinterpret_cast<u32*>(reinterpret_cast<u8*>(holder->itemObj[0]) + 0x164);
            u32 wait = isStorm ? 90 : (isolated ? 120 : 180);
            if (frame - spawnFrame >= wait) {
                found = true;
                break;
            }
        }
    }
    if (!found || !holder) return false;

    float fOff = MIN_FORWARD_OFFSET + (tm->random.NextLimited(0x8000) / 32767.0f) * XZ_RANGE;
    float rOff = RandomOffset(tm->random, XZ_RANGE);

    outEntry->itemObjId = static_cast<u8>(itemId);
    outEntry->targetPlayer = static_cast<u8>(idx);
    outEntry->forwardOffset = static_cast<s16>(fOff / OFFSET_SCALE);
    outEntry->rightOffset = static_cast<s16>(rOff / OFFSET_SCALE);

    return true;
}

void PackItemData(ItemRainSyncData* dest) {
    if (!IsItemRainEnabled() || !IsHost()) {
        dest->itemCount = 0;
        dest->syncFrame = 0;
        return;
    }
    dest->itemCount = sState.pendingSpawns.itemCount;
    dest->syncFrame = sState.pendingSpawns.syncFrame;
    for (u32 i = 0; i < sState.pendingSpawns.itemCount && i < MAX_RAIN_ITEMS_PER_PACKET; i++) {
        dest->items[i] = sState.pendingSpawns.items[i];
    }
}

void UnpackAndSpawn(const ItemRainSyncData* src, u8 senderAid) {
    if (!IsItemRainEnabled()) return;
    if (IsHost()) return;
    if (src->itemCount == 0) return;

    if (src->syncFrame == sState.lastReceivedSyncFrame[senderAid]) return;
    sState.lastReceivedSyncFrame[senderAid] = src->syncFrame;

    bool isStorm = Pulsar::System::sInstance->IsContext(PULSAR_ITEMMODESTORM);

    for (u32 i = 0; i < src->itemCount && i < MAX_RAIN_ITEMS_PER_PACKET; i++) {
        const RainItemEntry& entry = src->items[i];
        ItemObjId itemId = static_cast<ItemObjId>(entry.itemObjId);
        if (itemId >= 0xF) continue;  // Skip invalid item IDs
        float fOff = static_cast<float>(entry.forwardOffset) * OFFSET_SCALE;
        float rOff = static_cast<float>(entry.rightOffset) * OFFSET_SCALE;
        DoSpawnItem(itemId, entry.targetPlayer, fOff, rOff, isStorm);
    }
}
kmWrite32(0x8065F630, 0x60000000);
static void OnTimerUpdate(u32 oldFrame) {
    RaceTimerMgr* tm = Raceinfo::sInstance->timerMgr;
    tm->raceFrameCounter = oldFrame + 1;
    if (!(tm->hasRaceStarted & 1)) return;

    SectionMgr* sm = SectionMgr::sInstance;
    if (!sm || !sm->curSection) return;
    Page* page = sm->curSection->GetTopLayerPage();
    if (!page) return;

    PageId pid = page->pageId;
    if (pid == PAGE_GP_CLASS_SELECT || pid == PAGE_CHARACTER_SELECT ||
        pid == PAGE_CUP_SELECT || pid == PAGE_COURSE_SELECT) return;

    u32 frame = tm->raceFrameCounter;
    if (frame == sState.lastFrame) return;
    sState.lastFrame = frame;

    if (frame % 6 != 0) return;

    sState.pendingSpawns.itemCount = 0;

    if (frame == 0x2) {
        sState.playerIdx = -1;
        sState.syncFrame = 0;
        for (int i = 0; i < 12; i++) sState.lastReceivedSyncFrame[i] = 0xFF;
        if (pid != PAGE_TT_LEADERBOARDS)
            sState.seed = tm->random.Reset();
        else
            tm->random.seed = sState.seed;
    }

    Kart::Manager* km = Kart::Manager::sInstance;
    if (!km) return;
    s32 count = km->playerCount;

    if (count >= 13) {
        Item::ObjProperties::objProperties[OBJ_GREEN_SHELL].canFallOnTheGround = 2;
        Item::ObjProperties::objProperties[OBJ_RED_SHELL].canFallOnTheGround = 2;
    }

    bool isStorm = Pulsar::System::sInstance->IsContext(PULSAR_ITEMMODESTORM);
    u32 itemsPerPlayer = isStorm ? ITEMS_PER_PLAYER_PER_FRAME * 3 : ITEMS_PER_PLAYER_PER_FRAME;
    u32 items = count * itemsPerPlayer;

    bool isOnline = IsOnline();
    bool isHost = IsHost();

    if (!isOnline || isHost) {
        sState.pendingSpawns.syncFrame = ++sState.syncFrame;
        u32 spawnCount = 0;

        for (u32 i = 0; i < items && spawnCount < MAX_RAIN_ITEMS_PER_PACKET; i++) {
            sState.playerIdx = (sState.playerIdx + 1) % count;
            if (pid >= PAGE_ERASE_LICENSE && pid <= PAGE_BATTLE_SETTINGS) {
                if (sState.playerIdx != static_cast<s32>(sState.trackPlayerCount / 0x248)) continue;
            }

            RainItemEntry entry;
            if (TryGenerateItemSpawn(sState.playerIdx, tm, km, isStorm, &entry)) {
                if (isOnline) {
                    sState.pendingSpawns.items[spawnCount] = entry;
                    spawnCount++;
                }
                float fOff = static_cast<float>(entry.forwardOffset) * OFFSET_SCALE;
                float rOff = static_cast<float>(entry.rightOffset) * OFFSET_SCALE;
                DoSpawnItem(static_cast<ItemObjId>(entry.itemObjId), entry.targetPlayer, fOff, rOff, isStorm);
            }
        }
        sState.pendingSpawns.itemCount = static_cast<u8>(spawnCount);
    } else {
        // Non-host online: read ItemRain data from host's RH1 packet buffer
        RKNet::Controller* controller = RKNet::Controller::sInstance;
        if (controller) {
            const RKNet::ControllerSub& sub = controller->subs[controller->currentSub];
            u8 hostAid = sub.hostAid;

            // Validate host is still connected before accessing network data
            if (hostAid >= 12) return;  // Invalid hostAid
            if ((controller->disconnectedAids >> hostAid) & 1) return;  // Host disconnected
            if (!((sub.availableAids >> hostAid) & 1)) return;  // Host not available

            // Get the last received RH1 packet buffer from the host
            u32 lastBufferUsed = controller->lastReceivedBufferUsed[hostAid][RKNet::PACKET_RACEHEADER1];
            if (lastBufferUsed >= 2) return;  // Invalid buffer index

            RKNet::SplitRACEPointers* splitPacket = controller->splitReceivedRACEPackets[lastBufferUsed][hostAid];
            if (!splitPacket) return;

            RKNet::PacketHolder<Network::PulRH1>* holder = splitPacket->GetPacketHolder<Network::PulRH1>();
            if (!holder || holder->packetSize != sizeof(Network::PulRH1)) return;

            const Network::PulRH1* packet = holder->packet;
            if (!packet) return;

            u8 packetItemCount = packet->itemRainItemCount;
            if (packetItemCount > 0 && packetItemCount <= MAX_RAIN_ITEMS_PER_PACKET) {
                ItemRainSyncData syncData;
                syncData.itemCount = packetItemCount;
                syncData.syncFrame = packet->itemRainSyncFrame;
                for (u32 i = 0; i < syncData.itemCount; i++) {
                    const u8* src = &packet->itemRainItems[i * 6];
                    syncData.items[i].itemObjId = src[0];
                    syncData.items[i].targetPlayer = src[1];
                    syncData.items[i].forwardOffset = static_cast<s16>((src[2] << 8) | src[3]);
                    syncData.items[i].rightOffset = static_cast<s16>((src[4] << 8) | src[5]);
                }
                UnpackAndSpawn(&syncData, hostAid);
            }
        }
    }
}

static void ProcessOtherCollisionWrapper(Item::Obj* obj, u32 result, Vec3* otherPos, Vec3* otherSpeed) {
    if (!obj) return;
    if (obj->itemObjId == OBJ_BOBOMB && result == 2) sState.bobombSurvive = true;
    obj->ProcessOtherCollision(result, *otherPos, *otherSpeed);
}

static void SendBreakEvent(Item::Obj* obj, u8 playerId = 0xC, u32 breakType = 2) {
    u16 eventBitfield = *reinterpret_cast<u16*>(reinterpret_cast<u8*>(obj) + 0xC);
    reinterpret_cast<void (*)(ItemObjId, u32, u32, u16)>(kmRuntimeAddr(0x8079c3c4))(obj->itemObjId, breakType, playerId, eventBitfield);
}

static void KillFromPlayerCollisionHook(Item::Obj* obj, bool sendBreak, u8 playerId) {
    if (IsItemRainEnabled() && IsOnline() && sendBreak) {
        SendBreakEvent(obj, playerId, 2);
    }
    obj->KillFromPlayerCollision(sendBreak, playerId);
}

static void KillFromOtherCollisionHook(Item::Obj* obj, bool sendBreak) {
    if (sState.bobombSurvive && obj->itemObjId == OBJ_BOBOMB) {
        sState.bobombSurvive = false;
        return;
    }
    if (obj->duration == 1) {
        obj->KillFromPlayerCollision(true, 12);
        return;
    }
    if (IsItemRainEnabled() && IsOnline()) {
        SendBreakEvent(obj);
    }
    obj->KillFromOtherCollision(sendBreak);
}

static void ObjSpawnHook(Item::Obj* obj, ItemObjId id, u8 playerId, const Vec3& pos, bool r7) {
    obj->Spawn(id, playerId, pos, r7);
    if (Raceinfo::sInstance && Raceinfo::sInstance->timerMgr)
        *reinterpret_cast<u32*>(reinterpret_cast<u8*>(obj) + 0x164) = Raceinfo::sInstance->timerMgr->raceFrameCounter;
}

static void StoreTrackPlayerCount(u32 val) { sState.trackPlayerCount = val; }
kmCall(0x807EF0EC, StoreTrackPlayerCount);

static void BombExplosion() {
    register Item::Obj* obj;
    register void* r0_val;
    asm {
        mr obj, r30
        mr r0_val, r0
    }

    if (obj->itemObjId == OBJ_BOBOMB) {
        if (obj->entity) {
            Item::ObjBomb* bomb = reinterpret_cast<Item::ObjBomb*>(obj);
            bomb->timer = 300;
            r0_val = *reinterpret_cast<void**>(0x808D1BDC);
        } else {
            obj->KillFromOtherCollision(false);
            return;
        }
    }
    *reinterpret_cast<void**>(reinterpret_cast<u8*>(obj) + 0x170) = r0_val;
}
kmCall(0x807A7170, BombExplosion);

kmRuntimeUse(0x80786f7c);
static void SafeBombExplosionResize(Entity* entity, float radius, float maxSpeed) {
    if (entity) reinterpret_cast<void (*)(Entity*, float, float)>(kmRuntimeAddr(0x80786f7c))(entity, radius, maxSpeed);
}
kmCall(0x807a4714, SafeBombExplosionResize);

kmRuntimeUse(0x8079ec44);
static void SafeOffroadEntityWrapper(Item::Obj* obj, u32 param) {
    Entity* entity = obj->entity;
    if (entity) {
        entity->paramsBitfield &= ~0x100;
    }
    float radius = reinterpret_cast<float (*)(Item::Obj*, u32)>(kmRuntimeAddr(0x8079ec44))(obj, param);
    entity = obj->entity;
    if (entity) {
        entity->radius = radius;
        entity->range = radius;
        entity->paramsBitfield |= 0x800;
    }
    obj->bitfield78 &= ~0x10;
}
kmCall(0x807b6ebc, SafeOffroadEntityWrapper);
kmWrite32(0x807b6eb0, 0x60000000);
kmWrite32(0x807b6eb4, 0x60000000);
kmWrite32(0x807b6eb8, 0x60000000);
kmWrite32(0x807b6ec0, 0x60000000);
kmWrite32(0x807b6ec4, 0x60000000);
kmWrite32(0x807b6ec8, 0x60000000);
kmWrite32(0x807b6ecc, 0x60000000);
kmWrite32(0x807b6ed0, 0x60000000);
kmWrite32(0x807b6ed4, 0x60000000);
kmWrite32(0x807b6ed8, 0x60000000);
kmWrite32(0x807b6edc, 0x60000000);
kmWrite32(0x807b6ee0, 0x60000000);
kmWrite32(0x807b6ee4, 0x60000000);
kmWrite32(0x807b6ee8, 0x60000000);

kmRuntimeUse(0x80535C7C);
kmRuntimeUse(0x807a0ffc);
kmRuntimeUse(0x807a1010);
kmRuntimeUse(0x807a1c68);
kmRuntimeUse(0x80795f00);
kmRuntimeUse(0x807a3838);

static void HookItemRain() {
    kmRuntimeWrite32A(0x80535C7C, 0x901D0048);
    kmRuntimeWrite32A(0x807a0ffc, 0x48000901);
    kmRuntimeWrite32A(0x807a1010, 0x480008ED);
    kmRuntimeWrite32A(0x807a1c68, 0x480048F9);
    kmRuntimeWrite32A(0x80795f00, 0x48008651);
    kmRuntimeWrite32A(0x807a3838, 0x48002DDC);

    if (!IsItemRainEnabled()) return;

    kmRuntimeCallA(0x80535C7C, OnTimerUpdate);
    kmRuntimeCallA(0x807a0ffc, ProcessOtherCollisionWrapper);
    kmRuntimeCallA(0x807a1010, ProcessOtherCollisionWrapper);
    kmRuntimeCallA(0x807a1c68, KillFromOtherCollisionHook);
    kmRuntimeCallA(0x80795f00, ObjSpawnHook);
    kmRuntimeCallA(0x807a3838, KillFromPlayerCollisionHook);
}
static SectionLoadHook itemRainHook(HookItemRain);

}  // namespace ItemRain
}  // namespace Pulsar