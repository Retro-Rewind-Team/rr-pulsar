#ifndef _PUL_BATTLE_ROYALE_
#define _PUL_BATTLE_ROYALE_

#include <kamek.hpp>

namespace Pulsar {
namespace Network {
struct PulRH1;
}

namespace BattleRoyale {

extern u8 eliminationAttackerIds[12];

bool ShouldApplyBattleRoyale();
void WriteRH1Packet(Network::PulRH1 &packet);

}  // namespace BattleRoyale
}  // namespace Pulsar

#endif
