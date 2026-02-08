#ifndef _RETRO_REWIND_CHANNEL_
#define _RETRO_REWIND_CHANNEL_

namespace Pulsar {

// Signature written by the new launcher.
#define RRC_SIGNATURE_ADDRESS 0x817ffff8
#define RRC_SIGNATURE 0xDEADBEEF

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

}  // namespace Pulsar

#endif
