#include <Driver/CustomCharacters.hpp>
#include <MarioKartWii/GlobalFunctions.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <Race/CustomCharacters.hpp>
#include <UI/UI.hpp>

namespace Pulsar {
namespace UI {

u32 GetCharacterNameBMGId(u32 character, bool useGenericMiiName, u32 playerId) {
    if (character < Driver::CHARACTER_COUNT &&
        (playerId >= 12 || Racedata::sInstance->racesScenario.settings.gamemode < MODE_PRIVATE_VS ||
         Racedata::sInstance->racesScenario.settings.gamemode > MODE_PRIVATE_BATTLE)) {
        const u32 slot = Race::GetPlayerCustomCharacterSlot(playerId, static_cast<CharacterId>(character));
        if (slot != 0) {
            const u32 customBmgId = (character << 16) | BMG_CUSTOM_CHARACTER_NAME_START | slot;
            const wchar_t *customName = GetCustomMsg(customBmgId);
            if (customName != nullptr && customName[0] != L'\0') return customBmgId;
        }
    }
    return GetCharacterBMGId(static_cast<CharacterId>(character), useGenericMiiName);
}

u32 GetCharacterAuthorBMGId(u32 character, u32 slot) {
    if (character >= Driver::CHARACTER_COUNT || slot == 0 || slot > Driver::MAX_CUSTOM_CHARACTER_SLOTS) return 0;
    return (character << 16) | BMG_CUSTOM_CHARACTER_AUTHOR_START | slot;
}

bool SetCustomCharacterAuthorMessage(LayoutUIControl &control, u32 bmgId) {
    const wchar_t *author = GetCustomMsg(bmgId);
    if (author == nullptr || author[0] == L'\0') return false;
    control.SetMessage(bmgId, nullptr);
    return true;
}

static u32 GetNameBalloonCharacterNameBMGId(u32 character, bool useGenericMiiName) {
    register u32 playerId;
    asm(mr playerId, r30;);
    return GetCharacterNameBMGId(character, useGenericMiiName, playerId);
}
kmCall(0x807f056c, GetNameBalloonCharacterNameBMGId);
kmCall(0x807f0694, GetNameBalloonCharacterNameBMGId);

static u32 GetRaceResultCharacterNameBMGId(u32 character, bool useGenericMiiName) {
    register u32 playerId;
    asm(mr playerId, r31;);
    return GetCharacterNameBMGId(character, useGenericMiiName, playerId);
}
kmCall(0x807f53cc, GetRaceResultCharacterNameBMGId);

static u32 GetTeamResultCharacterNameBMGId(u32 character, bool useGenericMiiName) {
    register u32 playerId;
    asm(mr playerId, r19;);
    return GetCharacterNameBMGId(character, useGenericMiiName, playerId);
}
kmCall(0x807f6dfc, GetTeamResultCharacterNameBMGId);

}  // namespace UI
}  // namespace Pulsar
