#ifndef _PUL_ELIMINATION_DISPLAY_
#define _PUL_ELIMINATION_DISPLAY_

#include <kamek.hpp>

namespace Pulsar {
namespace EliminationDisplay {

extern u8 recentAttackerIds[4];
extern bool recentHits[4];

void Reset();
void ResetBattleTracking();
void Tick();
void RecordRoundElimination(u8 playerId, u8 concludedRound, u8 attackerPlayerId = 0xff, bool isHit = false);
void TrackBattleElimination(u8 playerId, bool eliminated);
u16 GetTimer();
u8 GetRecentCount();
u8 GetRecentPlayerId(u8 index);

}  // namespace EliminationDisplay
}  // namespace Pulsar

#endif
