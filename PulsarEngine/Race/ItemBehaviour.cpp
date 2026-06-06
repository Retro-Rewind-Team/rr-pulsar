#include <kamek.hpp>
#include <MarioKartWii/Item/ItemBehaviour.hpp>
#include <MarioKartWii/Item/Obj/ObjProperties.hpp>
#include <RetroRewind.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <Race/CustomItems.hpp>
#include <Gamemodes/BattleRoyale/BattleRoyale.hpp>

// Original code from VP, adapted to Pulsar 2.0.
namespace RetroRewind {
namespace Race {

static u32 CountEnabledItems(u32 bitfield) {
    u32 count = 0;
    while (bitfield) {
        count += bitfield & 1;
        bitfield >>= 1;
    }
    return count;
}

static void ChangeBlueOBJProperties(Item::ObjProperties* dest, const Item::ObjProperties& rel) {
    bool itemModeRandom = false;
    bool itemModeBlast = false;
    if (RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_HOST || RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_NONHOST) {
        itemModeRandom = System::sInstance->IsContext(Pulsar::PULSAR_ITEMMODERANDOM);
        itemModeBlast = System::sInstance->IsContext(Pulsar::PULSAR_ITEMMODEBLAST);
    }
    new (dest) Item::ObjProperties(rel);
    if (itemModeBlast) {
        dest->limit = 25;
    } else if (itemModeRandom) {
        dest->limit = 5;
    } else {
        u32 bitfield = Pulsar::Race::GetEffectiveCustomItemsBitfield();
        if (Pulsar::BattleRoyale::ShouldApplyBattleRoyale() || (bitfield != 0x7FFFF && CountEnabledItems(bitfield) <= 5)) {
            dest->limit = 12;
        } else {
            dest->limit = 1;
        }
    }
}

kmCall(0x80790b74, ChangeBlueOBJProperties);

static void ChangeBillOBJProperties(Item::ObjProperties* dest, const Item::ObjProperties& rel) {
    bool itemModeRandom = false;
    bool itemModeBlast = false;
    if (RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_HOST || RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_NONHOST) {
        itemModeRandom = System::sInstance->IsContext(Pulsar::PULSAR_ITEMMODERANDOM);
        itemModeBlast = System::sInstance->IsContext(Pulsar::PULSAR_ITEMMODEBLAST);
    }
    new (dest) Item::ObjProperties(rel);
    if (itemModeRandom) {
        dest->limit = 25;
    } else if (itemModeBlast) {
        dest->limit = 5;
    } else if (Pulsar::BattleRoyale::ShouldApplyBattleRoyale()) {
        dest->limit = 16;
    } else {
        dest->limit = 1;
    }
}

kmCall(0x80790bf4, ChangeBillOBJProperties);

static void ChangeBombOBJProperties(Item::ObjProperties* dest, const Item::ObjProperties& rel) {
    bool itemModeRandom = false;
    bool itemModeBlast = false;
    if (RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_HOST || RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_NONHOST) {
        itemModeRandom = System::sInstance->IsContext(Pulsar::PULSAR_ITEMMODERANDOM);
        itemModeBlast = System::sInstance->IsContext(Pulsar::PULSAR_ITEMMODEBLAST);
    }
    new (dest) Item::ObjProperties(rel);
    if (itemModeRandom) {
        dest->limit = 20;
    } else if (itemModeBlast) {
        dest->limit = 25;
    } else if (Pulsar::BattleRoyale::ShouldApplyBattleRoyale()) {
        dest->limit = 16;
    } else {
        dest->limit = 3;
    }
}

kmCall(0x80790bb4, ChangeBombOBJProperties);

static void ChangeItemOBJProperties(Item::ObjProperties* dest, const Item::ObjProperties& rel) {
    new (dest) Item::ObjProperties(rel);
    if (Pulsar::Race::GetEffectiveCustomItemsBitfield() != 0x7FFFF || Pulsar::BattleRoyale::ShouldApplyBattleRoyale()) {
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
