#include <PulsarSystem.hpp>
#include "Debug/Debug.hpp"
#include "RetroRewindChannel.hpp"

namespace Pulsar {

bool IsNewChannel() {
    return *reinterpret_cast<u32*>(RRC_SIGNATURE_ADDRESS) == RRC_SIGNATURE;
}

bool NewChannel_UseSeparateSavegame() {
    return (*reinterpret_cast<u8*>(RRC_BITFLAGS_ADDRESS) & RRC_BITFLAG_SEPARATE_SAVEGAME) == RRC_BITFLAG_SEPARATE_SAVEGAME;
}

void NewChannel_SetLoadedFromRRFlag() {
    *reinterpret_cast<u8*>(RRC_BITFLAGS_ADDRESS) |= RRC_BITFLAG_LOADED_FROM_RR;
}

void NewChannel_SetCrashFlag() {
    *reinterpret_cast<u8*>(RRC_BITFLAGS_ADDRESS) |= RRC_BITFLAG_RR_CRASHED;
}

void NewChannel_Init() {
    u32 channelVersion = *reinterpret_cast<u32*>(RRC_ABI_VERSION_ADDRESS);
    u32 requiredVersion = RRC_ABI_VERSION;

    // Make sure the channel is compatible with this Code.pul
    if (channelVersion != requiredVersion) {
        char message[256];
        snprintf(message, sizeof(message), "This version of Retro Rewind is incompatible with the version of the channel (abi%d != abi%d).\n"
            "You can usually fix this by updating both RR and the channel to the latest version.", channelVersion, requiredVersion);
        Debug::FatalError(message);
    }

    NewChannel_SetLoadedFromRRFlag();
}

}  // namespace Pulsar