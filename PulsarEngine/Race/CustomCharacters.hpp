#ifndef _RACECUSTOMCHARACTERS_
#define _RACECUSTOMCHARACTERS_

#include <kamek.hpp>
#include <MarioKartWii/System/Identifiers.hpp>

class RacedataScenario;

namespace Pulsar {
namespace Race {

extern u8 racePlayerSlots[12];

void RandomizeCPUCharacterTables(const RacedataScenario &scenario);
u32 GetPlayerCustomCharacterSlot(u32 playerId, CharacterId character);

}  // namespace Race
}  // namespace Pulsar

#endif
