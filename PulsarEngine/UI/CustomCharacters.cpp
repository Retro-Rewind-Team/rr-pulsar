#include <Driver/CustomCharacters.hpp>
#include <MarioKartWii/GlobalFunctions.hpp>
#include <UI/UI.hpp>

namespace Pulsar {
namespace UI {

u32 GetCharacterNameBMGId(u32 character, bool useGenericMiiName) {
    const u8 slot = Driver::selectedSlots[character];
    if (slot != 0) {
        const u32 customBmgId = (character << 16) | BMG_CUSTOM_CHARACTER_NAME_START | slot;
        const wchar_t *customName = GetCustomMsg(customBmgId);
        if (customName != nullptr && customName[0] != L'\0') return customBmgId;
    }
    return GetCharacterBMGId(static_cast<CharacterId>(character), useGenericMiiName);
}

}  // namespace UI
}  // namespace Pulsar
