#ifndef _PULSAR_RATING_CONNECTION_BLOCK_HPP_
#define _PULSAR_RATING_CONNECTION_BLOCK_HPP_

#include <kamek.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>

namespace Pulsar {
namespace Network {

// Hook into ConnectToWFC to block players with > 2000 VR
void CheckVRAndLogin(wchar_t* miiName, int unk, void* callback, RKNet::Controller* self);

} // namespace Network
} // namespace Pulsar

#endif // _PULSAR_RATING_CONNECTION_BLOCK_HPP_
