/*
    PlayerRating.cpp
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
#include <Network/Rating/PlayerRating.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/RKSYS/RKSYSMgr.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <Network/PacketExpansion.hpp>

namespace Pulsar {
namespace PointRating {

u8 remoteDecimalVR[12][2];
float lastRaceDeltas[12];

static const int kDeltaClamp = 9998;
static const int kSplineBias = 7499;
static const float kSplineScale = 0.00020004001271445304f;
static const float kOneSixth = 0.16666667f;

static const s16 kSplineControlPoints[5] = {0, 1, 8, 50, 125};

inline float Abs(float value) {
	return (value >= 0.0f) ? value : -value;
}

float EvaluateSpline(float x) {
	float total = 0.0f;
	for (int offset = -2; offset <= 6; ++offset) {
		int tableIdx = offset;
		if (tableIdx < 0) {
			tableIdx = 0;
		} else if (tableIdx > 4) {
			tableIdx = 4;
		}

		float delta = Abs(x - static_cast<float>(offset));
		float weight = 0.0f;

		if (delta <= 1.0f) {
			const float delta2 = delta * delta;
			const float delta3 = delta2 * delta;
			weight = (4.0f - 6.0f * delta2 + 3.0f * delta3) * kOneSixth;
		} else if (delta < 2.0f) {
			const float t = 2.0f - delta;
			weight = (t * t * t) * kOneSixth;
		} else {
			continue;
		}

		total += weight * static_cast<float>(kSplineControlPoints[tableIdx]);
	}
	return total;
}

static float CalcPosPoints(float selfPoints, float opponentPoints) {
    float diff = (opponentPoints - selfPoints) * 20.0f;
    
    float sample = (float)kSplineBias + diff;
    float result = EvaluateSpline(kSplineScale * sample);
    if (result > 2.50f) result = 2.50f;
    if (result < 0.20f) result = 0.20f;
    return result / 10.0f;
}

static float CalcNegPoints(float selfPoints, float opponentPoints) {
    float diff = (opponentPoints - selfPoints) * 20.0f;

    float sample = (float)kSplineBias - diff;
    float result = -EvaluateSpline(kSplineScale * sample);
    if (result < -1.80f) result = -1.80f;
    if (result > 0.0f) result = 0.0f;
    return result / 10.0f;
}

static bool IsBattle(GameMode mode) {
    return (mode == MODE_BATTLE || mode == MODE_PUBLIC_BATTLE || mode == MODE_PRIVATE_BATTLE);
}

static float GetPlayerRating(const RacedataScenario& scenario, int playerIdx) {
    const RacedataPlayer& player = scenario.players[playerIdx];
    if (player.playerType == PLAYER_REAL_LOCAL) {
        int localCount = 0;
        for (int i = 0; i < playerIdx; ++i) {
            if (scenario.players[i].playerType == PLAYER_REAL_LOCAL) localCount++;
        }
        if (localCount == 0) {
            RKSYS::Mgr* rksys = RKSYS::Mgr::sInstance;
            RKNet::Controller* controller = RKNet::Controller::sInstance;
            if (rksys) {
                bool isBattle = IsBattle(scenario.settings.gamemode);
                if (isBattle) {
                    return GetUserBR(rksys->curLicenseId);
                }
                bool isRegionalVs = false;
                if (controller != nullptr) {
                    isRegionalVs = (controller->roomType == RKNet::ROOMTYPE_VS_REGIONAL || controller->roomType == RKNet::ROOMTYPE_JOINING_REGIONAL);
                }
                if (!isBattle && isRegionalVs && scenario.settings.gamemode == MODE_PUBLIC_VS) {
                    return GetUserVR(rksys->curLicenseId);
                }
            }
        }
    } else if (player.playerType == PLAYER_REAL_ONLINE) {
        const Network::CustomRKNetController* controller = reinterpret_cast<const Network::CustomRKNetController*>(RKNet::Controller::sInstance);
        u8 aid = controller->aidsBelongingToPlayerIds[playerIdx];

        int playerIndexOnConsole = 0;
        for (int i = 0; i < playerIdx; ++i) {
            if (controller->aidsBelongingToPlayerIds[i] == aid) {
                playerIndexOnConsole++;
            }
        }

        if (playerIndexOnConsole < 2) {
            float baseRating = (float)player.rating.points;
            float decimal = (float)remoteDecimalVR[aid][playerIndexOnConsole] / 100.0f;
            return baseRating + decimal;
        }
    }
    return (float)player.rating.points;
}

static void UpdatePlayerRating(RacedataScenario& scenario, int playerIdx, float delta) {
    float current = GetPlayerRating(scenario, playerIdx);
    float next = current + delta;
    if (next < (float)MinRating) next = (float)MinRating;
    if (next > (float)MaxRating) next = (float)MaxRating;
    
    next = (float)((int)(next * 100.0f)) / 100.0f;

    scenario.players[playerIdx].rating.points = (u16)next;
    
    const RacedataPlayer& player = scenario.players[playerIdx];
    if (player.playerType == PLAYER_REAL_LOCAL) {
        int localCount = 0;
        for(int i=0; i<playerIdx; ++i) {
            if(scenario.players[i].playerType == PLAYER_REAL_LOCAL) localCount++;
        }
        if(localCount == 0) {
            RKSYS::Mgr* rksys = RKSYS::Mgr::sInstance;
            RKNet::Controller* controller = RKNet::Controller::sInstance;
            if (rksys) {
                bool isBattle = IsBattle(scenario.settings.gamemode);
                if (isBattle) {
                    SetUserBR(rksys->curLicenseId, next);
                } else {
                    bool isRegionalVs = false;
                    if (controller != nullptr) {
                        isRegionalVs = (controller->roomType == RKNet::ROOMTYPE_VS_REGIONAL || controller->roomType == RKNet::ROOMTYPE_JOINING_REGIONAL);
                    }
                    if (isRegionalVs && scenario.settings.gamemode == MODE_PUBLIC_VS) {
                        SetUserVR(rksys->curLicenseId, next);
                    }
                }
            }
        }
    }
}

void RR_UpdatePoints(RacedataScenario* scenario) {
    u32 playerCount = scenario->playerCount;
    if (scenario->settings.gametype == GAMETYPE_DEFAULT) {
        float deltas[12];
        for(int i=0; i<12; ++i) deltas[i] = 0.0f;

        Raceinfo* raceInfo = Raceinfo::sInstance;
        
        for (u32 i = 0; i < playerCount; ++i) {
            bool isBattle = IsBattle(scenario->settings.gamemode);
            u8 myPos = raceInfo->players[i]->position;
            u16 myScore = isBattle ? raceInfo->players[i]->battleScore : 0;
            
            bool shouldCalc = (scenario->settings.gamemode > MODE_6 && scenario->settings.gamemode < MODE_AWARD);
            
            if (shouldCalc) {
                float myRating = GetPlayerRating(*scenario, i);
                
                for (u32 j = 0; j < playerCount; ++j) {
                    if (i == j) continue;
                    
                    bool win = false;
                    bool lose = false;
                    
                    if (isBattle) {
                        u16 oppScore = raceInfo->players[j]->battleScore;
                        if (oppScore < myScore) win = true;
                        else if (myScore < oppScore) lose = true;
                    } else {
                        u8 oppPos = raceInfo->players[j]->position;
                        if (myPos < oppPos) win = true;
                        else if (oppPos < myPos) lose = true;
                    }
                    
                    float oppRating = GetPlayerRating(*scenario, j);
                    
                    if (win) {
                        deltas[i] += CalcPosPoints(myRating, oppRating);
                    } else if (lose) {
                        deltas[i] += CalcNegPoints(myRating, oppRating);
                    }
                }
            }
            
            if (myPos != 0 && playerCount != 0) {
                scenario->players[i].finishPos = myPos;
                
                u16 points = 0;
                if (!isBattle && (scenario->settings.gamemode < 9 || scenario->settings.gamemode > 10)) {
                     if(playerCount <= 12 && myPos <= 12 && myPos > 0) {
                         points = Racedata::pointsRoom[playerCount-1][myPos-1];
                     }
                } else {
                    points = raceInfo->players[i]->battleScore;
                }
                scenario->players[i].score = scenario->players[i].previousScore + points;
            }
        }

        float multiplier = GetRatingMultiplier();
        for (u32 i = 0; i < playerCount; ++i) {
            deltas[i] *= multiplier;
            float oldRating = GetPlayerRating(*scenario, i);
            float next = oldRating + deltas[i];
            if (next < (float)MinRating) next = (float)MinRating;
            if (next > (float)MaxRating) next = (float)MaxRating;
            next = (float)((int)(next * 100.0f)) / 100.0f;

            UpdatePlayerRating(*scenario, i, deltas[i]);
            lastRaceDeltas[i] = next - oldRating;
        }
    }
}
kmBranch(0x8052e950, RR_UpdatePoints);

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