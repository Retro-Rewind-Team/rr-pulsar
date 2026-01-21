#include <MarioKartWii/System/Identifiers.hpp>
#include <hooks.hpp>
#include <kamek.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/Item/ItemManager.hpp>
#include <MarioKartWii/Item/ItemBehaviour.hpp>
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

// Constants
static const float SPAWN_HEIGHT = 2500.0f;
static const float XZ_RANGE = 8000.0f;
static const float MIN_FORWARD_OFFSET = 3500.0f;
static const u32 BOBOMB_DURATION_EXTRA = 20;
static const float PLAYER_PROXIMITY_SQ = 7000.0f * 7000.0f;
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
    bool bobombSurvive;
    u8 syncFrame;
    ItemRainSyncData pendingSpawns;
    u8 lastReceivedSyncFrame[12];
};
static State sState;

extern "C" {
void SpawnItemInternal__Q24Item9ObjHolderFPQ24Item3Obj(Item::ObjHolder*, Item::Obj*);
void InitProperties__Q24Item3ObjFUiP4Vec3P4Vec3P4Vec3(Item::Obj*, u32, const Vec3*, const Vec3*, const Vec3*);
void LoadEntity__Q24Item3ObjFb(Item::Obj*, bool);
void AddHitFreeEVENTEntry__Q24Item3ObjF9ItemObjIdUiUcUs(ItemObjId, u32, u8, u16);
void Resize__Q24Item6EntityFff(void*, float, float);
float GetRadius__Q24Item3ObjFUi(Item::Obj*, u32);
}

static ItemObjId GetRandomItem(u32 rnd) {
    for (size_t i = 0; i < sizeof(ITEM_WEIGHTS) / sizeof(ItemWeight); ++i) {
        if (rnd < ITEM_WEIGHTS[i].threshold) return ITEM_WEIGHTS[i].id;
    }
    return OBJ_LIGHTNING;
}

static float RandomOffset(Random& rng, float range) {
    return ((rng.NextLimited(0x8000) / 32767.0f) - 0.5f) * 2.0f * range;
}

bool IsItemRainEnabled() {
    System* sys = System::sInstance;
    if (!sys->IsContext(PULSAR_ITEMMODERAIN) && !sys->IsContext(PULSAR_ITEMMODESTORM)) return false;
    if (sys->IsContext(PULSAR_MODE_OTT)) return false;

    RKNet::Controller* controller = RKNet::Controller::sInstance;
    if (controller->roomType == RKNet::ROOMTYPE_VS_REGIONAL ||
        controller->roomType == RKNet::ROOMTYPE_JOINING_REGIONAL ||
        controller->roomType == RKNet::ROOMTYPE_FROOM_HOST ||
        controller->roomType == RKNet::ROOMTYPE_FROOM_NONHOST ||
        controller->roomType == RKNet::ROOMTYPE_NONE) {
        GameMode mode = Racedata::sInstance->racesScenario.settings.gamemode;
        return mode == MODE_VS_RACE || mode == MODE_GRAND_PRIX ||
               mode == MODE_PUBLIC_VS || mode == MODE_PRIVATE_VS;
    }
    return false;
}

bool IsOnline() {
    RKNet::Controller* controller = RKNet::Controller::sInstance;
    return controller && controller->roomType != RKNet::ROOMTYPE_NONE;
}

