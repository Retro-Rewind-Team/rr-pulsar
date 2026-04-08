#ifndef _PULSAR_CUSTOM_CHARACTERS_
#define _PULSAR_CUSTOM_CHARACTERS_

#include <kamek.hpp>

namespace Pulsar {
namespace CustomCharacters {

void ResetOnlineCustomCharacterFlags();
void RefreshLocalOnlineCustomCharacterFlags();
void UpdateOnlineCustomCharacterFlagsFromAid(u8 aid, const u8* playerIdToAid, u8 customCharacterFlags);
u8 GetLocalOnlineCustomCharacterFlags();
bool ShouldUseCustomCharacterForPlayer(u8 playerId);

}  // namespace CustomCharacters
}  // namespace Pulsar

#endif
