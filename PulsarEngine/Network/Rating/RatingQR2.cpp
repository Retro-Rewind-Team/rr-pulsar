/*
    RatingQR2.cpp
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
#include <core/rvl/os/OS.hpp>
#include <include/c_stdio.h>
#include <MarioKartWii/RKSYS/RKSYSMgr.hpp>
#include <Network/Rating/PlayerRating.hpp>

namespace Pulsar {
namespace Rating {

kmRuntimeUse(0x8010f434);
kmRuntimeUse(0x800e4c88);
typedef void (*qr2_buffer_addA_t)(void* buffer, const char* value);
static const qr2_buffer_addA_t qr2_buffer_addA = (qr2_buffer_addA_t)kmRuntimeAddr(0x8010f434);
typedef void (*ServerKeyCallback)(int key, void* buffer);
static const ServerKeyCallback OriginalServerKeyCallback = (ServerKeyCallback)kmRuntimeAddr(0x800e4c88);

static void MyServerKeyCallback(int key, void* buffer) {
    RKSYS::Mgr* rksys = RKSYS::Mgr::sInstance;
    u32 licenseId = 0;
    if (rksys) {
        licenseId = rksys->curLicenseId;
    }

    if (key == 0x65) { // ev
        float vr = PointRating::GetUserVR(licenseId);
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", (int)(vr * 100.0f)); 
        qr2_buffer_addA(buffer, buf);
        return;
    }
    if (key == 0x66) { // eb
        float br = PointRating::GetUserBR(licenseId);
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", (int)(br * 100.0f));
        qr2_buffer_addA(buffer, buf);
        return;
    }
    OriginalServerKeyCallback(key, buffer);
}

kmRuntimeUse(0x8010ecac);
typedef int (*qr2_init_socketA_t)(void* q, int s, int bound_port, const char* gamename, const char* secret_key, int is_public, int nat_negotiate, void* server_key_callback, void* player_key_callback, void* team_key_callback, void* key_list_callback, void* count_callback, void* adderror_callback, void* userdata);
static const qr2_init_socketA_t qr2_init_socketA_Real = (qr2_init_socketA_t)kmRuntimeAddr(0x8010ecac);

static int Hook_qr2_init_socketA(void* q, int s, int bound_port, const char* gamename, const char* secret_key, int is_public, int nat_negotiate, void* server_key_callback, void* player_key_callback, void* team_key_callback, void* key_list_callback, void* count_callback, void* adderror_callback, void* userdata) {
    return qr2_init_socketA_Real(q, s, bound_port, gamename, secret_key, is_public, nat_negotiate, (void*)MyServerKeyCallback, player_key_callback, team_key_callback, key_list_callback, count_callback, adderror_callback, userdata);
}
kmCall(0x800d4f28, Hook_qr2_init_socketA);

} // namespace Rating
} // namespace Pulsar
