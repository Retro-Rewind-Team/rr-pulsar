#ifndef _PULSAR_GRAVITY_CAMERA_
#define _PULSAR_GRAVITY_CAMERA_

#include <kamek.hpp>
#include <MarioKartWii/Kart/KartLink.hpp>

namespace Pulsar {
namespace Race {
namespace GravityCamera {

void RefreshCameraOnRespawn(const Kart::Link& link);

}  // namespace GravityCamera
}  // namespace Race
}  // namespace Pulsar

#endif