bool IsHost() {
    RKNet::Controller* controller = RKNet::Controller::sInstance;
    if (!controller || controller->roomType == RKNet::ROOMTYPE_NONE) return true;
    if (controller->roomType == RKNet::ROOMTYPE_FROOM_HOST) return true;
    if (controller->roomType == RKNet::ROOMTYPE_VS_REGIONAL || controller->roomType == RKNet::ROOMTYPE_JOINING_REGIONAL) {
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

static void DoSpawnItem(ItemObjId itemId, s32 playerIdx, float fOff, float rOff, bool isStorm) {
    Kart::Manager* km = Kart::Manager::sInstance;
    Item::Manager* im = Item::Manager::sInstance;
    if (!km || !im || itemId >= 0xF) return;

    Kart::Player* player = km->players[playerIdx];
    if (!player) return;

    Item::ObjHolder* holder = &im->itemObjHolders[itemId];
    const Kart::PhysicsHolder* physics = player->pointers.kartBody->kartPhysicsHolder;
    const Vec3& pos = physics->position;
    const Mtx34& mtx = physics->transforMtx;

    Vec3 spawnPos(
        pos.x + fOff * mtx.mtx[0][2] + rOff * mtx.mtx[0][0],
        pos.y + SPAWN_HEIGHT,
        pos.z + fOff * mtx.mtx[2][2] + rOff * mtx.mtx[2][0]);

    Item::Obj* obj = nullptr;
    u8 ownerId = IsOnline() ? 0 : static_cast<u8>(playerIdx);
    holder->Spawn(1u, &obj, ownerId, spawnPos, false);
    if (!obj) return;

    if (!obj->entity) LoadEntity__Q24Item3ObjFb(obj, false);

    obj->bitfield78 &= ~0x20000;
    obj->playerUsedItemId = 0;
    obj->bitfield7c &= ~0x20;

    SpawnItemInternal__Q24Item9ObjHolderFPQ24Item3Obj(holder, obj);
    Vec3 dir(0.0f, 0.0f, -1.0f), zero(0.0f, 0.0f, 0.0f);
    InitProperties__Q24Item3ObjFUiP4Vec3P4Vec3P4Vec3(obj, 0, &dir, &zero, &zero);

    if (obj->itemObjId == OBJ_BOBOMB && !isStorm) obj->duration += BOBOMB_DURATION_EXTRA;
    if (isStorm) obj->duration = static_cast<u32>(obj->duration * 0.01f);

    if (obj->itemObjId == OBJ_BOBOMB) {
        Item::ObjBomb* bomb = static_cast<Item::ObjBomb*>(obj);
        bomb->timer = 90;
        // Use nextState (offset 0x1ac) to trigger a clean transition to Ticking state in the next Update()
        *reinterpret_cast<u32*>(reinterpret_cast<u8*>(obj) + 0x1ac) = Item::ObjBomb::STATE_TICKING;
    }

    if (Raceinfo::sInstance->timerMgr)
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
            u32 wait = isStorm ? 90 : (isolated ? 180 : 240);
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
    *dest = sState.pendingSpawns;
}

void UnpackAndSpawn(const ItemRainSyncData* src, u8 senderAid) {
    if (!IsItemRainEnabled() || IsHost() || src->itemCount == 0) return;
    if (src->syncFrame == sState.lastReceivedSyncFrame[senderAid]) return;
    sState.lastReceivedSyncFrame[senderAid] = src->syncFrame;

    bool isStorm = System::sInstance->IsContext(PULSAR_ITEMMODESTORM);
    for (u32 i = 0; i < src->itemCount && i < MAX_RAIN_ITEMS_PER_PACKET; i++) {
        const RainItemEntry& entry = src->items[i];
        if (entry.itemObjId >= 0xF) continue;
        float fOff = static_cast<float>(entry.forwardOffset) * OFFSET_SCALE;
        float rOff = static_cast<float>(entry.rightOffset) * OFFSET_SCALE;
        DoSpawnItem(static_cast<ItemObjId>(entry.itemObjId), entry.targetPlayer, fOff, rOff, isStorm);
    }
}

static void OnTimerUpdate(u32 oldFrame) {
    RaceTimerMgr* tm = Raceinfo::sInstance->timerMgr;
    tm->raceFrameCounter = oldFrame + 1;
    if (!IsItemRainEnabled()) return;

    Item::Manager* im = Item::Manager::sInstance;
    if (im) {
        u32 currentFrame = tm->raceFrameCounter;
        for (int i = 0; i < 15; i++) {
            Item::ObjHolder& holder = im->itemObjHolders[i];
            for (u32 j = 0; j < holder.capacity; j++) {
                Item::Obj* obj = holder.itemObj[j];
                if (obj && (obj->bitfield74 & 1) == 0) {
                    u32 spawnFrame = *reinterpret_cast<u32*>(reinterpret_cast<u8*>(obj) + 0x164);
                    if (spawnFrame != 0 && (currentFrame - spawnFrame) > 300) {
                        obj->KillFromOtherCollision(true);
                    }
                }
            }
        }
    }

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
        memset(sState.lastReceivedSyncFrame, 0xFF, sizeof(sState.lastReceivedSyncFrame));
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

    bool isStorm = System::sInstance->IsContext(PULSAR_ITEMMODESTORM);
    u32 itemsPerPlayer = isStorm ? ITEMS_PER_PLAYER_PER_FRAME * 3 : ITEMS_PER_PLAYER_PER_FRAME;
    u32 items = count * itemsPerPlayer;

    if (!IsOnline() || IsHost()) {
        sState.pendingSpawns.syncFrame = ++sState.syncFrame;
        u32 spawnCount = 0;

        for (u32 i = 0; i < items && spawnCount < MAX_RAIN_ITEMS_PER_PACKET; i++) {
            sState.playerIdx = (sState.playerIdx + 1) % count;
            RainItemEntry entry;
            if (TryGenerateItemSpawn(sState.playerIdx, tm, km, isStorm, &entry)) {
                if (IsOnline()) sState.pendingSpawns.items[spawnCount++] = entry;
                float fOff = static_cast<float>(entry.forwardOffset) * OFFSET_SCALE;
                float rOff = static_cast<float>(entry.rightOffset) * OFFSET_SCALE;
                DoSpawnItem(static_cast<ItemObjId>(entry.itemObjId), entry.targetPlayer, fOff, rOff, isStorm);
            }
        }
        sState.pendingSpawns.itemCount = static_cast<u8>(spawnCount);
    } else {
        RKNet::Controller* controller = RKNet::Controller::sInstance;
        if (controller) {
            const RKNet::ControllerSub& sub = controller->subs[controller->currentSub];
            u8 hostAid = sub.hostAid;

            if (hostAid < 12 && !((controller->disconnectedAids >> hostAid) & 1)) {
                u32 lastBufferUsed = controller->lastReceivedBufferUsed[hostAid][RKNet::PACKET_RACEHEADER1];
                if (lastBufferUsed < 2) {
                    RKNet::SplitRACEPointers* splitPacket = controller->splitReceivedRACEPackets[lastBufferUsed][hostAid];
                    if (splitPacket) {
                        RKNet::PacketHolder<Network::PulRH1>* holder = splitPacket->GetPacketHolder<Network::PulRH1>();
                        // Accept both base and full Pulsar packet sizes
                        if (holder && holder->packetSize >= Network::PulRH1SizeBase) {
                            const Network::PulRH1* packet = holder->packet;
                            if (packet && packet->itemRainItemCount > 0 && packet->itemRainItemCount <= MAX_RAIN_ITEMS_PER_PACKET) {
                                ItemRainSyncData syncData;
                                syncData.itemCount = packet->itemRainItemCount;
                                syncData.syncFrame = packet->itemRainSyncFrame;
                                memcpy(syncData.items, packet->itemRainItems, syncData.itemCount * sizeof(RainItemEntry));
                                UnpackAndSpawn(&syncData, hostAid);
                            }
                        }
                    }
                }
            }
        }
    }
}

static void ProcessOtherCollisionWrapper(Item::Obj* obj, u32 result, Vec3* otherPos, Vec3* otherSpeed) {
    if (!IsItemRainEnabled() || !obj) return;
    if (obj->itemObjId == OBJ_BOBOMB && result == 2) sState.bobombSurvive = true;
    obj->ProcessOtherCollision(result, *otherPos, *otherSpeed);
}

static void KILL_FROM_PLAYER_COLLISION_POINT();  // dummy for address
static void KillFromPlayerCollisionHook(Item::Obj* obj, bool sendBreak, u8 playerId) {
    if (IsItemRainEnabled() && IsOnline() && sendBreak) {
        AddHitFreeEVENTEntry__Q24Item3ObjF9ItemObjIdUiUcUs(obj->itemObjId, 2, playerId, *reinterpret_cast<u16*>(reinterpret_cast<u8*>(obj) + 0xC));
    }
    obj->KillFromPlayerCollision(sendBreak, playerId);
}

static void KillFromOtherCollisionHook(Item::Obj* obj, bool sendBreak) {
    if (IsItemRainEnabled()) {
        if (sState.bobombSurvive && obj->itemObjId == OBJ_BOBOMB) {
            sState.bobombSurvive = false;
            return;
        }
        if (obj->duration == 1) {
            obj->KillFromOtherCollision(true);
            return;
        }
        if (IsOnline()) {
            AddHitFreeEVENTEntry__Q24Item3ObjF9ItemObjIdUiUcUs(obj->itemObjId, 1, 0xC, *reinterpret_cast<u16*>(reinterpret_cast<u8*>(obj) + 0xC));
        }
    }
    obj->KillFromOtherCollision(sendBreak);
}

static void ObjSpawnHook(Item::Obj* obj, ItemObjId id, u8 playerId, const Vec3& pos, bool r7) {
    obj->Spawn(id, playerId, pos, r7);
    if (IsItemRainEnabled() && Raceinfo::sInstance->timerMgr)
        *reinterpret_cast<u32*>(reinterpret_cast<u8*>(obj) + 0x164) = Raceinfo::sInstance->timerMgr->raceFrameCounter;
}

kmRuntimeUse(0x808D1BDC);
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
            r0_val = *reinterpret_cast<void**>(kmRuntimeAddr(0x808D1BDC));
        } else {
            obj->KillFromOtherCollision(false);
            return;
        }
    }
    *reinterpret_cast<void**>(reinterpret_cast<u8*>(obj) + 0x170) = r0_val;
}

