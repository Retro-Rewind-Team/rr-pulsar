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
#include <Gamemodes/ItemRain/ItemRain.hpp>
#include <runtimeWrite.hpp>

namespace Pulsar {
namespace ItemRain {

static const float SPAWN_HEIGHT = 2500.0f;
static const float XZ_RANGE = 8000.0f;
static const float MIN_FORWARD_OFFSET = 4500.0f;
static const u32 BOBOMB_DURATION_EXTRA = 20;
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
    bool bobombSurvive;
};
static State sState;

extern "C" {
void SpawnItemInternal__Q24Item9ObjHolderFPQ24Item3Obj(Item::ObjHolder*, Item::Obj*);
void InitProperties__Q24Item3ObjFUiP4Vec3P4Vec3P4Vec3(Item::Obj*, u32, const Vec3*, const Vec3*, const Vec3*);
void LoadEntity__Q24Item3ObjFb(Item::Obj*, bool);
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

static bool IsLocalPlayer(s32 idx) {
    Kart::Manager* km = Kart::Manager::sInstance;
    if (!km || idx < 0 || idx >= km->playerCount) return false;

    Kart::Player* player = km->players[idx];
    if (!player) return false;
    return player->IsLocal();
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
    holder->Spawn(1u, &obj, static_cast<u8>(playerIdx), spawnPos, false);
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
        *reinterpret_cast<u32*>(reinterpret_cast<u8*>(obj) + 0x1ac) = Item::ObjBomb::STATE_TICKING;
    }

    if (Raceinfo::sInstance->timerMgr)
        *reinterpret_cast<u32*>(reinterpret_cast<u8*>(obj) + 0x164) = Raceinfo::sInstance->timerMgr->raceFrameCounter;
}

static bool TryGenerateItemSpawn(s32 idx, RaceTimerMgr* tm, bool isStorm, float* outFOff, float* outROff, ItemObjId* outItemId) {
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
            u32 wait = isStorm ? 90 : 180;
            if (frame - spawnFrame >= wait) {
                found = true;
                break;
            }
        }
    }
    if (!found || !holder) return false;

    *outFOff = MIN_FORWARD_OFFSET + (tm->random.NextLimited(0x8000) / 32767.0f) * XZ_RANGE;
    *outROff = RandomOffset(tm->random, XZ_RANGE);
    *outItemId = itemId;

    return true;
}

static void OnTimerUpdate(u32 oldFrame) {
    if (!Raceinfo::sInstance) return;
    RaceTimerMgr* tm = Raceinfo::sInstance->timerMgr;
    if (!tm) return;
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
                        obj->DisappearDueToExcess(false);
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

    if (frame == 0x2) {
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
    u32 spawnsPerPlayer = isStorm ? 4 : 2;

    for (s32 idx = 0; idx < count; idx++) {
        if (!IsLocalPlayer(idx)) continue;

        for (u32 s = 0; s < spawnsPerPlayer; s++) {
            float fOff, rOff;
            ItemObjId itemId;
            if (TryGenerateItemSpawn(idx, tm, isStorm, &fOff, &rOff, &itemId)) {
                DoSpawnItem(itemId, idx, fOff, rOff, isStorm);
            }
        }
    }
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

kmCall(0x80535C7C, OnTimerUpdate);
kmCall(0x807A7170, BombExplosion);
kmCall(0x807a4714, SafeBombExplosionResize);

}  // namespace ItemRain
}  // namespace Pulsar