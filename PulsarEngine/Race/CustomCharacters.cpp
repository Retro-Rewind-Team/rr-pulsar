#include <Driver/CustomCharacters.hpp>
#include <MarioKartWii/Archive/ArchiveMgr.hpp>
#include <core/rvl/dvd/dvd.hpp>

namespace Pulsar {
namespace Race {

static s32 LoadCustomCharacters(char *path, u32 size, const char *format, const char *vehicleName, const char *teamSuffix, const char *characterName, const char *modeSuffix) {
    u32 character = 0;
    while (character < Driver::CHARACTER_COUNT && strcmp(characterName, ArchiveMgr::GetKartArchivePostfix(static_cast<CharacterId>(character))) != 0) {
        ++character;
    }
    const u32 slot = character < Driver::CHARACTER_COUNT ? Driver::selectedSlots[character] : 0;
    if (slot != 0) {
        char archivePath[0x80];
        snprintf(archivePath, sizeof(archivePath), "/Race/Kart/%s%s-%s-%u%s.szs", vehicleName, teamSuffix, characterName, slot, modeSuffix);
        if (DVD::ConvertPathToEntryNum(archivePath) >= 0)
            return snprintf(path, size, "Race/Kart/%s%s-%s-%u%s", vehicleName, teamSuffix, characterName, slot, modeSuffix);
    }
    return snprintf(path, size, format, vehicleName, teamSuffix, characterName, modeSuffix);
}
kmCall(0x80540d9c, LoadCustomCharacters);
kmCall(0x80540ef4, LoadCustomCharacters);
kmCall(0x80541048, LoadCustomCharacters);

}  // namespace Race
}  // namespace Pulsar
