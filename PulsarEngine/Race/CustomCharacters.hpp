#ifndef _RACECUSTOMCHARACTERS_
#define _RACECUSTOMCHARACTERS_

#include <kamek.hpp>
#include <MarioKartWii/System/Identifiers.hpp>

class RacedataScenario;

namespace Pulsar {
namespace Race {

void RandomizeCPUCharacterTables(const RacedataScenario &scenario);
u32 GetPlayerCustomCharacterSlot(u32 playerId, CharacterId character);

}  // namespace Race
}  // namespace Pulsar

#endif
