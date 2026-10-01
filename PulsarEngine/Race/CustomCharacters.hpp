#ifndef _RACECUSTOMCHARACTERS_
#define _RACECUSTOMCHARACTERS_

#include <kamek.hpp>
#include <MarioKartWii/System/Identifiers.hpp>

class RacedataScenario;
namespace nw4r { namespace lyt { class Pane; } }

namespace Pulsar {
namespace Race {

extern u8 racePlayerSlots[12];

void RandomizeCPUCharacterTables(const RacedataScenario &scenario);
u32 GetPlayerCustomCharacterSlot(u32 playerId, CharacterId character, bool isAward = false);
void LoadCustomCharacterIcon(CharacterId character, u32 slot, nw4r::lyt::Pane *pane, nw4r::lyt::Pane *shadow0, nw4r::lyt::Pane *shadow1 = nullptr);

}  // namespace Race
}  // namespace Pulsar

#endif
