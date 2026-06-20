#ifndef _PULSAR_CUSTOM_CHARACTERS_
#define _PULSAR_CUSTOM_CHARACTERS_

#include <hooks.hpp>
#include <runtimeWrite.hpp>
#include <PulsarSystem.hpp>
#include <Settings/Settings.hpp>
#include <include/c_string.h>
#include <MarioKartWii/Archive/ArchiveMgr.hpp>
#include <MarioKartWii/UI/Page/Menu/CharacterSelect.hpp>
#include <MarioKartWii/Driver/DriverController.hpp>
#include <MarioKartWii/Driver/Tico.hpp>
#include <MarioKartWii/Driver/Toadette.hpp>
#include <MarioKartWii/Kart/KartLink.hpp>
#include <MarioKartWii/System/Identifiers.hpp>
#include <MarioKartWii/System/Random.hpp>
#include <MarioKartWii/GlobalFunctions.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>
#include <MarioKartWii/Scene/GameScene.hpp>
#include <MarioKartWii/Audio/RSARPlayer.hpp>
#include <MarioKartWii/Input/Controller.hpp>
#include <MarioKartWii/UI/Ctrl/CtrlRace/CtrlRace2DMap.hpp>
#include <MarioKartWii/UI/Ctrl/CtrlRace/CtrlRaceResult.hpp>
#include <MarioKartWii/Audio/Actors/CharacterActor.hpp>
#include <core/RK/RKSystem.hpp>
#include <core/egg/DVD/DvdRipper.hpp>
#include <core/egg/mem/ExpHeap.hpp>
#include <core/rvl/dvd/dvd.hpp>
#include <core/rvl/os/OS.hpp>
#include <core/nw4r/ut/List.hpp>
#include <MarioKartWii/3D/Scn/ScnMgr.hpp>
#include <MarioKartWii/3D/Model/Menu/MenuDriverModel.hpp>
#include <RetroRewindChannel.hpp>
#include <UI/UI.hpp>

namespace Pulsar {
namespace CustomCharacters {

#define ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))

// Shared limits for skin ids, packet packing, local UI slots, and loose metadata.
enum {
    PACKET_BITS = 6,
    PACKET_MASK = (1 << PACKET_BITS) - 1,
    TABLE_DEFAULT = 0,
    CUSTOM_TABLE_LIMIT = 50,
    TABLE_COUNT = CUSTOM_TABLE_LIMIT + 1,
    TABLE_INVALID = TABLE_COUNT,

