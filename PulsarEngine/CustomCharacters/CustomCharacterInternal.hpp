#ifndef _PULSAR_CUSTOM_CHARACTERS_INTERNAL_
#define _PULSAR_CUSTOM_CHARACTERS_INTERNAL_

#include <hooks.hpp>
#include <runtimeWrite.hpp>
#include <CustomCharacters/CustomCharacters.hpp>
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
#include <UI/UI.hpp>


namespace Pulsar {
namespace CustomCharacters {

#define ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))

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
    MII_C_COUNT = 6,
    NAME_ENTRY_COUNT = 64,
    NAME_TEXT_LENGTH = 32,
    NAME_FILE_MAX_SIZE = 4096
};

extern "C" const char* characterNames[];

static_assert(TABLE_COUNT <= (1 << PACKET_BITS), "SELECT packet skin table field is too small");
static_assert(PACKET_BITS * 2 <= 16, "SELECT packet skin table fields must fit in two bytes");

struct RawBRRES {
    EGG::ExpHeap* heap;
    void* file;
    bool failed;
    bool bound;
};

struct RawTPL {
    bool failed;
};

struct LooseVoiceInfo {
    bool scanned;
    bool hasFiles;
    bool silent;
    CharacterId voiceCharacter;
    u32 suffixMask;
};

struct NameEntry {
    char id[16];
    char characterName[NAME_TEXT_LENGTH];
    char authorName[NAME_TEXT_LENGTH];
    wchar_t characterNameWide[NAME_TEXT_LENGTH];
    wchar_t authorNameWide[NAME_TEXT_LENGTH];
};

enum BmgTextState {
    BMG_TEXT_MISSING,
    BMG_TEXT_BLANK,
    BMG_TEXT_NONBLANK
};

struct VoiceGroupBase {
    CharacterId character;
    u32 groupId;
};

struct CharacterNameMap {
    const char* name;
    CharacterId character;
};

enum { AUTHOR_NAME_CONTROL_WORDS = (sizeof(CharaName) + sizeof(u32) - 1) / sizeof(u32) };

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
extern u16 heldToggleButtons;
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
extern bool nameEntriesLoaded;
extern LooseVoiceInfo looseVoiceInfo[TABLE_COUNT][CHARACTER_COUNT];
extern NameEntry nameEntries[NAME_ENTRY_COUNT];
extern u32 nameEntryCount;
extern Audio::CharacterActor* voiceInitActor;
extern const char* const looseVoiceGroupSuffixes[];
extern const char* const looseVoiceTimeAttackGroupSuffixAliases[];
extern const VoiceGroupBase voiceGroupBases[];
extern const CharacterNameMap voiceCharacterNames[];

u8 MinLocalPlayers(u32 count);
bool IsCharacter(CharacterId character);
bool IsMiiCharacter(CharacterId character);
const char** CharacterNameEntry(CharacterId character);
void CacheDefaults();
const char* GetDefaultCharacterPostfix(CharacterId character);
CharacterId StateCharacter(CharacterId character);
const char* CustomPostfixBase(CharacterId character);
const char* GeneratedCustomPostfix(CharacterId character, u8 table);
bool CustomDriverFileExists(CharacterId character, u8 table);
CharacterId MenuBRRESCharacter(CharacterId character);
bool HasSkin(CharacterId character, u8 table);
u8 NormalizeTable(CharacterId character, u8 table);
void CopyText(char* dest, u32 destSize, const char* source);
void CopyTextWide(wchar_t* dest, u32 destCount, const char* source);
char* SkipNameTextSpace(char* text);
void TrimNameTextEnd(char* text);
void AddNameEntry(const char* id, const char* characterName, const char* authorName);
void ParseNameEntryLine(char* line);
bool ReadNameEntriesFile(const char* path);
void LoadNameEntries();
const NameEntry* FindNameEntryById(const char* id);
const NameEntry* FindNameEntry(CharacterId character, u8 table);
const char* SkinName(CharacterId character, u8 table);
u32 SkinBmgId(u32 start, CharacterId character, u8 table);
u32 SkinNameBmgId(CharacterId character, u8 table);
u32 SkinAuthorBmgId(CharacterId character, u8 table);
const NameEntry* NameEntryForBmgId(u32 bmgId, bool author);
u32 DefaultNameBmgIdForSkinBmgId(u32 bmgId);
BmgTextState GetBmgTextState(const BMGHolder& holder, u32 bmgId);
BmgTextState GetCustomCharacterBmgTextState(const LayoutUIControl& control, u32 bmgId);
u32 ResolveCustomCharacterNameBmgId(const LayoutUIControl& control, u32 bmgId, const NameEntry* entry);
bool SetNameEntryMessage(LayoutUIControl& control, const char* paneName, const NameEntry& entry, bool author);
bool SetCustomCharacterNameMessage(LayoutUIControl& control, const char* paneName, u32 bmgId);
bool SetCustomCharacterNameMessage(LayoutUIControl& control, u32 bmgId);
bool SetCustomCharacterAuthorMessage(LayoutUIControl& control, u32 bmgId);
const char* DefaultMenuBRRESName(CharacterId character);
const char* DriverBRRESName(CharacterId character, u8 table);
void ApplyName(CharacterId character, u8 table);
u8 SectionPlayerCount(const SectionMgr* mgr);
u8 RacePlayerCount(const RacedataScenario& scenario);
bool IsVotingSection(SectionId section);
SectionId CurrentSectionId();
bool ShouldForceDefaultVotingMenuTable();
bool IsLocalMultiplayer();
u8 SelectedTable(CharacterId character);
void ApplySelectedNames();
bool AnyCustomSkin();
bool IsCustomCharacterTableActive();
void ResetOnlineCustomCharacterFlags();
bool IsOnlineRoom(const RKNet::Controller* controller);
bool DisplayOnlineSkins();
bool IsOnlineMultiLocal(const RKNet::Controller* controller);
void ResetOfflineCpuSkinTables();
void ClearCustomCharacterFileCaches();
void ResetAllCharacterTablesToDefault();
void ResetCharacterTablesOnLooseArchiveOverrideChange();
bool IsOfflineCpuSkinResetSection(SectionId section);
void ResetOfflineCpuSkinTablesForSection();
bool IsLocalRacePlayer(u8 playerId);
void RefreshLocalOnlineCustomCharacterFlags();
bool SetSelectedTable(CharacterId character, u8 table);
bool CycleSkin(CharacterId character, int step);
u8 RaceSkinTable(u8 playerId, CharacterId character);
TicoModel* CreateTicoModelHook(void* memory, DriverController* controller);
const char** BeginNameSwap(u8 playerId, CharacterId character, const char*& oldName);
CharacterId PreviewCharacter(u8 hud);
CharacterId SelectedCharacterForHud(u8 hud);
void UpdateOnlineCharacterTablesFromAid(u8 aid, const u8* playerIdToAid, u16 characterTables);
u16 GetLocalOnlineCharacterTables();
bool ShouldUseCustomCharacterForPlayer(u8 playerId);
bool IsCharacterSelectActive();
void CacheHoveredFromSection();
u32 PreviewAuthorBmgId(u8 hud);
u32 PreviewNameBmgId(u8 hud);
bool IsOnlineRaceMode(GameMode mode);
u32 RaceNameBmgId(u8 playerId);
bool SetRaceNameTextIfCustom(LayoutUIControl& control, const char* paneName, u8 playerId);
void SetRaceCharacterNameHook(LayoutUIControl* control, const char* paneName, u32 bmgId, const Text::Info* info);
bool RaceResultUsesMiiName(const RacedataScenario& scenario, u8 playerId);
void FillRaceResultNameHook(CtrlRaceResult* result, u8 playerId);
void LoadRaceResultHook(CtrlRaceResult* result);
void ResetCharacterSelectNameTextCache();
void RestoreCharacterSelectNameText(CharaName& name, CharacterId character);
void UpdateCharacterSelectNameText(Pages::CharacterSelect* page, u8 hud);
CharaName* GetAuthorNameControl(u8 hud);
void UpdateCharacterSelectAuthorText(Pages::CharacterSelect* page, u8 hud);
void UpdateCurrentCharacterSelectAuthorText(u8 hud);
void SetPaneVisibleIfPresent(LayoutUIControl& control, const char* paneName, bool visible);
void HideAuthorNameDecoration(LayoutUIControl& control);
void PositionAuthorNameControl(LayoutUIControl& control, const LayoutUIControl& characterNameControl);
CharaName* ConstructCharaName(CharaName* name);
void AttachAuthorNameControl(CharaName& name, const char* folderName, const char* ctrName, const char* variant);
void CharacterSelectNameLoadHook(ControlLoader* loader, const char* folderName, const char* ctrName, const char* variant, const char** animNames);
void UnlockHeap(EGG::Heap* heap);
bool IsInHeap(const EGG::ExpHeap* heap, const void* ptr);
void DetachHeapListNodes(nw4r::ut::List* list, const EGG::ExpHeap* heap);
void DetachHeapFromScnMgrs(const EGG::ExpHeap* heap);
void DestroyHeap(EGG::ExpHeap*& heap);
bool IsGameplaySectionLoading();
void ClearRawCache(RawBRRES& cache, bool destroyHeap);
void ClearRawCaches(bool destroyHeap);
void SyncRawCachesToCurrentScene();
u8 ResolveMenuTable(CharacterId character);
u32 AlignUp(u32 value, u32 alignment);
bool BuildDriverPath(CharacterId character, u8 table, char* path, u32 pathSize);
bool DiscFileSize(const char* path, u32& size);
void CopyUpperPostfix(char* dest, u32 destSize, const char* postfix);
bool BuildLooseVoicePath(const char* postfix, const char* suffix, const char* extension, const char* voiceName, char* path, u32 pathSize);
bool LooseVoiceFileExists(const char* postfix, const char* suffix, const char* extension, const char* voiceName = nullptr);
const char* LooseVoiceSuffixForGroupOffset(u32 offset);
bool LooseVoiceStemExists(const char* postfix, const char* suffix, const char* voiceName = nullptr);
bool SilentVoiceMarkerExists(CharacterId character, u8 table, const char* postfix);
const char* VoiceNameForCharacter(CharacterId character);
const LooseVoiceInfo& GetLooseVoiceInfo(CharacterId character, u8 table);
bool LooseVoiceInfoHasSuffix(const LooseVoiceInfo& info, const char* suffix);
bool CharacterHasOnlyBaseVoiceGroup(CharacterId character);
bool FindVoiceGroupBaseCharacter(u32 groupId, CharacterId& character);
bool FindVoiceGroupBase(CharacterId character, u32& groupId);
bool VoiceBaseGroupForTable(CharacterId character, u8 table, u32& groupId);
bool ActorRaceCharacter(const Audio::CharacterActor* actor, CharacterId& character);
bool VoiceBaseGroupForActor(const Audio::CharacterActor* actor, CharacterId& character, u32& groupId, CharacterId& groupCharacter);
CharacterId VoiceBaseCharacterForActor(const Audio::CharacterActor* actor);
Audio::CharacterVoiceActionTable VoiceActionTable(CharacterId character);
Audio::CharacterVoiceActionTable& CharacterActorVoiceActionTableSlot(Audio::CharacterActor& actor);
u16& CharacterActorCharacterSlot(Audio::CharacterActor& actor);
bool ApplyVoiceBaseActionTable(Audio::CharacterActor* actor);
void InitCharacterVoiceRangesHook(Audio::CharacterActor* actor);
void* DriverSoundSetForLinkHook(void* manager, CharacterId character, u32 type);
u32 CharacterVoiceGroupHook(Audio::CharacterActor* actor);
u32 CharacterCannonVoiceGroupHook(Audio::CharacterActor* actor);
u32 CharacterGoalVoiceGroupHook(Audio::CharacterActor* actor, u32 type);
bool FindVoiceGroup(u32 groupId, CharacterId& character, u32& offset);
bool PlayerMatchesVoiceGroupOffset(u8 playerId, u32 offset);
const char* GetLooseVoicePostfixForGroup(u32 groupId, const char*& groupSuffix, const char*& voiceName);
EGG::Heap* RawParentHeap(GameScene& scene, u32 heapSize);
bool BindRawBRRES(nw4r::g3d::ResFile& resFile, const char* path);
bool LoadRawBRRES(void* holder, RawBRRES& cache, const char* path);
RawBRRES* RawCacheForModel(const ModelDirector* model);
bool LoadRawBRRESIntoHeap(void* holder, EGG::ExpHeap* heap, const char* path, u32 fileSize);
u8 MiiCIndex(CharacterId character);
bool TryLoadCustomMenuBRRES(void* holder, CharacterId character);
bool TryLoadLooseMiiCBRRES(void* holder, CharacterId character);
bool TryLoadCustomMenuBRRESIntoHeap(void* holder, CharacterId character, EGG::ExpHeap* heap, bool& fileExists);
bool TryLoadLooseMiiCBRRESIntoHeap(void* holder, CharacterId character, EGG::ExpHeap* heap, bool& fileExists);
u32 LoadMenuDriverBRRESHook(void* holder, CharacterId character);
bool LoadMenuDriverBRRESForReload(void* holder, CharacterId character, EGG::ExpHeap* heap);
bool BuildMinimapTPLPath(CharacterId character, u8 table, char* path, u32 pathSize);
EGG::Heap* MinimapTPLHeap(GameScene& scene, u32 fileSize);
TPLPalettePtr LoadLooseMinimapTPL(CharacterId character, u8 table);
void ReplacePaneTPL(nw4r::lyt::Pane* pane, TPLPalettePtr tpl);
void ApplyLooseMinimapTPL(CtrlRace2DMapCharacter* control);
void InitMinimapCharacterHook(CtrlRace2DMapCharacter* control);
ArchivesHolder* LoadKartArchiveHook(ArchiveMgr* archiveMgr, u8 playerId, KartId kart, CharacterId character, u32 color, u32 type, EGG::Heap* decompressedHeap, EGG::Heap* archiveHeap);
bool RequestLoadKartArchivesImmediate(ArchiveMgr* archiveMgr, u8 hudSlotId, CharacterId character, u32 gamemode);
const char* GetMenuDriverBRRESNameHook(u32 character);
void DetachListNodeIfPresent(nw4r::ut::List* list, void* target);
void DetachModelDirectorFromScnMgrs(ModelDirector* model);
void RemoveInitializedModelDirector(ModelDirector* model);
void DestroyModelDirector(ModelDirector* model);
void ForgetReloadedMenuDriverModelHeaps();
void DestroyReloadedMenuDriverModel(u8 idx, ModelDirector** modelSlot);
void DestroyAllReloadedMenuDriverModels();
void SyncReloadedMenuDriverModelHeaps(MenuDriverModel* models);
void CleanupReloadedMenuDriverModels();
void DestroyMenuModelMgrInstanceHook();
EGG::Allocator** MenuAllocatorSlot();
EGG::Allocator* CreateScnObjAllocator(EGG::Heap* parent);
EGG::ExpHeap* CreateMenuDriverModelHeap(GameScene& scene);
void UnlockMenuModelHeaps(MenuModelMgr& modelMgr);
ModelDirector** MenuDriverModelDirectorSlot(MenuDriverModel* models, u8 idx);
MenuDriverModel* MenuDriverModelSlot(MenuDriverModel* models, u8 idx);
u32& MenuDriverModelStateSlot(MenuDriverModel& model);
ModelTransformator** MenuDriverModelCharSelTransformatorSlot(MenuDriverModel& model);
ModelTransformator** MenuDriverModelOnKartTransformatorSlot(MenuDriverModel& model);
u32& MenuDriverModelIdSlot(MenuDriverModel& model);
KartId& MenuDriverModelVehicleSlot(MenuDriverModel& model);
void ConstructMenuModelBRRESHandle(void* handle);
void DestroyMenuModelBRRESHandle(void* handle);
bool LoadMenuDriverModel(void* handle, ModelDirector* model, CharacterId character);
ToadetteHair* LoadMenuDriverToadetteHair(void* handle, EGG::ExpHeap* heap, ModelDirector* model);
bool IsLoadedMenuDriverModelReady(const ModelDirector* model);
bool LoadReloadedMenuDriverModel(GameScene& scene, ScnMgr& scnMgr, CharacterId character, ModelDirector*& newModel, ToadetteHair*& newHair, EGG::ExpHeap*& newHeap);
bool LoadDefaultReloadedMenuDriverModel(GameScene& scene, ScnMgr& scnMgr, CharacterId character, ModelDirector*& newModel, ToadetteHair*& newHair, EGG::ExpHeap*& newHeap);
void DestroyOldMenuDriverModelForReload(u8 idx, ModelDirector** modelSlot, ModelDirector* oldModel, ToadetteHair** hairSlot, ToadetteHair* oldHair);
void ResetReloadedMenuDriverModel(MenuDriverModel& menuModel, CharacterId character);
bool ReloadMenuDriverModel(MenuDriverModelMgr& driverMgr, CharacterId character);
void ReinitMenuDriverModelMgr(u8 hud, CharacterId character);
KartId SelectedMenuKartForHud(u8 hud);
void ApplyVoteRandomMessageBoxKartState();
void RestoreVotingMenuDriverModels();
bool RandomizeSelectedCharacterTable(CharacterId character);
void ResetCustomCharacterMenuState();
void CharacterSelectHoverHook(Pages::CharacterSelect* page, CtrlMenuCharacterSelect::ButtonDriver* button, u32 buttonId, u8 hud);
ControllerType ControllerForHud(const SectionMgr& mgr, u8 hud);
void SetHintPanes(CharaName& name, ControllerType type, bool visible);
void UpdateHintPanes();
void ToggleInputs(ControllerType type, u16& prevButton, u16& nextButton, u16& prevAction, u16& nextAction);
void EatButton(Input::RealControllerHolder& holder, u16 button, u16 action);
bool ProcessSkinInput();
void MenuSceneSectionUpdateHook(SectionMgr* mgr);


}  // namespace CustomCharacters
}  // namespace Pulsar

#endif
