#include <Network/RoomKey.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <PulsarSystem.hpp>
#include <Network/MatchCommand.hpp>
#include <Settings/Settings.hpp>

namespace Pulsar {
namespace Network {

// PULSAR_RANDOM_KEY is defined in the Makefile and changes every build to ensure room keys are unique to each build.
#ifndef PULSAR_RANDOM_KEY
#define PULSAR_RANDOM_KEY 0xADD2BFAF
#endif

u32 HARD_CODED_ROOM_KEY = PULSAR_RANDOM_KEY;

u32 GetRoomKey() {
    return PULSAR_RANDOM_KEY;
}

}  // namespace Network
}  // namespace Pulsar