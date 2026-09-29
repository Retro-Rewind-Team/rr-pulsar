#include <Driver/CustomCharacters.hpp>
#include <MarioKartWii/Archive/ArchiveMgr.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/System/Random.hpp>
#include <core/rvl/dvd/dvd.hpp>

namespace Pulsar {
namespace Race {

void RandomizeCPUCharacterTables(const RacedataScenario &scenario) {
    static bool randomized = false;
    if (scenario.settings.raceNumber != 0) {
        randomized = false;
        return;
    }
    bool localCharacters[Driver::CHARACTER_COUNT] = {};
    bool randomizedCharacters[Driver::CHARACTER_COUNT] = {};
    for (u32 player = 0; player < scenario.playerCount; ++player) {
        const RacedataPlayer &entry = scenario.players[player];
        const u32 character = static_cast<u32>(entry.characterId);
        if (entry.playerType != PLAYER_CPU && character < Driver::CHARACTER_COUNT)
            localCharacters[character] = true;
    }
    Random random;
    for (u32 player = 0; player < scenario.playerCount; ++player) {
        const RacedataPlayer &entry = scenario.players[player];
        const u32 character = static_cast<u32>(entry.characterId);
        if (entry.playerType != PLAYER_CPU || character >= Driver::CHARACTER_COUNT ||
            localCharacters[character] || randomizedCharacters[character])
            continue;

        randomizedCharacters[character] = true;
        u32 slotCount = 0;
        for (u32 slot = 1; slot <= Driver::MAX_CUSTOM_CHARACTER_SLOTS; ++slot) {
            if (Driver::characterTables[character][slot]) ++slotCount;
        }
        if (slotCount == 0) continue;
        u32 selected = random.NextLimited(slotCount);
        for (u32 slot = 1; slot <= Driver::MAX_CUSTOM_CHARACTER_SLOTS; ++slot) {
            if (!Driver::characterTables[character][slot]) continue;
            if (selected == 0) {
                Driver::selectedSlots[character] = slot;
                break;
            }
            --selected;
        }
    }
    randomized = true;
}

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
