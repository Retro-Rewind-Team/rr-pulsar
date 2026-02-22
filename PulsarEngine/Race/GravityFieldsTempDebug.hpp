#ifndef _PULSAR_GRAVITY_FIELDS_TEMP_DEBUG_
#define _PULSAR_GRAVITY_FIELDS_TEMP_DEBUG_

#include <kamek.hpp>
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

}  // namespace TempDebug
}  // namespace GravityFields
}  // namespace Race
}  // namespace Pulsar

#endif
