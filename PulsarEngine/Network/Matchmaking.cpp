#include <kamek.hpp>
#include <runtimeWrite.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/RKSYS/RKSYSMgr.hpp>
#include <Network/Rating/PlayerRating.hpp>
#include <Network/Rating/MogiRating.hpp>
#include <Network/Mogi.hpp>
#include <Settings/Settings.hpp>
#include <include/c_stdlib.h>

namespace Pulsar {
namespace Network {

kmRuntimeUse(0x8011d2b0);
kmRuntimeUse(0x8011d1e4);
kmRuntimeUse(0x8011e488);
kmRuntimeUse(0x8011e480);
kmRuntimeUse(0x8011e490);
kmRuntimeUse(0x800d6c94);
kmRuntimeUse(0x800d6ee4);
static u32 sJoinAttempts = 0;
static u32 sPreviousRoomGroupId = 0;

static bool IsPublicMatchmakingRoomType(const RKNet::RoomType roomType) {
    switch (roomType) {
        case RKNet::ROOMTYPE_VS_WW:
        case RKNet::ROOMTYPE_VS_REGIONAL:
        case RKNet::ROOMTYPE_BT_WW:
        case RKNet::ROOMTYPE_BT_REGIONAL:
        case RKNet::ROOMTYPE_JOINING_WW:
        case RKNet::ROOMTYPE_JOINING_REGIONAL:
            return true;
        default:
            return false;
    }
}

static u32 GetTrackedRoomGroupId(const RKNet::Controller* controller) {
    if (controller == nullptr) return 0;
    const u32 currentSub = static_cast<u32>(controller->currentSub) & 1;
    for (u32 i = 0; i < 2; ++i) {
        const u32 groupId = controller->subs[(currentSub + i) & 1].groupId;
        if (groupId != 0) return groupId;
    }
    return 0;
}

static void RememberPreviousPublicRoomGroupId(const RKNet::Controller* controller) {
    if (controller == nullptr || !IsPublicMatchmakingRoomType(controller->roomType)) return;

    const u32 activeGroupId = GetTrackedRoomGroupId(controller);
    if (activeGroupId != 0) sPreviousRoomGroupId = activeGroupId;
}

static void ApplyMatchmakingTimeoutPatch() {
    const u8 timeoutSetting = Settings::Mgr::Get().GetUserSettingValue(
        Settings::SETTINGSTYPE_ONLINE,
        RADIO_INFINITEMATCHMAKINGTIMEOUT);

    const u32 timeoutMs =
        (timeoutSetting == MATCHMAKINGTIMEOUT_INFINITE || Mogi::IsEnabled()) ? 0x7fff : 0x4e20;
    const u32 liR6TimeoutMs = 0x38c00000 | timeoutMs;

    kmRuntimeWrite32A(0x800d6c94, liR6TimeoutMs);
    kmRuntimeWrite32A(0x800d6ee4, liR6TimeoutMs);
}

typedef int (*SBServerGetIntValueA_t)(void* server, const char* key, int defaultValue);
static const SBServerGetIntValueA_t SBServerGetIntValueA = (SBServerGetIntValueA_t)kmRuntimeAddr(0x8011d2b0);

typedef void (*SBServerSetIntValueA_t)(void* server, const char* key, int value);
static const SBServerSetIntValueA_t SBServerSetIntValueA = (SBServerSetIntValueA_t)kmRuntimeAddr(0x8011d1e4);

typedef int (*ServerBrowserCountA_t)(void* sb);
static const ServerBrowserCountA_t ServerBrowserCountA = (ServerBrowserCountA_t)kmRuntimeAddr(0x8011e488);

typedef void* (*ServerBrowserGetServerAtIndexA_t)(void* sb, int index);
static const ServerBrowserGetServerAtIndexA_t ServerBrowserGetServerAtIndexA = (ServerBrowserGetServerAtIndexA_t)kmRuntimeAddr(0x8011e480);

typedef void (*ServerBrowserSortA_t)(void* sb, bool ascending, const char* sortKey, int sortType);
static const ServerBrowserSortA_t ServerBrowserSortA = (ServerBrowserSortA_t)kmRuntimeAddr(0x8011e490);

static bool HasNonSmallRoomOption(void* sb, int count) {
    for (int i = 0; i < count; ++i) {
        void* server = ServerBrowserGetServerAtIndexA(sb, i);
        if (!server) continue;

        const int serverPlayerCount = SBServerGetIntValueA(server, "numplayers", -1) + 1;
        if (serverPlayerCount >= 6) return true;
    }
    return false;
}

static bool HasAlternativeRoomOption(void* sb, int count, bool blockSmallRooms) {
    for (int i = 0; i < count; ++i) {
        void* server = ServerBrowserGetServerAtIndexA(sb, i);
        if (!server) continue;

        const int serverGroupId = SBServerGetIntValueA(server, "dwc_groupid", 0);
        if (sPreviousRoomGroupId != 0 && serverGroupId == (int)sPreviousRoomGroupId) continue;

        const int serverPlayerCount = SBServerGetIntValueA(server, "numplayers", -1) + 1;
        const bool isSmallRoom = serverPlayerCount > 0 && serverPlayerCount < 6;
        if (blockSmallRooms && isSmallRoom) continue;

        return true;
    }
    return false;
}

// Hook DWCi_RandomizeServers to sort by VR proximity
kmRuntimeUse(0x8038630C);
void CustomRandomizeServers() {
    // dwcControl lives at r13 - 0x68f4.
    void* dwcControl = *(void**)kmRuntimeAddr(0x8038630C);
    if (!dwcControl) return;

    void* sb = *(void**)((u8*)dwcControl + 0x6dc);
    if (!sb) return;

    int count = ServerBrowserCountA(sb);
    if (count <= 0) return;

    const u8 timeoutSetting = Settings::Mgr::Get().GetUserSettingValue(
        Settings::SETTINGSTYPE_ONLINE,
        RADIO_INFINITEMATCHMAKINGTIMEOUT);
    const bool isCompetitiveMatchmakingEnabled =
        (timeoutSetting == MATCHMAKINGTIMEOUT_INFINITE || Mogi::IsEnabled());
    const bool blockSmallRooms = isCompetitiveMatchmakingEnabled && HasNonSmallRoomOption(sb, count);
    const bool hasAlternativeRoomOption = HasAlternativeRoomOption(sb, count, blockSmallRooms);
    const int previousRoomPenalty = hasAlternativeRoomOption ? 2000000000 : 0;
    const int ratingMismatchEval = 999999;
    const int blockedSmallRoomEval = 0x50000000;
    const int fullRoomEval = 0x60000000;

    if (sJoinAttempts < 3) {
        u32 licenseId = RKSYS::Mgr::sInstance->curLicenseId;

        RKNet::Controller* net = RKNet::Controller::sInstance;
        bool isBattle = false;
        if (net) {
            isBattle = (net->roomType == RKNet::ROOMTYPE_BT_WW || net->roomType == RKNet::ROOMTYPE_BT_REGIONAL);
        }

        int playerRating;
        const char* key;
        const bool isMogi = Mogi::IsEnabled() && !isBattle;
        if (isMogi) {
            playerRating = (int)(MogiRating::GetUserMMR(licenseId) * 100.0f + 0.5f);
            key = "em";
        } else if (isBattle) {
            playerRating = (int)(PointRating::GetUserBR(licenseId) * 100.0f + 0.5f);
            key = "eb";
        } else {
            playerRating = (int)(PointRating::GetUserVR(licenseId) * 100.0f + 0.5f);
            key = "ev";
        }

        // For players below 150 VR, filter out rooms above their VR + 200
        bool isLowVR = !isBattle && !isMogi && playerRating < 15000;  // 150 VR * 100
        int maxRoomRating = playerRating + 20000;  // Player VR + 200 VR * 100

        // For players above 600 VR, heavily deprioritize rooms below 300 VR
        bool isHighVR = !isBattle && !isMogi && playerRating > 60000;  // 600 VR * 100
        int lowRoomThreshold = 30000;  // 300 VR * 100
        int highRoomThreshold = playerRating + 40000;  // Player VR + 400 VR * 100

        for (int i = 0; i < count; ++i) {
            void* server = ServerBrowserGetServerAtIndexA(sb, i);
            if (!server) continue;

            int serverPlayerCount = SBServerGetIntValueA(server, "numplayers", -1) + 1;
            if (isCompetitiveMatchmakingEnabled && serverPlayerCount >= 12) {
                SBServerSetIntValueA(server, "dwc_eval", fullRoomEval);
                continue;
            }

            int serverGroupId = SBServerGetIntValueA(server, "dwc_groupid", 0);
            if (sPreviousRoomGroupId != 0 && serverGroupId == (int)sPreviousRoomGroupId) {
                SBServerSetIntValueA(server, "dwc_eval", previousRoomPenalty);
                continue;
            }

            bool isSmallRoom = serverPlayerCount > 0 && serverPlayerCount < 6;
            if (blockSmallRooms && isSmallRoom) {
                SBServerSetIntValueA(server, "dwc_eval", blockedSmallRoomEval);
                continue;
            }

            int serverRating = SBServerGetIntValueA(server, key, 0);
            if (isMogi && serverRating == 0) {
                SBServerSetIntValueA(server, "dwc_eval", ratingMismatchEval);
                continue;
            }
            int diff = playerRating - serverRating;
            if (diff < 0) diff = -diff;

            int eval;

            // If player is low VR and room is above the threshold, mark it with very high eval
            if (isLowVR && serverRating > maxRoomRating) {
                eval = ratingMismatchEval;
            } else if (!isBattle && serverRating > highRoomThreshold) {
                eval = ratingMismatchEval;
            } else if (isHighVR && serverRating > 0 && serverRating < lowRoomThreshold) {
                eval = ratingMismatchEval;
            } else {
                eval = diff;
            }

            SBServerSetIntValueA(server, "dwc_eval", eval);
        }
        // Sort by dwc_eval ascending (closest first)
        ServerBrowserSortA(sb, true, "dwc_eval", 0);
    } else {
        // Fallback to random
        for (int i = 0; i < count; ++i) {
            void* server = ServerBrowserGetServerAtIndexA(sb, i);
            if (!server) continue;
            int serverPlayerCount = SBServerGetIntValueA(server, "numplayers", -1) + 1;
            if (isCompetitiveMatchmakingEnabled && serverPlayerCount >= 12) {
                SBServerSetIntValueA(server, "dwc_eval", fullRoomEval);
                continue;
            }

            int serverGroupId = SBServerGetIntValueA(server, "dwc_groupid", 0);
            bool isSmallRoom = serverPlayerCount > 0 && serverPlayerCount < 6;
            if (sPreviousRoomGroupId != 0 && serverGroupId == (int)sPreviousRoomGroupId) {
                SBServerSetIntValueA(server, "dwc_eval", previousRoomPenalty);
            } else if (blockSmallRooms && isSmallRoom) {
                SBServerSetIntValueA(server, "dwc_eval", blockedSmallRoomEval);
            } else {
                int eval = blockSmallRooms ? (rand() & 0x3fffffff) : rand();
                SBServerSetIntValueA(server, "dwc_eval", eval);
            }
        }
        ServerBrowserSortA(sb, true, "dwc_eval", 0);
    }
}
kmBranch(0x800e4ad0, CustomRandomizeServers);

static void UpdateMatchmakingInfosAndRememberGroupId(RKNet::Controller* self) {
    self->UpdateSubsAndVR();
    Mogi::UpdateRoomState();
    RememberPreviousPublicRoomGroupId(self);
}
kmCall(0x80657990, UpdateMatchmakingInfosAndRememberGroupId);

// Reset when starting ConnectToAnyoneAsync
static void OnConnectToAnyoneAsync(RKNet::Controller* self) {
    ApplyMatchmakingTimeoutPatch();
    sJoinAttempts = 0;
    RememberPreviousPublicRoomGroupId(self);
    self->ConnectToAnybodyAsync();
}
kmCall(0x806590b4, OnConnectToAnyoneAsync);

// Increment in DWCi_RetryReserving
kmRuntimeUse(0x800df094);
typedef void (*DWCi_RetryReserving_t)(int);
static void OnRetryReserving(int r3) {
    ++sJoinAttempts;
    ((DWCi_RetryReserving_t)kmRuntimeAddr(0x800df094))(r3);
}
kmCall(0x800d66ac, OnRetryReserving);
kmCall(0x800d6950, OnRetryReserving);

// Patch timeouts to 20s (20000ms = 0x4e20)
kmWrite32(0x800d6c94, 0x38c04e20);  // li r6, 20000 (default)
kmWrite32(0x800d6ee4, 0x38c04e20);  // li r6, 20000 (default)

}  // namespace Network
}  // namespace Pulsar
