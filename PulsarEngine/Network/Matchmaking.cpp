/*
    Matchmaking.cpp
    Copyright (C) 2025 ZPL

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <kamek.hpp>
#include <runtimeWrite.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/RKSYS/RKSYSMgr.hpp>
#include <Network/Rating/PlayerRating.hpp>
#include <include/c_stdlib.h>

namespace Pulsar {
namespace Network {

kmRuntimeUse(0x8011d2b0);
kmRuntimeUse(0x8011d1e4);
kmRuntimeUse(0x8011e488);
kmRuntimeUse(0x8011e480);
kmRuntimeUse(0x8011e490);
static u32 joinAttempts = 0;

// SBServerGetIntValueA
typedef int (*SBServerGetIntValueA_t)(void* server, const char* key, int defaultValue);
static const SBServerGetIntValueA_t SBServerGetIntValueA = (SBServerGetIntValueA_t)kmRuntimeAddr(0x8011d2b0);

// SBServerSetIntValueA
typedef void (*SBServerSetIntValueA_t)(void* server, const char* key, int value);
static const SBServerSetIntValueA_t SBServerSetIntValueA = (SBServerSetIntValueA_t)kmRuntimeAddr(0x8011d1e4);
// ServerBrowserCountA
typedef int (*ServerBrowserCountA_t)(void* sb);
static const ServerBrowserCountA_t ServerBrowserCountA = (ServerBrowserCountA_t)kmRuntimeAddr(0x8011e488);

// ServerBrowserGetServerAtIndexA
typedef void* (*ServerBrowserGetServerAtIndexA_t)(void* sb, int index);
static const ServerBrowserGetServerAtIndexA_t ServerBrowserGetServerAtIndexA = (ServerBrowserGetServerAtIndexA_t)kmRuntimeAddr(0x8011e480);
// ServerBrowserSortA
typedef void (*ServerBrowserSortA_t)(void* sb, bool ascending, const char* sortKey, int sortType);
static const ServerBrowserSortA_t ServerBrowserSortA = (ServerBrowserSortA_t)kmRuntimeAddr(0x8011e490);

// Hook DWCi_RandomizeServers to sort by VR proximity
kmRuntimeUse(0x8038630C);
void CustomRandomizeServers() {
    // Load dwcControl from r13 - 0x68f4
    void* dwcControl = *(void**)kmRuntimeAddr(0x8038630C);
    if (!dwcControl) return;

    void* sb = *(void**)((u8*)dwcControl + 0x6dc);
    if (!sb) return;

    int count = ServerBrowserCountA(sb);
    if (count <= 0) return;

    if (joinAttempts < 3) {
        u32 licenseId = RKSYS::Mgr::sInstance->curLicenseId;

        RKNet::Controller* net = RKNet::Controller::sInstance;
        bool isBattle = false;
        if (net) {
            isBattle = (net->roomType == RKNet::ROOMTYPE_BT_WW || net->roomType == RKNet::ROOMTYPE_BT_REGIONAL);
        }

        int playerRating;
        const char* key;
        if (isBattle) {
            playerRating = (int)(PointRating::GetUserBR(licenseId) * 100.0f + 0.5f);
            key = "eb";
        } else {
            playerRating = (int)(PointRating::GetUserVR(licenseId) * 100.0f + 0.5f);
            key = "ev";
        }

        for (int i = 0; i < count; ++i) {
            void* server = ServerBrowserGetServerAtIndexA(sb, i);
            if (!server) continue;
            int serverRating = SBServerGetIntValueA(server, key, 0);
            int diff = playerRating - serverRating;
            if (diff < 0) diff = -diff;
            SBServerSetIntValueA(server, "dwc_eval", diff);
        }
        // Sort by dwc_eval ascending (closest first)
        ServerBrowserSortA(sb, true, "dwc_eval", 0);
    } else {
        // Fallback to random
        for (int i = 0; i < count; ++i) {
            void* server = ServerBrowserGetServerAtIndexA(sb, i);
            if (!server) continue;
            SBServerSetIntValueA(server, "dwc_eval", rand());
        }
        ServerBrowserSortA(sb, true, "dwc_eval", 0);
    }
}
kmBranch(0x800e4ad0, CustomRandomizeServers);

// Reset when starting ConnectToAnyoneAsync
static void OnConnectToAnyoneAsync(RKNet::Controller* self) {
    joinAttempts = 0;
    self->ConnectToAnybodyAsync();
}
kmCall(0x806590b4, OnConnectToAnyoneAsync);

// Increment in DWCi_RetryReserving
kmRuntimeUse(0x800df094);
typedef void (*DWCi_RetryReserving_t)(int);
static void OnRetryReserving(int r3) {
    joinAttempts++;
    ((DWCi_RetryReserving_t)kmRuntimeAddr(0x800df094))(r3);
}
kmCall(0x800d66ac, OnRetryReserving);
kmCall(0x800d6950, OnRetryReserving);

// Patch timeouts to 20s (20000ms = 0x4e20)
kmWrite32(0x800d6c94, 0x38c04e20);  // li r6, 20000
kmWrite32(0x800d6ee4, 0x38c04e20);  // li r6, 20000

}  // namespace Network
}  // namespace Pulsar