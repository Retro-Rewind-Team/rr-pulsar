#include <Dolphin/DolphinIOS.hpp>

namespace Dolphin {

static s32 s_dolphinFD = -1;
static bool s_isEmulator = false;
static bool s_isEmulatorChecked = false;

int OpenDolphin() {
    if (s_dolphinFD < 0) {
        s_dolphinFD = IOS::Open("/dev/dolphin", IOS::MODE_NONE);
        if (s_dolphinFD < 0) {
            s_isEmulator = false;
        } else {
            s_isEmulator = true;
        }

        s_isEmulatorChecked = true;
    }

    return s_dolphinFD;
}

int CloseDolphin() {
    if (s_dolphinFD >= 0) {
        IOS::Close(s_dolphinFD);
        s_dolphinFD = -1;
    }

    return s_dolphinFD;
}

bool IsEmulator() {
    if (!s_isEmulatorChecked) {
        OpenDolphin();
    }

    return s_isEmulator;
}

bool GetElapsedTime(u32& elapsedTime) {
    if (OpenDolphin() < 0) {
        return false;
    }

    IOS::IOCtlvRequest request;
    request.address = &elapsedTime;
    request.size = sizeof(u32);

    return IOS::IOCtlv(
               s_dolphinFD,
               (IOS::IOCtlType)IOCTL_DOLPHIN_GET_ELAPSED_TIME,
               0,
               1,
               &request) >= 0;
}

bool GetVersion(char* version, u32 length) {
    if (OpenDolphin() < 0) {
        return false;
    }

    IOS::IOCtlvRequest request;
    request.address = version;
    request.size = length;

    return IOS::IOCtlv(
               s_dolphinFD,
               (IOS::IOCtlType)IOCTL_DOLPHIN_GET_VERSION,
               0,
               1,
               &request) >= 0;
}

bool SetDiscordClient(char* clientID) {
    if (OpenDolphin() < 0) {
        return false;
    }

    IOS::IOCtlvRequest request;
    request.address = clientID;
    request.size = strlen(clientID) + 1;

    s32 ret = IOS::IOCtlv(
        s_dolphinFD,
        (IOS::IOCtlType)IOCTL_DOLPHIN_DISCORD_SET_CLIENT,
        1,
        0,
        &request);

    return ret >= 0;
}

bool SetDiscordPresence(
    char* details,
    char* state,
    char* largeImageKey,
    char* largeImageText,
    char* smallImageKey,
    char* smallImageText,
    u64 startTimestamp,
    u64 endTimestamp,
    u32 partySize,
    u32 partyMax) {
    if (OpenDolphin() < 0) {
        return false;
    }

    IOS::IOCtlvRequest request[10];
    request[0].address = details;
    request[0].size = strlen(details) + 1;

    request[1].address = state;
    request[1].size = strlen(state) + 1;

    request[2].address = largeImageKey;
    request[2].size = strlen(largeImageKey) + 1;

    request[3].address = largeImageText;
    request[3].size = strlen(largeImageText) + 1;

    request[4].address = smallImageKey;
    request[4].size = strlen(smallImageKey) + 1;

    request[5].address = smallImageText;
    request[5].size = strlen(smallImageText) + 1;

    request[6].address = &startTimestamp;
    request[6].size = sizeof(u64);

    request[7].address = &endTimestamp;
    request[7].size = sizeof(u64);

    request[8].address = &partySize;
    request[8].size = sizeof(u32);

    request[9].address = &partyMax;
    request[9].size = sizeof(u32);

    return IOS::IOCtlv(
               s_dolphinFD,
               (IOS::IOCtlType)IOCTL_DOLPHIN_DISCORD_SET_PRESENCE,
               10,
               0,
               request) >= 0;
}

bool ResetDiscord() {
    if (OpenDolphin() < 0) {
        return false;
    }

    return IOS::IOCtlv(
               s_dolphinFD,
               (IOS::IOCtlType)IOCTL_DOLPHIN_DISCORD_RESET,
               0,
               0,
               nullptr) >= 0;
}

bool GetSystemTime(u64& systemTime) {
    if (OpenDolphin() < 0) {
        return false;
    }

    IOS::IOCtlvRequest request;
    request.address = &systemTime;
    request.size = sizeof(u64);

    return IOS::IOCtlv(
               s_dolphinFD,
               (IOS::IOCtlType)IOCTL_DOLPHIN_GET_SYSTEM_TIME,
               0,
               1,
               &request) >= 0;
}

}  // namespace Dolphin