static void SafeBombExplosionResize(void* entity, float radius, float maxSpeed) {
    if (entity) Resize__Q24Item6EntityFff(entity, radius, maxSpeed);
}

static void SafeOffroadEntityWrapper(Item::Obj* obj, u32 param) {
    if (obj->entity) obj->entity->paramsBitfield &= ~0x100;
    float radius = GetRadius__Q24Item3ObjFUi(obj, param);
    if (obj->entity) {
        obj->entity->radius = radius;
        obj->entity->range = radius;
        obj->entity->paramsBitfield |= 0x800;
    }
    obj->bitfield78 &= ~0x10;
}

static int IsSpawnLimitNotReachedHook(ItemId id) {
    if (IsItemRainEnabled()) return 1;
    if (id >= 19) return 0;
    const Item::Behavior& behave = Item::Behavior::behaviourTable[id];
    Item::Manager* im = Item::Manager::sInstance;
    Item::ObjHolder& holder = im->itemObjHolders[behave.objId];
    int currentCount = holder.GetTotalItemCount();
    return behave.numberOfItems <= (holder.capacity2 - currentCount);
}

static void TotalItemCountHook() {
    register int total;
    register Item::Manager* manager;
    asm {
        mr total, r27
        mr manager, r30
    }
    int res = total - 30;
    if (IsItemRainEnabled()) res = -30;
    manager->totalItemCountMinus30 = res;
}

// Hooks
kmCall(0x80535C7C, OnTimerUpdate);
kmCall(0x807a0ffc, ProcessOtherCollisionWrapper);
kmCall(0x807a1010, ProcessOtherCollisionWrapper);
kmCall(0x807a1c68, KillFromOtherCollisionHook);
kmCall(0x80795f00, ObjSpawnHook);
kmCall(0x807a3838, KillFromPlayerCollisionHook);

kmCall(0x807A7170, BombExplosion);
kmCall(0x807a4714, SafeBombExplosionResize);
kmCall(0x807b6ebc, SafeOffroadEntityWrapper);
kmWrite32(0x807b6eb0, 0x60000000);  // NOPs for FIB offroad entity setup
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

kmBranch(0x80799be8, IsSpawnLimitNotReachedHook);
kmWrite32(0x8079992c, 0x60000000);
kmCall(0x80799930, TotalItemCountHook);

}  // namespace ItemRain
}  // namespace Pulsar