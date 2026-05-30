#include <PulsarSystem.hpp>
#include "Debug/Debug.hpp"
#include "RetroRewindChannel.hpp"
#include "IO/SDIO.hpp"
#include <Dolphin/DolphinIOS.hpp>

namespace Pulsar {

bool IsNewChannel() {
    return *reinterpret_cast<u32*>(RRC_SIGNATURE_ADDRESS) == RRC_SIGNATURE;
}

bool NewChannel_UseSeparateSavegame() {
    return (*reinterpret_cast<u8*>(RRC_BITFLAGS_ADDRESS) & RRC_BITFLAG_SEPARATE_SAVEGAME) == RRC_BITFLAG_SEPARATE_SAVEGAME;
}

void NewChannel_WriteLoadedFromRREphFile() {
    if(IO::sInstance == nullptr) return;
    IO::sInstance->CreateAndOpen(RRC_LOADED_FROM_RR_EPH_FILE_PATH, IOS::MODE_READ_WRITE);
    IO::sInstance->Close();
    // Check the file was actually written
    // Also for some reason without this it doesnt get wrtten. I guess there's some flush/sync issues
    if (!IO::sInstance->OpenFile(RRC_LOADED_FROM_RR_EPH_FILE_PATH, IOS::MODE_READ)) {
        Debug::FatalError(RRC_LOADED_FROM_RR_EPH_FILE_PATH " was not written properly.");
    }
}

void NewChannel_WriteCrashEphFile() {
    if (IO::sInstance == nullptr) return;
    IO::sInstance->CreateAndOpen(RRC_CRASH_EPH_FILE_PATH, IOS::MODE_READ_WRITE);
    IO::sInstance->Close();
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
}

}  // namespace Pulsar
