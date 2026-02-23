#ifndef _PULSAR_GRAVITY_FIELDS_
#define _PULSAR_GRAVITY_FIELDS_

#include <kamek.hpp>
#include <MarioKartWii/Kart/KartLink.hpp>
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

Vec3 GetGravityDownAtPosition(const Vec3& position);
float GetBodyGravityScalar(const Vec3& gravityVector, float gravityStrength, float previousScalar);

}  // namespace GravityFields
}  // namespace Race
}  // namespace Pulsar

#endif
