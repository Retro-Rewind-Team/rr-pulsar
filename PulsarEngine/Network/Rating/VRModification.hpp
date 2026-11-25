#ifndef _PULSAR_VR_MODIFICATION_
#define _PULSAR_VR_MODIFICATION_

#include <types.hpp>

namespace Pulsar {
namespace Network {

const char vrLeaderboardTopUrl[] = "http://rwfc.net/api/leaderboard/in-game?page=1";
const s32 VR_MODIFICATION_ERROR_CODE = 42069;
const u32 vrModificationMargin = 2000;
const u32 vrModificationTimeoutMs = 10000;
const u32 vrLeaderboardBufferSize = 0x20000;

// -1 while the request is pending, 0 if it failed, otherwise #1's VR.
extern volatile s32 topLeaderboardVR;

bool PreventVRModification();

}  // namespace Network
}  // namespace Pulsar

#endif
