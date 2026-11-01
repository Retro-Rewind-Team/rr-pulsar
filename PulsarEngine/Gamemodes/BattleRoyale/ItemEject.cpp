#include <kamek.hpp>
#include <Gamemodes/BattleRoyale/BattleRoyale.hpp>
#include <MarioKartWii/Item/ItemManager.hpp>
#include <MarioKartWii/Item/ItemPlayer.hpp>
#include <MarioKartWii/Kart/KartDamage.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <MarioKartWii/System/Identifiers.hpp>
#include <core/nw4r/math/Triangular.hpp>
#include <PulsarSystem.hpp>

namespace Pulsar {
namespace BattleRoyale {

static const u32 itemWeightTotal = 4000;
static const u8 maxPlayers = 12;
static const float spawnRadius = 120.0f;
static const float ejectSpeed = 55.0f;
static const float upSpeed = 18.0f;
static u16 sLastEjectFrame[maxPlayers];

void EjectItemsFromItemDamage(u8 playerId) {
    Item::Manager *itemMgr = Item::Manager::sInstance;
    if (itemMgr == nullptr || playerId >= itemMgr->playerCount)
        return;

    const Raceinfo *raceinfo = Raceinfo::sInstance;
    const u16 raceFrames = raceinfo == nullptr ? 0xffff : raceinfo->raceFrames;
    if (sLastEjectFrame[playerId] == raceFrames)
        return;
    sLastEjectFrame[playerId] = raceFrames;

    const System *system = System::sInstance;
    if (system == nullptr || !system->IsContext(PULSAR_ALLITEMSCANLAND) || !ShouldApplyBattleRoyale())
        return;

    Item::Player &player = itemMgr->players[playerId];
    Raceinfo *activeRaceinfo = Raceinfo::sInstance;
    if (activeRaceinfo == nullptr || activeRaceinfo->timerMgr == nullptr)
        return;
    if (player.id >= itemMgr->playerCount || player.isRemote)
        return;

    Random &random = activeRaceinfo->timerMgr->random;
    const u32 count = static_cast<u32>(random.NextLimited(3) + 1);
    for (u32 i = 0; i < count; ++i) {
        const float baseAngle = static_cast<float>(random.NextLimited(256));
        const float spacing = 256.0f / static_cast<float>(count);
        const float angle = baseAngle + spacing * static_cast<float>(i);

        float sin = 0.0f;
        float cos = 0.0f;
        nw4r::math::SinCosFIdx(&sin, &cos, angle);

        const Vec3 &playerPos = player.GetPosition();
        Vec3 spawnPos(playerPos.x + sin * spawnRadius, playerPos.y + 40.0f, playerPos.z + cos * spawnRadius);
        Vec3 direction(sin * ejectSpeed, upSpeed, cos * ejectSpeed);
        const u32 roll = random.NextLimited(itemWeightTotal);
        ItemObjId item = OBJ_BULLET_BILL;
        if (roll < 100)
            item = OBJ_LIGHTNING;
        else if (roll < 1300)
            item = OBJ_GOLDEN_MUSHROOM;
        else if (roll < 2500)
            item = OBJ_MEGA_MUSHROOM;
        else if (roll < 2700)
            item = OBJ_POW_BLOCK;
        else if (roll < 3900)
            item = OBJ_STAR;
        itemMgr->CreateItemDirect(item, &spawnPos, &direction, player.id);
    }
}

static void ApplyBananaDamageAndEject(Kart::Damage *damage, u32 playerObjIdx) {
    damage->ApplyBananaDamage(playerObjIdx);
    EjectItemsFromItemDamage(damage->GetPlayerIdx());
}
kmWritePointer(0x808b4d48, ApplyBananaDamageAndEject);

static void ApplyBananaSpinDamageAndEject(Kart::Damage *damage, u32 spinType, u32 playerObjIdx) {
    damage->ApplySpinDamage(spinType, playerObjIdx);
    EjectItemsFromItemDamage(damage->GetPlayerIdx());
}
kmCall(0x80567f84, ApplyBananaSpinDamageAndEject);

static void ApplyFireDamageAndEject(Kart::Damage *damage, u32 playerObjIdx) {
    damage->ApplyFireDamage(playerObjIdx);
    EjectItemsFromItemDamage(damage->GetPlayerIdx());
}
kmWritePointer(0x808b4db4, ApplyFireDamageAndEject);

static void ApplyShockDamageAndEject(Kart::Damage *damage, u32 playerObjIdx) {
    damage->ApplyShockDamage(playerObjIdx);
    EjectItemsFromItemDamage(damage->GetPlayerIdx());
}
kmWritePointer(0x808b4dc0, ApplyShockDamageAndEject);
kmWritePointer(0x808b4e14, ApplyShockDamageAndEject);

static void ApplyZapperDamageAndEject(Kart::Damage *damage, u32 playerObjIdx) {
    damage->ApplyZapperDamage(playerObjIdx);
    EjectItemsFromItemDamage(damage->GetPlayerIdx());
}
kmWritePointer(0x808b4dfc, ApplyZapperDamageAndEject);

static void ApplyPOWDamageAndEject(Kart::Damage *damage, u32 playerObjIdx) {
    damage->ApplyPOWDamage(playerObjIdx);
    EjectItemsFromItemDamage(damage->GetPlayerIdx());
}
kmWritePointer(0x808b4dcc, ApplyPOWDamageAndEject);

static void ApplyShellFIBDamageAndEject(Kart::Damage *damage, u32 playerObjIdx) {
    damage->ApplyShellFIBDamage(playerObjIdx);
    EjectItemsFromItemDamage(damage->GetPlayerIdx());
}
kmWritePointer(0x808b4d60, ApplyShellFIBDamageAndEject);

static void ApplyLaunchDamageAndEject(Kart::Damage *damage, u32 playerObjIdx) {
    damage->ApplyLaunchDamage(playerObjIdx);
    EjectItemsFromItemDamage(damage->GetPlayerIdx());
}
kmWritePointer(0x808b4d9c, ApplyLaunchDamageAndEject);

}  // namespace BattleRoyale
}  // namespace Pulsar
