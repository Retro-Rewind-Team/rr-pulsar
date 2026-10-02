#include <Driver/CustomCharacters.hpp>
#include <IO/LooseArchiveOverrides.hpp>
#include <Race/CustomCharacters.hpp>
#include <Settings/Settings.hpp>
#include <MarioKartWii/Archive/ArchiveMgr.hpp>
#include <MarioKartWii/3D/Model/ModelDirector.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/Scene/GameScene.hpp>
#include <MarioKartWii/UI/Ctrl/CtrlRace/CtrlRace2DMap.hpp>
#include <core/egg/DVD/DvdRipper.hpp>
#include <MarioKartWii/System/Random.hpp>
#include <core/rvl/dvd/dvd.hpp>
#include <core/rvl/tpl.hpp>

namespace Pulsar {
namespace Race {

u8 racePlayerSlots[12];
u32 GetPlayerCustomCharacterSlot(u32 playerId, CharacterId character, bool isAward) {
    const u32 characterId = static_cast<u32>(character);
    if (characterId >= Driver::CHARACTER_COUNT)
        return 0;
    if (playerId >= 12 || Racedata::sInstance == nullptr)
        return Driver::selectedSlots[characterId];
    const RacedataScenario &scenario = isAward ? Racedata::sInstance->awardScenario : Racedata::sInstance->racesScenario;
    if (playerId >= scenario.playerCount || scenario.players[playerId].characterId != character)
        return Driver::selectedSlots[characterId];
    const RacedataPlayer &player = scenario.players[playerId];
    if (player.playerType == PLAYER_GHOST) {
        const u8 offset = Racedata::sInstance->racesScenario.players[0].playerType != PLAYER_GHOST ? 1 : 0;
        const int rkgIndex = static_cast<int>(playerId) - offset;
        if (rkgIndex < 0 || rkgIndex >= 2)
            return 0;
        const u32 slot = Racedata::sInstance->ghosts[rkgIndex].header.unknown_6;
        return slot <= Driver::MAX_CUSTOM_CHARACTER_SLOTS && Driver::characterTables[characterId][slot] ? slot : 0;
    }
    if (player.playerType != PLAYER_REAL_LOCAL && Settings::Mgr::Get().GetSettingValue(Settings::SETTING_DISPLAYCUSTOMSKINS) == DISPLAYCUSTOMSKINS_DISABLED)
        return 0;
    const u32 slot = racePlayerSlots[playerId];
    return slot <= Driver::MAX_CUSTOM_CHARACTER_SLOTS && Driver::characterTables[characterId][slot] ? slot : 0;
}

void RandomizeCPUCharacterTables(const RacedataScenario &scenario) {
    Random random;
    for (u32 player = 0; player < scenario.playerCount; ++player) {
        const RacedataPlayer &entry = scenario.players[player];
        const u32 character = static_cast<u32>(entry.characterId);
        if (character >= Driver::CHARACTER_COUNT)
            continue;
        if (entry.playerType == PLAYER_REAL_ONLINE)
            continue;
        if (entry.playerType != PLAYER_CPU) {
            racePlayerSlots[player] = Driver::selectedSlots[character];
            continue;
        }
        if (scenario.settings.raceNumber != 0)
            continue;
        racePlayerSlots[player] = 0;

        u32 slotCount = 0;
        for (u32 slot = 0; slot <= Driver::MAX_CUSTOM_CHARACTER_SLOTS; ++slot) {
            if (Driver::characterTables[character][slot])
                ++slotCount;
        }
        if (slotCount == 0)
            continue;
        u32 selected = random.NextLimited(slotCount);
        for (u32 slot = 0; slot <= Driver::MAX_CUSTOM_CHARACTER_SLOTS; ++slot) {
            if (!Driver::characterTables[character][slot])
                continue;
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
        if (IOOverrides::ConvertPathToEntryNumWithLooseOverride(archivePath) >= 0)
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

static void BindAwardCharacterBRRES(g3d::ResFile &file, ArchiveSource source, const char *name) {
    register const CharacterId *character;
    register const Mii *mii;
    asm(mr character, r23;);
    asm(mr mii, r17;);
    if (static_cast<u32>(*character) < Driver::CHARACTER_COUNT) {
        const RacedataScenario &scenario = Racedata::sInstance->awardScenario;
        for (u32 player = 0; player < 12; ++player) {
            if (scenario.players[player].playerType == PLAYER_NONE || scenario.players[player].characterId != *character)
                continue;
            // Award models use spare character IDs for duplicates; r17 identifies their original player.
            if (mii != nullptr && mii != &scenario.players[player].mii)
                continue;
            const u32 slot = GetPlayerCustomCharacterSlot(player, *character, true);
            if (slot != 0) {
                char path[0x80];
                snprintf(path, sizeof(path), "/Scene/Model/Driver/%s-%u.brres", ArchiveMgr::GetKartArchivePostfix(*character), slot);
                if (IOOverrides::ConvertPathToEntryNumWithLooseOverride(path) >= 0) {
                    ModelDirector::RipAndBindBRRES(file, path, static_cast<EGG::ExpHeap *>(ScnMgr::sInstance[0]->curHeap), true);
                    return;
                }
            }
            break;
        }
    }
    ModelDirector::BindBRRES(file, source, name);
}
kmCall(0x80789728, BindAwardCharacterBRRES);

static bool LinkCustomAwardAnimations(ModelDirector *model, g3d::ResFile &file) {
    if (file.GetResAnmChr("sel_wait").data == nullptr)
        return false;
    for (u32 id = 0; id < 6; ++id) {
        const AnmType type = id % 3 == 0 ? ANMTYPE_CHR : id % 3 == 1 ? ANMTYPE_TEXPAT : ANMTYPE_TEXSRT;
        const bool exists =
          type == ANMTYPE_CHR || (type == ANMTYPE_TEXPAT && file.GetResAnmTexPat("sel_wait").data != nullptr) || (type == ANMTYPE_TEXSRT && file.GetResAnmTexSrt("sel_wait").data != nullptr);
        if (exists)
            model->LinkAnimation(id, file, "sel_wait", type, false, nullptr, ARCHIVE_HOLDER_KART, 0);
        else
            model->LinkEmptyAnm(id);
    }
    return true;
}

// Replace the six award animation bindings only for driver BRRES files containing sel_wait.
extern "C" g3d::ResAnmChr GetResAnmChr__Q34nw4r3g3d7ResFileCFPCc(const g3d::ResFile *file, const char *name);
static asmFunc LinkAwardAnimations() {
    ASM(
        nofralloc;
        stwu r1, -0x20(r1);
        mflr r0;
        stw r0, 0x24(r1);
        stw r3, 0x8(r1);
        stw r4, 0xc(r1);
        mr r4, r3;
        mr r3, r15;
        bl LinkCustomAwardAnimations;
        cmpwi r3, 0;
        beq original;
        lwz r12, 0x24(r1);
        addi r12, r12, 0x23c;
        mtlr r12;
        addi r1, r1, 0x20;
        blr;
    original:
        lwz r3, 0x8(r1);
        lwz r4, 0xc(r1);
        lwz r0, 0x24(r1);
        mtlr r0;
        addi r1, r1, 0x20;
        b GetResAnmChr__Q34nw4r3g3d7ResFileCFPCc;
    )
}
kmCall(0x807897e0, LinkAwardAnimations);

void LoadCustomCharacterIcon(CharacterId character, u32 slot, nw4r::lyt::Pane *pane, nw4r::lyt::Pane *shadow0, nw4r::lyt::Pane *shadow1) {
    if (static_cast<u32>(character) >= Driver::CHARACTER_COUNT || slot == 0)
        return;

    char path[0x40];
    snprintf(path, sizeof(path), "/Race/Map/%s-%u.tpl", ArchiveMgr::GetKartArchivePostfix(character), slot);
    if (IOOverrides::ConvertPathToEntryNumWithLooseOverride(path) < 0)
        return;

    // Result controls initialize after GameScene locks its dynamic heaps.
    EGG::Heap *heap = GameScene::GetCurrent()->structsHeaps.heaps[0];
    const u16 heapFlags = heap->dameFlag;
    heap->dameFlag &= ~1;
    TPLPalettePtr icon = static_cast<TPLPalettePtr>(EGG::DvdRipper::LoadToMainRAM(path, nullptr, heap, EGG::DvdRipper::ALLOC_FROM_HEAD, 0, nullptr, nullptr));
    heap->dameFlag = heapFlags;
    if (icon == nullptr)
        return;

    pane->GetMaterial()->GetTexMapAry()->ReplaceImage(icon);
    if (shadow0 != nullptr)
        shadow0->GetMaterial()->GetTexMapAry()->ReplaceImage(icon);
    if (shadow1 != nullptr)
        shadow1->GetMaterial()->GetTexMapAry()->ReplaceImage(icon);
}

}  // namespace Race
}  // namespace Pulsar
