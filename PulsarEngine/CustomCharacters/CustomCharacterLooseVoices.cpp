#include <CustomCharacters/CustomCharacters.hpp>
#include <core/rvl/OS/OSBootInfo.hpp>

namespace Pulsar {
namespace CustomCharacters {

struct DiscFSTEntry {
    u32 typeName;
    u32 offset;
    u32 size;
};

static bool FSTEntryIsDir(const DiscFSTEntry& entry) {
    return (entry.typeName & 0xff000000) != 0;
}

static u32 FSTNameOffset(const DiscFSTEntry& entry) {
    return entry.typeName & 0x00ffffff;
}

static char ToUpperAscii(char c) {
    if (c >= 'a' && c <= 'z') return static_cast<char>(c - 'a' + 'A');
    return c;
}

static bool EqualIgnoreCase(char left, char right) {
    return ToUpperAscii(left) == ToUpperAscii(right);
}

static bool EqualIgnoreCaseN(const char* left, const char* right, u32 count) {
    if (left == nullptr || right == nullptr) return false;
    for (u32 i = 0; i < count; ++i) {
        if (!EqualIgnoreCase(left[i], right[i])) return false;
    }
    return true;
}

static bool EqualIgnoreCaseString(const char* left, const char* right) {
    if (left == nullptr || right == nullptr) return false;
    while (*left != '\0' && *right != '\0') {
        if (!EqualIgnoreCase(*left, *right)) return false;
        ++left;
        ++right;
    }
    return *left == '\0' && *right == '\0';
}

static bool StartsWithIgnoreCase(const char* str, const char* prefix) {
    if (str == nullptr || prefix == nullptr) return false;
    while (*prefix != '\0') {
        if (!EqualIgnoreCase(*str, *prefix)) return false;
        ++str;
        ++prefix;
    }
    return true;
}

// Loose voice files use upper-case character postfixes in GRP_VO filenames.
void CopyUpperPostfix(char* dest, u32 destSize, const char* postfix) {
    if (dest == nullptr || destSize == 0) return;
    u32 i = 0;
    if (postfix != nullptr) {
        for (; i + 1 < destSize && postfix[i] != '\0'; ++i) {
            char c = postfix[i];
            if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
            dest[i] = c;
        }
    }
    dest[i] = '\0';
}

bool BuildLooseVoicePath(const char* postfix, const char* suffix, const char* extension, const char* voiceName, char* path,
                         u32 pathSize) {
    char upperPostfix[32];
    CopyUpperPostfix(upperPostfix, sizeof(upperPostfix), postfix);
    if (upperPostfix[0] == '\0' || suffix == nullptr || extension == nullptr) return false;
    const int written = voiceName == nullptr ? snprintf(path, pathSize, "/sound/GRP_VO_%s_%s.%s", upperPostfix, suffix, extension)
                                             : snprintf(path, pathSize, "/sound/GRP_VO_%s_%s.%s.%s", upperPostfix, suffix,
                                                        extension, voiceName);
    return written > 0 && static_cast<u32>(written) < pathSize;
}

bool LooseVoiceFileExists(const char* postfix, const char* suffix, const char* extension, const char* voiceName) {
    char path[0x80];
    if (!BuildLooseVoicePath(postfix, suffix, extension, voiceName, path, sizeof(path))) return false;
    const s32 entryNum = DVD::ConvertPathToEntryNum(path);
    if (entryNum < 0) return false;

    DVD::FileInfo info;
    if (!DVD::FastOpen(entryNum, &info)) return false;
    const bool exists = info.length != 0;
    DVD::Close(&info);
    return exists;
}

const char* const looseVoiceGroupSuffixes[] = {
    "PC",      "NPC",      "CAN_PC",  "CAN_NPC", "GOL_TOP", "GOL_TOP2", "GOL_TOP3",
    "GOL_GOD", "GOL_GOD2", "GOL_GOD3", "GOL_BAD", "GOL_BAD2", "GOL_BAD3",
};

const char* const looseVoiceTimeAttackGroupSuffixAliases[] = {
    "GOL_TOP", "GOL_TOP2", "GOL_TOP3", "GOL_BAD", "GOL_BAD2", "GOL_BAD3", "GOL_BAD3",
};

const char* LooseVoiceSuffixForGroupOffset(u32 offset) {
    if (offset < ARRAY_COUNT(looseVoiceGroupSuffixes)) return looseVoiceGroupSuffixes[offset];
    const u32 taOffset = offset - ARRAY_COUNT(looseVoiceGroupSuffixes);
    if (taOffset < ARRAY_COUNT(looseVoiceTimeAttackGroupSuffixAliases)) return looseVoiceTimeAttackGroupSuffixAliases[taOffset];
    return nullptr;
}

const u32 SILENT_VOICE_GROUP = 0xffffffff;

bool LooseVoiceStemExistsForCharacter(const char* postfix, const char* suffix, CharacterId character);

const VoiceGroupBase voiceGroupBases[] = {
    {MARIO, BRSAR_GROUP_MARIO},
    {BABY_PEACH, BRSAR_GROUP_BABY_PEACH},
    {WALUIGI, BRSAR_GROUP_WALUIGI},
    {BOWSER, BRSAR_GROUP_BOWSER},
    {BABY_DAISY, BRSAR_GROUP_BABY_DAISY},
    {DRY_BONES, BRSAR_GROUP_DRY_BONES},
    {BABY_MARIO, BRSAR_GROUP_BABY_MARIO},
    {LUIGI, BRSAR_GROUP_LUIGI},
    {TOAD, BRSAR_GROUP_TOAD},
    {DONKEY_KONG, BRSAR_GROUP_DONKEY_KONG},
    {YOSHI, BRSAR_GROUP_YOSHI},
    {WARIO, BRSAR_GROUP_WARIO},
    {BABY_LUIGI, BRSAR_GROUP_BABY_LUIGI},
    {TOADETTE, BRSAR_GROUP_TOADETTE},
    {KOOPA_TROOPA, BRSAR_GROUP_KOOPA_TROOPA},
    {DAISY, BRSAR_GROUP_DAISY},
    {PEACH, BRSAR_GROUP_PEACH},
    {BIRDO, BRSAR_GROUP_BIRDO},
    {DIDDY_KONG, BRSAR_GROUP_DIDDY_KONG},
    {KING_BOO, BRSAR_GROUP_KING_BOO},
    {BOWSER_JR, BRSAR_GROUP_BOWSER_JR},
    {DRY_BOWSER, BRSAR_GROUP_DRY_BOWSER},
    {FUNKY_KONG, BRSAR_GROUP_FUNKY_KONG},
    {ROSALINA, BRSAR_GROUP_ROSALINA},
};

const CharacterNameMap voiceCharacterNames[] = {
    {"MARIO", MARIO},
    {"BABY_PEACH", BABY_PEACH},
    {"WALUIGI", WALUIGI},
    {"BOWSER", BOWSER},
    {"BABY_DAISY", BABY_DAISY},
    {"DRY_BONES", DRY_BONES},
    {"BABY_MARIO", BABY_MARIO},
    {"LUIGI", LUIGI},
    {"TOAD", TOAD},
    {"DONKEY_KONG", DONKEY_KONG},
    {"YOSHI", YOSHI},
    {"WARIO", WARIO},
    {"BABY_LUIGI", BABY_LUIGI},
    {"TOADETTE", TOADETTE},
    {"KOOPA_TROOPA", KOOPA_TROOPA},
    {"DAISY", DAISY},
    {"PEACH", PEACH},
    {"BIRDO", BIRDO},
    {"DIDDY_KONG", DIDDY_KONG},
    {"KING_BOO", KING_BOO},
    {"BOWSER_JR", BOWSER_JR},
    {"DRY_BOWSER", DRY_BOWSER},
    {"FUNKY_KONG", FUNKY_KONG},
    {"ROSALINA", ROSALINA},
};

const char* VoiceNameForCharacter(CharacterId character) {
    for (u32 i = 0; i < ARRAY_COUNT(voiceCharacterNames); ++i) {
        if (voiceCharacterNames[i].character == character) return voiceCharacterNames[i].name;
    }
    return nullptr;
}

const char* VoicePostfixNameForCharacter(CharacterId character) {
    const char* postfix = GetDefaultCharacterPostfix(character);
    return postfix != nullptr ? postfix : VoiceNameForCharacter(character);
}

bool LooseVoiceStemExists(const char* postfix, const char* suffix, const char* voiceName) {
    return LooseVoiceFileExists(postfix, suffix, "brwsd", voiceName) || LooseVoiceFileExists(postfix, suffix, "brbnk", voiceName);
}

// A .silent marker suppresses voice groups only when no loose voices exist.
bool SilentVoiceMarkerExists(CharacterId character, u8 table, const char* postfix) {
    if (table == TABLE_DEFAULT || table >= TABLE_COUNT || !IsCharacter(character)) return false;
    if (postfix == nullptr) return false;
    char path[0x60];
    const int written = snprintf(path, sizeof(path), "/sound/%s.silent", postfix);
    return written > 0 && static_cast<u32>(written) < sizeof(path) && DVD::ConvertPathToEntryNum(path) >= 0;
}

static bool MatchLooseVoiceSuffix(const char* suffix, u32 suffixLength, u32& suffixIndex) {
    for (u32 i = 0; i < ARRAY_COUNT(looseVoiceGroupSuffixes); ++i) {
        const char* expected = looseVoiceGroupSuffixes[i];
        if (strlen(expected) != suffixLength) continue;
        if (!EqualIgnoreCaseN(suffix, expected, suffixLength)) continue;
        suffixIndex = i;
        return true;
    }
    return false;
}

static bool IsLooseVoiceExtension(const char* extension) {
    if (extension == nullptr) return false;
    for (u32 i = 0; i < 5; ++i) {
        if (extension[i] == '\0') return false;
    }
    return EqualIgnoreCaseN(extension, "brwsd", 5) || EqualIgnoreCaseN(extension, "brbnk", 5);
}

static bool MatchLooseVoiceAlias(const char* alias, u32& characterIndex) {
    if (alias == nullptr || alias[0] == '\0') return false;
    for (u32 i = 0; i < ARRAY_COUNT(voiceCharacterNames); ++i) {
        const CharacterId character = voiceCharacterNames[i].character;
        const char* voiceName = voiceCharacterNames[i].name;
        if (EqualIgnoreCaseString(alias, voiceName)) {
            characterIndex = i;
            return true;
        }

        const char* postfixName = VoicePostfixNameForCharacter(character);
        if (postfixName == nullptr || EqualIgnoreCaseString(postfixName, voiceName)) continue;
        if (EqualIgnoreCaseString(alias, postfixName)) {
            characterIndex = i;
            return true;
        }
    }
    return false;
}

static void ApplyLooseVoiceMasks(LooseVoiceInfo& info, u32 directMask, const u32* characterMasks) {
    for (u32 suffixIndex = 0; suffixIndex < ARRAY_COUNT(looseVoiceGroupSuffixes); ++suffixIndex) {
        const u32 suffixBit = 1 << suffixIndex;
        for (u32 characterIndex = 0; characterIndex < ARRAY_COUNT(voiceCharacterNames); ++characterIndex) {
            if ((characterMasks[characterIndex] & suffixBit) == 0) continue;
            info.hasFiles = true;
            info.suffixMask |= suffixBit;
            if (!IsCharacter(info.voiceCharacter)) info.voiceCharacter = voiceCharacterNames[characterIndex].character;
            break;
        }
        if ((info.suffixMask & suffixBit) != 0) continue;

        if ((directMask & suffixBit) != 0) {
            info.hasFiles = true;
            info.suffixMask |= suffixBit;
        }
    }
}

static bool ScanLooseVoiceInfoFromDiscFST(const char* postfix, LooseVoiceInfo& info) {
    if (postfix == nullptr || OS::BootInfo::mInstance.FSTLocation == nullptr) return false;

    const DiscFSTEntry* fst = static_cast<const DiscFSTEntry*>(OS::BootInfo::mInstance.FSTLocation);
    const u32 entryCount = fst[0].size;
    const s32 soundEntryNum = DVD::ConvertPathToEntryNum("/sound");
    if (soundEntryNum < 0 || static_cast<u32>(soundEntryNum) >= entryCount) return false;

    const DiscFSTEntry& soundEntry = fst[soundEntryNum];
    if (!FSTEntryIsDir(soundEntry)) return false;
    const u32 soundEnd = soundEntry.size;
    if (soundEnd <= static_cast<u32>(soundEntryNum) || soundEnd > entryCount) return false;

    char upperPostfix[32];
    CopyUpperPostfix(upperPostfix, sizeof(upperPostfix), postfix);
    if (upperPostfix[0] == '\0') return false;

    char prefix[48];
    const int written = snprintf(prefix, sizeof(prefix), "GRP_VO_%s_", upperPostfix);
    if (written <= 0 || static_cast<u32>(written) >= sizeof(prefix)) return false;
    const u32 prefixLength = static_cast<u32>(written);

    u32 directMask = 0;
    u32 characterMasks[ARRAY_COUNT(voiceCharacterNames)];
    for (u32 i = 0; i < ARRAY_COUNT(characterMasks); ++i) characterMasks[i] = 0;

    const char* stringTable = reinterpret_cast<const char*>(fst) + (entryCount * sizeof(DiscFSTEntry));
    for (u32 entryIndex = static_cast<u32>(soundEntryNum) + 1; entryIndex < soundEnd;) {
        const DiscFSTEntry& entry = fst[entryIndex];
        if (FSTEntryIsDir(entry)) {
            if (entry.size <= entryIndex || entry.size > soundEnd) return false;
            entryIndex = entry.size;
            continue;
        }
        ++entryIndex;

        const char* fileName = stringTable + FSTNameOffset(entry);
        if (!StartsWithIgnoreCase(fileName, prefix)) continue;

        const char* suffix = fileName + prefixLength;
        const char* dot = strchr(suffix, '.');
        if (dot == nullptr || dot == suffix) continue;

        u32 suffixIndex = 0;
        if (!MatchLooseVoiceSuffix(suffix, static_cast<u32>(dot - suffix), suffixIndex)) continue;

        const char* extension = dot + 1;
        if (!IsLooseVoiceExtension(extension)) continue;

        const u32 suffixBit = 1 << suffixIndex;
        if (extension[5] == '\0') {
            directMask |= suffixBit;
            continue;
        }
        if (extension[5] != '.') continue;

        u32 characterIndex = 0;
        if (MatchLooseVoiceAlias(extension + 6, characterIndex)) characterMasks[characterIndex] |= suffixBit;
    }

    ApplyLooseVoiceMasks(info, directMask, characterMasks);
    return info.hasFiles;
}

static bool ScanLooseVoiceInfoFromPaths(const char* postfix, LooseVoiceInfo& info) {
    if (postfix == nullptr) return false;

    u32 directMask = 0;
    u32 characterMasks[ARRAY_COUNT(voiceCharacterNames)];
    for (u32 i = 0; i < ARRAY_COUNT(characterMasks); ++i) characterMasks[i] = 0;

    for (u32 suffixIndex = 0; suffixIndex < ARRAY_COUNT(looseVoiceGroupSuffixes); ++suffixIndex) {
        const char* suffix = looseVoiceGroupSuffixes[suffixIndex];
        const u32 suffixBit = 1 << suffixIndex;
        for (u32 characterIndex = 0; characterIndex < ARRAY_COUNT(voiceCharacterNames); ++characterIndex) {
            const CharacterId character = voiceCharacterNames[characterIndex].character;
            if (LooseVoiceStemExistsForCharacter(postfix, suffix, character)) {
                characterMasks[characterIndex] |= suffixBit;
            }
        }

        if (LooseVoiceStemExists(postfix, suffix)) directMask |= suffixBit;
    }

    ApplyLooseVoiceMasks(info, directMask, characterMasks);
    return info.hasFiles;
}

bool LooseVoiceStemExistsForCharacter(const char* postfix, const char* suffix, CharacterId character) {
    const char* voiceName = VoiceNameForCharacter(character);
    if (LooseVoiceStemExists(postfix, suffix, voiceName)) return true;

    const char* postfixName = VoicePostfixNameForCharacter(character);
    if (postfixName == nullptr || (voiceName != nullptr && strcmp(postfixName, voiceName) == 0)) return false;
    return LooseVoiceStemExists(postfix, suffix, postfixName);
}

const char* ExistingLooseVoiceNameForCharacter(const char* postfix, const char* suffix, CharacterId character) {
    const char* voiceName = VoiceNameForCharacter(character);
    if (LooseVoiceStemExists(postfix, suffix, voiceName)) return voiceName;

    const char* postfixName = VoicePostfixNameForCharacter(character);
    if (postfixName != nullptr && (voiceName == nullptr || strcmp(postfixName, voiceName) != 0) &&
        LooseVoiceStemExists(postfix, suffix, postfixName)) {
        return postfixName;
    }
    if (LooseVoiceStemExists(postfix, suffix)) return nullptr;
    return voiceName;
}

// Scan once per skin table to discover loose voice stems or aliases.
const LooseVoiceInfo& GetLooseVoiceInfo(CharacterId character, u8 table) {
    static const LooseVoiceInfo empty = {true, false, false, CHARACTER_NONE, 0};
    if (table == TABLE_DEFAULT || table >= TABLE_COUNT || !IsCharacter(character)) return empty;
    LooseVoiceInfo& info = looseVoiceInfo[table][character];
    if (info.scanned) return info;

    info.scanned = true;
    info.hasFiles = false;
    info.silent = false;
    info.voiceCharacter = CHARACTER_NONE;
    info.suffixMask = 0;

    const char* postfix = GeneratedCustomPostfix(character, table);
    if (postfix == nullptr) return info;
    const bool silent = SilentVoiceMarkerExists(character, table, postfix);

    ScanLooseVoiceInfoFromDiscFST(postfix, info);
    if (!info.hasFiles) ScanLooseVoiceInfoFromPaths(postfix, info);
    if (!info.hasFiles && silent) info.silent = true;
    return info;
}

bool LooseVoiceInfoHasSuffix(const LooseVoiceInfo& info, const char* suffix) {
    if (!info.hasFiles || suffix == nullptr) return false;
    for (u32 i = 0; i < ARRAY_COUNT(looseVoiceGroupSuffixes); ++i) {
        if ((info.suffixMask & (1 << i)) == 0) continue;
        if (strcmp(suffix, looseVoiceGroupSuffixes[i]) == 0) return true;
    }
    return false;
}

bool CharacterHasOnlyBaseVoiceGroup(CharacterId character) {
    return character == DRY_BONES || character == KOOPA_TROOPA || character == KING_BOO;
}

bool FindVoiceGroupBaseCharacter(u32 groupId, CharacterId& character) {
    for (u32 i = 0; i < ARRAY_COUNT(voiceGroupBases); ++i) {
        if (voiceGroupBases[i].groupId != groupId) continue;
        character = voiceGroupBases[i].character;
        return true;
    }
    return false;
}

bool FindVoiceGroupBase(CharacterId character, u32& groupId) {
    for (u32 i = 0; i < ARRAY_COUNT(voiceGroupBases); ++i) {
        if (voiceGroupBases[i].character != character) continue;
        groupId = voiceGroupBases[i].groupId;
        return true;
    }
    return false;
}

bool VoiceBaseGroupForTable(CharacterId character, u8 table, u32& groupId) {
    if (table != TABLE_DEFAULT) {
        const LooseVoiceInfo& info = GetLooseVoiceInfo(character, table);
        if (info.silent) {
            groupId = SILENT_VOICE_GROUP;
            return true;
        }
        if (IsCharacter(info.voiceCharacter)) return FindVoiceGroupBase(info.voiceCharacter, groupId);
    }
    return FindVoiceGroupBase(character, groupId);
}

bool ActorRaceCharacter(const Audio::CharacterActor* actor, CharacterId& character) {
    const Racedata* racedata = Racedata::sInstance;
    if (actor == nullptr || racedata == nullptr) return false;
    const u8 playerId = actor->playerId;
    if (playerId >= racedata->racesScenario.playerCount) return false;
    character = racedata->racesScenario.players[playerId].characterId;
    return IsCharacter(character) && !IsMiiCharacter(character);
}

// Resolve the voice group an actor should use for its selected skin.
bool VoiceBaseGroupForActor(const Audio::CharacterActor* actor, CharacterId& character, u32& groupId, CharacterId& groupCharacter) {
    if (!ActorRaceCharacter(actor, character)) return false;
    const u8 table = RaceSkinTable(actor->playerId, character);
    if (!IsLocalRacePlayer(actor->playerId) && GetLooseVoiceInfo(character, table).hasFiles) {
        groupId = SILENT_VOICE_GROUP;
        groupCharacter = CHARACTER_NONE;
        return true;
    }
    if (!VoiceBaseGroupForTable(character, table, groupId)) return false;
    if (groupId == SILENT_VOICE_GROUP) {
        groupCharacter = CHARACTER_NONE;
        return true;
    }
    if (!FindVoiceGroupBaseCharacter(groupId, groupCharacter)) return false;
    return groupCharacter != character;
}

CharacterId VoiceBaseCharacterForActor(const Audio::CharacterActor* actor) {
    CharacterId character = CHARACTER_NONE;
    CharacterId groupCharacter = CHARACTER_NONE;
    u32 groupId = 0;
    return VoiceBaseGroupForActor(actor, character, groupId, groupCharacter) ? groupCharacter : CHARACTER_NONE;
}

Audio::CharacterActor* voiceInitActor;

Audio::CharacterVoiceActionTable VoiceActionTable(CharacterId character) {
    if (!IsCharacter(character)) return nullptr;
    return Audio::CharacterActor::voiceActionTables[character];
}

void SilentVoiceActionTable(s32* type, bool isReal) {
    if (type != nullptr) *type = -1;
}

Audio::CharacterVoiceActionTable& CharacterActorVoiceActionTableSlot(Audio::CharacterActor& actor) {
    return *reinterpret_cast<Audio::CharacterVoiceActionTable*>(reinterpret_cast<u8*>(&actor) + 0x134);
}

u16& CharacterActorCharacterSlot(Audio::CharacterActor& actor) {
    return *reinterpret_cast<u16*>(reinterpret_cast<u8*>(&actor) + 0x9c);
}

bool ApplyVoiceBaseActionTable(Audio::CharacterActor* actor) {
    CharacterId character = CHARACTER_NONE;
    CharacterId groupCharacter = CHARACTER_NONE;
    u32 groupId = 0;
    if (!VoiceBaseGroupForActor(actor, character, groupId, groupCharacter)) return false;
    if (groupId == SILENT_VOICE_GROUP) {
        CharacterActorVoiceActionTableSlot(*actor) = SilentVoiceActionTable;
        return true;
    }

    Audio::CharacterVoiceActionTable table = VoiceActionTable(groupCharacter);
    if (table == nullptr) return false;
    CharacterActorVoiceActionTableSlot(*actor) = table;
    return true;
}

// Initialize ranges against the borrowed voice character, then restore actor state.
void InitCharacterVoiceRangesHook(Audio::CharacterActor* actor) {
    voiceInitActor = actor;
    const CharacterId voiceCharacter = VoiceBaseCharacterForActor(actor);
    if (!IsCharacter(voiceCharacter)) {
        actor->InitVoiceRanges();
        return;
    }

    u16& character = CharacterActorCharacterSlot(*actor);
    const u16 oldCharacter = character;
    character = static_cast<u16>(voiceCharacter);
    actor->InitVoiceRanges();
    character = oldCharacter;
    ApplyVoiceBaseActionTable(actor);
}
kmCall(0x80863ccc, InitCharacterVoiceRangesHook);

void* DriverSoundSetForLinkHook(void* manager, CharacterId character, u32 type) {
    ApplyVoiceBaseActionTable(voiceInitActor);
    const CharacterId voiceCharacter = VoiceBaseCharacterForActor(voiceInitActor);
    if (IsCharacter(voiceCharacter)) {
        CharacterId actorCharacter = CHARACTER_NONE;
        if (ActorRaceCharacter(voiceInitActor, actorCharacter) && character == actorCharacter) character = voiceCharacter;
    }
    return static_cast<Audio::DriverSoundManager*>(manager)->GetCharacterVoiceSoundSet(character, type);
}
kmCall(0x80863dd8, DriverSoundSetForLinkHook);

// Main race voice groups can borrow a base character or return the silent marker.
u32 CharacterVoiceGroupHook(Audio::CharacterActor* actor) {
    ApplyVoiceBaseActionTable(actor);
    CharacterId character = CHARACTER_NONE;
    CharacterId groupCharacter = CHARACTER_NONE;
    u32 groupId = 0;
    if (VoiceBaseGroupForActor(actor, character, groupId, groupCharacter)) {
        if (groupId == SILENT_VOICE_GROUP) return SILENT_VOICE_GROUP;
        if (!actor->isLocal && !CharacterHasOnlyBaseVoiceGroup(groupCharacter)) ++groupId;
        return groupId;
    }
    return actor->GetCharacterGroupId();
}
kmCall(0x80716224, CharacterVoiceGroupHook);

u32 CharacterCannonVoiceGroupHook(Audio::CharacterActor* actor) {
    ApplyVoiceBaseActionTable(actor);
    CharacterId character = CHARACTER_NONE;
    CharacterId groupCharacter = CHARACTER_NONE;
    u32 groupId = 0;
    if (VoiceBaseGroupForActor(actor, character, groupId, groupCharacter)) {
        if (groupId == SILENT_VOICE_GROUP) return SILENT_VOICE_GROUP;
        if (CharacterHasOnlyBaseVoiceGroup(groupCharacter)) return 0xffffffff;
        return groupId + (actor->isLocal ? 2 : 3);
    }
    return actor->GetCharacterCannonGroupId();
}
kmCall(0x80716280, CharacterCannonVoiceGroupHook);

u32 CharacterGoalVoiceGroupHook(Audio::CharacterActor* actor, u32 type) {
    ApplyVoiceBaseActionTable(actor);
    CharacterId character = CHARACTER_NONE;
    CharacterId groupCharacter = CHARACTER_NONE;
    u32 groupId = 0;
    if (!VoiceBaseGroupForActor(actor, character, groupId, groupCharacter)) {
        return actor->GetCharacterGoalGroupId(type);
    }
    if (groupId == SILENT_VOICE_GROUP) return SILENT_VOICE_GROUP;
    u16& actorCharacter = CharacterActorCharacterSlot(*actor);
    const u16 oldCharacter = actorCharacter;
    actorCharacter = static_cast<u16>(groupCharacter);
    const u32 group = actor->GetCharacterGoalGroupId(type);
    actorCharacter = oldCharacter;
    return group;
}
kmCall(0x80716254, CharacterGoalVoiceGroupHook);

// Reverse map a vanilla group id back to base character plus group offset.
bool FindVoiceGroup(u32 groupId, CharacterId& character, u32& offset) {
    for (u32 i = 0; i < ARRAY_COUNT(voiceGroupBases); ++i) {
        if (voiceGroupBases[i].groupId == groupId) {
            character = voiceGroupBases[i].character;
            offset = 0;
            return true;
        }
    }
    for (u32 i = 0; i < ARRAY_COUNT(voiceGroupBases); ++i) {
        const u32 base = voiceGroupBases[i].groupId;
        if (groupId <= base) continue;
        const u32 candidateOffset = groupId - base;
        if (candidateOffset >= ARRAY_COUNT(looseVoiceGroupSuffixes)) continue;
        if (CharacterHasOnlyBaseVoiceGroup(voiceGroupBases[i].character)) continue;
        character = voiceGroupBases[i].character;
        offset = candidateOffset;
        return true;
    }
    for (u32 i = 0; i < ARRAY_COUNT(voiceGroupBases); ++i) {
        const u32 base = voiceGroupBases[i].groupId;
        if (groupId <= base) continue;
        const u32 candidateOffset = groupId - base;
        const u32 taOffset = candidateOffset - ARRAY_COUNT(looseVoiceGroupSuffixes);
        if (taOffset >= ARRAY_COUNT(looseVoiceTimeAttackGroupSuffixAliases)) continue;
        if (CharacterHasOnlyBaseVoiceGroup(voiceGroupBases[i].character)) continue;
        character = voiceGroupBases[i].character;
        offset = candidateOffset;
        return true;
    }
    return false;
}

bool PlayerMatchesVoiceGroupOffset(u8 playerId, u32 offset) {
    const bool npcGroup = offset == 1 || offset == 3;
    return !npcGroup && IsLocalRacePlayer(playerId);
}

// BRSAR load hooks ask for the loose postfix that owns the requested group.
const char* GetLooseVoicePostfixForGroup(u32 groupId, const char*& groupSuffix, const char*& voiceName) {
    groupSuffix = nullptr;
    voiceName = nullptr;
    CharacterId groupCharacter = CHARACTER_NONE;
    u32 groupOffset = 0;
    if (!FindVoiceGroup(groupId, groupCharacter, groupOffset)) return nullptr;
    groupSuffix = LooseVoiceSuffixForGroupOffset(groupOffset);
    if (groupSuffix == nullptr) return nullptr;
    voiceName = VoiceNameForCharacter(groupCharacter);

    const Racedata* racedata = Racedata::sInstance;
    if (racedata == nullptr) return nullptr;
    const RacedataScenario& scenario = racedata->racesScenario;
    const u32 groupBaseId = groupId - groupOffset;
    for (u8 playerId = 0; playerId < scenario.playerCount && playerId < ONLINE_PLAYER_COUNT; ++playerId) {
        const RacedataPlayer& player = scenario.players[playerId];
        if (!PlayerMatchesVoiceGroupOffset(playerId, groupOffset)) continue;
        const CharacterId character = player.characterId;
        const u8 table = RaceSkinTable(playerId, character);
        u32 playerGroupBaseId = 0;
        if (!VoiceBaseGroupForTable(character, table, playerGroupBaseId) || playerGroupBaseId != groupBaseId) continue;
        if (!LooseVoiceInfoHasSuffix(GetLooseVoiceInfo(character, table), groupSuffix)) continue;
        const char* postfix = GeneratedCustomPostfix(character, table);
        if (postfix != nullptr) {
            voiceName = ExistingLooseVoiceNameForCharacter(postfix, groupSuffix, groupCharacter);
            return postfix;
        }
    }
    return nullptr;
}

}  // namespace CustomCharacters
}  // namespace Pulsar
