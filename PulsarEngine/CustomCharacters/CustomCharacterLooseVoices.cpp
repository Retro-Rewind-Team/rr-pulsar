#include <CustomCharacters/CustomCharacters.hpp>

namespace Pulsar {
namespace CustomCharacters {

struct VoiceGroupBase {
    CharacterId character;
    u32 groupId;
};

struct CharacterNameMap {
    const char *name;
    CharacterId character;
};

static LooseVoiceInfo looseVoiceInfo[TABLE_COUNT][CHARACTER_COUNT];
static Audio::CharacterActor *voiceInitActor;

kmRuntimeUse(0x80866fc0);
kmRuntimeUse(0x809c4738);

typedef bool (*ShouldPlayRandomSoundFn)(void *randomMgr, u8 chancePercent);

// The vanilla picker can spin forever if the used mask/count become inconsistent
// and every selectable sound resolves back to prevSoundId.
static s32 PickRandomSoundSafe(Audio::RandomSoundPicker *picker) {
    if (picker == nullptr) return -1;
    if (picker->usedSoundCount >= static_cast<s16>(picker->soundCount)) return -1;
    void *randomMgr = *reinterpret_cast<void **>(kmRuntimeAddr(0x809c4738));
    if (randomMgr == nullptr ||
        !reinterpret_cast<ShouldPlayRandomSoundFn>(kmRuntimeAddr(0x80866fc0))(randomMgr, picker->playChancePercent)) {
        return -1;
    }

    const u32 soundCount = picker->soundCount > 32 ? 32 : picker->soundCount;
    if (soundCount == 0) return -1;

    u32 availableCount = 0;
    u32 nonPreviousCount = 0;
    s32 onlyAvailable = -1;
    s32 onlyNonPrevious = -1;

    for (u32 i = 0; i < soundCount; ++i) {
        if ((picker->usedSoundMask & (1u << i)) != 0) continue;
        const s32 soundId = static_cast<s32>(picker->initialSoundId + i);
        onlyAvailable = soundId;
        ++availableCount;
        if (soundId == static_cast<s32>(picker->prevSoundId)) continue;
        onlyNonPrevious = soundId;
        ++nonPreviousCount;
    }

    if (availableCount == 0) return -1;
    if (availableCount == 1 || nonPreviousCount == 0) return onlyAvailable;
    if (nonPreviousCount == 1) return onlyNonPrevious;

    const s32 selected = picker->NextLimited(static_cast<int>(nonPreviousCount));
    u32 index = 0;
    for (u32 i = 0; i < soundCount; ++i) {
        if ((picker->usedSoundMask & (1u << i)) != 0) continue;
        const s32 soundId = static_cast<s32>(picker->initialSoundId + i);
        if (soundId == static_cast<s32>(picker->prevSoundId)) continue;
        if (index == static_cast<u32>(selected)) return soundId;
        ++index;
    }

    return onlyNonPrevious;
}
kmBranch(0x80867194, PickRandomSoundSafe);

static bool LooseVoiceFileExists(const char *postfix, const char *suffix, const char *extension, const char *voiceName) {
    char upperPostfix[32];
    u32 i = 0;
    if (postfix != nullptr) {
        for (; i + 1 < sizeof(upperPostfix) && postfix[i] != '\0'; ++i) {
            const char c = postfix[i];
            upperPostfix[i] = c >= 'a' && c <= 'z' ? static_cast<char>(c - 'a' + 'A') : c;
        }
    }
    upperPostfix[i] = '\0';
    if (upperPostfix[0] == '\0' || suffix == nullptr || extension == nullptr) return false;

    char path[0x80];
    const int written = voiceName == nullptr ? snprintf(path, sizeof(path), "/sound/GRP_VO_%s_%s.%s", upperPostfix, suffix, extension)
                                             : snprintf(path, sizeof(path), "/sound/GRP_VO_%s_%s.%s.%s", upperPostfix, suffix,
                                                        extension, voiceName);
    if (written <= 0 || static_cast<u32>(written) >= sizeof(path)) return false;
    const s32 entryNum = DVD::ConvertPathToEntryNum(path);
    if (entryNum < 0) return false;

    DVD::FileInfo info;
    if (!DVD::FastOpen(entryNum, &info)) return false;
    const bool exists = info.length != 0;
    DVD::Close(&info);
    return exists;
}

static const char *const looseVoiceGroupSuffixes[] = {
    "PC",
    "NPC",
    "CAN_PC",
    "CAN_NPC",
    "GOL_TOP",
    "GOL_TOP2",
    "GOL_TOP3",
    "GOL_GOD",
    "GOL_GOD2",
    "GOL_GOD3",
    "GOL_BAD",
    "GOL_BAD2",
    "GOL_BAD3",
};

static const char *const looseVoiceTimeAttackGroupSuffixAliases[] = {
    "GOL_TOP",
    "GOL_TOP2",
    "GOL_TOP3",
    "GOL_BAD",
    "GOL_BAD2",
    "GOL_BAD3",
    "GOL_BAD3",
};

const u32 SILENT_VOICE_GROUP = 0xffffffff;

static const VoiceGroupBase voiceGroupBases[] = {
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

static const CharacterNameMap voiceCharacterNames[] = {
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

static const char *VoiceNameForCharacter(CharacterId character) {
    for (u32 i = 0; i < ARRAY_COUNT(voiceCharacterNames); ++i) {
        if (voiceCharacterNames[i].character == character) return voiceCharacterNames[i].name;
    }
    return nullptr;
}

static bool LooseVoiceStemExists(const char *postfix, const char *suffix, const char *voiceName = nullptr) {
    return LooseVoiceFileExists(postfix, suffix, "brwsd", voiceName) || LooseVoiceFileExists(postfix, suffix, "brbnk", voiceName);
}

// Scan once per skin table to discover loose voice stems or aliases.
const LooseVoiceInfo &GetLooseVoiceInfo(CharacterId character, u8 table) {
    static const LooseVoiceInfo empty = {true, false, false, CHARACTER_NONE, 0};
    if (table == TABLE_DEFAULT || table >= TABLE_COUNT || !IsCharacter(character)) return empty;
    LooseVoiceInfo &info = looseVoiceInfo[table][character];
    if (info.scanned) return info;

    info.scanned = true;
    info.hasFiles = false;
    info.silent = false;
    info.voiceCharacter = CHARACTER_NONE;
    info.suffixMask = 0;

    const char *postfix = GeneratedCustomPostfix(character, table);
    if (postfix == nullptr) return info;

    char silentPath[0x60];
    const int silentPathLength = snprintf(silentPath, sizeof(silentPath), "/sound/%s.silent", postfix);
    const bool silent = silentPathLength > 0 && static_cast<u32>(silentPathLength) < sizeof(silentPath) &&
                        DVD::ConvertPathToEntryNum(silentPath) >= 0;

    for (u32 suffixIndex = 0; suffixIndex < ARRAY_COUNT(looseVoiceGroupSuffixes); ++suffixIndex) {
        const char *suffix = looseVoiceGroupSuffixes[suffixIndex];
        const u32 suffixBit = 1 << suffixIndex;
        bool foundNamedVoice = false;

        for (u32 characterIndex = 0; characterIndex < ARRAY_COUNT(voiceCharacterNames); ++characterIndex) {
            const CharacterId voiceCharacter = voiceCharacterNames[characterIndex].character;
            const char *voiceName = voiceCharacterNames[characterIndex].name;
            bool exists = LooseVoiceStemExists(postfix, suffix, voiceName);
            const char *postfixName = GetDefaultCharacterPostfix(voiceCharacter);
            if (!exists && postfixName != nullptr && strcmp(postfixName, voiceName) != 0) {
                exists = LooseVoiceStemExists(postfix, suffix, postfixName);
            }
            if (!exists) continue;

            info.hasFiles = true;
            info.suffixMask |= suffixBit;
            if (!IsCharacter(info.voiceCharacter)) info.voiceCharacter = voiceCharacter;
            foundNamedVoice = true;
            break;
        }

        if (!foundNamedVoice && LooseVoiceStemExists(postfix, suffix)) {
            info.hasFiles = true;
            info.suffixMask |= suffixBit;
        }
    }

    if (!info.hasFiles && silent) info.silent = true;
    return info;
}

void ClearLooseVoiceCache() {
    memset(looseVoiceInfo, 0, sizeof(looseVoiceInfo));
}

static bool CharacterHasOnlyBaseVoiceGroup(CharacterId character) {
    return character == DRY_BONES || character == KOOPA_TROOPA || character == KING_BOO;
}

static bool VoiceBaseGroupForTable(CharacterId character, u8 table, u32 &groupId) {
    CharacterId voiceCharacter = character;
    if (table != TABLE_DEFAULT) {
        const LooseVoiceInfo &info = GetLooseVoiceInfo(character, table);
        if (info.silent) {
            groupId = SILENT_VOICE_GROUP;
            return true;
        }
        if (IsCharacter(info.voiceCharacter)) voiceCharacter = info.voiceCharacter;
    }
    for (u32 i = 0; i < ARRAY_COUNT(voiceGroupBases); ++i) {
        if (voiceGroupBases[i].character == voiceCharacter) {
            groupId = voiceGroupBases[i].groupId;
            return true;
        }
    }
    return false;
}

// Resolve the voice group an actor should use for its selected skin.
static bool VoiceBaseGroupForActor(const Audio::CharacterActor *actor, CharacterId &character, u32 &groupId,
                                   CharacterId &groupCharacter) {
    const Racedata *racedata = Racedata::sInstance;
    if (actor == nullptr || racedata == nullptr || actor->playerId >= racedata->racesScenario.playerCount) return false;
    character = racedata->racesScenario.players[actor->playerId].characterId;
    if (!IsCharacter(character) || IsMiiCharacter(character)) return false;

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

    for (u32 i = 0; i < ARRAY_COUNT(voiceGroupBases); ++i) {
        if (voiceGroupBases[i].groupId == groupId) {
            groupCharacter = voiceGroupBases[i].character;
            return groupCharacter != character;
        }
    }
    return false;
}

static void SilentVoiceActionTable(s32 *type, bool isReal) {
    if (type != nullptr) *type = -1;
}

static bool ApplyVoiceBaseActionTable(Audio::CharacterActor *actor) {
    CharacterId character = CHARACTER_NONE;
    CharacterId groupCharacter = CHARACTER_NONE;
    u32 groupId = 0;
    if (!VoiceBaseGroupForActor(actor, character, groupId, groupCharacter)) return false;
    Audio::CharacterVoiceActionTable &slot = *reinterpret_cast<Audio::CharacterVoiceActionTable *>(reinterpret_cast<u8 *>(actor) + 0x134);
    if (groupId == SILENT_VOICE_GROUP) {
        slot = SilentVoiceActionTable;
        return true;
    }

    Audio::CharacterVoiceActionTable table = nullptr;
    if (IsCharacter(groupCharacter)) table = Audio::CharacterActor::voiceActionTables[groupCharacter];
    if (table == nullptr) return false;
    slot = table;
    return true;
}

// Initialize ranges against the borrowed voice character, then restore actor state.
void InitCharacterVoiceRangesHook(Audio::CharacterActor *actor) {
    voiceInitActor = actor;
    CharacterId character = CHARACTER_NONE;
    CharacterId voiceCharacter = CHARACTER_NONE;
    u32 groupId = 0;
    if (!VoiceBaseGroupForActor(actor, character, groupId, voiceCharacter) || !IsCharacter(voiceCharacter)) {
        actor->InitVoiceRanges();
        return;
    }

    u16 &actorCharacter = *reinterpret_cast<u16 *>(reinterpret_cast<u8 *>(actor) + 0x9c);
    const u16 oldCharacter = actorCharacter;
    actorCharacter = static_cast<u16>(voiceCharacter);
    actor->InitVoiceRanges();
    actorCharacter = oldCharacter;
    ApplyVoiceBaseActionTable(actor);
}
kmCall(0x80863ccc, InitCharacterVoiceRangesHook);

void *DriverSoundSetForLinkHook(void *manager, CharacterId character, u32 type) {
    ApplyVoiceBaseActionTable(voiceInitActor);
    CharacterId actorCharacter = CHARACTER_NONE;
    CharacterId voiceCharacter = CHARACTER_NONE;
    u32 groupId = 0;
    if (VoiceBaseGroupForActor(voiceInitActor, actorCharacter, groupId, voiceCharacter) && IsCharacter(voiceCharacter) &&
        character == actorCharacter) {
        character = voiceCharacter;
    }
    return static_cast<Audio::DriverSoundManager *>(manager)->GetCharacterVoiceSoundSet(character, type);
}
kmCall(0x80863dd8, DriverSoundSetForLinkHook);

// Main race voice groups can borrow a base character or return the silent marker.
u32 CharacterVoiceGroupHook(Audio::CharacterActor *actor) {
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

u32 CharacterCannonVoiceGroupHook(Audio::CharacterActor *actor) {
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

u32 CharacterGoalVoiceGroupHook(Audio::CharacterActor *actor, u32 type) {
    ApplyVoiceBaseActionTable(actor);
    CharacterId character = CHARACTER_NONE;
    CharacterId groupCharacter = CHARACTER_NONE;
    u32 groupId = 0;
    if (!VoiceBaseGroupForActor(actor, character, groupId, groupCharacter)) {
        return actor->GetCharacterGoalGroupId(type);
    }
    if (groupId == SILENT_VOICE_GROUP) return SILENT_VOICE_GROUP;
    u16 &actorCharacter = *reinterpret_cast<u16 *>(reinterpret_cast<u8 *>(actor) + 0x9c);
    const u16 oldCharacter = actorCharacter;
    actorCharacter = static_cast<u16>(groupCharacter);
    const u32 group = actor->GetCharacterGoalGroupId(type);
    actorCharacter = oldCharacter;
    return group;
}
kmCall(0x80716254, CharacterGoalVoiceGroupHook);

// BRSAR load hooks ask for the loose postfix that owns the requested group.
const char *GetLooseVoicePostfixForGroup(u32 groupId, const char *&groupSuffix, const char *&voiceName) {
    groupSuffix = nullptr;
    voiceName = nullptr;
    CharacterId groupCharacter = CHARACTER_NONE;
    u32 groupOffset = 0;
    bool foundGroup = false;

    for (u32 i = 0; i < ARRAY_COUNT(voiceGroupBases); ++i) {
        if (voiceGroupBases[i].groupId == groupId) {
            groupCharacter = voiceGroupBases[i].character;
            foundGroup = true;
            break;
        }
    }
    for (u32 i = 0; !foundGroup && i < ARRAY_COUNT(voiceGroupBases); ++i) {
        const u32 base = voiceGroupBases[i].groupId;
        if (groupId <= base || CharacterHasOnlyBaseVoiceGroup(voiceGroupBases[i].character)) continue;
        const u32 candidateOffset = groupId - base;
        if (candidateOffset >= ARRAY_COUNT(looseVoiceGroupSuffixes)) continue;
        groupCharacter = voiceGroupBases[i].character;
        groupOffset = candidateOffset;
        foundGroup = true;
    }
    for (u32 i = 0; !foundGroup && i < ARRAY_COUNT(voiceGroupBases); ++i) {
        const u32 base = voiceGroupBases[i].groupId;
        if (groupId <= base || CharacterHasOnlyBaseVoiceGroup(voiceGroupBases[i].character)) continue;
        const u32 candidateOffset = groupId - base;
        const u32 taOffset = candidateOffset - ARRAY_COUNT(looseVoiceGroupSuffixes);
        if (taOffset >= ARRAY_COUNT(looseVoiceTimeAttackGroupSuffixAliases)) continue;
        groupCharacter = voiceGroupBases[i].character;
        groupOffset = candidateOffset;
        foundGroup = true;
    }
    if (!foundGroup) return nullptr;

    if (groupOffset < ARRAY_COUNT(looseVoiceGroupSuffixes)) {
        groupSuffix = looseVoiceGroupSuffixes[groupOffset];
    } else {
        const u32 taOffset = groupOffset - ARRAY_COUNT(looseVoiceGroupSuffixes);
        if (taOffset < ARRAY_COUNT(looseVoiceTimeAttackGroupSuffixAliases)) groupSuffix = looseVoiceTimeAttackGroupSuffixAliases[taOffset];
    }
    if (groupSuffix == nullptr) return nullptr;
    voiceName = VoiceNameForCharacter(groupCharacter);

    const Racedata *racedata = Racedata::sInstance;
    if (racedata == nullptr) return nullptr;
    const RacedataScenario &scenario = racedata->racesScenario;
    const u32 groupBaseId = groupId - groupOffset;
    for (u8 playerId = 0; playerId < scenario.playerCount && playerId < ONLINE_PLAYER_COUNT; ++playerId) {
        const RacedataPlayer &player = scenario.players[playerId];
        if (groupOffset == 1 || groupOffset == 3 || !IsLocalRacePlayer(playerId)) continue;
        const CharacterId character = player.characterId;
        const u8 table = RaceSkinTable(playerId, character);
        u32 playerGroupBaseId = 0;
        if (!VoiceBaseGroupForTable(character, table, playerGroupBaseId) || playerGroupBaseId != groupBaseId) continue;

        const LooseVoiceInfo &info = GetLooseVoiceInfo(character, table);
        bool hasSuffix = false;
        if (info.hasFiles) {
            for (u32 i = 0; i < ARRAY_COUNT(looseVoiceGroupSuffixes); ++i) {
                if ((info.suffixMask & (1 << i)) != 0 && strcmp(groupSuffix, looseVoiceGroupSuffixes[i]) == 0) {
                    hasSuffix = true;
                    break;
                }
            }
        }
        if (!hasSuffix) continue;

        const char *postfix = GeneratedCustomPostfix(character, table);
        if (postfix != nullptr) {
            const char *postfixName = GetDefaultCharacterPostfix(groupCharacter);
            if (LooseVoiceStemExists(postfix, groupSuffix, voiceName)) {
                // Keep the vanilla voice name.
            } else if (postfixName != nullptr && (voiceName == nullptr || strcmp(postfixName, voiceName) != 0) &&
                       LooseVoiceStemExists(postfix, groupSuffix, postfixName)) {
                voiceName = postfixName;
            } else if (LooseVoiceStemExists(postfix, groupSuffix)) {
                voiceName = nullptr;
            }
            return postfix;
        }
    }
    return nullptr;
}

}  // namespace CustomCharacters
}  // namespace Pulsar
