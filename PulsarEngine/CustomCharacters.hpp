#ifndef _PULSAR_CUSTOM_CHARACTERS_
#define _PULSAR_CUSTOM_CHARACTERS_

#include <kamek.hpp>
#include <MarioKartWii/System/Identifiers.hpp>

namespace Kart {
class Link;
}

class LayoutUIControl;

namespace Pulsar {
namespace CustomCharacters {

void ResetOnlineCustomCharacterFlags();
void ResetAllCharacterTablesToDefault();
void RefreshLocalOnlineCustomCharacterFlags();
void RestoreVotingMenuDriverModels();
void UpdateOnlineCharacterTablesFromAid(u8 aid, const u8* playerIdToAid, u16 characterTables);
u16 GetLocalOnlineCharacterTables();
bool IsCustomCharacterTableActive();
bool ShouldUseCustomCharacterForPlayer(u8 playerId);
bool ShouldMuteCharacterVoice(const Kart::Link* link);
const char* GetLooseVoicePostfixForGroup(u32 groupId, const char*& groupSuffix);
bool SetRaceNameTextIfCustom(LayoutUIControl& control, const char* paneName, u8 playerId);
bool RandomizeSelectedCharacterTable(CharacterId character);

}  // namespace CustomCharacters
}  // namespace Pulsar

#endif
