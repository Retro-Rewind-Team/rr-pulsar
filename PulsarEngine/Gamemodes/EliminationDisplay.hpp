#ifndef _PUL_ELIMINATION_DISPLAY_
#define _PUL_ELIMINATION_DISPLAY_

#include <kamek.hpp>

namespace Pulsar {
namespace EliminationDisplay {

void Reset();
void ResetBattleTracking();
void Tick();
void RecordRoundElimination(u8 playerId, u8 concludedRound);
void TrackBattleElimination(u8 playerId, bool eliminated);
u16 GetTimer();
u8 GetRecentCount();
u8 GetRecentPlayerId(u8 index);

}  // namespace EliminationDisplay
}  // namespace Pulsar

#endif
