#ifndef _PUL_SELECTION_RESTRICTIONS_
#define _PUL_SELECTION_RESTRICTIONS_

#include <kamek.hpp>
#include <MarioKartWii/System/Identifiers.hpp>

namespace Pulsar {
namespace Restrictions {

static const u32 CHARACTER_SLOT_COUNT = 27;
static const u32 ALL_CHARACTERS = (1u << CHARACTER_SLOT_COUNT) - 1;
static const u16 ALL_VEHICLES = 0x0fff;
static const u32 VEHICLE_WEIGHT_COUNT = 3;
static const u32 VEHICLES_PER_WEIGHT = 12;

bool IsFriendRoom();
bool IsCharacterRestrictionEnabled();
bool IsVehicleRestrictionEnabled();
bool IsCharacterRestrictionConfigActive();
bool IsVehicleRestrictionConfigActive();
void SetCharacterRestrictionConfigActive(bool active);
void SetVehicleRestrictionConfigActive(bool active);

u32 GetCharacterMask();
u16 GetVehicleMask(u32 weight);
u32 GetCharacterSlot(CharacterId character);
u32 GetVehiclePosition(KartId kart);

bool IsCharacterSlotEnabled(u32 slot);
bool IsCharacterEnabled(CharacterId character);
bool IsVehicleEnabled(KartId kart);
bool AreOnlyMiisEnabled();
bool IsOnlyMiiOutfitCEnabled();

u32 GetFirstEnabledCharacterSlot();
u32 GetFirstEnabledVehiclePosition(u32 weight);

}  // namespace Restrictions
}  // namespace Pulsar

#endif
