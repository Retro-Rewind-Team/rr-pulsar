#ifndef _PULSAR_GRAVITY_FIELDS_TEMP_DEBUG_
#define _PULSAR_GRAVITY_FIELDS_TEMP_DEBUG_

#include <kamek.hpp>
#include <MarioKartWii/Kart/KartMovement.hpp>
#include <MarioKartWii/Kart/KartPhysics.hpp>
#include <MarioKartWii/Kart/KartStatus.hpp>
#include <MarioKartWii/Kart/KartLink.hpp>
#include <MarioKartWii/Item/Obj/ItemObj.hpp>

namespace Pulsar {
namespace Race {
namespace GravityFields {
namespace TempDebug {

void OnRaceLoad();
void OnPlayerGravityResolved(
    u8 playerIdx,
    const Vec3& position,
    s16 previousAreaId,
    s16 newAreaId,
    const Vec3& gravityDown,
    float gravityStrength,
    u16 blendFrames,
    bool snapTransition);
void OnRespawn(const Kart::Link& link, s16 areaId, const Vec3& gravityDown, float gravityStrength);
void OnItemGravityApplied(const Item::Obj& itemObj, s16 areaId, const Vec3& gravityDown, float gravityStrength);
void OnBodyGravityApplied(u8 playerIdx, s16 areaId, const Kart::Status& status, const Kart::Physics& physics, const Vec3& gravityVector);
void OnMovementUpApplied(u8 playerIdx, s16 areaId, const Kart::Status* status, const Kart::Movement& movement, const Vec3& localUp, bool overrideApplied);

}  // namespace TempDebug
}  // namespace GravityFields
}  // namespace Race
}  // namespace Pulsar

#endif
