#include <kamek.hpp>
#include <MarioKartWii/Item/ItemBehaviour.hpp>
#include <MarioKartWii/Item/Obj/ObjProperties.hpp>
#include <RetroRewind.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <Race/CustomItems.hpp>
#include <Gamemodes/BattleRoyale/BattleRoyale.hpp>
#include <GlobalFunction.hpp>

// Original code from VP, adapted to Pulsar 2.0.
namespace RetroRewind {
namespace Race {

static void ChangeBlueOBJProperties(Item::ObjProperties *dest, const Item::ObjProperties &rel) {
    bool itemModeRandom = false;
    if (Pulsar::IsFriendRoom()) {
        itemModeRandom = System::sInstance->IsContext(Pulsar::PULSAR_ITEMMODERANDOM);
    }
    new (dest) Item::ObjProperties(rel);
    if (itemModeRandom) {
        dest->limit = 5;
    } else {
        u32 bitfield = Pulsar::Race::GetEffectiveCustomItemsBitfield();
        if (Pulsar::BattleRoyale::ShouldApplyBattleRoyale()) {
            dest->limit = 12;
        } else {
            u32 enabledCount = 0;
            for (; bitfield != 0; bitfield >>= 1) enabledCount += bitfield & 1;
            dest->limit = enabledCount <= 5 ? 12 : 1;
        }
    }
}

kmCall(0x80790b74, ChangeBlueOBJProperties);

static void ChangeBillOBJProperties(Item::ObjProperties *dest, const Item::ObjProperties &rel) {
    bool itemModeRandom = false;
    if (Pulsar::IsFriendRoom()) {
        itemModeRandom = System::sInstance->IsContext(Pulsar::PULSAR_ITEMMODERANDOM);
    }
    new (dest) Item::ObjProperties(rel);
    if (itemModeRandom) {
        dest->limit = 25;
    } else if (Pulsar::BattleRoyale::ShouldApplyBattleRoyale()) {
        dest->limit = 16;
    } else {
        dest->limit = 1;
    }
}

kmCall(0x80790bf4, ChangeBillOBJProperties);

static void ChangeBombOBJProperties(Item::ObjProperties *dest, const Item::ObjProperties &rel) {
    bool itemModeRandom = false;
    if (Pulsar::IsFriendRoom()) {
        itemModeRandom = System::sInstance->IsContext(Pulsar::PULSAR_ITEMMODERANDOM);
    }
    new (dest) Item::ObjProperties(rel);
    if (itemModeRandom) {
        dest->limit = 20;
    } else if (Pulsar::BattleRoyale::ShouldApplyBattleRoyale()) {
        dest->limit = 16;
    } else {
        dest->limit = 3;
    }
}

kmCall(0x80790bb4, ChangeBombOBJProperties);

static void ChangeItemOBJProperties(Item::ObjProperties *dest, const Item::ObjProperties &rel) {
    new (dest) Item::ObjProperties(rel);
    if (Pulsar::Race::GetEffectiveCustomItemsBitfield() != Pulsar::Race::ALL_CUSTOM_ITEMS || Pulsar::BattleRoyale::ShouldApplyBattleRoyale()) {
        dest->limit = 16;
    } else {
        dest->limit = 1;
    }
}
kmCall(0x80790bc4, ChangeItemOBJProperties);  // Blooper
kmCall(0x80790bd4, ChangeItemOBJProperties);  // POW
kmCall(0x80790c04, ChangeItemOBJProperties);  // Thunder Cloud

}  // namespace Race
}  // namespace RetroRewind
