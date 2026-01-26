/*
    PlayerRating.cpp - Retro Rewind custom rating system
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
#include <Network/Rating/PlayerRating.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/RKSYS/RKSYSMgr.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <Network/PacketExpansion.hpp>
#include <Dolphin/DolphinIOS.hpp>

namespace Pulsar {
namespace PointRating {

u8 remoteDecimalVR[12][2];
float lastRaceDeltas[12];

static const s16 SPLINE_CONTROL_POINTS[5] = {0, 1, 8, 50, 125};
static const int SPLINE_BIAS = 7499;
static const float SPLINE_SCALE = 0.00020004f;  // 1/(2*SPLINE_BIAS)

static inline float Clamp(float val, float min, float max) {
    return (val < min) ? min : (val > max) ? max
                                           : val;
}

static float EvaluateSpline(float x) {
    float result = 0.0f;
    for (int i = -2; i <= 6; ++i) {
        int idx = (i < 0) ? 0 : (i > 4) ? 4
                                        : i;
        float d = x - (float)i;
        if (d < 0.0f) d = -d;

        float w = 0.0f;
        if (d <= 1.0f) {
            w = (4.0f - 6.0f * d * d + 3.0f * d * d * d) / 6.0f;
        } else if (d < 2.0f) {
            float t = 2.0f - d;
            w = (t * t * t) / 6.0f;
        }
        result += w * (float)SPLINE_CONTROL_POINTS[idx];
    }
    return result / 30.0f;
}

static float CalcPosPoints(float self, float opponent) {
    float sample = (float)SPLINE_BIAS + (opponent - self) * 4.0f;
    sample = Clamp(sample, 0.0f, (float)(SPLINE_BIAS * 2));
    return Clamp(EvaluateSpline(SPLINE_SCALE * sample), 0.02f, 0.24f);
}

static float CalcNegPoints(float self, float opponent) {
    float sample = (float)SPLINE_BIAS - (opponent - self) * 16.0f;
    sample = Clamp(sample, 0.0f, (float)(SPLINE_BIAS * 2));
    return Clamp(-EvaluateSpline(SPLINE_SCALE * sample), -0.19f, 0.0f);
}

static float GetGainCap(float rating) {
    if (rating < 1500.0f) return 1e6f;
    if (rating >= 9000.0f) return 0.10f;
    float t = (rating - 1500.0f) / 7500.0f;
    return 0.10f + 999.9f * (1.0f - t);
}

static float GetLossCap(float rating) {
    if (rating <= 150.0f) return -0.5f;
    if (rating >= 500.0f) return -2.09f;
    float t = (rating - 150.0f) / 350.0f;
    return -0.5f + (-2.09f + 0.5f) * t;
}

static bool IsBattle(GameMode mode) {
    return mode == MODE_BATTLE || mode == MODE_PUBLIC_BATTLE || mode == MODE_PRIVATE_BATTLE;
}

static bool IsRegionalVS() {
    RKNet::Controller* ctrl = RKNet::Controller::sInstance;
    return ctrl && (ctrl->roomType == RKNet::ROOMTYPE_VS_REGIONAL || ctrl->roomType == RKNet::ROOMTYPE_JOINING_REGIONAL);
}

static bool IsRankedFroom() {
    RKNet::Controller* ctrl = RKNet::Controller::sInstance;
    return ctrl && (ctrl->roomType == RKNet::ROOMTYPE_FROOM_HOST || ctrl->roomType == RKNet::ROOMTYPE_FROOM_NONHOST) &&
           System::sInstance->IsContext(PULSAR_VR);
}

static bool IsRegionalBT() {
    RKNet::Controller* ctrl = RKNet::Controller::sInstance;
    return ctrl && (ctrl->roomType == RKNet::ROOMTYPE_BT_REGIONAL);
}

static bool IsRankedMode(const RacedataSettings& settings) {
    return settings.gamemode > MODE_6 && settings.gamemode < MODE_AWARD;
}

static int CountLocalPlayersBefore(const RacedataScenario& scenario, int idx) {
    int count = 0;
    for (int i = 0; i < idx; ++i) {
        if (scenario.players[i].playerType == PLAYER_REAL_LOCAL) count++;
    }
    return count;
}

static float GetPlayerRating(const RacedataScenario& scenario, int idx) {
    const RacedataPlayer& player = scenario.players[idx];

    if (player.playerType == PLAYER_REAL_LOCAL && CountLocalPlayersBefore(scenario, idx) == 0) {
        RKSYS::Mgr* rksys = RKSYS::Mgr::sInstance;
        if (rksys) {
            if (IsBattle(scenario.settings.gamemode)) {
                return GetUserBR(rksys->curLicenseId);
            }
            if ((IsRegionalVS() && scenario.settings.gamemode == MODE_PUBLIC_VS) || IsRankedFroom()) {
                return GetUserVR(rksys->curLicenseId);
            }
        }
    } else if (player.playerType == PLAYER_REAL_ONLINE) {
        const Network::CustomRKNetController* ctrl =
            reinterpret_cast<const Network::CustomRKNetController*>(RKNet::Controller::sInstance);
        u8 aid = ctrl->aidsBelongingToPlayerIds[idx];

        int slot = 0;
        for (int i = 0; i < idx; ++i) {
            if (ctrl->aidsBelongingToPlayerIds[i] == aid) slot++;
        }
        if (slot < 2) {
            float base = (float)player.rating.points;
            float decimal = (float)remoteDecimalVR[aid][slot] / 100.0f;
            return base + decimal;
        }
    }
    return (float)player.rating.points;
}

static float TruncateToCentis(float val) {
    return (float)((int)(val * 100.0f)) / 100.0f;
}

static void SaveLocalRating(const RacedataScenario& scenario, int idx, float rating) {
    const RacedataPlayer& player = scenario.players[idx];
    if (player.playerType != PLAYER_REAL_LOCAL || CountLocalPlayersBefore(scenario, idx) != 0) return;

    RKSYS::Mgr* rksys = RKSYS::Mgr::sInstance;
    if (!rksys) return;

    if (IsBattle(scenario.settings.gamemode)) {
        if (IsRegionalBT() || IsRankedFroom()) SetUserBR(rksys->curLicenseId, rating);
    } else if ((IsRegionalVS() && scenario.settings.gamemode == MODE_PUBLIC_VS) || IsRankedFroom()) {
        SetUserVR(rksys->curLicenseId, rating);
    }
}

static void UpdatePlayerRating(RacedataScenario& scenario, int idx, float delta) {
    float next = Clamp(GetPlayerRating(scenario, idx) + delta, (float)MIN_RATING, (float)MAX_RATING);
    next = TruncateToCentis(next);
    scenario.players[idx].rating.points = (u16)next;
    SaveLocalRating(scenario, idx, next);
}

void RR_UpdatePoints(RacedataScenario* scenario) {
    if (scenario->settings.gametype != GAMETYPE_DEFAULT) return;

    const u32 playerCount = scenario->playerCount;
    Raceinfo* raceInfo = Raceinfo::sInstance;
    bool isBattle = IsBattle(scenario->settings.gamemode);
    bool isRanked = IsRankedMode(scenario->settings);
    bool isVR = !isBattle && ((IsRegionalVS() && scenario->settings.gamemode == MODE_PUBLIC_VS) || IsRankedFroom());

    float deltas[12] = {};
    bool allDisconnected = false;
    if (isVR) {
        const RKNet::Controller* rkCtrl = RKNet::Controller::sInstance;
        if (rkCtrl->subs[rkCtrl->currentSub].connectionCount <= 1) {
            allDisconnected = true;
        }
    }

    for (u32 i = 0; i < playerCount; ++i) {
        u8 myPos = raceInfo->players[i]->position;
        u16 myScore = isBattle ? raceInfo->players[i]->battleScore : 0;

        if (isRanked) {
            float myRating = GetPlayerRating(*scenario, i);
            for (u32 j = 0; j < playerCount; ++j) {
                if (i == j) continue;
                float oppRating = GetPlayerRating(*scenario, j);

                if (isBattle) {
                    u16 oppScore = raceInfo->players[j]->battleScore;
                    if (oppScore < myScore)
                        deltas[i] += CalcPosPoints(myRating, oppRating);
                    else if (myScore < oppScore)
                        deltas[i] += CalcNegPoints(myRating, oppRating);
                } else {
                    u8 oppPos = raceInfo->players[j]->position;
                    if (myPos < oppPos)
                        deltas[i] += CalcPosPoints(myRating, oppRating);
                    else if (oppPos < myPos)
                        deltas[i] += CalcNegPoints(myRating, oppRating);
                }
            }
        }

        if (myPos != 0 && playerCount != 0) {
            scenario->players[i].finishPos = myPos;
            u16 pts = 0;
            if (!isBattle && (scenario->settings.gamemode < 9 || scenario->settings.gamemode > 10)) {
                if (playerCount <= 12 && myPos <= 12 && myPos > 0)
                    pts = Racedata::pointsRoom[playerCount - 1][myPos - 1];
            } else {
                pts = raceInfo->players[i]->battleScore;
            }
            scenario->players[i].score = scenario->players[i].previousScore + pts;
        }
    }

    float multiplier = GetMultiplier();
    for (u32 i = 0; i < playerCount; ++i) {
        deltas[i] *= multiplier;
        float oldRating = GetPlayerRating(*scenario, i);
        deltas[i] = Clamp(deltas[i], GetLossCap(oldRating), GetGainCap(oldRating));

        if (isVR) {
            if (allDisconnected) {
                deltas[i] = (playerCount >= 4) ? -0.01f : 0.0f;
            } else if (deltas[i] >= -0.0101f && deltas[i] < 0.0f) {
                deltas[i] = 0.0f;
            }
        }

        float next = Clamp(oldRating + deltas[i], (float)MIN_RATING, (float)MAX_RATING);
        next = TruncateToCentis(next);
        UpdatePlayerRating(*scenario, i, deltas[i]);
        lastRaceDeltas[i] = next - oldRating;
    }
}
kmRuntimeUse(0x8052e950);
static void ApplyRatingPatch() {
    kmRuntimeBranchA(0x8052e950, RR_UpdatePoints);
    RKNet::Controller* ctrl = RKNet::Controller::sInstance;
    if ((ctrl->roomType == RKNet::ROOMTYPE_FROOM_HOST || ctrl->roomType == RKNet::ROOMTYPE_FROOM_NONHOST) && !System::sInstance->IsContext(PULSAR_VR)) {
        kmRuntimeWrite32A(0x8052e950, 0x9421ff70);
    }
}
static SectionLoadHook ratingHook(ApplyRatingPatch);

kmWrite16(0x8064F6DA, 30000);
kmWrite16(0x8064F6E6, 30000);
kmWrite16(0x8064F76A, 30000);
kmWrite16(0x8064F776, 30000);
kmWrite16(0x8085654E, 30000);
kmWrite16(0x80856556, 30000);
kmWrite16(0x808565BA, 30000);
kmWrite16(0x808565C2, 30000);
kmWrite16(0x8085C23E, 30000);
kmWrite16(0x8085C246, 30000);
kmWrite16(0x8085C322, 30000);
kmWrite16(0x8085C32A, 30000);

}  // namespace PointRating
}  // namespace Pulsar