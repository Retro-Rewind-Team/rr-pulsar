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

    CHARACTER_COUNT = 0x30,
    CUSTOM_CHARACTER_NAME_BMG_START = UI::BMG_CUSTOM_CHARACTER_NAME_START,
    CUSTOM_CHARACTER_AUTHOR_BMG_START = UI::BMG_CUSTOM_CHARACTER_AUTHOR_START,
    MENU_DRIVER_MODEL_COUNT = 0x18,
    LOCAL_PLAYER_COUNT = 4,
    ONLINE_PLAYER_COUNT = 12,
    MII_C_COUNT = 6
};

extern "C" const char *characterNames[];

static_assert(TABLE_COUNT <= (1 << PACKET_BITS), "SELECT packet skin table field is too small");
static_assert(PACKET_BITS * 2 <= 16, "SELECT packet skin table fields must fit in two bytes");

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

// Shared state owned by the CustomCharacters implementation files.
extern u8 selectedTable[CHARACTER_COUNT];
extern u8 onlineCharacterTables[ONLINE_PLAYER_COUNT];
extern u8 offlineCpuCharacterTables[ONLINE_PLAYER_COUNT];
extern CharacterId hoveredCharacters[LOCAL_PLAYER_COUNT];
extern SectionId votingMenuTableSection;
extern bool votingMenuTablesRestored;
extern bool voteRandomMessageBoxKartStateApplied;
extern bool forceDefaultMenuDriverBRRES;

// Character ids and generated file names.
bool IsCharacter(CharacterId character);
bool IsMiiCharacter(CharacterId character);
const char *GetDefaultCharacterPostfix(CharacterId character);
CharacterId StateCharacter(CharacterId character);
const char *GeneratedCustomPostfix(CharacterId character, u8 table);
CharacterId MenuBRRESCharacter(CharacterId character);
bool HasSkin(CharacterId character, u8 table);
u32 SkinNameBmgId(CharacterId character, u8 table);
void SetCustomCharacterNameMessage(LayoutUIControl &control, const char *paneName, u32 bmgId);
void SetCustomCharacterNameMessage(LayoutUIControl &control, u32 bmgId);
bool SetCustomCharacterAuthorMessage(LayoutUIControl &control, u32 bmgId);
const char *DriverBRRESName(CharacterId character, u8 table);

// Section and selection state.
u8 SectionPlayerCount(const SectionMgr *mgr);
bool ShouldForceDefaultVotingMenuTable();
bool IsLocalMultiplayer();
u8 SelectedTable(CharacterId character);
void ApplySelectedNames();
bool IsCustomCharacterTableActive();
void ResetOnlineCustomCharacterFlags();
bool IsOnlineRoom(const RKNet::Controller *controller);
void ResetAllCharacterTablesToDefault();
void ResetOfflineCpuSkinTablesForSection();
void CompactOfflineCpuSkinTable(u8 targetPlayerId, u8 sourcePlayerId);
bool IsLocalRacePlayer(u8 playerId);
void RefreshLocalOnlineCustomCharacterFlags();
bool SetSelectedTable(CharacterId character, u8 table);
u8 RaceSkinTable(u8 playerId, CharacterId character);
CharacterId PreviewCharacter(u8 hud);
void UpdateOnlineCharacterTablesFromAid(u8 aid, const u8 *playerIdToAid, u16 characterTables);
u16 GetLocalOnlineCharacterTables();

// Menu, race, and UI text updates.
bool SetRaceNameTextIfCustom(LayoutUIControl &control, const char *paneName, u8 playerId);
void UpdateCharacterSelectText(u8 hud);

// Heap and loose asset loading helpers.
void SyncRawCachesToCurrentScene();
u8 ResolveMenuTable(CharacterId character);
bool BuildDriverPath(CharacterId character, u8 table, char *path, u32 pathSize);
bool DiscFileSize(const char *path, u32 &size);
void *LoadFileToMainRAM(const char *path, EGG::Heap *heap, EGG::DvdRipper::EAllocDirection allocDirection, u32 *outSize);

// Loose voices and menu model reloads.
const char *GetLooseVoicePostfixForGroup(u32 groupId, const char *&groupSuffix, const char *&voiceName);
const LooseVoiceInfo &GetLooseVoiceInfo(CharacterId character, u8 table);
void ClearLooseVoiceCache();
void ReinitMenuDriverModelMgr(u8 hud, CharacterId character);
void RefreshMenuDriverModel(CharacterId character);
void ApplyVoteRandomMessageBoxKartState();
void RestoreVotingMenuDriverModels();
bool RandomizeSelectedCharacterTable(CharacterId character);
bool IsVotingSection(SectionId section);
bool IsCharacterSelectActive();
bool FindLooseSoundEffectPath(u32 fileId, const char *extension, char *path, u32 pathSize, u32 *outFileSize = nullptr);

}  // namespace CustomCharacters
}  // namespace Pulsar

#endif
