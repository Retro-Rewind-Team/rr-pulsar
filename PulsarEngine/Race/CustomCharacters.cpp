#include <Driver/CustomCharacters.hpp>
#include <Race/CustomCharacters.hpp>
#include <MarioKartWii/Archive/ArchiveMgr.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/UI/Ctrl/CtrlRace/CtrlRace2DMap.hpp>
#include <core/egg/DVD/DvdRipper.hpp>
#include <MarioKartWii/System/Random.hpp>
#include <core/rvl/dvd/dvd.hpp>
#include <core/rvl/tpl.hpp>

namespace Pulsar {
namespace Race {

static u8 racePlayerSlots[12];
u32 GetPlayerCustomCharacterSlot(u32 playerId, CharacterId character) {
    const u32 characterId = static_cast<u32>(character);
    if (characterId >= Driver::CHARACTER_COUNT) return 0;
    if (playerId >= 12 || Racedata::sInstance == nullptr || playerId >= Racedata::sInstance->racesScenario.playerCount || Racedata::sInstance->racesScenario.players[playerId].characterId != character)
        return Driver::selectedSlots[characterId];
    return racePlayerSlots[playerId];
}

void RandomizeCPUCharacterTables(const RacedataScenario &scenario) {
    if (scenario.settings.raceNumber != 0) return;

    Random random;
    for (u32 player = 0; player < scenario.playerCount; ++player) {
        const RacedataPlayer &entry = scenario.players[player];
        const u32 character = static_cast<u32>(entry.characterId);
        if (character >= Driver::CHARACTER_COUNT) continue;
        racePlayerSlots[player] = Driver::selectedSlots[character];
        if (entry.playerType != PLAYER_CPU) continue;

        u32 slotCount = 0;
        for (u32 slot = 0; slot <= Driver::MAX_CUSTOM_CHARACTER_SLOTS; ++slot) {
            if (Driver::characterTables[character][slot]) ++slotCount;
        }
        if (slotCount == 0) continue;
        u32 selected = random.NextLimited(slotCount);
        for (u32 slot = 0; slot <= Driver::MAX_CUSTOM_CHARACTER_SLOTS; ++slot) {
            if (!Driver::characterTables[character][slot]) continue;
            if (selected == 0) {
                racePlayerSlots[player] = slot;
                break;
            }
            --selected;
        }
    }
}

static s32 LoadCustomCharactersForPlayer(char *path, u32 size, const char *format, const char *vehicleName, const char *teamSuffix, const char *characterName, const char *modeSuffix, u32 playerId) {
    u32 character = 0;
    while (character < Driver::CHARACTER_COUNT && strcmp(characterName, ArchiveMgr::GetKartArchivePostfix(static_cast<CharacterId>(character))) != 0) {
        ++character;
    }
    const u32 slot = character < Driver::CHARACTER_COUNT ? GetPlayerCustomCharacterSlot(playerId, static_cast<CharacterId>(character)) : 0;
    if (slot != 0) {
        char archivePath[0x80];
        snprintf(archivePath, sizeof(archivePath), "/Race/Kart/%s%s-%s-%u%s.szs", vehicleName, teamSuffix, characterName, slot, modeSuffix);
        if (DVD::ConvertPathToEntryNum(archivePath) >= 0)
            return snprintf(path, size, "Race/Kart/%s%s-%s-%u%s", vehicleName, teamSuffix, characterName, slot, modeSuffix);
    }
    return snprintf(path, size, format, vehicleName, teamSuffix, characterName, modeSuffix);
}

static s32 LoadCustomCharacters(char *path, u32 size, const char *format, const char *vehicleName, const char *teamSuffix, const char *characterName, const char *modeSuffix) {
    return LoadCustomCharactersForPlayer(path, size, format, vehicleName, teamSuffix, characterName, modeSuffix, 12);
}
kmCall(0x80540d9c, LoadCustomCharacters);

static s32 LoadCustomCharactersForRacePlayer(char *path, u32 size, const char *format, const char *vehicleName, const char *teamSuffix, const char *characterName, const char *modeSuffix) {
    register u32 resourceManager;
    register u32 archive;
    asm(mr resourceManager, r30;);
    asm(mr archive, r31;);
    const u32 playerId = (archive - resourceManager - 8) / 0x1c;
    return LoadCustomCharactersForPlayer(path, size, format, vehicleName, teamSuffix, characterName, modeSuffix, playerId);
}
kmCall(0x80540ef4, LoadCustomCharactersForRacePlayer);

static s32 LoadCustomCharactersForRacePlayerHolder2(char *path, u32 size, const char *format, const char *vehicleName, const char *teamSuffix, const char *characterName, const char *modeSuffix) {
    register u32 resourceManager;
    register u32 archive;
    asm(mr resourceManager, r30;);
    asm(mr archive, r31;);
    const u32 playerId = (archive - resourceManager - 0x158) / 0x1c;
    return LoadCustomCharactersForPlayer(path, size, format, vehicleName, teamSuffix, characterName, modeSuffix, playerId);
}
kmCall(0x80541048, LoadCustomCharactersForRacePlayerHolder2);

static void LoadMinimapIcon(CtrlRace2DMapCharacter *control) {
    control->CtrlRaceBase::InitSelf();

    const u32 playerId = control->playerId;
    const u32 character = static_cast<u32>(Racedata::sInstance->racesScenario.players[playerId].characterId);
    if (character >= Driver::CHARACTER_COUNT) return;

    const u32 slot = GetPlayerCustomCharacterSlot(playerId, static_cast<CharacterId>(character));
    if (slot == 0) return;

    char path[0x40];
    snprintf(path, sizeof(path), "/Race/Map/%s-%u.tpl", ArchiveMgr::GetKartArchivePostfix(static_cast<CharacterId>(character)), slot);
    if (DVD::ConvertPathToEntryNum(path) < 0) return;

    TPLPalettePtr icon = static_cast<TPLPalettePtr>(EGG::DvdRipper::LoadToMainRAM(path, nullptr, nullptr, EGG::DvdRipper::ALLOC_FROM_HEAD, 0, nullptr, nullptr));
    if (icon == nullptr) return;

    control->charaPane->GetMaterial()->GetTexMapAry()->ReplaceImage(icon);
    control->charaShadow0Pane->GetMaterial()->GetTexMapAry()->ReplaceImage(icon);
    control->charaShadow1Pane->GetMaterial()->GetTexMapAry()->ReplaceImage(icon);
}
kmCall(0x807eb22c, LoadMinimapIcon);

}  // namespace Race
}  // namespace Pulsar
