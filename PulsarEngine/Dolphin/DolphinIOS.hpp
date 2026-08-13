#ifndef __DOLPHINIOS_HPP
#define __DOLPHINIOS_HPP

#include <RetroRewind.hpp>

namespace Dolphin {

// https://github.com/dolphin-emu/dolphin/blob/af064c57dee4e116bfdb2e82f7bcc8376ae813a8/Source/Core/Core/IOS/DolphinDevice.cpp#L28-L41
enum DolphinIOCTL {
    IOCTL_DOLPHIN_GET_ELAPSED_TIME = 0x01,
    IOCTL_DOLPHIN_GET_VERSION = 0x02,
    IOCTL_DOLPHIN_GET_SPEED_LIMIT = 0x03,
    IOCTL_DOLPHIN_SET_SPEED_LIMIT = 0x04,
    IOCTL_DOLPHIN_GET_CPU_SPEED = 0x05,
    IOCTL_DOLPHIN_GET_REAL_PRODUCTCODE = 0x06,
    IOCTL_DOLPHIN_DISCORD_SET_CLIENT = 0x07,
    IOCTL_DOLPHIN_DISCORD_SET_PRESENCE = 0x08,
    IOCTL_DOLPHIN_DISCORD_RESET = 0x09,
    IOCTL_DOLPHIN_GET_SYSTEM_TIME = 0x0A,
};

/**
 * @brief Check if the game is running on an emulator
 * @return Indicates if the game is running on an emulator
 */
bool IsEmulator();

/**
 * @brief Get the elapsed time since the game started
 * @param elapsedTime The elapsed time in milliseconds
 * @return Indicates success or failure
 */
bool GetElapsedTime(u32 &elapsedTime);

/**
 * @brief Get the version of Dolphin
 * @param version Output string version
 * @param length Length of the output string buffer
 * @return Indicates success or failure
 */
bool GetVersion(char *version, u32 length);

/**
 * @brief Sets the emulation speed limit (percent)
 * @param speedLimit The speed limit in percent
 * @return Indicates success or failure
 */
bool GetSpeedLimit(u32 &speedLimit);

/**
 * @brief Sets the emulation speed limit (percent)
 * @param speedLimit The speed limit in percent
 * @return Indicates success or failure
 */
bool SetSpeedLimit(u32 &speedLimit);

/**
 * @brief Get the CPU speed
 * @param cpuSpeed The CPU speed in ticks per second
 * @return Indicates success or failure
 */
bool GetCPUSpeed(u32 &cpuSpeed);

/**
 * @brief Get the real product code of the game
 * @param productCode Output string product code
 * @param length Length of the output string buffer
 * @return Indicates success or failure
 */
bool GetRealProductCode(char *productCode, u32 length);

/**
 * @brief Set the Discord client ID
 * @param clientID The client ID
 * @return Indicates success or failure
 */
bool SetDiscordClient(char *clientID);

/**
 * @brief Set the Discord presence
 * @param details The details of the presence
 * @param state The state of the presence
 * @param largeImageKey The large image key
 * @param largeImageText The large image text
 * @param smallImageKey The small image key
 * @param smallImageText The small image text
 * @param startTimestamp The start timestamp
 * @param endTimestamp The end timestamp
 * @param partySize The party size
 * @param partyMax The party max
 * @return Indicates success or failure
 */
bool SetDiscordPresence(
    char *details,
    char *state,
    char *largeImageKey,
    char *largeImageText,
    char *smallImageKey,
    char *smallImageText,
    u64 startTimestamp,
    u64 endTimestamp,
    u32 partySize,
    u32 partyMax);

/**
 * @brief Reset the Discord presence
 * @return Indicates success or failure
 */
bool ResetDiscord();

/**
 * @brief Get the system time
 * @param systemTime The system time in milliseconds
 * @return Indicates success or failure
 */
bool GetSystemTime(u64 &systemTime);

}  // namespace Dolphin

#endif