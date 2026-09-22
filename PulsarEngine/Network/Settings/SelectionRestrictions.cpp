#include <Network/Settings/SelectionRestrictions.hpp>
#include <PulsarSystem.hpp>
#include <RetroRewind.hpp>
#include <MarioKartWii/GlobalFunctions.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/UI/Ctrl/Menu/CtrlMenuCharacterSelect.hpp>
#include <MarioKartWii/UI/Page/Menu/KartSelect.hpp>

namespace Pulsar {
namespace Restrictions {

static bool s_characterConfigActive = false;
static bool s_vehicleConfigActive = false;

bool IsFriendRoom() {
    const RKNet::Controller *controller = RKNet::Controller::sInstance;
    if (controller == nullptr) return false;
    return controller->roomType == RKNet::ROOMTYPE_FROOM_HOST || controller->roomType == RKNet::ROOMTYPE_FROOM_NONHOST;
}

bool IsCharacterRestrictionEnabled() {
    return IsFriendRoom() && System::sInstance != nullptr && System::sInstance->IsContext(PULSAR_CHARRESTRICT);
}

bool IsVehicleRestrictionEnabled() {
    return IsFriendRoom() && System::sInstance != nullptr && System::sInstance->IsContext(PULSAR_VEHICLERESTRICT);
}

bool IsCharacterRestrictionConfigActive() { return s_characterConfigActive; }
bool IsVehicleRestrictionConfigActive() { return s_vehicleConfigActive; }
void SetCharacterRestrictionConfigActive(bool active) { s_characterConfigActive = active; }
void SetVehicleRestrictionConfigActive(bool active) { s_vehicleConfigActive = active; }

u32 GetCharacterMask() {
    if (!IsCharacterRestrictionEnabled()) return ALL_CHARACTERS;
    const u32 mask = System::sInstance->netMgr.characterRestrictionMask & ALL_CHARACTERS;
    return mask == 0 ? ALL_CHARACTERS : mask;
}

u16 GetVehicleMask(u32 weight) {
    if (!IsVehicleRestrictionEnabled() || weight >= VEHICLE_WEIGHT_COUNT) return ALL_VEHICLES;
    const u16 mask = System::sInstance->netMgr.vehicleRestrictionMasks[weight] & ALL_VEHICLES;
    return mask == 0 ? ALL_VEHICLES : mask;
}

u32 GetCharacterSlot(CharacterId character) {
    for (u32 slot = 0; slot < 24; ++slot) {
        if (CtrlMenuCharacterSelect::buttonIdToCharacterId[slot] == character) return slot;
    }

    switch (character) {
        case MII_S_A_MALE:
        case MII_S_A_FEMALE:
        case MII_M_A_MALE:
        case MII_M_A_FEMALE:
        case MII_L_A_MALE:
        case MII_L_A_FEMALE:
            return RetroRewind::System::BUTTON_MII_A;
        case MII_S_B_MALE:
        case MII_S_B_FEMALE:
        case MII_M_B_MALE:
        case MII_M_B_FEMALE:
        case MII_L_B_MALE:
        case MII_L_B_FEMALE:
            return RetroRewind::System::BUTTON_MII_B;
        case MII_S_C_MALE:
        case MII_S_C_FEMALE:
        case MII_M_C_MALE:
        case MII_M_C_FEMALE:
        case MII_L_C_MALE:
        case MII_L_C_FEMALE:
            return RetroRewind::System::BUTTON_MII_C;
        default:
            return CHARACTER_SLOT_COUNT;
    }
}

u32 GetVehiclePosition(KartId kart) {
    const s32 weight = GetKartWeightClass(kart);
    if (weight < 0 || weight >= static_cast<s32>(VEHICLE_WEIGHT_COUNT)) return VEHICLES_PER_WEIGHT;
    for (u32 position = 0; position < VEHICLES_PER_WEIGHT; ++position) {
        if (kartsSortedByWeight[weight][position] == kart) return position;
    }
    return VEHICLES_PER_WEIGHT;
}

bool IsCharacterSlotEnabled(u32 slot) {
    return slot < CHARACTER_SLOT_COUNT && ((GetCharacterMask() >> slot) & 1) != 0;
}

bool IsCharacterEnabled(CharacterId character) {
    return IsCharacterSlotEnabled(GetCharacterSlot(character));
}

bool IsVehicleEnabled(KartId kart) {
    const s32 weight = GetKartWeightClass(kart);
    const u32 position = GetVehiclePosition(kart);
    return weight >= 0 && weight < static_cast<s32>(VEHICLE_WEIGHT_COUNT) && position < VEHICLES_PER_WEIGHT &&
           ((GetVehicleMask(weight) >> position) & 1) != 0;
}

bool AreOnlyMiisEnabled() {
    if (!IsCharacterRestrictionEnabled()) return false;
    const u32 miiMask = (1u << RetroRewind::System::BUTTON_MII_A) | (1u << RetroRewind::System::BUTTON_MII_B) |
                        (1u << RetroRewind::System::BUTTON_MII_C);
    return (GetCharacterMask() & ~miiMask) == 0;
}

bool IsOnlyMiiOutfitCEnabled() {
    return IsCharacterRestrictionEnabled() && GetCharacterMask() == (1u << RetroRewind::System::BUTTON_MII_C);
}

u32 GetFirstEnabledCharacterSlot() {
    const u32 mask = GetCharacterMask();
    for (u32 slot = 0; slot < CHARACTER_SLOT_COUNT; ++slot) {
        if ((mask >> slot) & 1) return slot;
    }
    return 0;
}

u32 GetFirstEnabledVehiclePosition(u32 weight) {
    const u16 mask = GetVehicleMask(weight);
    for (u32 position = 0; position < VEHICLES_PER_WEIGHT; ++position) {
        if ((mask >> position) & 1) return position;
    }
    return 0;
}

}  // namespace Restrictions
}  // namespace Pulsar
