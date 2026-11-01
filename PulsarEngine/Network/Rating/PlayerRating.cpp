#include <kamek.hpp>
#include <PulsarSystem.hpp>
#include <Network/Rating/PlayerRating.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/RKSYS/RKSYSMgr.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>

namespace Pulsar {
namespace PointRating {

u8 remoteDecimalVR[12][2];
float lastRaceDeltas[12];

static const s16 SPLINE_CONTROL_POINTS[5] = {0, 1, 8, 50, 125};
static const int SPLINE_BIAS = 749900;
static const float SPLINE_SCALE = 0.0000020004f;

static inline float Clamp(float val, float min, float max) {
    return (val < min) ? min : (val > max) ? max : val;
}

static float EvaluateSpline(float x) {
    float result = 0.0f;
    for (int i = -2; i <= 6; ++i) {
        int idx = (i < 0) ? 0 : (i > 4) ? 4 : i;
        float d = x - (float)i;
        if (d < 0.0f)
            d = -d;

        float w = 0.0f;
        if (d <= 1.0f) {
            w = (4.0f - 6.0f * d * d + 3.0f * d * d * d) / 6.0f;
        } else if (d < 2.0f) {
            float t = 2.0f - d;
            w = (t * t * t) / 6.0f;
        }
        result += w * (float)SPLINE_CONTROL_POINTS[idx];
    }
    return result * (100.0f / 30.0f);
}

void UpdatePoints(RacedataScenario *scenario) {
    const RKNet::Controller *controller = RKNet::Controller::sInstance;
    const bool isFroom = controller && (controller->roomType == RKNet::ROOMTYPE_FROOM_HOST || controller->roomType == RKNet::ROOMTYPE_FROOM_NONHOST);
    const bool isRankedFroom = isFroom && System::sInstance->IsContext(PULSAR_VR);
    if (!System::sInstance->IsContext(PULSAR_FFA) && ((isFroom && !isRankedFroom) || controller->roomType == RKNet::ROOMTYPE_NONE)) {
        scenario->UpdatePoints();
        return;
    }

    if (scenario->settings.gametype != GAMETYPE_DEFAULT)
        return;

    const u32 playerCount = scenario->playerCount;
    Raceinfo *raceInfo = Raceinfo::sInstance;
    const GameMode mode = scenario->settings.gamemode;
    const bool isBattle = mode == MODE_BATTLE || mode == MODE_PUBLIC_BATTLE || mode == MODE_PRIVATE_BATTLE;
    bool isRanked = scenario->settings.gamemode > MODE_6 && scenario->settings.gamemode < MODE_AWARD;
    const bool isRegionalVS = controller && (controller->roomType == RKNet::ROOMTYPE_VS_REGIONAL || controller->roomType == RKNet::ROOMTYPE_JOINING_REGIONAL);
    const bool isVR = !isBattle && ((isRegionalVS && mode == MODE_PUBLIC_VS) || isRankedFroom);
    RKSYS::Mgr *rksys = RKSYS::Mgr::sInstance;
    u32 localPlayerId = 12;
    float ratings[12];
    for (u32 i = 0; i < playerCount; ++i) {
        const RacedataPlayer &player = scenario->players[i];
        ratings[i] = player.rating.points * 100.0f;
        if (player.playerType == PLAYER_REAL_LOCAL && localPlayerId == 12) {
            localPlayerId = i;
            if (rksys) {
                if (isBattle)
                    ratings[i] = GetUserBR(rksys->curLicenseId);
                else if (isVR)
                    ratings[i] = GetUserVR(rksys->curLicenseId);
            }
        } else if (player.playerType == PLAYER_REAL_ONLINE) {
            const u8 aid = controller->aidsBelongingToPlayerIds[i];
            u32 slot = 0;
            for (u32 j = 0; j < i; ++j) {
                if (controller->aidsBelongingToPlayerIds[j] == aid)
                    ++slot;
            }
            if (slot < 2)
                ratings[i] += remoteDecimalVR[aid][slot];
        }
    }

    float deltas[12] = {};
    float totalVR = 0.0f;
    bool allDisconnected = false;
    if (isVR) {
        for (u32 i = 0; i < playerCount; ++i) {
            totalVR += ratings[i];
        }
        if (controller->subs[controller->currentSub].connectionCount <= 1) {
            allDisconnected = true;
        }
    }

    for (u32 i = 0; i < playerCount; ++i) {
        u8 myPos = raceInfo->players[i]->position;
        u16 myScore = isBattle ? raceInfo->players[i]->battleScore : 0;

        if (isRanked) {
            float myRating = ratings[i];
            for (u32 j = 0; j < playerCount; ++j) {
                if (i == j)
                    continue;
                float oppRating = ratings[j];

                const bool won = isBattle ? (u16)raceInfo->players[j]->battleScore < myScore : myPos < raceInfo->players[j]->position;
                const bool lost = isBattle ? myScore < (u16)raceInfo->players[j]->battleScore : raceInfo->players[j]->position < myPos;
                if (won) {
                    float sample = Clamp((float)SPLINE_BIAS + (oppRating - myRating), 0.0f, (float)(SPLINE_BIAS * 2));
                    float gain = Clamp(EvaluateSpline(SPLINE_SCALE * sample), 1.0f, 24.0f);
                    gain *= 0.1f + 0.9f * (1.0f - Clamp((myRating - 150000.0f) / 700000.0f, 0.0f, 1.0f));
                    if (isVR && System::sInstance->netMgr.region == 0x0A && myRating - totalVR / (float)playerCount >= 75000.0f && (totalVR - myRating) / (float)(playerCount - 1) <= 30000.0f)
                        gain *= 0.15f;
                    deltas[i] += gain;
                } else if (lost) {
                    float sample = Clamp((float)SPLINE_BIAS - (oppRating - myRating) * 16.0f, 0.0f, (float)(SPLINE_BIAS * 2));
                    deltas[i] += Clamp(-EvaluateSpline(SPLINE_SCALE * sample), -19.0f, 0.0f);
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
        float oldRating = ratings[i];
        if (isVR && oldRating < 15000.0f && deltas[i] < 0.0f) {
            float divider = oldRating <= 0.0f ? 7.5f : 7.5f - 6.5f * (oldRating / 15000.0f);
            if (divider > 1.0f)
                deltas[i] /= divider;
        }
        deltas[i] *= multiplier;
        float lossCap = oldRating >= 50000.0f ? -209.0f : -50.0f - 159.0f * ((oldRating - 15000.0f) / 35000.0f);
        if (deltas[i] < lossCap)
            deltas[i] = lossCap;

        if (isVR) {
            if (allDisconnected) {
                deltas[i] = (playerCount >= 4) ? -1.0f : 0.0f;
            } else if (deltas[i] >= -1.01f && deltas[i] < 0.0f) {
                deltas[i] = 0.0f;
            }
        }

        float next = Clamp(oldRating + deltas[i], (float)MIN_RATING, (float)MAX_RATING);
        next = (float)(int)next;  // Truncate only after applying the complete race delta.
        scenario->players[i].rating.points = (u16)(next / 100.0f);
        if (i == localPlayerId && rksys) {
            if (isBattle) {
                if ((controller && controller->roomType == RKNet::ROOMTYPE_BT_REGIONAL) || isRankedFroom)
                    SetUserBR(rksys->curLicenseId, next);
            } else if (isVR) {
                SetUserVR(rksys->curLicenseId, next);
            }
        }
        lastRaceDeltas[i] = next - oldRating;
    }
}
kmCall(0x8085cf64, UpdatePoints);
kmBranch(0x8085ea60, UpdatePoints);

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
