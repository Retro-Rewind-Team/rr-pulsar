#ifndef _PULSAR_GRAVITY_FIELDS_
#define _PULSAR_GRAVITY_FIELDS_

#include <kamek.hpp>
#include <MarioKartWii/Kart/KartLink.hpp>
#include <MarioKartWii/Kart/KartPhysics.hpp>
#include <MarioKartWii/Kart/KartStatus.hpp>
#include <MarioKartWii/Kart/KartSub.hpp>

namespace Pulsar {
namespace Race {
namespace GravityFields {

u8 GetGravityAreaType();
float GetDefaultGravityStrength();

bool TryGetGravityAtPosition(const Vec3& position, Vec3& gravityDown, float& gravityStrength);

void UpdateKartGravity(const Kart::Link& link, Vec3& gravityVector, float& gravityStrength);
void UpdateKartGravity(const Kart::Sub& sub, Vec3& gravityVector, float& gravityStrength);
void ForceKartGravityRefresh(const Kart::Link& link);
bool TryGetPlayerGravityUp(u8 playerIdx, Vec3& gravityUp);
void PrepareKartCollisionForGravity(Kart::Status& status);

Vec3 GetGravityDownAtPosition(const Vec3& position);
float GetBodyGravityScalar(const Vec3& gravityVector, float gravityStrength, float previousScalar);
void ApplyBodyGravityVector(Kart::Physics& physics, const Vec3& gravityVector);
s16 GetActiveAreaId(u8 playerIdx);

}  // namespace GravityFields
}  // namespace Race
}  // namespace Pulsar

#endif
