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

#define RRC_LOADED_FROM_RR_EPH_FILE_PATH "/RetroRewindChannel/.lfrr"
#define RRC_CRASH_EPH_FILE_PATH "/RetroRewindChannel/.crash"

bool IsNewChannel();
bool NewChannel_UseSeparateSavegame();
void NewChannel_WriteLoadedFromRREphFile();
void NewChannel_WriteCrashEphFile();
void NewChannel_Init();

}  // namespace Pulsar

#endif