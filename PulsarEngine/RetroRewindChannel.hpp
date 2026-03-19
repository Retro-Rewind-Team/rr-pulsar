#ifndef _RETRO_REWIND_CHANNEL_
#define _RETRO_REWIND_CHANNEL_

#include <kamek.hpp>

namespace Pulsar {

// Signature written by the new launcher.
#define RRC_SIGNATURE_ADDRESS 0x817ffff8
#define RRC_ABI_VERSION_ADDRESS 0x817ffffc
#define RRC_SIGNATURE 0xDEADBEEF

// The "ABI version" of the new channel; incremented each time a change is necessary that would break compatibility
// (for example moving addresses of fixed data elsewhere).
// This needs to match the version declared on the channel side, or otherwise an error is shown.
#define RRC_ABI_VERSION 1

// IMPORTANT: Always keep these bitflags in sync with the channel's code when adding new ones so they don't conflict!
#define RRC_BITFLAGS_ADDRESS 0x817ffff0
#define RRC_BITFLAG_SEPARATE_SAVEGAME 0x1
#define RRC_BITFLAG_LOADED_FROM_RR 0x2
#define RRC_BITFLAG_RR_CRASHED 0x4

bool IsNewChannel();
bool NewChannel_UseSeparateSavegame();
// If we call back into the channel, this flag indicates to skip some steps in the channel.
void NewChannel_SetLoadedFromRRFlag();
// This loads the channels' crash handler if set and the channel is called back into.
void NewChannel_SetCrashFlag();
void NewChannel_Init();

}  // namespace Pulsar

#endif