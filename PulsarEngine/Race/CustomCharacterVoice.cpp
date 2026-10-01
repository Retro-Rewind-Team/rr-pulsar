#include <Driver/CustomCharacters.hpp>
#include <IO/LooseArchiveOverrides.hpp>
#include <Race/CustomCharacters.hpp>
#include <Race/CustomCharacterVoice.hpp>
#include <Sound/LooseBRSAROverrides.hpp>
#include <MarioKartWii/Archive/ArchiveMgr.hpp>
#include <MarioKartWii/Audio/AudioManager.hpp>
#include <MarioKartWii/Audio/Actors/CharacterActor.hpp>
#include <MarioKartWii/Driver/DriverController.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <core/nw4r/snd/SoundArchive.hpp>
#include <core/nw4r/snd/WsdFile.hpp>
#include <core/rvl/OS/OSCache.hpp>
#include <core/rvl/dvd/dvd.hpp>
#include <include/c_stdio.h>
#include <include/c_string.h>

namespace Pulsar {
namespace Race {

static s8 sPlayerVoiceAliases[12];
static s8 sAliasPlayers[Driver::CHARACTER_COUNT];
static s8 sPlayerVoiceSources[12];
static u8 sPlayerVoiceSlots[12];
static bool sPlayerHasCustomVoice[12];
static bool sPlayerIsSilent[12];
static char sPlayerVoiceSourceSuffixes[12][0x20];
static bool sVoiceAssignmentsReady;

static bool ReadWaveSoundInfoSafely(const nw4r::snd::detail::WsdFileReader *reader, nw4r::snd::detail::WaveSoundInfo *info, int index) {
    using namespace nw4r::snd::detail;
    if (reader == nullptr || info == nullptr || reader->header == nullptr || reader->dataBlock == nullptr || index < 0) return false;

    const WsdFile::Header *header = reader->header;
    const WsdFile::DataBlock *dataBlock = reader->dataBlock;
    const u32 fileSize = header->fileHeader.fileSize;
    if (fileSize < sizeof(WsdFile::Header)) return false;

    const u32 fileStart = reinterpret_cast<u32>(header);
    const u32 dataBlockAddress = reinterpret_cast<u32>(dataBlock);
    const u32 refTableOffset = sizeof(nw4r::ut::BinaryBlockHeader) + sizeof(u32);
    const u32 refSize = sizeof(Util::DataRef<WsdFile::Wsd>);
    if (dataBlockAddress < fileStart) return false;
    const u32 dataBlockOffset = dataBlockAddress - fileStart;
    if (dataBlockOffset > fileSize || refTableOffset > fileSize - dataBlockOffset) return false;

    const u32 dataBlockSize = dataBlock->blockHeader.size;
    if (dataBlockSize > fileSize - dataBlockOffset || dataBlockSize < refTableOffset ||
        static_cast<u32>(index) >= dataBlock->wsdCount ||
        static_cast<u32>(index) >= (dataBlockSize - refTableOffset) / refSize)
        return false;

    const u32 baseAddress = dataBlockAddress + sizeof(nw4r::ut::BinaryBlockHeader);
    const Util::DataRef<WsdFile::Wsd> &wsdRef = dataBlock->refWsd[index];
    u32 wsdAddress;
    if (wsdRef.refType == Util::REF_TYPE_ADDR) {
        wsdAddress = wsdRef.value;
    } else if (wsdRef.refType == Util::REF_TYPE_OFFSET && wsdRef.value <= 0xffffffff - baseAddress) {
        wsdAddress = baseAddress + wsdRef.value;
    } else {
        return false;
    }
    if (wsdAddress < fileStart || wsdAddress - fileStart > fileSize ||
        sizeof(WsdFile::Wsd) > fileSize - (wsdAddress - fileStart))
        return false;
    const WsdFile::Wsd *wsd = reinterpret_cast<const WsdFile::Wsd *>(wsdAddress);

    const u16 version = header->fileHeader.version;
    memset(info, 0, sizeof(*info));
    info->pitch = 1.0f;
    info->pan = 0x40;
    info->ainSend = 0x7f;
    if (version < 0x101) return true;

    const u32 infoSize = version >= 0x102 ? 10 : 6;
    const Util::DataRef<WsdFile::WsdInfo> &infoRef = wsd->refWsdInfo;
    u32 infoAddress;
    if (infoRef.refType == Util::REF_TYPE_ADDR) {
        infoAddress = infoRef.value;
    } else if (infoRef.refType == Util::REF_TYPE_OFFSET && infoRef.value <= 0xffffffff - baseAddress) {
        infoAddress = baseAddress + infoRef.value;
    } else {
        return true;
    }
    if (infoAddress < fileStart || infoAddress - fileStart > fileSize ||
        infoSize > fileSize - (infoAddress - fileStart))
        return true;
    const WsdFile::WsdInfo *wsdInfo = reinterpret_cast<const WsdFile::WsdInfo *>(infoAddress);

    info->pitch = wsdInfo->pitch;
    info->pan = wsdInfo->pan;
    info->surroundPan = wsdInfo->surroundPan;
    if (version >= 0x102) {
        info->fxSendA = wsdInfo->fxSendA;
        info->fxSendB = wsdInfo->fxSendB;
        info->fxSendC = wsdInfo->fxSendC;
        info->ainSend = wsdInfo->ainSend;
    }
    return true;
}
kmBranch(0x800ada40, ReadWaveSoundInfoSafely);

static const char *sVoiceSourceNames[Driver::CHARACTER_COUNT] = {
    "mario",
    "baby_peach",
    "waluigi",
    "bowser",
    "baby_daisy",
    "drybones",
    "baby_mario",
    "luigi",
    "toad",
    "donkey_kong",
    "yoshi",
    "wario",
    "baby_luigi",
    "toadette",
    "koopa_troopa",
    "daisy",
    "peach",
    "birdo",
    "diddy_kong",
    "king_boo",
    "bowser_jr",
    "dry_bowser",
    "funky_kong",
    "rosalina",
};

static bool EqualCharacterCode(const char *lhs, const char *rhs) {
    while (*lhs != '\0' && *rhs != '\0') {
        char left = *lhs++;
        char right = *rhs++;
        if (left >= 'a' && left <= 'z') left -= 'a' - 'A';
        if (right >= 'a' && right <= 'z') right -= 'a' - 'A';
        if (left != right) return false;
    }
    return *lhs == *rhs;
}

static bool GetVoiceGroupParts(const char *label, char *characterCode, u32 characterCodeSize,
                               const char *&typeSuffix) {
    if (label == nullptr || strncmp(label, "GRP_VO_", 7) != 0) return false;
    const char *separator = strchr(label + 7, '_');
    if (separator == nullptr || separator == label + 7 || separator - (label + 7) >= characterCodeSize) return false;
    memcpy(characterCode, label + 7, separator - (label + 7));
    characterCode[separator - (label + 7)] = '\0';
    typeSuffix = separator;
    return true;
}

static bool GetCharacterVoiceCode(CharacterId character, char *code, u32 codeSize) {
    if (character >= Driver::CHARACTER_COUNT || Audio::Manager::soundArchive == nullptr) return false;
    const char *label = Audio::Manager::soundArchive->GetGroupLabelString(Audio::CharacterActor::charactersGroupIds[character]);
    const char *typeSuffix;
    return GetVoiceGroupParts(label, code, codeSize, typeSuffix);
}

static bool VoiceFileExists(const char *characterCode, u32 slot, const char *typeSuffix, const char *extension,
                            const char *sourceSuffix) {
    char path[0x100];
    if (sourceSuffix != nullptr && sourceSuffix[0] != '\0')
        snprintf(path, sizeof(path), "/sound/GRP_VO_%s-%u%s.%s.%s", characterCode, slot, typeSuffix, extension,
                 sourceSuffix);
    else
        snprintf(path, sizeof(path), "/sound/GRP_VO_%s-%u%s.%s", characterCode, slot, typeSuffix, extension);
    return IOOverrides::ConvertPathToEntryNumWithLooseOverride(path) >= 0;
}

static bool HasSuffixedVoiceFiles(const char *characterCode, u32 slot, const char *sourceSuffix) {
    nw4r::snd::SoundArchive *archive = Audio::Manager::soundArchive;
    if (archive == nullptr) return false;

    for (u32 groupId = 0; groupId < archive->GetGroupCount(); ++groupId) {
        char groupCharacterCode[0x20];
        const char *typeSuffix;
        if (!GetVoiceGroupParts(archive->GetGroupLabelString(groupId), groupCharacterCode, sizeof(groupCharacterCode),
                                typeSuffix) ||
            !EqualCharacterCode(groupCharacterCode, characterCode))
            continue;
        if (VoiceFileExists(characterCode, slot, typeSuffix, "brbnk", sourceSuffix) ||
            VoiceFileExists(characterCode, slot, typeSuffix, "brwsd", sourceSuffix))
            return true;
    }
    return false;
}

static bool HasUnsuffixedVoiceFiles(const char *characterCode, u32 slot) {
    nw4r::snd::SoundArchive *archive = Audio::Manager::soundArchive;
    if (archive == nullptr) return false;

    for (u32 groupId = 0; groupId < archive->GetGroupCount(); ++groupId) {
        char groupCharacterCode[0x20];
        const char *typeSuffix;
        if (!GetVoiceGroupParts(archive->GetGroupLabelString(groupId), groupCharacterCode, sizeof(groupCharacterCode),
                                typeSuffix) ||
            !EqualCharacterCode(groupCharacterCode, characterCode))
            continue;
        if (VoiceFileExists(characterCode, slot, typeSuffix, "brbnk", nullptr) ||
            VoiceFileExists(characterCode, slot, typeSuffix, "brwsd", nullptr))
            return true;
    }
    return false;
}

static bool FindCustomVoiceSource(CharacterId character, u32 slot, s8 &sourceCharacter, char *sourceSuffix,
                                  u32 sourceSuffixSize) {
    char characterCode[0x20];
    if (!GetCharacterVoiceCode(character, characterCode, sizeof(characterCode))) return false;

    for (u32 source = 0; source < Driver::CHARACTER_COUNT; ++source) {
        if (HasSuffixedVoiceFiles(characterCode, slot, sVoiceSourceNames[source])) {
            sourceCharacter = static_cast<s8>(source);
            snprintf(sourceSuffix, sourceSuffixSize, "%s", sVoiceSourceNames[source]);
            return true;
        }

        const char *postfix = ArchiveMgr::GetKartArchivePostfix(static_cast<CharacterId>(source));
        if (postfix != nullptr && HasSuffixedVoiceFiles(characterCode, slot, postfix)) {
            sourceCharacter = static_cast<s8>(source);
            snprintf(sourceSuffix, sourceSuffixSize, "%s", postfix);
            return true;
        }

        char sourceCode[0x20];
        if (!GetCharacterVoiceCode(static_cast<CharacterId>(source), sourceCode, sizeof(sourceCode))) continue;
        for (char *letter = sourceCode; *letter != '\0'; ++letter)
            if (*letter >= 'A' && *letter <= 'Z') *letter += 'a' - 'A';
        if ((postfix == nullptr || strcmp(postfix, sourceCode) != 0) &&
            HasSuffixedVoiceFiles(characterCode, slot, sourceCode)) {
            sourceCharacter = static_cast<s8>(source);
            snprintf(sourceSuffix, sourceSuffixSize, "%s", sourceCode);
            return true;
        }
    }

    if (!HasUnsuffixedVoiceFiles(characterCode, slot)) return false;
    sourceCharacter = static_cast<s8>(character);
    sourceSuffix[0] = '\0';
    return true;
}

static bool IsSilentSlot(CharacterId character, u32 slot) {
    char path[0x80];
    snprintf(path, sizeof(path), "/sound/%s-%u.silent", ArchiveMgr::GetKartArchivePostfix(character), slot);
    return IOOverrides::ConvertPathToEntryNumWithLooseOverride(path) >= 0;
}

static void ResetVoiceAssignments() {
    for (u32 playerId = 0; playerId < 12; ++playerId) {
        sPlayerVoiceAliases[playerId] = -1;
        sPlayerVoiceSources[playerId] = -1;
        sPlayerVoiceSlots[playerId] = 0;
        sPlayerHasCustomVoice[playerId] = false;
        sPlayerIsSilent[playerId] = false;
        sPlayerVoiceSourceSuffixes[playerId][0] = '\0';
    }
    for (u32 character = 0; character < Driver::CHARACTER_COUNT; ++character) sAliasPlayers[character] = -1;
    sVoiceAssignmentsReady = false;
}

static void LoadCharacterVoices() {
    ResetVoiceAssignments();
    if (Racedata::sInstance == nullptr || Audio::Manager::soundArchive == nullptr) return;

    RacedataScenario &scenario = Racedata::sInstance->racesScenario;
    bool usedCharacters[Driver::CHARACTER_COUNT] = {};
    for (u32 playerId = 0; playerId < scenario.playerCount; ++playerId) {
        const u32 character = static_cast<u32>(scenario.players[playerId].characterId);
        if (character < Driver::CHARACTER_COUNT) usedCharacters[character] = true;
    }

    for (u32 playerId = 0; playerId < scenario.playerCount; ++playerId) {
        const u32 character = static_cast<u32>(scenario.players[playerId].characterId);
        if (character >= Driver::CHARACTER_COUNT) continue;

        sPlayerVoiceSlots[playerId] = GetPlayerCustomCharacterSlot(playerId, static_cast<CharacterId>(character));
        if (sPlayerVoiceSlots[playerId] == 0 ||
            !FindCustomVoiceSource(static_cast<CharacterId>(character), sPlayerVoiceSlots[playerId],
                                   sPlayerVoiceSources[playerId], sPlayerVoiceSourceSuffixes[playerId],
                                   sizeof(sPlayerVoiceSourceSuffixes[playerId]))) {
            sPlayerIsSilent[playerId] = sPlayerVoiceSlots[playerId] != 0 &&
                                        IsSilentSlot(static_cast<CharacterId>(character),
                                                     sPlayerVoiceSlots[playerId]);
            continue;
        }

        sPlayerHasCustomVoice[playerId] = true;
        char targetCode[0x20];
        GetCharacterVoiceCode(static_cast<CharacterId>(character), targetCode, sizeof(targetCode));
        s8 bestAlias = -1;
        u32 bestGroupCount = 0;
        u32 bestMatchedGroupCount = 0;
        for (u32 alias = 0; alias < Driver::CHARACTER_COUNT; ++alias) {
            if (usedCharacters[alias]) continue;

            char aliasCode[0x20];
            GetCharacterVoiceCode(static_cast<CharacterId>(alias), aliasCode, sizeof(aliasCode));
            u32 groupCount = 0;
            u32 matchedGroupCount = 0;
            for (u32 groupId = 0; groupId < Audio::Manager::soundArchive->GetGroupCount(); ++groupId) {
                char groupCode[0x20];
                const char *typeSuffix;
                if (!GetVoiceGroupParts(Audio::Manager::soundArchive->GetGroupLabelString(groupId), groupCode,
                                        sizeof(groupCode), typeSuffix) ||
                    !EqualCharacterCode(groupCode, targetCode))
                    continue;

                ++groupCount;
                char aliasLabel[0x80];
                snprintf(aliasLabel, sizeof(aliasLabel), "GRP_VO_%s%s", aliasCode, typeSuffix);
                if (Audio::Manager::soundArchive->ConvertLabelStringToGroupId(aliasLabel) != 0xffffffff)
                    ++matchedGroupCount;
            }

            if (bestAlias < 0 || matchedGroupCount > bestMatchedGroupCount) {
                bestAlias = static_cast<s8>(alias);
                bestGroupCount = groupCount;
                bestMatchedGroupCount = matchedGroupCount;
            }
            if (matchedGroupCount == groupCount) break;
        }
        sPlayerVoiceAliases[playerId] = bestAlias;
        if (bestAlias >= 0) {
            usedCharacters[static_cast<u32>(bestAlias)] = true;
            sAliasPlayers[static_cast<u32>(bestAlias)] = static_cast<s8>(playerId);
        }
    }

    sVoiceAssignmentsReady = true;
}
static RaceLoadHook LoadCharacterVoicesOnRaceLoadHook(LoadCharacterVoices);

static void EnsureVoiceAssignments() {
    if (!sVoiceAssignmentsReady && Racedata::sInstance != nullptr && Audio::Manager::soundArchive != nullptr)
        LoadCharacterVoices();
}

static TicoModel *CreateTicoModelForCustomVoice(void *memory, DriverController *controller) {
    if (memory == nullptr) return nullptr;
    EnsureVoiceAssignments();
    const u8 playerId = controller->GetPlayerIdx();
    if (playerId < 12 && (sPlayerHasCustomVoice[playerId] || sPlayerIsSilent[playerId])) return nullptr;
    return new (memory) TicoModel(controller);
}
kmCall(0x807c8994, CreateTicoModelForCustomVoice);

static Audio::Handle *HoldRosalinaLumaSound(Audio::RaceActor *actor, u32 soundId) {
    Audio::CharacterActor *characterActor = static_cast<Audio::CharacterActor *>(actor);
    EnsureVoiceAssignments();
    const u8 playerId = characterActor->playerId;
    if ((soundId == 0xf68 || soundId == 0xf69) && playerId < 12 &&
        Racedata::sInstance->racesScenario.players[playerId].characterId == ROSALINA &&
        (sPlayerHasCustomVoice[playerId] || sPlayerIsSilent[playerId]))
        return nullptr;
    return actor->Audio::RaceActor::HoldSoundLimited(soundId);
}
kmWritePointer(0x808dbcd8, HoldRosalinaLumaSound);

static s8 GetPlayerVoiceAlias(u32 playerId) {
    EnsureVoiceAssignments();
    return sVoiceAssignmentsReady && playerId < 12 ? sPlayerVoiceAliases[playerId] : -1;
}

static u32 GetAliasedVoiceGroupId(Audio::CharacterActor *actor, u32 groupId) {
    if (actor->playerId >= 12) return groupId;
    const s8 alias = GetPlayerVoiceAlias(actor->playerId);
    if (alias < 0 || Audio::Manager::soundArchive == nullptr) return groupId;

    const char *label = Audio::Manager::soundArchive->GetGroupLabelString(groupId);
    char originalCode[0x20];
    const char *typeSuffix;
    if (!GetVoiceGroupParts(label, originalCode, sizeof(originalCode), typeSuffix)) return groupId;

    char aliasCode[0x20];
    if (!GetCharacterVoiceCode(static_cast<CharacterId>(alias), aliasCode, sizeof(aliasCode))) return groupId;
    char aliasLabel[0x80];
    snprintf(aliasLabel, sizeof(aliasLabel), "GRP_VO_%s%s", aliasCode, typeSuffix);
    const u32 aliasGroupId = Audio::Manager::soundArchive->ConvertLabelStringToGroupId(aliasLabel);
    return aliasGroupId != 0xffffffff ? aliasGroupId : groupId;
}

static u32 GetCharacterGroupIdForCustomVoice(Audio::CharacterActor *actor) {
    return GetAliasedVoiceGroupId(actor, actor->GetCharacterGroupId());
}
kmCall(0x80716224, GetCharacterGroupIdForCustomVoice);

static u32 GetCharacterGoalGroupIdForCustomVoice(Audio::CharacterActor *actor, u32 type) {
    return GetAliasedVoiceGroupId(actor, actor->GetCharacterGoalGroupId(type));
}
kmCall(0x80716254, GetCharacterGoalGroupIdForCustomVoice);

static u32 GetCharacterCannonGroupIdForCustomVoice(Audio::CharacterActor *actor) {
    return GetAliasedVoiceGroupId(actor, actor->GetCharacterCannonGroupId());
}
kmCall(0x80716280, GetCharacterCannonGroupIdForCustomVoice);

static Audio::RandomSoundPicker *GetCharacterVoiceSoundSetForCustomVoice(Audio::DriverSoundManager *manager, CharacterId character, u32 type) {
    register Audio::CharacterActor *actor;
    asm(mr actor, r30;);
    const u32 playerId = actor->playerId;
    const s8 alias = GetPlayerVoiceAlias(playerId);
    if (alias >= 0) character = static_cast<CharacterId>(alias);
    Audio::RandomSoundPicker *picker = manager->GetCharacterVoiceSoundSet(character, type);
    return picker;
}
kmCall(0x80863dd8, GetCharacterVoiceSoundSetForCustomVoice);

static Audio::RandomSoundPicker *GetCharacterVoiceSoundSetForCustomVoicePlayback(Audio::DriverSoundManager *manager, CharacterId character, u32 type) {
    register Audio::CharacterActor *actor;
    asm(mr actor, r26;);
    const u32 playerId = actor->playerId;
    const s8 alias = GetPlayerVoiceAlias(playerId);
    if (alias >= 0) character = static_cast<CharacterId>(alias);
    Audio::RandomSoundPicker *picker = manager->GetCharacterVoiceSoundSet(character, type);
    return picker;
}
kmCall(0x80864e4c, GetCharacterVoiceSoundSetForCustomVoicePlayback);

static Audio::RandomSoundPicker *GetCharacterVoiceSoundSetForCustomGoalVoice(Audio::DriverSoundManager *manager, CharacterId character, u32 type) {
    register Audio::CharacterActor *actor;
    asm(mr actor, r27;);
    const u32 playerId = actor->playerId;
    const s8 alias = GetPlayerVoiceAlias(playerId);
    if (alias >= 0) character = static_cast<CharacterId>(alias);
    Audio::RandomSoundPicker *picker = manager->GetCharacterVoiceSoundSet(character, type);
    return picker;
}
kmCall(0x8086654c, GetCharacterVoiceSoundSetForCustomGoalVoice);

static void InitCharacterVoiceRanges(Audio::CharacterActor *actor) {
    EnsureVoiceAssignments();
    const u32 playerId = actor->playerId;
    if (playerId < 12 && sPlayerIsSilent[playerId]) {
        reinterpret_cast<u8 *>(actor)[0x6ff] = false;
        actor->InitVoiceRanges();
        return;
    }

    if (playerId >= 12 || !sPlayerHasCustomVoice[playerId] || sPlayerVoiceAliases[playerId] < 0) {
        actor->InitVoiceRanges();
        return;
    }

    u16 *characterId = reinterpret_cast<u16 *>(reinterpret_cast<u8 *>(actor) + 0x9c);
    const u16 originalCharacter = *characterId;
    *characterId = static_cast<u16>(sPlayerVoiceAliases[playerId]);
    actor->InitVoiceRanges();
    *characterId = originalCharacter;
}
kmCall(0x80863ccc, InitCharacterVoiceRanges);
kmCall(0x80866214, InitCharacterVoiceRanges);

static u32 SetCustomVoiceActionTable() {
    register Audio::CharacterActor *actor;
    asm(mr actor, r30;);
    EnsureVoiceAssignments();
    const u32 playerId = actor->playerId;
    if (playerId < 12 && sPlayerIsSilent[playerId]) {
        reinterpret_cast<u8 *>(actor)[0x6ff] = false;
    } else if (playerId < 12 && sPlayerHasCustomVoice[playerId]) {
        const u32 baseCharacter =
            *reinterpret_cast<u16 *>(reinterpret_cast<u8 *>(actor) + 0x9c);
        const u32 source = sPlayerVoiceSources[playerId] < 0
                               ? baseCharacter
                               : static_cast<u32>(sPlayerVoiceSources[playerId]);
        *reinterpret_cast<Audio::CharacterVoiceActionTable *>(reinterpret_cast<u8 *>(actor) + 0x134) =
            Audio::CharacterActor::voiceActionTables[source];
    }
    return 0x28;
}
kmCall(0x80863e18, SetCustomVoiceActionTable);

struct VoiceFileLayout {
    u32 fileSize;
    u32 waveOffset;
    u32 waveSize;
};

static u32 ReadVoiceBE32(const void *data) {
    const u8 *bytes = reinterpret_cast<const u8 *>(data);
    return (static_cast<u32>(bytes[0]) << 24) | (static_cast<u32>(bytes[1]) << 16) |
           (static_cast<u32>(bytes[2]) << 8) | static_cast<u32>(bytes[3]);
}

static u32 AlignVoiceBuffer(u32 value) {
    return (value + 0x1f) & ~0x1f;
}

static bool ReadVoiceFileRange(DVD::FileInfo &file, void *dest, u32 size, u32 offset) {
    if (dest == nullptr || size == 0) return false;
    const u32 start = reinterpret_cast<u32>(dest) & ~0x1f;
    const u32 end = AlignVoiceBuffer(reinterpret_cast<u32>(dest) + size);
    OS::DCInvalidateRange(reinterpret_cast<void *>(start), end - start);
    return DVD::ReadPrio(&file, dest, static_cast<s32>(size), static_cast<s32>(offset), 2) == static_cast<s32>(size);
}

static bool FindVoiceWaveData(DVD::FileInfo &file, u32 searchStart, u32 fileSize, u32 &waveOffset, u32 &waveSize) {
    waveOffset = 0;
    waveSize = 0;
    if (searchStart >= fileSize) return false;

    u8 header[0x20] __attribute__((aligned(32)));
    if (searchStart + sizeof(header) <= fileSize && ReadVoiceFileRange(file, header, sizeof(header), searchStart) &&
        memcmp(header, "RWAR", 4) == 0) {
        const u32 size = ReadVoiceBE32(header + 8);
        if (size >= 0x20 && searchStart + size <= fileSize) {
            waveOffset = searchStart;
            waveSize = size;
            return true;
        }
    }

    u8 chunk[0x800] __attribute__((aligned(32)));
    u32 offset = AlignVoiceBuffer(searchStart + 0x20);
    while (offset + 0x20 <= fileSize) {
        u32 readSize = fileSize - offset;
        if (readSize > sizeof(chunk)) readSize = sizeof(chunk);
        readSize &= ~0x1f;
        if (readSize < 0x20) break;
        if (ReadVoiceFileRange(file, chunk, readSize, offset)) {
            for (u32 chunkOffset = 0; chunkOffset + 0x20 <= readSize; chunkOffset += 0x20) {
                if (memcmp(chunk + chunkOffset, "RWAR", 4) != 0) continue;
                const u32 size = ReadVoiceBE32(chunk + chunkOffset + 8);
                const u32 candidateOffset = offset + chunkOffset;
                if (size >= 0x20 && candidateOffset + size <= fileSize) {
                    waveOffset = candidateOffset;
                    waveSize = size;
                    return true;
                }
            }
        }
        offset += readSize;
    }
    return false;
}

static bool ReadVoiceFileLayout(DVD::FileInfo &file, const char *magic, VoiceFileLayout &layout) {
    layout.fileSize = 0;
    layout.waveOffset = 0;
    layout.waveSize = 0;
    if (file.length < 0x20) return false;

    u8 header[0x20] __attribute__((aligned(32)));
    if (!ReadVoiceFileRange(file, header, sizeof(header), 0) || memcmp(header, magic, 4) != 0) return false;
    layout.fileSize = ReadVoiceBE32(header + 8);
    if (layout.fileSize < 0x20 || layout.fileSize > static_cast<u32>(file.length)) return false;
    if (memcmp(magic, "RWSD", 4) == 0 || memcmp(magic, "RBNK", 4) == 0)
        FindVoiceWaveData(file, AlignVoiceBuffer(layout.fileSize), file.length, layout.waveOffset, layout.waveSize);
    return true;
}

static bool GetVoiceGroupItemCapacity(const nw4r::snd::SoundArchive &archive, u32 groupId, const nw4r::snd::SoundArchive::GroupInfo &groupInfo, u32 itemIndex, const nw4r::snd::SoundArchive::GroupItemInfo &target, bool waveData, u32 groupSize, u32 &capacity) {
    const u32 targetOffset = waveData ? target.waveDataOffset : target.offset;
    const u32 targetSize = waveData ? target.waveDataSize : target.size;
    if (targetSize == 0 || targetOffset >= groupSize) return false;

    u32 nextOffset = groupSize;
    for (u32 index = 0; index < groupInfo.itemCount; ++index) {
        if (index == itemIndex) continue;
        nw4r::snd::SoundArchive::GroupItemInfo other;
        if (!archive.detail_ReadGroupItemInfo(groupId, index, &other)) continue;
        const u32 otherOffset = waveData ? other.waveDataOffset : other.offset;
        const u32 otherSize = waveData ? other.waveDataSize : other.size;
        if (otherSize > 0 && otherOffset > targetOffset && otherOffset < nextOffset) nextOffset = otherOffset;
    }
    capacity = nextOffset - targetOffset;
    return capacity > 0;
}

static bool ReadVoiceFileToMemory(DVD::FileInfo &file, void *dest, u32 size, u32 offset) {
    if (!ReadVoiceFileRange(file, dest, size, offset)) return false;
    OS::DCStoreRange(dest, size);
    return true;
}

static bool PatchVoiceGroupItem(nw4r::snd::detail::SoundArchiveLoader *loader, u32 groupId, u32 itemIndex, const nw4r::snd::SoundArchive::GroupInfo &groupInfo, const nw4r::snd::SoundArchive::GroupItemInfo &item, nw4r::snd::SoundMemoryAllocatable *allocater, void *groupData, void *waveData, const char *path, const char *magic, bool &wavePatched) {
    wavePatched = false;
    DVD::FileInfo file;
    if (!DVD::Open(path, &file)) {
        return false;
    }
    VoiceFileLayout layout;
    if (!ReadVoiceFileLayout(file, magic, layout)) {
        DVD::Close(&file);
        return false;
    }

    Sound::SetLooseBRSARGroupItemBuffer(item.fileId, false, nullptr);
    Sound::SetLooseBRSARGroupItemBuffer(item.fileId, true, nullptr);
    u32 capacity = 0;
    const bool fitsInGroup = GetVoiceGroupItemCapacity(loader->archive, groupId, groupInfo, itemIndex, item, false,
                                                       groupInfo.size, capacity) &&
                             capacity >= layout.fileSize;
    bool filePatched = false;
    if (fitsInGroup) {
        u8 *dest = reinterpret_cast<u8 *>(groupData) + item.offset;
        filePatched = ReadVoiceFileToMemory(file, dest, layout.fileSize, 0);
        if (filePatched && layout.fileSize < item.size) memset(dest + layout.fileSize, 0, item.size - layout.fileSize);
    } else if (allocater != nullptr) {
        const u32 allocSize = AlignVoiceBuffer(layout.fileSize);
        void *buffer = allocater->Alloc(allocSize);
        if (buffer != nullptr && ReadVoiceFileToMemory(file, buffer, layout.fileSize, 0)) {
            if (allocSize > layout.fileSize) memset(reinterpret_cast<u8 *>(buffer) + layout.fileSize, 0, allocSize - layout.fileSize);
            OS::DCStoreRange(buffer, allocSize);
            Sound::SetLooseBRSARGroupItemBuffer(item.fileId, false, buffer);
            filePatched = true;
        }
    }

    if (filePatched && layout.waveSize > 0) {
        capacity = 0;
        const bool waveFits = waveData != nullptr &&
                              GetVoiceGroupItemCapacity(loader->archive, groupId, groupInfo, itemIndex, item, true,
                                                        groupInfo.waveDataSize, capacity) &&
                              capacity >= layout.waveSize;
        if (waveFits) {
            u8 *dest = reinterpret_cast<u8 *>(waveData) + item.waveDataOffset;
            wavePatched = ReadVoiceFileToMemory(file, dest, layout.waveSize, layout.waveOffset);
            if (wavePatched && layout.waveSize < item.waveDataSize)
                memset(dest + layout.waveSize, 0, item.waveDataSize - layout.waveSize);
        } else if (allocater != nullptr) {
            const u32 allocSize = AlignVoiceBuffer(layout.waveSize);
            void *buffer = allocater->Alloc(allocSize);
            if (buffer != nullptr && ReadVoiceFileToMemory(file, buffer, layout.waveSize, layout.waveOffset)) {
                if (allocSize > layout.waveSize)
                    memset(reinterpret_cast<u8 *>(buffer) + layout.waveSize, 0, allocSize - layout.waveSize);
                OS::DCStoreRange(buffer, allocSize);
                Sound::SetLooseBRSARGroupItemBuffer(item.fileId, true, buffer);
                wavePatched = true;
            }
        }
    }

    DVD::Close(&file);
    return filePatched;
}

static void CopyVoiceGroupItem(nw4r::snd::detail::SoundArchiveLoader *loader, u32 targetGroupId, u32 itemIndex, const nw4r::snd::SoundArchive::GroupInfo &targetGroupInfo, const nw4r::snd::SoundArchive::GroupItemInfo &targetItem, u32 sourceGroupId, nw4r::snd::SoundMemoryAllocatable *allocater, void *groupData, void *waveData, const char *magic, bool copyFile, bool copyWave) {
    nw4r::snd::SoundArchive::GroupInfo sourceGroupInfo;
    nw4r::snd::SoundArchive::GroupItemInfo sourceItem;
    if (!loader->archive.ReadGroupInfo(sourceGroupId, &sourceGroupInfo) || itemIndex >= sourceGroupInfo.itemCount || !loader->archive.detail_ReadGroupItemInfo(sourceGroupId, itemIndex, &sourceItem) || sourceItem.fileId == targetItem.fileId) return;

    nw4r::snd::SoundArchive::FileInfo fileInfo;
    if (!loader->archive.detail_ReadFileInfo(sourceItem.fileId, &fileInfo)) return;
    if (copyFile && fileInfo.fileSize > 0) {
        Sound::SetLooseBRSARGroupItemBuffer(targetItem.fileId, false, nullptr);
        void *sourceFile = loader->LoadFile(sourceItem.fileId, allocater);
        if (sourceFile != nullptr && memcmp(sourceFile, magic, 4) == 0) {
            u32 capacity = 0;
            if (GetVoiceGroupItemCapacity(loader->archive, targetGroupId, targetGroupInfo, itemIndex, targetItem, false, targetGroupInfo.size, capacity) &&
                capacity >= fileInfo.fileSize) {
                u8 *dest = reinterpret_cast<u8 *>(groupData) + targetItem.offset;
                memcpy(dest, sourceFile, fileInfo.fileSize);
                if (fileInfo.fileSize < targetItem.size) memset(dest + fileInfo.fileSize, 0, targetItem.size - fileInfo.fileSize);
                OS::DCStoreRange(dest, fileInfo.fileSize);
            } else {
                Sound::SetLooseBRSARGroupItemBuffer(targetItem.fileId, false, sourceFile);
            }
        }
    }

    if (copyWave && fileInfo.waveDataFileSize > 0) {
        Sound::SetLooseBRSARGroupItemBuffer(targetItem.fileId, true, nullptr);
        void *sourceWave = loader->LoadWaveDataFile(sourceItem.fileId, allocater);
        if (sourceWave != nullptr) {
            u32 capacity = 0;
            if (waveData != nullptr &&
                GetVoiceGroupItemCapacity(loader->archive, targetGroupId, targetGroupInfo, itemIndex, targetItem, true, targetGroupInfo.waveDataSize, capacity) &&
                capacity >= fileInfo.waveDataFileSize) {
                u8 *dest = reinterpret_cast<u8 *>(waveData) + targetItem.waveDataOffset;
                memcpy(dest, sourceWave, fileInfo.waveDataFileSize);
                if (fileInfo.waveDataFileSize < targetItem.waveDataSize) memset(dest + fileInfo.waveDataFileSize, 0, targetItem.waveDataSize - fileInfo.waveDataFileSize);
                OS::DCStoreRange(dest, fileInfo.waveDataFileSize);
            } else {
                Sound::SetLooseBRSARGroupItemBuffer(targetItem.fileId, true, sourceWave);
            }
        }
    }
}

void PatchLoadedCustomVoiceGroup(nw4r::snd::detail::SoundArchiveLoader *loader, u32 groupId,
                                 nw4r::snd::SoundMemoryAllocatable *allocater, void *groupData, void *waveData) {
    EnsureVoiceAssignments();
    if (!sVoiceAssignmentsReady || Audio::Manager::soundArchive == nullptr || groupData == nullptr) return;

    const char *groupLabel = loader->archive.GetGroupLabelString(groupId);
    char groupCharacterCode[0x20];
    const char *typeSuffix;
    if (!GetVoiceGroupParts(groupLabel, groupCharacterCode, sizeof(groupCharacterCode), typeSuffix)) return;

    s8 aliasCharacter = -1;
    for (u32 character = 0; character < Driver::CHARACTER_COUNT; ++character) {
        char characterCode[0x20];
        if (GetCharacterVoiceCode(static_cast<CharacterId>(character), characterCode, sizeof(characterCode)) && EqualCharacterCode(characterCode, groupCharacterCode)) {
            aliasCharacter = static_cast<s8>(character);
            break;
        }
    }
    if (aliasCharacter < 0) return;

    const s8 playerId = sAliasPlayers[static_cast<u32>(aliasCharacter)];
    if (playerId < 0 || !sPlayerHasCustomVoice[static_cast<u32>(playerId)]) return;
    const u32 baseCharacter = static_cast<u32>(Racedata::sInstance->racesScenario.players[static_cast<u32>(playerId)].characterId);
    char targetCode[0x20];
    if (baseCharacter >= Driver::CHARACTER_COUNT || !GetCharacterVoiceCode(static_cast<CharacterId>(baseCharacter), targetCode, sizeof(targetCode))) return;
    nw4r::snd::SoundArchive::GroupInfo groupInfo;
    if (!loader->archive.ReadGroupInfo(groupId, &groupInfo)) return;
    for (u32 index = 0; index < groupInfo.itemCount; ++index) {
        nw4r::snd::SoundArchive::GroupItemInfo item;
        if (!loader->archive.detail_ReadGroupItemInfo(groupId, index, &item) || item.size < 4) continue;

        char magic[4];
        memcpy(magic, reinterpret_cast<u8 *>(groupData) + item.offset, sizeof(magic));
        const char *extension = nullptr;
        if (memcmp(magic, "RBNK", 4) == 0)
            extension = "brbnk";
        else if (memcmp(magic, "RWSD", 4) == 0)
            extension = "brwsd";
        if (extension == nullptr) continue;

        char path[0x100];
        bool wavePatched = false;
        bool filePatched = false;
        if (sPlayerVoiceSourceSuffixes[static_cast<u32>(playerId)][0] != '\0') {
            snprintf(path, sizeof(path), "/sound/GRP_VO_%s-%u%s.%s.%s", targetCode, sPlayerVoiceSlots[static_cast<u32>(playerId)], typeSuffix, extension, sPlayerVoiceSourceSuffixes[static_cast<u32>(playerId)]);
            filePatched = PatchVoiceGroupItem(loader, groupId, index, groupInfo, item, allocater, groupData, waveData, path, magic, wavePatched);
        }
        if (!filePatched) {
            snprintf(path, sizeof(path), "/sound/GRP_VO_%s-%u%s.%s", targetCode, sPlayerVoiceSlots[static_cast<u32>(playerId)], typeSuffix, extension);
            filePatched = PatchVoiceGroupItem(loader, groupId, index, groupInfo, item, allocater, groupData, waveData, path, magic, wavePatched);
        }

        u32 sourceCharacter = sPlayerVoiceSources[static_cast<u32>(playerId)] < 0 ? baseCharacter : static_cast<u32>(sPlayerVoiceSources[static_cast<u32>(playerId)]);
        char sourceCode[0x20];
        if (sourceCharacter >= Driver::CHARACTER_COUNT || !GetCharacterVoiceCode(static_cast<CharacterId>(sourceCharacter), sourceCode, sizeof(sourceCode))) sourceCharacter = baseCharacter;
        if (!GetCharacterVoiceCode(static_cast<CharacterId>(sourceCharacter), sourceCode, sizeof(sourceCode))) continue;

        char sourceLabel[0x80];
        snprintf(sourceLabel, sizeof(sourceLabel), "GRP_VO_%s%s", sourceCode, typeSuffix);
        u32 sourceGroupId = Audio::Manager::soundArchive->ConvertLabelStringToGroupId(sourceLabel);
        if (sourceGroupId == 0xffffffff && sourceCharacter != baseCharacter &&
            GetCharacterVoiceCode(static_cast<CharacterId>(baseCharacter), sourceCode, sizeof(sourceCode))) {
            snprintf(sourceLabel, sizeof(sourceLabel), "GRP_VO_%s%s", sourceCode, typeSuffix);
            sourceGroupId = Audio::Manager::soundArchive->ConvertLabelStringToGroupId(sourceLabel);
        }
        if (sourceGroupId != 0xffffffff)
            CopyVoiceGroupItem(loader, groupId, index, groupInfo, item, sourceGroupId, allocater, groupData, waveData, magic, !filePatched, !wavePatched);
    }
}

}  // namespace Race
}  // namespace Pulsar
