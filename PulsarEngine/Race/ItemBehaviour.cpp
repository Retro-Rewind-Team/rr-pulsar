#include <kamek.hpp>
#include <MarioKartWii/Item/ItemBehaviour.hpp>
#include <MarioKartWii/Item/Obj/ObjProperties.hpp>
#include <RetroRewind.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <Race/CustomItems.hpp>

// Origial code from VP, adapted to Pulsar 2.0
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
    bool itemModeRandom = Pulsar::GAMEMODE_DEFAULT;
    bool itemModeBlast = Pulsar::GAMEMODE_DEFAULT;
    if (RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_HOST || RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_NONHOST) {
        itemModeRandom = System::sInstance->IsContext(Pulsar::PULSAR_ITEMMODERANDOM) ? Pulsar::GAMEMODE_RANDOM : Pulsar::GAMEMODE_DEFAULT;
        itemModeBlast = System::sInstance->IsContext(Pulsar::PULSAR_ITEMMODEBLAST) ? Pulsar::GAMEMODE_BLAST : Pulsar::GAMEMODE_DEFAULT;
    }
    new (dest) Item::ObjProperties(rel);
    if (itemModeBlast == Pulsar::GAMEMODE_BLAST) {
        dest->limit = 25;
    } else if (itemModeRandom == Pulsar::GAMEMODE_RANDOM) {
        dest->limit = 5;
    } else {
        u32 bitfield = Pulsar::Race::GetEffectiveCustomItemsBitfield();
        if (bitfield != 0x7FFFF && CountEnabledItems(bitfield) <= 5) {
            dest->limit = 12;
        }
    }
}

kmCall(0x80790b74, ChangeBlueOBJProperties);

static void ChangeBillOBJProperties(Item::ObjProperties* dest, const Item::ObjProperties& rel) {
    bool itemModeRandom = Pulsar::GAMEMODE_DEFAULT;
    bool itemModeBlast = Pulsar::GAMEMODE_DEFAULT;
    if (RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_HOST || RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_NONHOST) {
        itemModeRandom = System::sInstance->IsContext(Pulsar::PULSAR_ITEMMODERANDOM) ? Pulsar::GAMEMODE_RANDOM : Pulsar::GAMEMODE_DEFAULT;
        itemModeBlast = System::sInstance->IsContext(Pulsar::PULSAR_ITEMMODEBLAST) ? Pulsar::GAMEMODE_BLAST : Pulsar::GAMEMODE_DEFAULT;
    }
    new (dest) Item::ObjProperties(rel);
    if (itemModeRandom == Pulsar::GAMEMODE_RANDOM) {
        dest->limit = 25;
    } else if (itemModeBlast == Pulsar::GAMEMODE_BLAST) {
        dest->limit = 5;
    }
}

kmCall(0x80790bf4, ChangeBillOBJProperties);

static void ChangeBombOBJProperties(Item::ObjProperties* dest, const Item::ObjProperties& rel) {
    bool itemModeRandom = Pulsar::GAMEMODE_DEFAULT;
    bool itemModeBlast = Pulsar::GAMEMODE_DEFAULT;
    if (RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_HOST || RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_NONHOST) {
        itemModeRandom = System::sInstance->IsContext(Pulsar::PULSAR_ITEMMODERANDOM) ? Pulsar::GAMEMODE_RANDOM : Pulsar::GAMEMODE_DEFAULT;
        itemModeBlast = System::sInstance->IsContext(Pulsar::PULSAR_ITEMMODEBLAST) ? Pulsar::GAMEMODE_BLAST : Pulsar::GAMEMODE_DEFAULT;
    }
    new (dest) Item::ObjProperties(rel);
    if (itemModeRandom == Pulsar::GAMEMODE_RANDOM) {
        dest->limit = 20;
    } else if (itemModeBlast == Pulsar::GAMEMODE_BLAST) {
        dest->limit = 25;
    }
}

kmCall(0x80790bb4, ChangeBombOBJProperties);

static void ChangeItemOBJProperties(Item::ObjProperties* dest, const Item::ObjProperties& rel) {
    new (dest) Item::ObjProperties(rel);
    if (Pulsar::Race::GetEffectiveCustomItemsBitfield() != 0x7FFFF) {
        dest->limit = 16;
    }
}
kmCall(0x80790bc4, ChangeItemOBJProperties);  // Blooper
kmCall(0x80790bd4, ChangeItemOBJProperties);  // POW
kmCall(0x80790c04, ChangeItemOBJProperties);  // Thunder Cloud

}  // namespace Race
}  // namespace RetroRewind