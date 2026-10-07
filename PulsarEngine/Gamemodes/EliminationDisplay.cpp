#include <Gamemodes/EliminationDisplay.hpp>
#include <Gamemodes/BattleRoyale/BattleRoyale.hpp>
#include <Settings/Settings.hpp>

namespace Pulsar {
namespace EliminationDisplay {

static const u8 maxPlayers = 12;
static const u8 maxDisplayedEliminations = 4;
static const u16 displayDuration = 180;

static u8 sRecentEliminations[maxDisplayedEliminations] = {0xFF, 0xFF, 0xFF, 0xFF};
u8 recentAttackerIds[maxDisplayedEliminations] = {0xff, 0xff, 0xff, 0xff};
bool recentHits[maxDisplayedEliminations];
static u16 sRecentTimers[maxDisplayedEliminations];
static u8 sRecentCount = 0;
static u8 sRecentRound = 0;
static u16 sTimer = 0;
static bool sBattleEliminationRecorded[maxPlayers];

static void ResetDisplay() {
    sRecentCount = 0;
    sRecentRound = 0;
    sTimer = 0;
    for (u8 i = 0; i < maxDisplayedEliminations; ++i) {
        sRecentEliminations[i] = 0xFF;
        recentAttackerIds[i] = 0xff;
        recentHits[i] = false;
        sRecentTimers[i] = 0;
    }
}

static void Record(u8 playerId, bool keepNewest, u8 attackerPlayerId = 0xff, bool isHit = false) {
    if (playerId >= maxPlayers)
        return;

    const u8 displayCount = BattleRoyale::ShouldApplyBattleRoyale() ? 3 : maxDisplayedEliminations;
    if (sRecentCount >= displayCount) {
        if (!keepNewest) {
            sTimer = displayDuration;
            return;
        }
        for (u8 i = 1; i < displayCount; ++i) {
            sRecentEliminations[i - 1] = sRecentEliminations[i];
            recentAttackerIds[i - 1] = recentAttackerIds[i];
            recentHits[i - 1] = recentHits[i];
            sRecentTimers[i - 1] = sRecentTimers[i];
        }
        sRecentCount = displayCount - 1;
    }

    recentAttackerIds[sRecentCount] = attackerPlayerId;
    recentHits[sRecentCount] = isHit;
    sRecentTimers[sRecentCount] = displayDuration;
    sRecentEliminations[sRecentCount++] = playerId;
    sTimer = displayDuration;
}

void Reset() {
    ResetDisplay();
    ResetBattleTracking();
}

void ResetBattleTracking() {
    for (u8 i = 0; i < maxPlayers; ++i) sBattleEliminationRecorded[i] = false;
}

void Tick() {
    if (sTimer == 0)
        return;
    --sTimer;
    if (BattleRoyale::ShouldApplyBattleRoyale()) {
        for (u8 i = 0; i < sRecentCount; ++i) --sRecentTimers[i];
        while (sRecentCount != 0 && sRecentTimers[0] == 0) {
            for (u8 i = 1; i < sRecentCount; ++i) {
                sRecentEliminations[i - 1] = sRecentEliminations[i];
                recentAttackerIds[i - 1] = recentAttackerIds[i];
                recentHits[i - 1] = recentHits[i];
                sRecentTimers[i - 1] = sRecentTimers[i];
            }
            --sRecentCount;
            sRecentEliminations[sRecentCount] = 0xff;
            recentAttackerIds[sRecentCount] = 0xff;
            recentHits[sRecentCount] = false;
            sRecentTimers[sRecentCount] = 0;
        }
    }
    if (sTimer == 0)
        ResetDisplay();
}

void RecordRoundElimination(u8 playerId, u8 concludedRound, u8 attackerPlayerId, bool isHit) {
    const bool battleRoyale = BattleRoyale::ShouldApplyBattleRoyale();
    const bool everyHit = battleRoyale
      && Settings::Mgr::Get().GetSettingValue(Settings::SETTING_KOROYALEDISPLAY) == KOROYALEDISPLAY_EVERY_HIT;
    if (isHit && !everyHit)
        return;
    if (playerId >= maxPlayers)
        return;
    if (sTimer == 0 || sRecentRound != concludedRound) {
        ResetDisplay();
        sRecentRound = concludedRound;
    }
    Record(playerId, battleRoyale, attackerPlayerId, isHit);
}

void TrackBattleElimination(u8 playerId, bool eliminated) {
    if (playerId >= maxPlayers)
        return;
    if (!eliminated) {
        sBattleEliminationRecorded[playerId] = false;
        return;
    }
    if (sBattleEliminationRecorded[playerId])
        return;

    sBattleEliminationRecorded[playerId] = true;
    Record(playerId, true);
}

u16 GetTimer() {
    return sTimer;
}

u8 GetRecentCount() {
    return sRecentCount;
}

u8 GetRecentPlayerId(u8 index) {
    return index < sRecentCount ? sRecentEliminations[index] : 0xFF;
}

}  // namespace EliminationDisplay
}  // namespace Pulsar
