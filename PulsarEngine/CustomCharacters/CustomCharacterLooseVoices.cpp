#include <CustomCharacters/CustomCharacterInternal.hpp>

namespace Pulsar {
namespace CustomCharacters {

bool ShouldMuteCharacterVoice(const Kart::Link*) {
    return false;
}

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
    u32 fileSize = 0;
    return DiscFileSize(path, fileSize);
}

extern const char* const looseVoiceGroupSuffixes[] = {
    "PC",      "NPC",      "CAN_PC",  "CAN_NPC", "GOL_TOP", "GOL_TOP2", "GOL_TOP3",
    "GOL_GOD", "GOL_GOD2", "GOL_GOD3", "GOL_BAD", "GOL_BAD2", "GOL_BAD3",
};

extern const char* const looseVoiceTimeAttackGroupSuffixAliases[] = {
    "GOL_TOP", "GOL_TOP2", "GOL_TOP3", "GOL_BAD", "GOL_BAD2", "GOL_BAD3", "GOL_BAD3",
};

const char* LooseVoiceSuffixForGroupOffset(u32 offset) {
    if (offset < ARRAY_COUNT(looseVoiceGroupSuffixes)) return looseVoiceGroupSuffixes[offset];
    const u32 taOffset = offset - ARRAY_COUNT(looseVoiceGroupSuffixes);
    if (taOffset < ARRAY_COUNT(looseVoiceTimeAttackGroupSuffixAliases)) return looseVoiceTimeAttackGroupSuffixAliases[taOffset];
    return nullptr;
}

const u32 SILENT_VOICE_GROUP = 0xffffffff;

extern const VoiceGroupBase voiceGroupBases[] = {
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

extern const CharacterNameMap voiceCharacterNames[] = {
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

bool LooseVoiceStemExists(const char* postfix, const char* suffix, const char* voiceName) {
    return LooseVoiceFileExists(postfix, suffix, "brwsd", voiceName) || LooseVoiceFileExists(postfix, suffix, "brbnk", voiceName);
}

bool SilentVoiceMarkerExists(CharacterId character, u8 table, const char* postfix) {
    if (table == TABLE_DEFAULT || table >= TABLE_COUNT || !IsCharacter(character)) return false;
    if (postfix == nullptr) return false;
    char path[0x60];
    const int written = snprintf(path, sizeof(path), "/sound/%s.silent", postfix);
    if (written > 0 && static_cast<u32>(written) < sizeof(path)) {
        DVD::FileInfo info;
        if (DVD::Open(path, &info)) {
            DVD::Close(&info);
            return true;
        }
    }
    return false;
}

const char* VoiceNameForCharacter(CharacterId character) {
    for (u32 i = 0; i < ARRAY_COUNT(voiceCharacterNames); ++i) {
        if (voiceCharacterNames[i].character == character) return voiceCharacterNames[i].name;
    }
    return nullptr;
}

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
    if (SilentVoiceMarkerExists(character, table, postfix)) {
        info.silent = true;
        return info;
    }

    for (u32 suffixIndex = 0; suffixIndex < ARRAY_COUNT(looseVoiceGroupSuffixes); ++suffixIndex) {
        const char* suffix = looseVoiceGroupSuffixes[suffixIndex];
        if (LooseVoiceStemExists(postfix, suffix)) {
            info.hasFiles = true;
            info.suffixMask |= 1 << suffixIndex;
            continue;
        }
        for (u32 characterIndex = 0; characterIndex < ARRAY_COUNT(voiceCharacterNames); ++characterIndex) {
            const char* voiceName = voiceCharacterNames[characterIndex].name;
            if (LooseVoiceStemExists(postfix, suffix, voiceName)) {
                info.hasFiles = true;
                info.voiceCharacter = voiceCharacterNames[characterIndex].character;
                info.suffixMask |= 1 << suffixIndex;
                return info;
            }
        }
    }
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

bool VoiceBaseGroupForActor(const Audio::CharacterActor* actor, CharacterId& character, u32& groupId, CharacterId& groupCharacter) {
    if (!ActorRaceCharacter(actor, character)) return false;
    const u8 table = RaceSkinTable(actor->playerId, character);
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

Audio::CharacterVoiceActionTable& CharacterActorVoiceActionTableSlot(Audio::CharacterActor& actor) {
    return *reinterpret_cast<Audio::CharacterVoiceActionTable*>(reinterpret_cast<u8*>(&actor) + 0x134);
}

u16& CharacterActorCharacterSlot(Audio::CharacterActor& actor) {
    return *reinterpret_cast<u16*>(reinterpret_cast<u8*>(&actor) + 0x9c);
}

bool ApplyVoiceBaseActionTable(Audio::CharacterActor* actor) {
    const CharacterId voiceCharacter = VoiceBaseCharacterForActor(actor);
    Audio::CharacterVoiceActionTable table = VoiceActionTable(voiceCharacter);
    if (table == nullptr) return false;
    CharacterActorVoiceActionTableSlot(*actor) = table;
    return true;
}

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

bool PlayerMatchesVoiceGroupOffset(PlayerType playerType, u32 offset) {
    if (playerType == PLAYER_GHOST || playerType == PLAYER_NONE) return false;
    const bool npcGroup = offset == 1 || offset == 3;
    if (npcGroup) return playerType != PLAYER_REAL_LOCAL;
    return playerType == PLAYER_REAL_LOCAL;
}

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
        if (!PlayerMatchesVoiceGroupOffset(player.playerType, groupOffset)) continue;
        const CharacterId character = player.characterId;
        const u8 table = RaceSkinTable(playerId, character);
        u32 playerGroupBaseId = 0;
        if (!VoiceBaseGroupForTable(character, table, playerGroupBaseId) || playerGroupBaseId != groupBaseId) continue;
        if (!LooseVoiceInfoHasSuffix(GetLooseVoiceInfo(character, table), groupSuffix)) continue;
        const char* postfix = GeneratedCustomPostfix(character, table);
        if (postfix != nullptr) return postfix;
    }
    return nullptr;
}

}  // namespace CustomCharacters
}  // namespace Pulsar
