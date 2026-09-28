#include <Gamemodes/EliminationDisplay.hpp>

namespace Pulsar {
namespace EliminationDisplay {

static const u8 maxPlayers = 12;
static const u8 maxDisplayedEliminations = 4;
static const u16 displayDuration = 180;

static u8 sRecentEliminations[maxDisplayedEliminations] = {0xFF, 0xFF, 0xFF, 0xFF};
static u8 sRecentCount = 0;
static u8 sRecentRound = 0;
static u16 sTimer = 0;
static bool sBattleEliminationRecorded[maxPlayers];

static void ResetDisplay() {
    sRecentCount = 0;
    sRecentRound = 0;
    sTimer = 0;
    for (u8 i = 0; i < maxDisplayedEliminations; ++i) sRecentEliminations[i] = 0xFF;
}

static void Record(u8 playerId, bool keepNewest) {
    if (playerId >= maxPlayers) return;

    if (sRecentCount >= maxDisplayedEliminations) {
        if (!keepNewest) {
            sTimer = displayDuration;
            return;
        }
        for (u8 i = 1; i < maxDisplayedEliminations; ++i) {
            sRecentEliminations[i - 1] = sRecentEliminations[i];
        }
        sRecentCount = maxDisplayedEliminations - 1;
    }

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
    if (sTimer == 0) return;
    --sTimer;
    if (sTimer == 0) ResetDisplay();
}

void RecordRoundElimination(u8 playerId, u8 concludedRound) {
    if (playerId >= maxPlayers) return;
    if (sTimer == 0 || sRecentRound != concludedRound) {
        ResetDisplay();
        sRecentRound = concludedRound;
    }
    Record(playerId, false);
}

void TrackBattleElimination(u8 playerId, bool eliminated) {
    if (playerId >= maxPlayers) return;
    if (!eliminated) {
        sBattleEliminationRecorded[playerId] = false;
        return;
    }
    if (sBattleEliminationRecorded[playerId]) return;

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
