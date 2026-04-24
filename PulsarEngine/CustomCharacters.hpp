#ifndef _PULSAR_CUSTOM_CHARACTERS_
#define _PULSAR_CUSTOM_CHARACTERS_

#include <kamek.hpp>

namespace Kart {
class Link;
}

namespace Pulsar {
namespace CustomCharacters {

void ResetOnlineCustomCharacterFlags();
void RefreshLocalOnlineCustomCharacterFlags();
void UpdateOnlineCharacterTablesFromAid(u8 aid, const u8* playerIdToAid, u8 characterTables);
u8 GetLocalOnlineCharacterTables();
bool IsCustomCharacterTableActive();
bool ShouldUseCustomCharacterForPlayer(u8 playerId);
bool ShouldMuteCharacterVoice(const Kart::Link* link);

}  // namespace CustomCharacters
}  // namespace Pulsar

#endif
