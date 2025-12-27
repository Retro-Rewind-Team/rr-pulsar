#include <kamek.hpp>
#include <runtimeWrite.hpp>
#include <Network/WiiLink.hpp>

namespace Pulsar {
namespace Network {

// NATNEG: retry interval for a negotiation step (current_time() + 300ms).
kmWrite32(0x8011b6f4, 0x3803012C);
kmWrite32(0x8011b998, 0x388300C8);
kmWrite32(0x8011b99c, 0x3860000F);

// RKNet: allow sending a RACE packet each frame (gate is milliseconds since last send).
kmWrite32(0x80657ea8, 0x28040009);

// RKNet: smoother lag compensation.
kmWrite32(0x80654c00, 0x38030001);

// ProcessLagFrames also has a tail-path that can add +1 or +2 depending on an RKSystem flag.
kmWrite32(0x80654ce8, 0x38600001);

// GTI2: tolerate brief packet loss better by retrying more aggressively.
kmWrite32(0x8010a6c4, 0x28000190);
kmWrite32(0x8010a720, 0x2800001E);

// GTI2: avoid disconnecting due to a single keepalive send failure during quiet periods.
kmWrite32(0x8010a664, 0x280088B8);

// GTI2: slightly increase pre-connection timeout upper bound (limited to 16-bit immediate).
kmWrite32(0x8010a58c, 0x2800FFFF);

// Spectating remote smoothing: tolerate longer gaps before forcing a neutral input.
kmWrite32(0x80589bb8, 0x280000C8);

}  // namespace Network
}  // namespace Pulsar