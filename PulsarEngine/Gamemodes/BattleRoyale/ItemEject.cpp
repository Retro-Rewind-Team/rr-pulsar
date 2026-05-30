#include <kamek.hpp>
#include <Gamemodes/BattleRoyale/BattleRoyale.hpp>
#include <MarioKartWii/Item/ItemManager.hpp>
#include <MarioKartWii/Item/ItemPlayer.hpp>
#include <MarioKartWii/Kart/KartDamage.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <MarioKartWii/System/Identifiers.hpp>
#include <core/nw4r/math/Triangular.hpp>
#include <PulsarSystem.hpp>
#include <runtimeWrite.hpp>

namespace Pulsar {
namespace BattleRoyale {

static const u32 itemWeightTotal = 4000;
static const u8 maxPlayers = 12;
static const float spawnRadius = 120.0f;
static const float ejectSpeed = 55.0f;
static const float upSpeed = 18.0f;
static u16 sLastEjectFrame[maxPlayers];

struct EjectItemWeight {
    u16 threshold;
    ItemObjId itemObjId;
};

static const EjectItemWeight ejectItemWeights[] = {
    {100, OBJ_LIGHTNING},
    {1300, OBJ_GOLDEN_MUSHROOM},
    {2500, OBJ_MEGA_MUSHROOM},
    {2700, OBJ_POW_BLOCK},
    {3900, OBJ_STAR},
    {4000, OBJ_BULLET_BILL},
};

typedef void (*DamageApplyFn)(Kart::Damage* damage, u32 playerObjIdx);
typedef void (*SpinDamageFn)(Kart::Damage* damage, u32 spinType, u32 playerObjIdx);

kmRuntimeUse(0x80567d3c);
kmRuntimeUse(0x80567f68);
kmRuntimeUse(0x80568000);
kmRuntimeUse(0x8056804c);
kmRuntimeUse(0x80568058);
kmRuntimeUse(0x80568064);
kmRuntimeUse(0x8056865c);
kmRuntimeUse(0x805690a0);

static ItemObjId GetRandomEjectItem(Random& random) {
    const u32 roll = random.NextLimited(itemWeightTotal);
    for (u32 i = 0; i < sizeof(ejectItemWeights) / sizeof(ejectItemWeights[0]); ++i) {
        if (roll < ejectItemWeights[i].threshold) return ejectItemWeights[i].itemObjId;
    }
    return OBJ_BULLET_BILL;
}

static bool ShouldUseItemDamageEject() {
    const System* system = System::sInstance;
    return system != nullptr && system->IsContext(PULSAR_ALLITEMSCANLAND) && ShouldApplyBattleRoyale();
}

static void SpawnEjectItem(Item::Manager& itemMgr, Item::Player& player, Random& random, u32 index, u32 count) {
    const float baseAngle = static_cast<float>(random.NextLimited(256));
    const float spacing = 256.0f / static_cast<float>(count);
    const float angle = baseAngle + spacing * static_cast<float>(index);

    float sin = 0.0f;
    float cos = 0.0f;
    nw4r::math::SinCosFIdx(&sin, &cos, angle);

    const Vec3& playerPos = player.GetPosition();
    Vec3 spawnPos(playerPos.x + sin * spawnRadius, playerPos.y + 40.0f, playerPos.z + cos * spawnRadius);
    Vec3 direction(sin * ejectSpeed, upSpeed, cos * ejectSpeed);
    itemMgr.CreateItemDirect(GetRandomEjectItem(random), &spawnPos, &direction, player.id);
}

static void EjectBattleRoyaleItems(Item::Player& player) {
    if (!ShouldUseItemDamageEject()) return;

    Item::Manager* itemMgr = Item::Manager::sInstance;
    Raceinfo* raceinfo = Raceinfo::sInstance;
    if (itemMgr == nullptr || raceinfo == nullptr || raceinfo->timerMgr == nullptr) return;
    if (player.id >= itemMgr->playerCount || player.isRemote) return;

    Random& random = raceinfo->timerMgr->random;
    const u32 count = static_cast<u32>(random.NextLimited(3) + 1);
    for (u32 i = 0; i < count; ++i) SpawnEjectItem(*itemMgr, player, random, i, count);
}

void EjectItemsFromItemDamage(u8 playerId) {
    Item::Manager* itemMgr = Item::Manager::sInstance;
    if (itemMgr == nullptr || playerId >= itemMgr->playerCount) return;

    const Raceinfo* raceinfo = Raceinfo::sInstance;
    const u16 raceFrames = raceinfo == nullptr ? 0xffff : raceinfo->raceFrames;
    if (sLastEjectFrame[playerId] == raceFrames) return;
    sLastEjectFrame[playerId] = raceFrames;

    EjectBattleRoyaleItems(itemMgr->players[playerId]);
}

static void FinishItemDamageEject(Kart::Damage* damage) {
    EjectItemsFromItemDamage(damage->GetPlayerIdx());
}

static void ApplyBananaDamageAndEject(Kart::Damage* damage, u32 playerObjIdx) {
    DamageApplyFn original = reinterpret_cast<DamageApplyFn>(kmRuntimeAddr(0x80567f68));
    original(damage, playerObjIdx);
    FinishItemDamageEject(damage);
}

static void ApplyBananaSpinDamageAndEject(Kart::Damage* damage, u32 spinType, u32 playerObjIdx) {
    SpinDamageFn original = reinterpret_cast<SpinDamageFn>(kmRuntimeAddr(0x80567d3c));
    original(damage, spinType, playerObjIdx);
    FinishItemDamageEject(damage);
}

static void ApplyFireDamageAndEject(Kart::Damage* damage, u32 playerObjIdx) {
    DamageApplyFn original = reinterpret_cast<DamageApplyFn>(kmRuntimeAddr(0x80568000));
    original(damage, playerObjIdx);
    FinishItemDamageEject(damage);
}

static void ApplyShockDamageAndEject(Kart::Damage* damage, u32 playerObjIdx) {
    DamageApplyFn original = reinterpret_cast<DamageApplyFn>(kmRuntimeAddr(0x8056804c));
    original(damage, playerObjIdx);
    FinishItemDamageEject(damage);
}

static void ApplyZapperDamageAndEject(Kart::Damage* damage, u32 playerObjIdx) {
    DamageApplyFn original = reinterpret_cast<DamageApplyFn>(kmRuntimeAddr(0x80568058));
    original(damage, playerObjIdx);
    FinishItemDamageEject(damage);
}

static void ApplyPOWDamageAndEject(Kart::Damage* damage, u32 playerObjIdx) {
    DamageApplyFn original = reinterpret_cast<DamageApplyFn>(kmRuntimeAddr(0x80568064));
    original(damage, playerObjIdx);
    FinishItemDamageEject(damage);
}

static void ApplyShellFIBDamageAndEject(Kart::Damage* damage, u32 playerObjIdx) {
    DamageApplyFn original = reinterpret_cast<DamageApplyFn>(kmRuntimeAddr(0x8056865c));
    original(damage, playerObjIdx);
    FinishItemDamageEject(damage);
}

static void ApplyLaunchDamageAndEject(Kart::Damage* damage, u32 playerObjIdx) {
    DamageApplyFn original = reinterpret_cast<DamageApplyFn>(kmRuntimeAddr(0x805690a0));
    original(damage, playerObjIdx);
    FinishItemDamageEject(damage);
}

kmWritePointer(0x808b4d48, ApplyBananaDamageAndEject);
kmWritePointer(0x808b4d60, ApplyShellFIBDamageAndEject);
kmWritePointer(0x808b4d9c, ApplyLaunchDamageAndEject);
kmWritePointer(0x808b4db4, ApplyFireDamageAndEject);
kmWritePointer(0x808b4dc0, ApplyShockDamageAndEject);
kmWritePointer(0x808b4dcc, ApplyPOWDamageAndEject);
kmWritePointer(0x808b4dfc, ApplyZapperDamageAndEject);
kmWritePointer(0x808b4e14, ApplyShockDamageAndEject);
kmCall(0x80567f84, ApplyBananaSpinDamageAndEject);

}  // namespace BattleRoyale
}  // namespace Pulsar
