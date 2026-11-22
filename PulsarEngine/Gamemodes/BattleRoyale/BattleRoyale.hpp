#ifndef _PUL_BATTLE_ROYALE_
#define _PUL_BATTLE_ROYALE_

#include <kamek.hpp>

class RaceBalloonManager;

namespace Pulsar {
namespace Network {
struct PulRH1;
}

namespace BattleRoyale {

bool ShouldApplyBattleRoyale();
void AddBalloons(RaceBalloonManager *mgr, u8 playerId, u8 count);
void WriteRH1Packet(Network::PulRH1 &packet);
void EjectItemsFromItemDamage(u8 playerId);

}  // namespace BattleRoyale
}  // namespace Pulsar

#endif
