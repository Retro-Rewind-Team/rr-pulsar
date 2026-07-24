#include <kamek.hpp>
#include <runtimeWrite.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/RKSYS/RKSYSMgr.hpp>
#include <Network/Rating/PlayerRating.hpp>
#include <Settings/Settings.hpp>
#include <UI/PlayerCount.hpp>

namespace Pulsar {
namespace Network {

kmRuntimeUse(0x8011d2b0);
kmRuntimeUse(0x8011d1e4);
kmRuntimeUse(0x8011e488);
kmRuntimeUse(0x8011e480);
kmRuntimeUse(0x8011e490);
kmRuntimeUse(0x8011e384);
kmRuntimeUse(0x800d1984);
static u32 sPreviousRoomGroupId = 0;
extern "C" u32 sMatchmakingTimeoutMs = 0x4e20;

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

static void RememberPreviousPublicRoomGroupId(const RKNet::Controller* controller) {
    if (controller == nullptr || !IsPublicMatchmakingRoomType(controller->roomType)) return;

    const u32 currentSub = static_cast<u32>(controller->currentSub) & 1;
    for (u32 i = 0; i < 2; ++i) {
        const u32 groupId = controller->subs[(currentSub + i) & 1].groupId;
        if (groupId != 0) {
            sPreviousRoomGroupId = groupId;
            return;
        }
    }
}

static bool IsInfiniteMatchmakingEnabled() {
    int totalPlayers = 0;
    PlayerCount::GetNumbersTotal(totalPlayers);
    return totalPlayers >= 40 && Settings::Mgr::Get().GetUserSettingValue(
        Settings::SETTINGSTYPE_ONLINE,
        RADIO_INFINITEMATCHMAKINGTIMEOUT) == MATCHMAKINGTIMEOUT_INFINITE;
}

static void ApplyMatchmakingTimeoutPatch() {
    sMatchmakingTimeoutMs =
        IsInfiniteMatchmakingEnabled() ? 0x7fff : 0x4e20;
}

asmFunc LoadMatchmakingTimeout() {
    ASM(
        nofralloc;
        lis r6, sMatchmakingTimeoutMs @ha;
        lwz r6, sMatchmakingTimeoutMs @l(r6);
        blr;)
}

typedef int (*DWCSetupGameServer_t)(int maxPlayers, void* callback, void* callbackParam,
                                    void* option0, void* option1, void* playerValidCallback,
                                    void* playerUserData, void* userData);
static const DWCSetupGameServer_t DWCSetupGameServer =
    (DWCSetupGameServer_t)kmRuntimeAddr(0x800d1984);

static int PreventInfiniteMatchmakingRoomCreation(int maxPlayers, void* callback, void* callbackParam,
                                                  void* option0, void* option1,
                                                  void* playerValidCallback, void* playerUserData,
                                                  void* userData) {
    const RKNet::Controller* const net = RKNet::Controller::sInstance;
    if (IsInfiniteMatchmakingEnabled() && net != nullptr &&
        IsPublicMatchmakingRoomType(net->roomType)) {
        return 0;
    }

    return DWCSetupGameServer(maxPlayers, callback, callbackParam, option0, option1,
                              playerValidCallback, playerUserData, userData);
}
kmCall(0x806577a4, PreventInfiniteMatchmakingRoomCreation);
kmCall(0x80659100, PreventInfiniteMatchmakingRoomCreation);
kmCall(0x80659608, PreventInfiniteMatchmakingRoomCreation);

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

typedef void (*ServerBrowserRemoveServerA_t)(void* sb, void* server);
static const ServerBrowserRemoveServerA_t ServerBrowserRemoveServerA =
    (ServerBrowserRemoveServerA_t)kmRuntimeAddr(0x8011e384);

static int GetRoomPlayerCount(void* server) {
    return SBServerGetIntValueA(server, "numplayers", -1) + 1;
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

    const bool isInfiniteMatchmakingEnabled = IsInfiniteMatchmakingEnabled();
    const u32 licenseId = RKSYS::Mgr::sInstance->curLicenseId;
    RKNet::Controller* const net = RKNet::Controller::sInstance;
    const bool isBattle = net != nullptr &&
        (net->roomType == RKNet::ROOMTYPE_BT_WW || net->roomType == RKNet::ROOMTYPE_BT_REGIONAL);
    const int playerRating = isBattle
        ? (int)(PointRating::GetUserBR(licenseId) * 100.0f + 0.5f)
        : (int)(PointRating::GetUserVR(licenseId) * 100.0f + 0.5f);
    const char* const ratingKey = isBattle ? "eb" : "ev";
    const int maximumRoomVR = playerRating + 4000000;  // 40,000 VR, stored at 100x precision.
    // Preserve the high-VR rooms only when no otherwise eligible room exists.
    bool hasRoomWithinVRLimit = isBattle;

    for (int i = 0; i < count && !hasRoomWithinVRLimit; ++i) {
        void* const server = ServerBrowserGetServerAtIndexA(sb, i);
        if (!server) continue;

        const int roomPlayerCount = GetRoomPlayerCount(server);
        const bool isPreviousRoom = sPreviousRoomGroupId != 0 &&
            SBServerGetIntValueA(server, "dwc_groupid", 0) == (int)sPreviousRoomGroupId;
        const bool isEligibleRoom = roomPlayerCount < 12 && !isPreviousRoom &&
            (!isInfiniteMatchmakingEnabled || roomPlayerCount >= 6);
        if (isEligibleRoom && SBServerGetIntValueA(server, ratingKey, 0) <= maximumRoomVR) {
            hasRoomWithinVRLimit = true;
        }
    }

    for (int i = count - 1; i >= 0; --i) {
        void* const server = ServerBrowserGetServerAtIndexA(sb, i);
        if (!server) continue;

        const int roomPlayerCount = GetRoomPlayerCount(server);
        // Do not let the DWC fallback try a full room. Infinite matchmaking also
        // excludes small rooms, so an empty browser causes another search rather
        // than a new-room attempt.
        const int roomRating = SBServerGetIntValueA(server, ratingKey, 0);
        if (roomPlayerCount >= 12 ||
            (sPreviousRoomGroupId != 0 &&
                SBServerGetIntValueA(server, "dwc_groupid", 0) == (int)sPreviousRoomGroupId) ||
            (!isBattle && hasRoomWithinVRLimit && roomRating > maximumRoomVR) ||
            (isInfiniteMatchmakingEnabled && roomPlayerCount < 6)) {
            ServerBrowserRemoveServerA(sb, server);
        }
    }

    count = ServerBrowserCountA(sb);
    for (int i = 0; i < count; ++i) {
        void* const server = ServerBrowserGetServerAtIndexA(sb, i);
        if (!server) continue;

        int ratingDifference = playerRating - SBServerGetIntValueA(server, ratingKey, 0);
        if (ratingDifference < 0) ratingDifference = -ratingDifference;

        // The browser has no pre-join latency/NAT measurement. Keep VR decisively
        // primary and use a fuller room only as a small continuity tiebreaker.
        const int eval = ratingDifference * 32 - GetRoomPlayerCount(server);
        SBServerSetIntValueA(server, "dwc_eval", eval);
    }
    ServerBrowserSortA(sb, true, "dwc_eval", 0);

    if (isInfiniteMatchmakingEnabled) {
        // Once VR-ranked, retain only the three closest eligible rooms. Subsequent
        // retries receive this same constrained set and never use random fallback.
        for (int i = ServerBrowserCountA(sb) - 1; i >= 3; --i) {
            void* const server = ServerBrowserGetServerAtIndexA(sb, i);
            if (server) ServerBrowserRemoveServerA(sb, server);
        }
    }
}
kmBranch(0x800e4ad0, CustomRandomizeServers);

static void UpdateMatchmakingInfos(RKNet::Controller* self) {
    self->UpdateSubsAndVR();
    RememberPreviousPublicRoomGroupId(self);
}
kmCall(0x80657990, UpdateMatchmakingInfos);

// Reset when starting ConnectToAnyoneAsync
static void OnConnectToAnyoneAsync(RKNet::Controller* self) {
    ApplyMatchmakingTimeoutPatch();
    RememberPreviousPublicRoomGroupId(self);
    self->ConnectToAnybodyAsync();
}
kmCall(0x806590b4, OnConnectToAnyoneAsync);

kmCall(0x800d6c94, LoadMatchmakingTimeout);
kmCall(0x800d6ee4, LoadMatchmakingTimeout);

}  // namespace Network
}  // namespace Pulsar