    CHARACTER_COUNT = 0x30,
    CUSTOM_CHARACTER_NAME_BMG_START = UI::BMG_CUSTOM_CHARACTER_NAME_START,
    CUSTOM_CHARACTER_AUTHOR_BMG_START = UI::BMG_CUSTOM_CHARACTER_AUTHOR_START,
    MENU_DRIVER_MODEL_COUNT = 0x18,
    LOCAL_PLAYER_COUNT = 4,
    ONLINE_PLAYER_COUNT = 12,
    MII_C_COUNT = 6
};

extern "C" const char* characterNames[];

static_assert(TABLE_COUNT <= (1 << PACKET_BITS), "SELECT packet skin table field is too small");
static_assert(PACKET_BITS * 2 <= 16, "SELECT packet skin table fields must fit in two bytes");

// Raw model cache used when a loose BRRES replaces the disc archive model.
struct RawBRRES {
    EGG::ExpHeap* heap;
    void* file;
    bool failed;
    bool bound;
};

// File scan result for loose BRRES minimap icons.
struct RawTPL {
    bool failed;
};

// Cached loose voice state for one character/table pair.
struct LooseVoiceInfo {
    bool scanned;
    bool hasFiles;
    bool silent;
    CharacterId voiceCharacter;
    u32 suffixMask;
};

// Distinguishes missing BMG ids from intentional blank text.
enum BmgTextState {
    BMG_TEXT_MISSING,
    BMG_TEXT_BLANK,
    BMG_TEXT_NONBLANK
};

// Maps vanilla voice groups back to their owning character.
struct VoiceGroupBase {
    CharacterId character;
    u32 groupId;
};

// Maps voice suffixes to character ids.
struct CharacterNameMap {
    const char* name;
    CharacterId character;
};

enum { AUTHOR_NAME_CONTROL_WORDS = (sizeof(CharaName) + sizeof(u32) - 1) / sizeof(u32) };

// Shared state owned by the CustomCharacters implementation files.
extern u8 selectedTable[CHARACTER_COUNT];
extern u8 onlineCharacterTables[ONLINE_PLAYER_COUNT];
extern u8 offlineCpuCharacterTables[ONLINE_PLAYER_COUNT];
extern const char* defaultNames[CHARACTER_COUNT];
extern bool cachedDefaultNames;
extern char customPostfixes[CHARACTER_COUNT][TABLE_COUNT][16];
extern u8 customSkinExists[CHARACTER_COUNT][TABLE_COUNT];
extern CharacterId hoveredCharacters[LOCAL_PLAYER_COUNT];
extern RawBRRES rawBRRES[TABLE_COUNT][CHARACTER_COUNT];
extern RawBRRES looseMiiCBRRES[MII_C_COUNT];
extern RawTPL looseMinimapTPL[TABLE_COUNT][CHARACTER_COUNT];
extern const GameScene* rawCacheSceneOwner;
extern u32 offlineCpuSkinSignature;
extern u8 offlineCpuSkinRaceNumber;
extern bool offlineCpuSkinTablesValid;
extern u16 heldToggleButtons[LOCAL_PLAYER_COUNT];
extern u32 authorNameControlStorage[LOCAL_PLAYER_COUNT][AUTHOR_NAME_CONTROL_WORDS];
extern bool authorNameControlConstructed[LOCAL_PLAYER_COUNT];
extern bool authorNameControlLoaded[LOCAL_PLAYER_COUNT];
extern bool loadingAuthorNameControl;
extern CharaName* authorTextControl;
extern u32 authorTextValue;
extern CharaName* characterNameTextControl[LOCAL_PLAYER_COUNT];
extern u32 characterNameTextValue[LOCAL_PLAYER_COUNT];
extern bool characterNameTextOverridden[LOCAL_PLAYER_COUNT];
extern SectionId votingMenuTableSection;
extern bool votingMenuTablesRestored;
extern bool voteRandomMessageBoxKartStateApplied;
extern EGG::ExpHeap* reloadedMenuDriverModelHeaps[MENU_DRIVER_MODEL_COUNT];
extern ModelDirector* reloadedMenuDriverModels[MENU_DRIVER_MODEL_COUNT];
extern ToadetteHair* reloadedMenuDriverModelHairs[MENU_DRIVER_MODEL_COUNT];
extern const GameScene* reloadedMenuDriverModelSceneOwner;
extern MenuDriverModel* reloadedMenuDriverModelOwner;
extern bool forceDefaultMenuDriverBRRES;
extern LooseVoiceInfo looseVoiceInfo[TABLE_COUNT][CHARACTER_COUNT];
extern Audio::CharacterActor* voiceInitActor;
extern const char* const looseVoiceGroupSuffixes[];
extern const char* const looseVoiceTimeAttackGroupSuffixAliases[];
extern const VoiceGroupBase voiceGroupBases[];
extern const CharacterNameMap voiceCharacterNames[];

// Character ids and generated file names.
bool IsCharacter(CharacterId character);
bool IsMiiCharacter(CharacterId character);
const char** CharacterNameEntry(CharacterId character);
const char* GetDefaultCharacterPostfix(CharacterId character);
CharacterId StateCharacter(CharacterId character);
const char* GeneratedCustomPostfix(CharacterId character, u8 table);
CharacterId MenuBRRESCharacter(CharacterId character);
bool HasSkin(CharacterId character, u8 table);
u32 SkinNameBmgId(CharacterId character, u8 table);
u32 SkinAuthorBmgId(CharacterId character, u8 table);
bool SetCustomCharacterNameMessage(LayoutUIControl& control, const char* paneName, u32 bmgId);
bool SetCustomCharacterNameMessage(LayoutUIControl& control, u32 bmgId);
bool SetCustomCharacterAuthorMessage(LayoutUIControl& control, u32 bmgId);
const char* DriverBRRESName(CharacterId character, u8 table);

// Section and selection state.
u8 SectionPlayerCount(const SectionMgr* mgr);
bool ShouldForceDefaultVotingMenuTable();
bool IsLocalMultiplayer();
u8 SelectedTable(CharacterId character);
void ApplySelectedNames();
bool IsCustomCharacterTableActive();
void ResetOnlineCustomCharacterFlags();
bool IsOnlineRoom(const RKNet::Controller* controller);
void ResetAllCharacterTablesToDefault();
void ResetOfflineCpuSkinTablesForSection();
bool IsLocalRacePlayer(u8 playerId);
void RefreshLocalOnlineCustomCharacterFlags();
bool SetSelectedTable(CharacterId character, u8 table);
u8 RaceSkinTable(u8 playerId, CharacterId character);
const char** BeginNameSwap(u8 playerId, CharacterId character, const char*& oldName);
CharacterId PreviewCharacter(u8 hud);
void UpdateOnlineCharacterTablesFromAid(u8 aid, const u8* playerIdToAid, u16 characterTables);
u16 GetLocalOnlineCharacterTables();
bool ShouldUseCustomCharacterForPlayer(u8 playerId);

// Menu, race, and UI text updates.
void CacheHoveredFromSection();
bool SetRaceNameTextIfCustom(LayoutUIControl& control, const char* paneName, u8 playerId);
void UpdateCharacterSelectNameText(Pages::CharacterSelect* page, u8 hud);
void UpdateCharacterSelectAuthorText(Pages::CharacterSelect* page, u8 hud);
void UpdateCurrentCharacterSelectAuthorText(u8 hud);

// Heap and loose asset loading helpers.
void UnlockHeap(EGG::Heap* heap);
bool IsInHeap(const EGG::ExpHeap* heap, const void* ptr);
void DestroyHeap(EGG::ExpHeap*& heap);
void ClearRawCache(RawBRRES& cache, bool destroyHeap);
void SyncRawCachesToCurrentScene();
u8 ResolveMenuTable(CharacterId character);
u32 AlignUp(u32 value, u32 alignment);
bool BuildDriverPath(CharacterId character, u8 table, char* path, u32 pathSize);
bool DiscFileSize(const char* path, u32& size);
void* LoadFileToMainRAM(const char* path, EGG::Heap* heap, EGG::DvdRipper::EAllocDirection allocDirection, u32* outSize);

// Loose voices and menu model reloads.
const char* GetLooseVoicePostfixForGroup(u32 groupId, const char*& groupSuffix, const char*& voiceName);
const LooseVoiceInfo& GetLooseVoiceInfo(CharacterId character, u8 table);
void ReinitMenuDriverModelMgr(u8 hud, CharacterId character);
void ApplyVoteRandomMessageBoxKartState();
void RestoreVotingMenuDriverModels();
bool RandomizeSelectedCharacterTable(CharacterId character);
SectionId CurrentSectionId();
bool IsVotingSection(SectionId section);
bool IsCharacterSelectActive();
bool CycleSkin(CharacterId character, int step);
bool LooseVoiceStemExists(const char* postfix, const char* suffix, const char* voiceName = nullptr);
bool FindLooseSoundEffectPath(u32 fileId, const char* extension, char* path, u32 pathSize, u32* outFileSize = nullptr);

}  // namespace CustomCharacters
}  // namespace Pulsar

#endif
