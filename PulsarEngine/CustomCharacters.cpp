#include <runtimeWrite.hpp>
#include <CustomCharacters.hpp>
#include <Settings/Settings.hpp>
#include <MarioKartWii/Archive/ArchiveMgr.hpp>
#include <MarioKartWii/UI/Page/Menu/CharacterSelect.hpp>
#include <MarioKartWii/UI/Page/Other/ModelRenderer.hpp>
#include <MarioKartWii/3D/Model/Menu/MenuDriverModel.hpp>
#include <MarioKartWii/3D/Model/Menu/MenuKartModel.hpp>
#include <MarioKartWii/3D/Model/Menu/MenuModelMgr.hpp>
#include <MarioKartWii/Driver/Toadette.hpp>
#include <MarioKartWii/Kart/KartLink.hpp>
#include <MarioKartWii/Audio/Actors/RaceActor.hpp>
#include <MarioKartWii/Mii/MiiHeadsModel.hpp>
#include <MarioKartWii/System/Identifiers.hpp>
#include <MarioKartWii/System/Random.hpp>
#include <MarioKartWii/GlobalFunctions.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>
#include <MarioKartWii/Scene/GameScene.hpp>
#include <MarioKartWii/Audio/RSARPlayer.hpp>
#include <MarioKartWii/Input/Controller.hpp>
#include <core/egg/DVD/DvdRipper.hpp>
#include <core/rvl/dvd/dvd.hpp>

namespace Pulsar {
namespace CustomCharacters {

static u8 onlineCharacterTables[12];

enum CustomCharacterTable {
    CUSTOM_CHARACTER_TABLE_DEFAULT = 0,
    CUSTOM_CHARACTER_TABLE_SKIN1 = 1,
    CUSTOM_CHARACTER_TABLE_SKIN2 = 2,
    CUSTOM_CHARACTER_TABLE_SKIN3 = 3,
    CUSTOM_CHARACTER_TABLE_SKIN4 = 4,
    CUSTOM_CHARACTER_TABLE_SKIN5 = 5,
    CUSTOM_CHARACTER_TABLE_COUNT = 6,
    CUSTOM_CHARACTER_TABLE_INVALID = CUSTOM_CHARACTER_TABLE_COUNT
};

#define ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))

struct CharacterOverride {
    CharacterId character;
    u8 tableIdx;
    const char* postfix;
};

// Custom skin rows are keyed by the stock character id and the selected custom table.
// The default table is not mirrored here; it comes from the game's stock name table.
static const CharacterOverride customCharacterAssets[] = {
    {MARIO, CUSTOM_CHARACTER_TABLE_SKIN1, "mr-1"},
    {MARIO, CUSTOM_CHARACTER_TABLE_SKIN2, "mr-2"},
    {BABY_PEACH, CUSTOM_CHARACTER_TABLE_SKIN1, "bpc-1"},
    {BABY_PEACH, CUSTOM_CHARACTER_TABLE_SKIN2, "bpc-2"},
    {WALUIGI, CUSTOM_CHARACTER_TABLE_SKIN1, "wl-1"},
    {WALUIGI, CUSTOM_CHARACTER_TABLE_SKIN2, "wl-2"},
    {BOWSER, CUSTOM_CHARACTER_TABLE_SKIN1, "kp-1"},
    {BOWSER, CUSTOM_CHARACTER_TABLE_SKIN2, "kp-2"},
    {BOWSER, CUSTOM_CHARACTER_TABLE_SKIN3, "kp-3"},
    {BOWSER, CUSTOM_CHARACTER_TABLE_SKIN4, "kp-4"},
    {BABY_DAISY, CUSTOM_CHARACTER_TABLE_SKIN1, "bds-1"},
    {BABY_DAISY, CUSTOM_CHARACTER_TABLE_SKIN2, "bds-2"},
    {DRY_BONES, CUSTOM_CHARACTER_TABLE_SKIN1, "ka-1"},
    {DRY_BONES, CUSTOM_CHARACTER_TABLE_SKIN2, "ka-2"},
    {DRY_BONES, CUSTOM_CHARACTER_TABLE_SKIN3, "ka-3"},
    {DRY_BONES, CUSTOM_CHARACTER_TABLE_SKIN4, "ka-4"},
    {BABY_MARIO, CUSTOM_CHARACTER_TABLE_SKIN1, "bmr-1"},
    {BABY_MARIO, CUSTOM_CHARACTER_TABLE_SKIN2, "bmr-2"},
    {BABY_MARIO, CUSTOM_CHARACTER_TABLE_SKIN3, "bmr-3"},
    {BABY_MARIO, CUSTOM_CHARACTER_TABLE_SKIN4, "bmr-4"},
    {LUIGI, CUSTOM_CHARACTER_TABLE_SKIN1, "lg-1"},
    {LUIGI, CUSTOM_CHARACTER_TABLE_SKIN2, "lg-2"},
    {LUIGI, CUSTOM_CHARACTER_TABLE_SKIN3, "lg-3"},
    {LUIGI, CUSTOM_CHARACTER_TABLE_SKIN4, "lg-4"},
    {TOAD, CUSTOM_CHARACTER_TABLE_SKIN1, "ko-1"},
    {TOAD, CUSTOM_CHARACTER_TABLE_SKIN2, "ko-2"},
    {TOAD, CUSTOM_CHARACTER_TABLE_SKIN3, "ko-3"},
    {DONKEY_KONG, CUSTOM_CHARACTER_TABLE_SKIN1, "dk-1"},
    {DONKEY_KONG, CUSTOM_CHARACTER_TABLE_SKIN2, "dk-2"},
    {DONKEY_KONG, CUSTOM_CHARACTER_TABLE_SKIN3, "dk-3"},
    {DONKEY_KONG, CUSTOM_CHARACTER_TABLE_SKIN4, "dk-4"},
    {YOSHI, CUSTOM_CHARACTER_TABLE_SKIN1, "ys-1"},
    {YOSHI, CUSTOM_CHARACTER_TABLE_SKIN2, "ys-2"},
    {YOSHI, CUSTOM_CHARACTER_TABLE_SKIN3, "ys-3"},
    {WARIO, CUSTOM_CHARACTER_TABLE_SKIN1, "wr-1"},
    {WARIO, CUSTOM_CHARACTER_TABLE_SKIN2, "wr-2"},
    {WARIO, CUSTOM_CHARACTER_TABLE_SKIN3, "wr-3"},
    {BABY_LUIGI, CUSTOM_CHARACTER_TABLE_SKIN1, "blg-1"},
    {TOADETTE, CUSTOM_CHARACTER_TABLE_SKIN1, "kk-1"},
    {KOOPA_TROOPA, CUSTOM_CHARACTER_TABLE_SKIN1, "nk-1"},
    {KOOPA_TROOPA, CUSTOM_CHARACTER_TABLE_SKIN2, "nk-2"},
    {DAISY, CUSTOM_CHARACTER_TABLE_SKIN1, "ds-1"},
    {DAISY, CUSTOM_CHARACTER_TABLE_SKIN2, "ds-2"},
    {DAISY, CUSTOM_CHARACTER_TABLE_SKIN3, "ds-3"},
    {PEACH, CUSTOM_CHARACTER_TABLE_SKIN1, "pc-1"},
    {PEACH, CUSTOM_CHARACTER_TABLE_SKIN2, "pc-2"},
    {PEACH, CUSTOM_CHARACTER_TABLE_SKIN3, "pc-3"},
    {BIRDO, CUSTOM_CHARACTER_TABLE_SKIN1, "ca-1"},
    {BIRDO, CUSTOM_CHARACTER_TABLE_SKIN2, "ca-2"},
    {DIDDY_KONG, CUSTOM_CHARACTER_TABLE_SKIN1, "dd-1"},
    {DIDDY_KONG, CUSTOM_CHARACTER_TABLE_SKIN2, "dd-2"},
    {KING_BOO, CUSTOM_CHARACTER_TABLE_SKIN1, "kt-1"},
    {BOWSER_JR, CUSTOM_CHARACTER_TABLE_SKIN1, "jr-1"},
    {BOWSER_JR, CUSTOM_CHARACTER_TABLE_SKIN2, "jr-2"},
    {BOWSER_JR, CUSTOM_CHARACTER_TABLE_SKIN3, "jr-3"},
    {BOWSER_JR, CUSTOM_CHARACTER_TABLE_SKIN4, "jr-4"},
    {BOWSER_JR, CUSTOM_CHARACTER_TABLE_SKIN5, "jr-5"},
    {DRY_BOWSER, CUSTOM_CHARACTER_TABLE_SKIN1, "bk-1"},
    {FUNKY_KONG, CUSTOM_CHARACTER_TABLE_SKIN1, "fk-1"},
    {FUNKY_KONG, CUSTOM_CHARACTER_TABLE_SKIN2, "fk-2"},
    {FUNKY_KONG, CUSTOM_CHARACTER_TABLE_SKIN3, "fk-3"},
    {FUNKY_KONG, CUSTOM_CHARACTER_TABLE_SKIN4, "fk-4"},
    {ROSALINA, CUSTOM_CHARACTER_TABLE_SKIN1, "rs-1"},
    {ROSALINA, CUSTOM_CHARACTER_TABLE_SKIN2, "rs-2"},
    {ROSALINA, CUSTOM_CHARACTER_TABLE_SKIN3, "rs-3"},
    {PEACH_BIKER, CUSTOM_CHARACTER_TABLE_SKIN1, "pc-1"},
    {PEACH_BIKER, CUSTOM_CHARACTER_TABLE_SKIN2, "pc-2"},
    {PEACH_BIKER, CUSTOM_CHARACTER_TABLE_SKIN3, "pc-3"},
    {DAISY_BIKER, CUSTOM_CHARACTER_TABLE_SKIN1, "ds-1"},
    {DAISY_BIKER, CUSTOM_CHARACTER_TABLE_SKIN2, "ds-2"},
    {DAISY_BIKER, CUSTOM_CHARACTER_TABLE_SKIN3, "ds-3"},
    {ROSALINA_BIKER, CUSTOM_CHARACTER_TABLE_SKIN1, "rs-1"},
    {ROSALINA_BIKER, CUSTOM_CHARACTER_TABLE_SKIN2, "rs-2"},
    {ROSALINA_BIKER, CUSTOM_CHARACTER_TABLE_SKIN3, "rs-3"},
};

// Runtime state is split between three jobs:
// 1. per-character skin table selection,
// 2. online sync for remote players,
// 3. menu-driver preview rebuilds for custom menu BRRES files.
static const u8 CUSTOM_CHARACTER_COUNT = 0x30;
static const u8 MAX_LOCAL_PLAYERS = 4;
static const u8 MAX_ONLINE_PLAYERS = 12;
static const u8 MENU_DRIVER_MII_SLOTS_PER_PLAYER = 3;
static const u8 MAX_MENU_DRIVER_MODEL_COUNT = 0x18 + MAX_LOCAL_PLAYERS * MENU_DRIVER_MII_SLOTS_PER_PLAYER;
static const u8 MENU_DRIVER_MODEL_SIZE = 0x28;
static const u8 CUSTOM_CHARACTER_TABLE_PACKET_BITS = 3;
static const u8 CUSTOM_CHARACTER_TABLE_PACKET_MASK = (1 << CUSTOM_CHARACTER_TABLE_PACKET_BITS) - 1;
static const u8 CUSTOM_CHARACTER_TABLE_PACKET_COUNT = 1 << CUSTOM_CHARACTER_TABLE_PACKET_BITS;
static u8 selectedCharacterTableByCharacter[CUSTOM_CHARACTER_COUNT];
static u8 pendingMenuDriverReinitFrames = 0;
static u8 menuDriverReinitCharSelectWaitAttempts = 0;
// Last P1 preview character the menu driver heap was built for (early-out must not skip char changes on same table).
static CharacterId currentMenuDriverModelHud0Character = CHARACTER_NONE;
// Apply after previous frame's draw: FUN_80562888 walks ModelDirector lists; tearing down at the
// end of MenuUpdate runs before the same frame's Gfx pass and can corrupt nw4r::ut::List links.
static bool applyPendingMenuDriverModelReinit = false;
static bool applyVotingVRMenuDriverReinit = false;
static EGG::ExpHeap* rawMenuDriverBRRESHeaps[CUSTOM_CHARACTER_TABLE_COUNT][CUSTOM_CHARACTER_COUNT] = {};
static void* rawMenuDriverBRRESFiles[CUSTOM_CHARACTER_TABLE_COUNT][CUSTOM_CHARACTER_COUNT] = {};
static bool rawMenuDriverBRRESLoadFailed[CUSTOM_CHARACTER_TABLE_COUNT][CUSTOM_CHARACTER_COUNT] = {};
static EGG::ExpHeap* activeMenuDriverModelHeap = nullptr;
static EGG::ExpHeap* createdMenuDriverModelHeap = nullptr;
static MenuModelMgr* cachedMenuModelMgr = nullptr;
static u8 cachedMenuDriverModelPlayerCount = 0;
static u8 currentMenuDriverModelTable = CUSTOM_CHARACTER_TABLE_INVALID;
static u8 buildingMenuDriverModelTable = CUSTOM_CHARACTER_TABLE_INVALID;
static CharacterId currentMenuDriverModelCharacter = CHARACTER_NONE;
static CharacterId buildingMenuDriverModelCharacter = CHARACTER_NONE;
// Character-select hover state can briefly diverge from the committed section params.
static CharacterId hoveredCharacterByHud[MAX_LOCAL_PLAYERS] = {MARIO, MARIO, MARIO, MARIO};
static const MenuDriverModel::State MENUDRIVERMODEL_STATE_VEHICLE_SELECTED = static_cast<MenuDriverModel::State>(3);
static const Section* votingVRMenuDriverSection = nullptr;
static bool votingVRMenuDriverSawActive = false;
static bool votingVRMenuDriverExited = false;
static bool votingVRMenuDriverReinitialized = false;
static bool votingVRModelRendererPrimed = false;
static u8 votingVRMenuDriverReinitFrames = 0;
static u8 votingVRMenuDriverStateRefreshFrames = 0;
static u16 heldCustomCharacterToggleButtons = 0;
static_assert(CUSTOM_CHARACTER_TABLE_COUNT <= CUSTOM_CHARACTER_TABLE_PACKET_COUNT, "character table packet encoding cannot fit every table");
static_assert(CUSTOM_CHARACTER_TABLE_PACKET_BITS * 2 <= 8, "character table packet field is one byte");

kmRuntimeUse(0x808b3a90);
static const u32 CHARACTER_POSTFIX_TABLE_ADDRESS = kmRuntimeAddr(0x808b3a90);
static const char* defaultCharacterPostfixes[CUSTOM_CHARACTER_COUNT] = {};
static bool defaultCharacterPostfixesCached = false;

static u8 ClampLocalPlayerCount(u32 playerCount) {
    return playerCount > MAX_LOCAL_PLAYERS ? MAX_LOCAL_PLAYERS : static_cast<u8>(playerCount);
}

static bool IsCharacterIdInRange(CharacterId character) {
    return character >= 0 && character < CUSTOM_CHARACTER_COUNT;
}

static u8 GetSectionLocalPlayerCount(const SectionMgr& sectionMgr) {
    if (sectionMgr.sectionParams == nullptr) return 0;
    return ClampLocalPlayerCount(sectionMgr.sectionParams->localPlayerCount);
}

static u8 GetRaceLocalPlayerCount(const RacedataScenario& scenario) {
    return ClampLocalPlayerCount(scenario.localPlayerCount);
}

static void CacheHoveredCharactersFromSection(const SectionMgr& sectionMgr) {
    if (sectionMgr.sectionParams == nullptr) return;
    const u8 localPlayerCount = GetSectionLocalPlayerCount(sectionMgr);
    for (u8 hudSlotId = 0; hudSlotId < localPlayerCount; ++hudSlotId) {
        hoveredCharacterByHud[hudSlotId] = sectionMgr.sectionParams->characters[hudSlotId];
    }
}

static const char** GetCharacterPostfixEntry(CharacterId character) {
    if (!IsCharacterIdInRange(character)) return nullptr;

    const char** table = reinterpret_cast<const char**>(CHARACTER_POSTFIX_TABLE_ADDRESS);
    return &table[character];
}

static void CacheDefaultCharacterPostfixes() {
    if (defaultCharacterPostfixesCached) return;

    for (u32 character = 0; character < CUSTOM_CHARACTER_COUNT; ++character) {
        const char** const entry = GetCharacterPostfixEntry(static_cast<CharacterId>(character));
        const char* defaultPostfix = nullptr;
        if (entry != nullptr) defaultPostfix = *entry;
        defaultCharacterPostfixes[character] = defaultPostfix;
    }
    defaultCharacterPostfixesCached = true;
}

static CharacterId GetCustomCharacterStateId(CharacterId character) {
    switch (character) {
        case PEACH_BIKER:
            return PEACH;
        case DAISY_BIKER:
            return DAISY;
        case ROSALINA_BIKER:
            return ROSALINA;
        case MII_M:
        case MII_S:
        case MII_L:
            return CHARACTER_NONE;
        default:
            return IsCharacterIdInRange(character) ? character : CHARACTER_NONE;
    }
}

static bool IsCustomCharacterStateIdValid(CharacterId character) {
    return IsCharacterIdInRange(GetCustomCharacterStateId(character));
}

static const CharacterOverride* GetCharacterOverride(CharacterId character, u8 tableIdx) {
    if (tableIdx == CUSTOM_CHARACTER_TABLE_DEFAULT || tableIdx >= CUSTOM_CHARACTER_TABLE_COUNT) return nullptr;

    for (u32 i = 0; i < ARRAY_COUNT(customCharacterAssets); ++i) {
        const CharacterOverride& characterOverride = customCharacterAssets[i];
        if (characterOverride.character == character && characterOverride.tableIdx == tableIdx) return &characterOverride;
    }
    return nullptr;
}

static const char* GetDefaultMenuDriverBRRESName(CharacterId character) {
    switch (character) {
        case PEACH_BIKER:
            return "pc_menu";
        case DAISY_BIKER:
            return "ds_menu";
        case ROSALINA_BIKER:
            return "rs_menu";
        default:
            return nullptr;
    }
}

static bool IsCharacterTableValidForCharacter(CharacterId character, u8 tableIdx) {
    if (!IsCustomCharacterStateIdValid(character)) return false;
    if (tableIdx == CUSTOM_CHARACTER_TABLE_DEFAULT) return true;
    return GetCharacterOverride(character, tableIdx) != nullptr;
}

// Some characters only exist in a subset of the custom tables, so every table read
// gets normalized back to vanilla unless that character actually has an override there.
static u8 NormalizeCharacterTable(CharacterId character, u8 tableIdx) {
    if (IsCharacterTableValidForCharacter(character, tableIdx)) return tableIdx;
    return CUSTOM_CHARACTER_TABLE_DEFAULT;
}

static bool IsLocalMultiplayerActive() {
    const SectionMgr* const sectionMgr = SectionMgr::sInstance;
    if (sectionMgr != nullptr && sectionMgr->sectionParams != nullptr) {
        return GetSectionLocalPlayerCount(*sectionMgr) > 1;
    }

    const Racedata* const racedata = Racedata::sInstance;
    if (racedata != nullptr) {
        if (GetRaceLocalPlayerCount(racedata->racesScenario) > 1) return true;
        if (GetRaceLocalPlayerCount(racedata->menusScenario) > 1) return true;
    }

    return GetLocalPlayerCount() > 1;
}

static u8 GetSelectedCharacterTable(CharacterId character) {
    if (IsLocalMultiplayerActive()) return CUSTOM_CHARACTER_TABLE_DEFAULT;
    if (!IsCustomCharacterStateIdValid(character)) return CUSTOM_CHARACTER_TABLE_DEFAULT;

    const u8 tableIdx = selectedCharacterTableByCharacter[GetCustomCharacterStateId(character)];
    return NormalizeCharacterTable(character, tableIdx);
}

const char* GetDefaultCharacterPostfix(CharacterId character) {
    if (!IsCharacterIdInRange(character)) return nullptr;
    CacheDefaultCharacterPostfixes();
    return defaultCharacterPostfixes[character];
}

static bool IsCustomCharacterEnabled(CharacterId character) {
    return GetSelectedCharacterTable(character) != CUSTOM_CHARACTER_TABLE_DEFAULT;
}

static bool IsAnyCustomCharacterEnabled() {
    if (IsLocalMultiplayerActive()) return false;
    for (u8 character = 0; character < CUSTOM_CHARACTER_COUNT; ++character) {
        if (selectedCharacterTableByCharacter[character] != CUSTOM_CHARACTER_TABLE_DEFAULT) return true;
    }
    return false;
}

static const char* GetCharacterPostfix(CharacterId character, u8 tableIdx) {
    const CharacterOverride* characterOverride = GetCharacterOverride(character, tableIdx);
    if (characterOverride != nullptr && characterOverride->postfix != nullptr) return characterOverride->postfix;
    return GetDefaultCharacterPostfix(character);
}

static const char* GetDriverBRRESName(CharacterId character, u8 tableIdx) {
    const CharacterOverride* characterOverride = GetCharacterOverride(character, tableIdx);
    if (characterOverride != nullptr) {
        return characterOverride->postfix;
    }

    const char* const defaultDriverBrresName = GetDefaultMenuDriverBRRESName(character);
    if (defaultDriverBrresName != nullptr) return defaultDriverBrresName;
    return GetDefaultCharacterPostfix(character);
}

static void ApplyCharacterPostfix(CharacterId character, u8 tableIdx) {
    const char** entry = GetCharacterPostfixEntry(character);
    if (entry == nullptr) return;

    const char* postfix = GetCharacterPostfix(character, tableIdx);
    if (postfix != nullptr) *entry = postfix;
}

static void ApplyCharacterPostfixes() {
    CacheDefaultCharacterPostfixes();
    for (u32 character = 0; character < CUSTOM_CHARACTER_COUNT; ++character) {
        ApplyCharacterPostfix(static_cast<CharacterId>(character), GetSelectedCharacterTable(static_cast<CharacterId>(character)));
    }
}

void ResetOnlineCustomCharacterFlags() {
    for (u8 playerId = 0; playerId < MAX_ONLINE_PLAYERS; ++playerId) {
        onlineCharacterTables[playerId] = CUSTOM_CHARACTER_TABLE_DEFAULT;
    }
}

bool IsOnlineRoom(const RKNet::Controller* controller) {
    if (controller == nullptr) return false;

    switch (controller->roomType) {
        case RKNet::ROOMTYPE_VS_WW:
        case RKNet::ROOMTYPE_VS_REGIONAL:
        case RKNet::ROOMTYPE_BT_WW:
        case RKNet::ROOMTYPE_BT_REGIONAL:
        case RKNet::ROOMTYPE_FROOM_HOST:
        case RKNet::ROOMTYPE_FROOM_NONHOST:
        case RKNet::ROOMTYPE_JOINING_WW:
        case RKNet::ROOMTYPE_JOINING_REGIONAL:
            return true;
        default:
            return false;
    }
}

bool IsOnlineMultiLocal(const RKNet::Controller* controller) {
    return IsOnlineRoom(controller) && IsLocalMultiplayerActive();
}

bool isDisplayCustomSkinsEnabled() {
    return Pulsar::Settings::Mgr::Get().GetUserSettingValue(Pulsar::Settings::SETTINGSTYPE_ONLINE, Pulsar::RADIO_DISPLAYCUSTOMSKINS) ==
           Pulsar::DISPLAYCUSTOMSKINS_ENABLED;
}

void RefreshLocalOnlineCustomCharacterFlags() {
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    if (!IsOnlineRoom(controller)) return;
    if (IsLocalMultiplayerActive()) {
        ResetOnlineCustomCharacterFlags();
        return;
    }
    if (!isDisplayCustomSkinsEnabled()) return;

    const Racedata* racedata = Racedata::sInstance;
    if (racedata == nullptr) return;

    const RacedataScenario& scenario = racedata->racesScenario;
    const u8 localPlayerCount = GetRaceLocalPlayerCount(scenario);
    for (u8 hudSlotId = 0; hudSlotId < localPlayerCount; ++hudSlotId) {
        const u32 playerId = racedata->GetPlayerIdOfLocalPlayer(hudSlotId);
        if (playerId < MAX_ONLINE_PLAYERS) {
            onlineCharacterTables[playerId] = GetSelectedCharacterTable(scenario.players[playerId].characterId);
        }
    }
}

static bool SetCustomCharacterTable(CharacterId character, u8 tableIdx) {
    if (!IsCustomCharacterStateIdValid(character)) return false;
    if (!IsCharacterTableValidForCharacter(character, tableIdx)) return false;

    const CharacterId stateCharacter = GetCustomCharacterStateId(character);
    if (selectedCharacterTableByCharacter[stateCharacter] == tableIdx) return false;

    selectedCharacterTableByCharacter[stateCharacter] = tableIdx;
    ApplyCharacterPostfixes();
    RefreshLocalOnlineCustomCharacterFlags();
    return true;
}

static bool CycleCustomCharacterTable(CharacterId character, int direction) {
    if (GetLocalPlayerCount() != 1) return false;
    if (!IsCustomCharacterStateIdValid(character)) return false;
    if (CUSTOM_CHARACTER_TABLE_COUNT <= 1) return false;

    u8 tableIdx = GetSelectedCharacterTable(character);
    for (u8 i = 1; i < CUSTOM_CHARACTER_TABLE_COUNT; ++i) {
        if (direction < 0) {
            tableIdx = tableIdx == 0 ? CUSTOM_CHARACTER_TABLE_COUNT - 1 : tableIdx - 1;
        } else {
            tableIdx = tableIdx + 1 >= CUSTOM_CHARACTER_TABLE_COUNT ? CUSTOM_CHARACTER_TABLE_DEFAULT : tableIdx + 1;
        }

        // Some characters only have entries in a subset of skin tables.
        if (!IsCharacterTableValidForCharacter(character, tableIdx)) continue;
        if (!SetCustomCharacterTable(character, tableIdx)) return false;
        pendingMenuDriverReinitFrames = 2;
        return true;
    }
    return false;
}

static bool IsMiiCharacter(CharacterId character) {
    return character >= MII_S_A_MALE && character <= MII_L_C_FEMALE;
}

static void EnsureActiveCustomCharacterTable() {
    ApplyCharacterPostfixes();
}

bool IsCustomCharacterTableActive() {
    return IsAnyCustomCharacterEnabled();
}

static CharacterId GetMenuDriverBRRESCharacter(CharacterId character) {
    switch (character) {
        case PEACH:
            return PEACH_BIKER;
        case DAISY:
            return DAISY_BIKER;
        case ROSALINA:
            return ROSALINA_BIKER;
        default:
            return character;
    }
}

static void ApplyMenuDriverModelTablePostfixes(CharacterId targetCharacter, u8 tableIdx) {
    // The menu-driver constructor should see a mostly vanilla postfix table, with only
    // the target preview character temporarily redirected to its custom menu-scene BRRES.
    CacheDefaultCharacterPostfixes();
    for (u32 character = 0; character < CUSTOM_CHARACTER_COUNT; ++character) {
        ApplyCharacterPostfix(static_cast<CharacterId>(character), CUSTOM_CHARACTER_TABLE_DEFAULT);
    }
    if (tableIdx == CUSTOM_CHARACTER_TABLE_DEFAULT || tableIdx >= CUSTOM_CHARACTER_TABLE_COUNT) {
        return;
    }

    const CharacterId menuCharacter = GetMenuDriverBRRESCharacter(targetCharacter);
    ApplyCharacterPostfix(menuCharacter, NormalizeCharacterTable(menuCharacter, tableIdx));
}

bool IsLocalRacePlayer(u8 playerId) {
    const Racedata* racedata = Racedata::sInstance;
    if (racedata == nullptr) return false;

    const RacedataScenario& scenario = racedata->racesScenario;
    const u8 localPlayerCount = GetRaceLocalPlayerCount(scenario);

    for (u8 hudSlotId = 0; hudSlotId < localPlayerCount; ++hudSlotId) {
        const u32 localPlayerId = racedata->GetPlayerIdOfLocalPlayer(hudSlotId);
        if (localPlayerId == playerId) return true;
    }
    return false;
}

static u8 ResolveOnlineCharacterTable(u8 playerId, CharacterId character) {
    if (playerId >= MAX_ONLINE_PLAYERS) return CUSTOM_CHARACTER_TABLE_DEFAULT;
    return NormalizeCharacterTable(character, onlineCharacterTables[playerId]);
}

static u8 GetRaceCharacterTable(u8 playerId, CharacterId character) {
    if (IsLocalMultiplayerActive()) return CUSTOM_CHARACTER_TABLE_DEFAULT;

    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    if (IsOnlineRoom(controller) && !IsOnlineMultiLocal(controller) && isDisplayCustomSkinsEnabled()) {
        if (IsLocalRacePlayer(playerId)) return GetSelectedCharacterTable(character);
        return ResolveOnlineCharacterTable(playerId, character);
    }
    return IsLocalRacePlayer(playerId) ? GetSelectedCharacterTable(character) : CUSTOM_CHARACTER_TABLE_DEFAULT;
}

bool ShouldMuteCharacterVoice(const Kart::Link* link) {
    const Racedata* racedata = Racedata::sInstance;
    if (link == nullptr || racedata == nullptr) return false;

    const u8 playerId = link->GetPlayerIdx();
    if (playerId >= racedata->racesScenario.playerCount) return false;

    const CharacterId character = racedata->racesScenario.players[playerId].characterId;
    return GetRaceCharacterTable(playerId, character) >= CUSTOM_CHARACTER_TABLE_SKIN2;
}

class ScopedCharacterPostfixSwap {
   public:
    ScopedCharacterPostfixSwap(u8 playerId, CharacterId character) : entry(nullptr), previousValue(nullptr) {
        this->entry = GetCharacterPostfixEntry(character);
        if (this->entry == nullptr) return;

        this->previousValue = *this->entry;
        const u8 tableIdx = GetRaceCharacterTable(playerId, character);
        const char* postfix = GetCharacterPostfix(character, tableIdx);
        if (postfix != nullptr) {
            *this->entry = postfix;
        } else {
            *this->entry = GetDefaultCharacterPostfix(character);
        }
    }

    ~ScopedCharacterPostfixSwap() {
        if (this->entry != nullptr) *this->entry = this->previousValue;
    }

   private:
    const char** entry;
    const char* previousValue;
};

static void ResetVotingVRMenuDriverFlags() {
    votingVRMenuDriverSawActive = false;
    votingVRMenuDriverExited = false;
    votingVRMenuDriverReinitialized = false;
    votingVRModelRendererPrimed = false;
    votingVRMenuDriverReinitFrames = 0;
    votingVRMenuDriverStateRefreshFrames = 0;
}

static void ResetVotingVRMenuDriverState() {
    ResetVotingVRMenuDriverFlags();
    votingVRMenuDriverSection = nullptr;
    applyVotingVRMenuDriverReinit = false;
}

static CharacterId GetPreviewCharacterForHud(u8 hudSlotId) {
    if (hudSlotId >= MAX_LOCAL_PLAYERS) return MARIO;

    const SectionMgr* const sectionMgr = SectionMgr::sInstance;
    if (sectionMgr == nullptr || sectionMgr->sectionParams == nullptr) {
        return hoveredCharacterByHud[hudSlotId];
    }

    CharacterId previewCharacter = hoveredCharacterByHud[hudSlotId];
    if (!IsCharacterIdInRange(previewCharacter)) {
        previewCharacter = sectionMgr->sectionParams->characters[hudSlotId];
    }
    return previewCharacter;
}

static CharacterId GetSelectedCharacterForHud(u8 hudSlotId) {
    if (hudSlotId >= MAX_LOCAL_PLAYERS) return MARIO;

    const SectionMgr* const sectionMgr = SectionMgr::sInstance;
    if (sectionMgr != nullptr && sectionMgr->sectionParams != nullptr &&
        hudSlotId < sectionMgr->sectionParams->localPlayerCount) {
        return sectionMgr->sectionParams->characters[hudSlotId];
    }

    const Racedata* const racedata = Racedata::sInstance;
    if (racedata != nullptr && hudSlotId < racedata->menusScenario.localPlayerCount) {
        const u8 playerId = racedata->menusScenario.settings.hudPlayerIds[hudSlotId];
        if (playerId < MAX_ONLINE_PLAYERS) return racedata->menusScenario.players[playerId].characterId;
    }

    return hoveredCharacterByHud[hudSlotId];
}

static bool IsCharacterSelectPageActive() {
    const SectionMgr* sectionMgr = SectionMgr::sInstance;
    if (sectionMgr == nullptr || sectionMgr->curSection == nullptr) return false;

    const Pages::CharacterSelect* characterSelect = sectionMgr->curSection->Get<Pages::CharacterSelect>();
    if (characterSelect == nullptr) return false;

    const Page* topPage = sectionMgr->curSection->GetTopLayerPage();
    return topPage == characterSelect && characterSelect->currentState == STATE_ACTIVE && !characterSelect->updateState;
}

static void RefreshCharacterSelectModels() {
    SectionMgr* const sectionMgr = SectionMgr::sInstance;
    if (sectionMgr == nullptr || sectionMgr->sectionParams == nullptr || sectionMgr->curSection == nullptr) return;

    Pages::CharacterSelect* const characterSelect = sectionMgr->curSection->Get<Pages::CharacterSelect>();
    if (characterSelect == nullptr || characterSelect->models == nullptr) return;

    const u8 localPlayerCount = GetSectionLocalPlayerCount(*sectionMgr);
    for (u8 hudSlotId = 0; hudSlotId < localPlayerCount; ++hudSlotId) {
        characterSelect->models[hudSlotId].RequestModel(GetPreviewCharacterForHud(hudSlotId));
    }
}

static void UnlockHeap(EGG::Heap* heap) {
    if (heap != nullptr) heap->dameFlag &= ~0x1;
}

static void ResetRawMenuDriverBRRESCacheEntry(u8 tableIdx, CharacterId character) {
    if (tableIdx >= CUSTOM_CHARACTER_TABLE_COUNT || !IsCharacterIdInRange(character)) return;
    rawMenuDriverBRRESHeaps[tableIdx][character] = nullptr;
    rawMenuDriverBRRESFiles[tableIdx][character] = nullptr;
    rawMenuDriverBRRESLoadFailed[tableIdx][character] = false;
}

static void ClearRawMenuDriverBRRESCachePointers() {
    for (u32 tableIdx = 0; tableIdx < CUSTOM_CHARACTER_TABLE_COUNT; ++tableIdx) {
        for (u32 character = 0; character < CUSTOM_CHARACTER_COUNT; ++character) {
            ResetRawMenuDriverBRRESCacheEntry(tableIdx, static_cast<CharacterId>(character));
        }
    }
}

static void DestroyMenuDriverModelHeap(EGG::ExpHeap*& heap) {
    if (heap == nullptr) return;
    UnlockHeap(heap);
    heap->destroy();
    heap = nullptr;
}

static void ResetMenuDriverModelCache() {
    pendingMenuDriverReinitFrames = 0;
    applyPendingMenuDriverModelReinit = false;
    menuDriverReinitCharSelectWaitAttempts = 0;
    cachedMenuModelMgr = nullptr;
    cachedMenuDriverModelPlayerCount = 0;
    currentMenuDriverModelTable = IsAnyCustomCharacterEnabled() ? CUSTOM_CHARACTER_TABLE_INVALID : CUSTOM_CHARACTER_TABLE_DEFAULT;
    buildingMenuDriverModelTable = CUSTOM_CHARACTER_TABLE_INVALID;
    currentMenuDriverModelCharacter = CHARACTER_NONE;
    currentMenuDriverModelHud0Character = CHARACTER_NONE;
    buildingMenuDriverModelCharacter = CHARACTER_NONE;
    ResetVotingVRMenuDriverState();
    // Scene / MenuModelMgr teardown already destroys ExpHeaps we allocated under the old scene.
    // Calling destroy() here races section exit (see ResetCustomCharacterMenuState on load): the
    // activeMenuDriverModelHeap is often freed first, so destroying it here can double free.
    activeMenuDriverModelHeap = nullptr;
    createdMenuDriverModelHeap = nullptr;
    ClearRawMenuDriverBRRESCachePointers();
    const SectionMgr* const sectionMgr = SectionMgr::sInstance;
    if (sectionMgr != nullptr) CacheHoveredCharactersFromSection(*sectionMgr);
}

static void SyncMenuDriverModelCache() {
    MenuModelMgr* const menuModelMgr = MenuModelMgr::sInstance;
    if (menuModelMgr == nullptr) {
        ResetMenuDriverModelCache();
        return;
    }

    const bool shouldResetCache = cachedMenuModelMgr != menuModelMgr || cachedMenuDriverModelPlayerCount != menuModelMgr->playerCount;
    if (shouldResetCache) {
        cachedMenuModelMgr = menuModelMgr;
        cachedMenuDriverModelPlayerCount = menuModelMgr->playerCount;
        currentMenuDriverModelTable = IsAnyCustomCharacterEnabled() ? CUSTOM_CHARACTER_TABLE_INVALID : CUSTOM_CHARACTER_TABLE_DEFAULT;
        currentMenuDriverModelCharacter = CHARACTER_NONE;
        currentMenuDriverModelHud0Character = CHARACTER_NONE;
        ClearRawMenuDriverBRRESCachePointers();
    }
}

static bool IsMenuDriverModelManagerUsable(const MenuDriverModelMgr* driverModelMgr, u8 playerCount) {
    return driverModelMgr != nullptr &&
           driverModelMgr->models != nullptr &&
           driverModelMgr->playerCount != 0 &&
           driverModelMgr->playerCount <= MAX_LOCAL_PLAYERS &&
           (playerCount == 0 || driverModelMgr->playerCount == playerCount) &&
           driverModelMgr->modelCount == 0x18 + driverModelMgr->playerCount * MENU_DRIVER_MII_SLOTS_PER_PLAYER;
}

static void UnlockMenuDriverModelHeaps(GameScene& scene, MenuModelMgr& menuModelMgr) {
    UnlockHeap(scene.parentHeap);
    UnlockHeap(scene.otherMEMHeap);
    UnlockHeap(scene.mainMEMHeap);
    UnlockHeap(scene.debugHeap);

    for (u32 i = 0; i < 3; ++i) {
        UnlockHeap(scene.structsHeaps.heaps[i]);
        UnlockHeap(scene.archiveHeaps.heaps[i]);
    }

    UnlockHeap(menuModelMgr.heap);
    UnlockHeap(menuModelMgr.otherHeap);
}

static void ClearMenuDriverModels(MenuDriverModelMgr& driverModelMgr) {
    if (driverModelMgr.bangs != nullptr) {
        driverModelMgr.bangs->ToggleVisible(false);
    }

    MiiHeadsModel* const* miiHeads = reinterpret_cast<MiiHeadsModel* const*>(driverModelMgr.miiHeads);
    for (u8 playerId = 0; playerId < driverModelMgr.playerCount; ++playerId) {
        MenuDriverModel* const playerModel = driverModelMgr.players[playerId].playerModel;
        MiiHeadsModel* miiHeadModel = nullptr;
        if (miiHeads != nullptr) {
            miiHeadModel = miiHeads[playerId];
        }
        driverModelMgr.players[playerId].isVisible = false;
        if (playerModel != nullptr && playerModel->model != nullptr) {
            playerModel->ToggleVisible(false);
        }
        if (miiHeadModel != nullptr && miiHeadModel->curScnMdlEx != nullptr) {
            miiHeadModel->ToggleVisible(false);
        }
    }
}

kmRuntimeUse(0x80830180);
static MenuDriverModelMgr* CreateMenuDriverModelManager(u8 playerCount) {
    typedef MenuDriverModelMgr* (*CreateMenuDriverModelManagerFn)(MenuDriverModelMgr*, u8);
    GameScene* const currentScene = const_cast<GameScene*>(GameScene::GetCurrent());
    EGG::ExpHeap* modelHeap = nullptr;
    EGG::ExpHeap* parentModelHeap = nullptr;
    EGG::ExpHeap* originalStructMem1Heap = nullptr;
    EGG::ExpHeap* originalStructMem2Heap = nullptr;
    EGG::ExpHeap* originalSceneMem1Heap = nullptr;
    EGG::ExpHeap* originalSceneMem2Heap = nullptr;
    EGG::Heap* previousHeap = nullptr;
    createdMenuDriverModelHeap = nullptr;
    if (currentScene != nullptr) {
        parentModelHeap = currentScene->structsHeaps.heaps[1];
        modelHeap = parentModelHeap;
        originalStructMem1Heap = currentScene->structsHeaps.heaps[0];
        originalStructMem2Heap = currentScene->structsHeaps.heaps[1];
        originalSceneMem1Heap = currentScene->expHeapGroup.heaps[0];
        originalSceneMem2Heap = currentScene->expHeapGroup.heaps[1];
    }
    if (parentModelHeap != nullptr) {
        UnlockHeap(parentModelHeap);
        const u32 allocatableSize = parentModelHeap->getAllocatableSize(0x20);
        u32 heapSize = allocatableSize > 0x40000 ? allocatableSize - 0x40000 : allocatableSize;
        if (heapSize > 0x280000) heapSize = 0x280000;
        if (heapSize >= 0x80000) {
            createdMenuDriverModelHeap = EGG::ExpHeap::Create(static_cast<int>(heapSize), parentModelHeap, 0);
            if (createdMenuDriverModelHeap != nullptr) modelHeap = createdMenuDriverModelHeap;
        }
    }

    // Ghidra confirms the retail constructor allocates shared models first and then per-player
    // heads/handles from the inherited scene heaps, so both heap families must be redirected.
    // The vanilla constructor hardcodes structsHeaps[0] for model/scn allocations.
    if (currentScene != nullptr && modelHeap != nullptr) {
        previousHeap = modelHeap->BecomeCurrentHeap();
        currentScene->structsHeaps.heaps[0] = modelHeap;
        currentScene->structsHeaps.heaps[1] = modelHeap;
    }

    // Mii body/head setup uses the inherited scene heap group directly. Redirect it as well
    // so custom menu-driver caches do not exhaust the small globe MEM1 scene heap.
    if (currentScene != nullptr && modelHeap != nullptr) {
        currentScene->expHeapGroup.heaps[0] = modelHeap;
        currentScene->expHeapGroup.heaps[1] = modelHeap;
    }

    EGG::Heap* allocationHeap = modelHeap;

    void* memory = allocationHeap != nullptr ? operator new(sizeof(MenuDriverModelMgr), allocationHeap) : operator new(sizeof(MenuDriverModelMgr));
    if (memory == nullptr) {
        if (currentScene != nullptr && modelHeap != nullptr) {
            currentScene->expHeapGroup.heaps[0] = originalSceneMem1Heap;
            currentScene->expHeapGroup.heaps[1] = originalSceneMem2Heap;
        }
        if (currentScene != nullptr && modelHeap != nullptr) {
            currentScene->structsHeaps.heaps[0] = originalStructMem1Heap;
            currentScene->structsHeaps.heaps[1] = originalStructMem2Heap;
            if (previousHeap != nullptr) previousHeap->BecomeCurrentHeap();
        }
        DestroyMenuDriverModelHeap(createdMenuDriverModelHeap);
        return nullptr;
    }

    const CreateMenuDriverModelManagerFn original = reinterpret_cast<CreateMenuDriverModelManagerFn>(kmRuntimeAddr(0x80830180));
    MenuDriverModelMgr* const manager = original(reinterpret_cast<MenuDriverModelMgr*>(memory), playerCount);

    if (currentScene != nullptr && modelHeap != nullptr) {
        currentScene->expHeapGroup.heaps[0] = originalSceneMem1Heap;
        currentScene->expHeapGroup.heaps[1] = originalSceneMem2Heap;
    }
    if (currentScene != nullptr && modelHeap != nullptr) {
        currentScene->structsHeaps.heaps[0] = originalStructMem1Heap;
        currentScene->structsHeaps.heaps[1] = originalStructMem2Heap;
        if (previousHeap != nullptr) previousHeap->BecomeCurrentHeap();
    }
    if (manager == nullptr) DestroyMenuDriverModelHeap(createdMenuDriverModelHeap);
    return manager;
}

static bool IsVotingSection() {
    const SectionMgr* const sectionMgr = SectionMgr::sInstance;
    if (sectionMgr == nullptr || sectionMgr->curSection == nullptr) return false;

    switch (sectionMgr->curSection->sectionId) {
        case SECTION_P1_WIFI_VS_VOTING:
        case SECTION_P1_WIFI_BATTLE_VOTING:
        case SECTION_P2_WIFI_VS_VOTING:
        case SECTION_P2_WIFI_BATTLE_VOTING:
        case SECTION_P1_WIFI_FROOM_VS_VOTING:
        case SECTION_P1_WIFI_FROOM_TEAMVS_VOTING:
        case SECTION_P1_WIFI_FROOM_BALLOON_VOTING:
        case SECTION_P1_WIFI_FROOM_COIN_VOTING:
        case SECTION_P2_WIFI_FROOM_VS_VOTING:
        case SECTION_P2_WIFI_FROOM_TEAMVS_VOTING:
        case SECTION_P2_WIFI_FROOM_BALLOON_VOTING:
        case SECTION_P2_WIFI_FROOM_COIN_VOTING:
            return true;
        default:
            return false;
    }
}

static bool IsPageActive(PageId pageId) {
    const SectionMgr* const sectionMgr = SectionMgr::sInstance;
    if (sectionMgr == nullptr || sectionMgr->curSection == nullptr) return false;
    const Section* const section = sectionMgr->curSection;
    Page* const page = section->pages[pageId];
    if (page == nullptr) return false;

    for (u32 layerIdx = 0; layerIdx < section->layerCount; ++layerIdx) {
        if (section->activePages[layerIdx] == page) return true;
    }
    return false;
}

static bool IsVRPageActive() {
    return IsPageActive(PAGE_VR);
}

static MenuDriverModel::State GetVotingMenuDriverTargetState(bool isKartVisible) {
    if (!isKartVisible) return MenuDriverModel::MENUDRIVERMODEL_STATE_ONCHARSELECT;

    if (IsPageActive(PAGE_KART_SELECT) ||
        IsPageActive(PAGE_BATTLE_KART_SELECT) ||
        IsPageActive(PAGE_MULTIPLAYER_KART_SELECT)) {
        return MenuDriverModel::MENUDRIVERMODEL_STATE_ONKARTSELECT;
    }

    return MENUDRIVERMODEL_STATE_VEHICLE_SELECTED;
}

static bool SyncVotingVRMenuDriverState() {
    const SectionMgr* const sectionMgr = SectionMgr::sInstance;
    if (sectionMgr == nullptr || sectionMgr->curSection == nullptr || !IsVotingSection()) {
        ResetVotingVRMenuDriverState();
        return false;
    }

    const Section* const section = sectionMgr->curSection;
    if (votingVRMenuDriverSection != section) {
        votingVRMenuDriverSection = section;
        ResetVotingVRMenuDriverFlags();
    }
    return true;
}

static bool ShouldHandleVotingVRMenuDriverModels() {
    const CharacterId targetCharacter = GetPreviewCharacterForHud(0);
    return GetSelectedCharacterTable(targetCharacter) != CUSTOM_CHARACTER_TABLE_DEFAULT;
}

static bool ShouldUseDefaultMenuDriverTableForVotingVR() {
    if (!SyncVotingVRMenuDriverState()) return false;
    if (!ShouldHandleVotingVRMenuDriverModels()) return false;
    if (IsVRPageActive()) votingVRMenuDriverSawActive = true;
    return !votingVRMenuDriverExited;
}

kmRuntimeUse(0x80830748);
static void StartMenuDriverModelManager(MenuDriverModelMgr& driverModelMgr) {
    typedef void (*StartMenuDriverModelManagerFn)(MenuDriverModelMgr*);
    const StartMenuDriverModelManagerFn original = reinterpret_cast<StartMenuDriverModelManagerFn>(kmRuntimeAddr(0x80830748));
    original(&driverModelMgr);
}

static u32 AlignUp(u32 value, u32 alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

static bool ShouldUseRawMenuDriverBRRES(u8 tableIdx) {
    return tableIdx != CUSTOM_CHARACTER_TABLE_DEFAULT && tableIdx < CUSTOM_CHARACTER_TABLE_COUNT;
}

static CharacterId GetMenuDriverCacheCharacter(CharacterId character, u8 tableIdx) {
    return ShouldUseRawMenuDriverBRRES(tableIdx) ? GetMenuDriverBRRESCharacter(character) : CHARACTER_NONE;
}

static u8 ResolveMenuDriverModelTable(CharacterId character) {
    if (ShouldUseDefaultMenuDriverTableForVotingVR()) return CUSTOM_CHARACTER_TABLE_DEFAULT;
    return GetSelectedCharacterTable(character);
}

static u8 GetRawMenuDriverBRRESTable(CharacterId character) {
    const CharacterId menuCharacter = GetMenuDriverBRRESCharacter(character);
    const u8 tableIdx = buildingMenuDriverModelTable < CUSTOM_CHARACTER_TABLE_COUNT ? buildingMenuDriverModelTable : ResolveMenuDriverModelTable(menuCharacter);
    if (!ShouldUseRawMenuDriverBRRES(tableIdx)) return CUSTOM_CHARACTER_TABLE_INVALID;
    if (GetCharacterOverride(menuCharacter, tableIdx) == nullptr) return CUSTOM_CHARACTER_TABLE_INVALID;
    if (buildingMenuDriverModelTable < CUSTOM_CHARACTER_TABLE_COUNT &&
        menuCharacter != GetMenuDriverBRRESCharacter(buildingMenuDriverModelCharacter)) {
        return CUSTOM_CHARACTER_TABLE_INVALID;
    }
    return tableIdx;
}

static bool GetRawMenuDriverBRRESPath(CharacterId character, u8 tableIdx, char* path, u32 pathSize) {
    const char* const brresName = GetDriverBRRESName(character, tableIdx);
    if (brresName == nullptr) return false;

    const int written = snprintf(path, pathSize, "/Scene/Model/Driver/%s.brres", brresName);
    return written > 0 && static_cast<u32>(written) < pathSize;
}

static bool GetDiscFileSize(const char* path, u32& size) {
    DVD::FileInfo fileInfo;
    if (!DVD::Open(path, &fileInfo)) {
        size = 0;
        return false;
    }
    size = fileInfo.length;
    DVD::Close(&fileInfo);
    return size != 0;
}

static u32 CalculateRawMenuDriverBRRESHeapSize(u8 tableIdx, CharacterId character) {
    u32 heapSize = 0x1000;
    const CharacterId menuCharacter = GetMenuDriverBRRESCharacter(character);
    if (GetCharacterOverride(menuCharacter, tableIdx) == nullptr) return heapSize;

    char path[0x60];
    if (!GetRawMenuDriverBRRESPath(menuCharacter, tableIdx, path, sizeof(path))) return heapSize;

    u32 fileSize = 0;
    if (!GetDiscFileSize(path, fileSize)) return heapSize;
    heapSize += AlignUp(fileSize, 0x20) + 0x1000;
    return heapSize;
}

static EGG::Heap* GetRawMenuDriverBRRESParentHeap(GameScene& scene, u32 heapSize) {
    static const u32 PARENT_HEAP_RESERVE = 0x200000;
    EGG::Heap* candidates[] = {
        scene.structsHeaps.heaps[1],
        scene.structsHeaps.heaps[0],
    };

    EGG::Heap* bestHeap = nullptr;
    u32 bestSize = 0;
    for (u32 i = 0; i < ARRAY_COUNT(candidates); ++i) {
        EGG::Heap* const heap = candidates[i];
        if (heap == nullptr) continue;

        UnlockHeap(heap);
        const u32 allocatableSize = heap->getAllocatableSize(0x20);
        if (allocatableSize >= heapSize + PARENT_HEAP_RESERVE) return heap;
        if (allocatableSize > bestSize) {
            bestHeap = heap;
            bestSize = allocatableSize;
        }
    }

    if (bestSize >= heapSize) return bestHeap;
    return nullptr;
}

static EGG::ExpHeap* GetRawMenuDriverBRRESHeap(u8 tableIdx, CharacterId character) {
    if (tableIdx >= CUSTOM_CHARACTER_TABLE_COUNT || !IsCharacterIdInRange(character)) return nullptr;
    if (rawMenuDriverBRRESHeaps[tableIdx][character] != nullptr) {
        return rawMenuDriverBRRESHeaps[tableIdx][character];
    }

    GameScene* const currentScene = const_cast<GameScene*>(GameScene::GetCurrent());
    if (currentScene == nullptr) return nullptr;

    const u32 heapSize = CalculateRawMenuDriverBRRESHeapSize(tableIdx, character);
    EGG::Heap* const parentHeap = GetRawMenuDriverBRRESParentHeap(*currentScene, heapSize);
    if (parentHeap == nullptr) return nullptr;

    rawMenuDriverBRRESHeaps[tableIdx][character] = EGG::ExpHeap::Create(static_cast<int>(heapSize), parentHeap, 0);
    return rawMenuDriverBRRESHeaps[tableIdx][character];
}

kmRuntimeUse(0x8055b7f8);
static void BindRawMenuDriverBRRES(nw4r::g3d::ResFile& resFile, const char* name) {
    typedef void (*BindBRRESImplFn)(nw4r::g3d::ResFile*, const char*, const nw4r::g3d::ResFile*, u32);
    const BindBRRESImplFn bind = reinterpret_cast<BindBRRESImplFn>(kmRuntimeAddr(0x8055b7f8));
    bind(&resFile, name, nullptr, 0);
}

static bool LoadRawMenuDriverBRRES(void* holder, CharacterId character) {
    // Menu-scene custom skins are loaded as raw BRRES blobs so we can swap only the
    // preview driver model without disturbing the regular archive loader state.
    const CharacterId menuCharacter = GetMenuDriverBRRESCharacter(character);
    const u8 tableIdx = GetRawMenuDriverBRRESTable(menuCharacter);
    if (tableIdx >= CUSTOM_CHARACTER_TABLE_COUNT || !IsCharacterIdInRange(menuCharacter)) {
        return false;
    }

    char path[0x60];
    if (!GetRawMenuDriverBRRESPath(menuCharacter, tableIdx, path, sizeof(path))) {
        return false;
    }

    void*& rawFile = rawMenuDriverBRRESFiles[tableIdx][menuCharacter];
    if (rawFile == nullptr) {
        EGG::ExpHeap* const heap = GetRawMenuDriverBRRESHeap(tableIdx, menuCharacter);
        if (heap == nullptr) {
            rawMenuDriverBRRESLoadFailed[tableIdx][menuCharacter] = true;
            return false;
        }

        u32 fileSize = 0;
        rawFile = EGG::DvdRipper::LoadToMainRAM(path, nullptr, heap, EGG::DvdRipper::ALLOC_FROM_HEAD, 0, nullptr, &fileSize);
        if (rawFile == nullptr || fileSize == 0) {
            rawFile = nullptr;
            rawMenuDriverBRRESLoadFailed[tableIdx][menuCharacter] = true;
            return false;
        }
        rawMenuDriverBRRESLoadFailed[tableIdx][menuCharacter] = false;
    } else {
        rawMenuDriverBRRESLoadFailed[tableIdx][menuCharacter] = false;
    }

    if ((reinterpret_cast<u32>(rawFile) & 0x1f) != 0) {
        rawMenuDriverBRRESLoadFailed[tableIdx][menuCharacter] = true;
        return false;
    }

    nw4r::g3d::ResFile& resFile = *reinterpret_cast<nw4r::g3d::ResFile*>(reinterpret_cast<u8*>(holder) + 4);
    resFile.data = reinterpret_cast<nw4r::g3d::ResFileData*>(rawFile);
    BindRawMenuDriverBRRES(resFile, path);
    return true;
}

kmRuntimeUse(0x8081e358);
static u32 LoadMenuDriverBRRESHook(void* holder, CharacterId character) {
    if (LoadRawMenuDriverBRRES(holder, character)) {
        return 1;
    }

    typedef u32 (*LoadMenuDriverBRRESFn)(void*, CharacterId);
    const LoadMenuDriverBRRESFn original = reinterpret_cast<LoadMenuDriverBRRESFn>(kmRuntimeAddr(0x8081e358));
    const u32 result = original(holder, character);
    return result;
}
kmCall(0x80830368, LoadMenuDriverBRRESHook);
kmCall(0x80831234, LoadMenuDriverBRRESHook);
kmCall(0x8083183c, LoadMenuDriverBRRESHook);

kmRuntimeUse(0x807dbd80);
static MiiHeadsModel* CreateMenuMiiHeadModelHook(void* memory, u32 type, MiiDriverModel* driverModel, u32 miiId, Mii* mii, u32 r8) {
    const GameScene* const currentScene = GameScene::GetCurrent();
    if (currentScene != nullptr && (currentScene->id == SCENE_ID_GLOBE || currentScene->id == SCENE_ID_MENU) &&
        buildingMenuDriverModelTable < CUSTOM_CHARACTER_TABLE_COUNT) {
        return nullptr;
    }

    typedef MiiHeadsModel* (*CreateMiiHeadModelFn)(void*, u32, MiiDriverModel*, u32, Mii*, u32);
    const CreateMiiHeadModelFn original = reinterpret_cast<CreateMiiHeadModelFn>(kmRuntimeAddr(0x807dbd80));
    MiiHeadsModel* const result = original(memory, type, driverModel, miiId, mii, r8);
    return result;
}
kmCall(0x80830540, CreateMenuMiiHeadModelHook);

kmRuntimeUse(0x80830d00);
static void RequestDriverModelHook(MenuModelMgr* menuModelMgr, u8 playerId, CharacterId character) {
    if (menuModelMgr == nullptr || !menuModelMgr->isActive) return;

    MenuDriverModelMgr* const driverModels = menuModelMgr->driverModels;
    if (!IsMenuDriverModelManagerUsable(driverModels, menuModelMgr->playerCount) || playerId >= driverModels->playerCount) return;

    typedef void (*SetPlayerCharacterFn)(MenuDriverModelMgr*, u8, CharacterId);
    const SetPlayerCharacterFn original = reinterpret_cast<SetPlayerCharacterFn>(kmRuntimeAddr(0x80830d00));
    original(driverModels, playerId, character);
}
kmBranch(0x8059e568, RequestDriverModelHook);

static void SnapshotMenuMiiHeads(const MenuDriverModelMgr* driverModelMgr, MiiHeadsModel** preservedMiiHeads) {
    if (driverModelMgr == nullptr || preservedMiiHeads == nullptr) return;

    MiiHeadsModel* const* miiHeads = reinterpret_cast<MiiHeadsModel* const*>(driverModelMgr->miiHeads);
    if (miiHeads == nullptr) return;

    const u8 playerCount = ClampLocalPlayerCount(driverModelMgr->playerCount);
    for (u8 playerId = 0; playerId < playerCount; ++playerId) {
        preservedMiiHeads[playerId] = miiHeads[playerId];
    }
}

static void RestoreMenuMiiHeads(MenuDriverModelMgr* driverModelMgr, MiiHeadsModel* const* preservedMiiHeads) {
    if (driverModelMgr == nullptr || preservedMiiHeads == nullptr) return;

    MiiHeadsModel** const miiHeads = reinterpret_cast<MiiHeadsModel**>(driverModelMgr->miiHeads);
    if (miiHeads == nullptr) return;

    const u8 playerCount = ClampLocalPlayerCount(driverModelMgr->playerCount);
    for (u8 playerId = 0; playerId < playerCount; ++playerId) {
        if (miiHeads[playerId] == nullptr) {
            miiHeads[playerId] = preservedMiiHeads[playerId];
        }
    }
}

static bool ReinitializeMenuDriverModels() {
    MenuModelMgr* const menuModelMgr = MenuModelMgr::sInstance;
    GameScene* const currentScene = const_cast<GameScene*>(GameScene::GetCurrent());
    const SectionMgr* const sectionMgr = SectionMgr::sInstance;
    if (menuModelMgr == nullptr || currentScene == nullptr || sectionMgr == nullptr || sectionMgr->sectionParams == nullptr) {
        return false;
    }
    if (menuModelMgr->heap == nullptr) {
        return false;
    }

    SyncMenuDriverModelCache();

    const u8 playerCount = menuModelMgr->playerCount;
    if (playerCount == 0) {
        return false;
    }

    // Snapshot the old manager state before we rebuild the preview driver heap.
    MenuDriverModelMgr* const oldDriverModels = menuModelMgr->driverModels;
    bool playerWasVisible[MAX_LOCAL_PLAYERS] = {false, false, false, false};
    MiiHeadsModel* preservedMiiHeads[MAX_LOCAL_PLAYERS] = {nullptr, nullptr, nullptr, nullptr};
    MenuDriverModel::State playerState[MAX_LOCAL_PLAYERS] = {
        MenuDriverModel::MENUDRIVERMODEL_STATE_ONCHARSELECT,
        MenuDriverModel::MENUDRIVERMODEL_STATE_ONCHARSELECT,
        MenuDriverModel::MENUDRIVERMODEL_STATE_ONCHARSELECT,
        MenuDriverModel::MENUDRIVERMODEL_STATE_ONCHARSELECT
    };
    if (oldDriverModels != nullptr) {
        SnapshotMenuMiiHeads(oldDriverModels, preservedMiiHeads);
        const u8 snapshotPlayerCount = ClampLocalPlayerCount(oldDriverModels->playerCount);
        for (u8 playerId = 0; playerId < snapshotPlayerCount; ++playerId) {
            playerWasVisible[playerId] = oldDriverModels->players[playerId].isVisible;
            MenuDriverModel* const oldPlayerModel = oldDriverModels->players[playerId].playerModel;
            if (oldPlayerModel != nullptr) {
                const MenuDriverModel::State oldState = oldPlayerModel->state;
                if (oldState == MenuDriverModel::MENUDRIVERMODEL_STATE_ONCHARSELECT ||
                    oldState == MenuDriverModel::MENUDRIVERMODEL_STATE_ONKARTSELECT ||
                    oldState == MENUDRIVERMODEL_STATE_VEHICLE_SELECTED) {
                    playerState[playerId] = oldState;
                }
            }
        }
    }

    // The raw BRRES cache is only keyed by the menu-scene model, so the rebuild must
    // track both the selected table and the resolved menu character separately.
    const CharacterId targetCharacter = GetPreviewCharacterForHud(0);
    const u8 targetTable = ResolveMenuDriverModelTable(targetCharacter);
    const bool targetUsesRawBRRES = ShouldUseRawMenuDriverBRRES(targetTable);
    const CharacterId targetCacheCharacter = GetMenuDriverCacheCharacter(targetCharacter, targetTable);
    if (oldDriverModels != nullptr && targetTable == currentMenuDriverModelTable &&
        targetCharacter == currentMenuDriverModelHud0Character &&
        (!targetUsesRawBRRES || targetCacheCharacter == currentMenuDriverModelCharacter)) {
        return true;
    }

    // From this point on we are replacing the preview manager in place while preserving
    // enough state for the UI to keep the same visible driver/kart selection.
    EGG::ExpHeap* oldMenuDriverModelHeap = activeMenuDriverModelHeap;
    MenuDriverModelMgr* newDriverModels = nullptr;
    menuModelMgr->driverModels = nullptr;
    if (oldDriverModels != nullptr) {
        ClearMenuDriverModels(*oldDriverModels);
    }
    ClearRawMenuDriverBRRESCachePointers();
    DestroyMenuDriverModelHeap(oldMenuDriverModelHeap);
    activeMenuDriverModelHeap = nullptr;
    UnlockMenuDriverModelHeaps(*currentScene, *menuModelMgr);
    currentScene->structsHeaps.SetHeapsGroupId(3);
    buildingMenuDriverModelTable = targetTable;
    buildingMenuDriverModelCharacter = targetCharacter;
    ApplyMenuDriverModelTablePostfixes(targetCharacter, targetTable);
    newDriverModels = CreateMenuDriverModelManager(playerCount);
    buildingMenuDriverModelTable = CUSTOM_CHARACTER_TABLE_INVALID;
    buildingMenuDriverModelCharacter = CHARACTER_NONE;
    ApplyCharacterPostfixes();
    currentScene->structsHeaps.SetHeapsGroupId(0);
    if (!IsMenuDriverModelManagerUsable(newDriverModels, playerCount)) {
        currentScene->structsHeaps.SetHeapsGroupId(6);
        return false;
    }

    RestoreMenuMiiHeads(newDriverModels, preservedMiiHeads);
    menuModelMgr->driverModels = newDriverModels;
    activeMenuDriverModelHeap = createdMenuDriverModelHeap;
    createdMenuDriverModelHeap = nullptr;
    StartMenuDriverModelManager(*newDriverModels);
    currentScene->structsHeaps.SetHeapsGroupId(6);

    const u8 localPlayerCount = GetSectionLocalPlayerCount(*sectionMgr);
    for (u8 hudSlotId = 0; hudSlotId < localPlayerCount; ++hudSlotId) {
        menuModelMgr->RequestDriverModel(hudSlotId, sectionMgr->sectionParams->characters[hudSlotId]);
    }

    for (u8 playerId = 0; playerId < playerCount && playerId < MAX_LOCAL_PLAYERS; ++playerId) {
        MenuDriverModel* const playerModel = newDriverModels->players[playerId].playerModel;
        if (playerModel != nullptr && playerModel->model != nullptr) {
            if (playerState[playerId] != MenuDriverModel::MENUDRIVERMODEL_STATE_ONCHARSELECT) {
                playerModel->id = sectionMgr->sectionParams->karts[playerId];
            }
            playerModel->state = static_cast<MenuDriverModel::State>(0);
            playerModel->SwitchState(playerId, playerState[playerId]);
        }
        newDriverModels->players[playerId].isVisible = playerWasVisible[playerId];
        newDriverModels->TogglePlayerModel(playerId, playerWasVisible[playerId]);
    }

    RefreshCharacterSelectModels();
    currentMenuDriverModelTable = targetTable;
    currentMenuDriverModelCharacter = targetCacheCharacter;
    currentMenuDriverModelHud0Character = targetCharacter;
    return true;
}

void OnVotingVRPageExit() {
    if (!SyncVotingVRMenuDriverState() || votingVRMenuDriverReinitialized) return;
    if (!ShouldHandleVotingVRMenuDriverModels()) {
        votingVRMenuDriverReinitialized = true;
        return;
    }

    if (votingVRMenuDriverExited) return;
    votingVRMenuDriverExited = true;
    votingVRMenuDriverReinitFrames = 1;
}

kmRuntimeUse(0x805f59ac);
static void SetModelRendererVehicleVisible(Pages::ModelRenderer& modelRenderer, u8 hudSlotId) {
    typedef void (*SetModelRendererVisibilityFn)(Pages::ModelRenderer*, u8, u32, bool);
    const SetModelRendererVisibilityFn setModelRendererVisibility =
        reinterpret_cast<SetModelRendererVisibilityFn>(kmRuntimeAddr(0x805f59ac));
    setModelRendererVisibility(&modelRenderer, hudSlotId, 1, true);
}

static bool PrimeVotingModelRenderer() {
    SectionMgr* const sectionMgr = SectionMgr::sInstance;
    if (sectionMgr == nullptr || sectionMgr->curSection == nullptr || sectionMgr->sectionParams == nullptr) return false;

    Pages::ModelRenderer* const modelRenderer = sectionMgr->curSection->Get<Pages::ModelRenderer>();
    if (modelRenderer == nullptr) return false;

    const u8 localPlayerCount = GetSectionLocalPlayerCount(*sectionMgr);
    for (u8 hudSlotId = 0; hudSlotId < localPlayerCount; ++hudSlotId) {
        const CharacterId character = sectionMgr->sectionParams->characters[hudSlotId];
        const KartId kart = sectionMgr->sectionParams->karts[hudSlotId];
        modelRenderer->RequestCharacterModel(hudSlotId, character);
        modelRenderer->LoadKartModelsByCharacter(hudSlotId, character);
        Pages::ModelRenderer::PrepareParams(hudSlotId);
        SetModelRendererVehicleVisible(*modelRenderer, hudSlotId);
        modelRenderer->RequestKartModel(hudSlotId, kart);
    }
    votingVRModelRendererPrimed = true;
    return true;
}

static void ReinitializeVotingVRMenuDriverModels() {
    if (!SyncVotingVRMenuDriverState() || votingVRMenuDriverReinitialized) return;
    if (!ShouldHandleVotingVRMenuDriverModels()) {
        votingVRMenuDriverReinitialized = true;
        return;
    }

    currentMenuDriverModelTable = CUSTOM_CHARACTER_TABLE_INVALID;
    currentMenuDriverModelCharacter = CHARACTER_NONE;
    currentMenuDriverModelHud0Character = CHARACTER_NONE;
    if (ReinitializeMenuDriverModels()) {
        votingVRMenuDriverReinitialized = true;
        PrimeVotingModelRenderer();
        votingVRMenuDriverStateRefreshFrames = 20;
    } else {
        votingVRMenuDriverReinitFrames = 1;
    }
}

static void ProcessVotingVRMenuDriverModelReinit() {
    if (!SyncVotingVRMenuDriverState()) return;
    if (!ShouldHandleVotingVRMenuDriverModels()) return;
    if (IsVRPageActive()) {
        votingVRMenuDriverSawActive = true;
        return;
    }
    if (!votingVRMenuDriverExited && !votingVRMenuDriverSawActive) return;
    OnVotingVRPageExit();
    if (votingVRMenuDriverReinitFrames != 0) {
        --votingVRMenuDriverReinitFrames;
        return;
    }
    applyVotingVRMenuDriverReinit = true;
}

static bool RefreshVotingVRMenuDriverStates() {
    MenuModelMgr* const menuModelMgr = MenuModelMgr::sInstance;
    const SectionMgr* const sectionMgr = SectionMgr::sInstance;
    if (menuModelMgr == nullptr || sectionMgr == nullptr || sectionMgr->sectionParams == nullptr) return false;

    if (!votingVRModelRendererPrimed && !PrimeVotingModelRenderer()) return false;

    MenuDriverModelMgr* const driverModels = menuModelMgr->driverModels;
    MenuKartModelMgr* const kartModels = menuModelMgr->kartModels;
    if (!IsMenuDriverModelManagerUsable(driverModels, menuModelMgr->playerCount) || kartModels == nullptr) return false;

    bool allStatesReady = true;
    const u8 localPlayerCount = GetSectionLocalPlayerCount(*sectionMgr);
    for (u8 hudSlotId = 0; hudSlotId < localPlayerCount; ++hudSlotId) {
        if (hudSlotId >= driverModels->playerCount || hudSlotId >= kartModels->playerCount) continue;

        MenuDriverModel* const playerModel = driverModels->players[hudSlotId].playerModel;
        if (playerModel == nullptr || playerModel->model == nullptr) {
            allStatesReady = false;
            continue;
        }

        const MenuDriverModel::State targetState = GetVotingMenuDriverTargetState(kartModels->players[hudSlotId].isModelVisible);
        if (targetState != MenuDriverModel::MENUDRIVERMODEL_STATE_ONCHARSELECT) {
            if (playerModel->onKartTransformator == nullptr) {
                menuModelMgr->RequestKartModel(hudSlotId, sectionMgr->sectionParams->karts[hudSlotId]);
                allStatesReady = false;
                continue;
            }
            if (playerModel->state != targetState) {
                playerModel->id = sectionMgr->sectionParams->karts[hudSlotId];
                playerModel->SwitchState(hudSlotId, targetState);
            }
        } else if (playerModel->state != targetState) {
            playerModel->SwitchState(hudSlotId, targetState);
        }
    }
    return allStatesReady;
}

static void ProcessVotingVRMenuDriverStateRefresh() {
    if (votingVRMenuDriverStateRefreshFrames == 0) return;
    if (!SyncVotingVRMenuDriverState() || IsVRPageActive()) return;
    if (!ShouldHandleVotingVRMenuDriverModels()) {
        votingVRMenuDriverStateRefreshFrames = 0;
        return;
    }

    if (RefreshVotingVRMenuDriverStates()) {
        votingVRMenuDriverStateRefreshFrames = 0;
    } else {
        --votingVRMenuDriverStateRefreshFrames;
    }
}

static void ProcessPendingMenuDriverModelReinit() {
    if (pendingMenuDriverReinitFrames == 0) return;
    if (--pendingMenuDriverReinitFrames != 0) return;
    if (!IsCharacterSelectPageActive()) {
        if (menuDriverReinitCharSelectWaitAttempts >= 120) {
            menuDriverReinitCharSelectWaitAttempts = 0;
            return;
        }
        ++menuDriverReinitCharSelectWaitAttempts;
        pendingMenuDriverReinitFrames = 2;
        return;
    }
    menuDriverReinitCharSelectWaitAttempts = 0;

    applyPendingMenuDriverModelReinit = true;
}

static void ApplyDeferredMenuDriverReinitsAtFrameStart() {
    // Rebuild at frame start to avoid mutating nw4r lists while the previous frame is drawing.
    if (applyPendingMenuDriverModelReinit) {
        applyPendingMenuDriverModelReinit = false;
        if (IsCharacterSelectPageActive()) {
            ReinitializeMenuDriverModels();
        }
    }
    if (applyVotingVRMenuDriverReinit) {
        applyVotingVRMenuDriverReinit = false;
        ReinitializeVotingVRMenuDriverModels();
    }
}

void UpdateOnlineCharacterTablesFromAid(u8 aid, const u8* playerIdToAid, u8 characterTables) {
    if (playerIdToAid == nullptr) return;
    if (IsLocalMultiplayerActive()) {
        ResetOnlineCustomCharacterFlags();
        return;
    }

    u8 hudSlotId = 0;
    for (u8 playerId = 0; playerId < MAX_ONLINE_PLAYERS; ++playerId) {
        if (playerIdToAid[playerId] != aid) continue;

        // The SELECT packet only carries two local skin-table slots.
        const u8 shift = static_cast<u8>(hudSlotId * CUSTOM_CHARACTER_TABLE_PACKET_BITS);
        const u8 tableIdx = hudSlotId < 2 ? ((characterTables >> shift) & CUSTOM_CHARACTER_TABLE_PACKET_MASK) : CUSTOM_CHARACTER_TABLE_DEFAULT;
        onlineCharacterTables[playerId] = tableIdx < CUSTOM_CHARACTER_TABLE_COUNT ? tableIdx : CUSTOM_CHARACTER_TABLE_DEFAULT;
        ++hudSlotId;
    }
}

u8 GetLocalOnlineCharacterTables() {
    if (IsLocalMultiplayerActive()) return 0;

    u8 characterTables = 0;
    u8 localPlayerCount = GetLocalPlayerCount();
    const SectionMgr* const sectionMgr = SectionMgr::sInstance;
    if (sectionMgr != nullptr && sectionMgr->sectionParams != nullptr) {
        localPlayerCount = static_cast<u8>(sectionMgr->sectionParams->localPlayerCount);
    } else if (Racedata::sInstance != nullptr) {
        localPlayerCount = Racedata::sInstance->menusScenario.localPlayerCount;
    }
    if (localPlayerCount > 2) localPlayerCount = 2;

    for (u8 hudSlotId = 0; hudSlotId < localPlayerCount; ++hudSlotId) {
        const CharacterId character = GetSelectedCharacterForHud(hudSlotId);
        const u8 tableIdx = GetSelectedCharacterTable(character) & CUSTOM_CHARACTER_TABLE_PACKET_MASK;
        characterTables |= static_cast<u8>(tableIdx << (hudSlotId * CUSTOM_CHARACTER_TABLE_PACKET_BITS));
    }
    return characterTables;
}

bool ShouldUseCustomCharacterForPlayer(u8 playerId) {
    if (IsLocalMultiplayerActive()) return false;
    return playerId < MAX_ONLINE_PLAYERS && onlineCharacterTables[playerId] != CUSTOM_CHARACTER_TABLE_DEFAULT;
}

kmRuntimeUse(0x80540e3c);
static ArchivesHolder* LoadKartArchiveHook(ArchiveMgr* archiveMgr, u8 playerId, KartId kart, CharacterId character, u32 color, u32 type, EGG::Heap* decompressedHeap, EGG::Heap* archiveHeap) {
    typedef ArchivesHolder* (*LoadKartArchiveFn)(ArchiveMgr*, u8, KartId, CharacterId, u32, u32, EGG::Heap*, EGG::Heap*);
    const LoadKartArchiveFn original = reinterpret_cast<LoadKartArchiveFn>(kmRuntimeAddr(0x80540e3c));
    ScopedCharacterPostfixSwap swap(playerId, character);
    ArchivesHolder* const holder = original(archiveMgr, playerId, kart, character, color, type, decompressedHeap, archiveHeap);
    return holder;
}
kmCall(0x805540f4, LoadKartArchiveHook);

kmRuntimeUse(0x80540f90);
static ArchivesHolder* LoadKartArchiveHolder2Hook(ArchiveMgr* archiveMgr, u8 playerId, KartId kart, CharacterId character, u32 color, u32 type, EGG::Heap* decompressedHeap, EGG::Heap* archiveHeap) {
    typedef ArchivesHolder* (*LoadKartArchiveHolder2Fn)(ArchiveMgr*, u8, KartId, CharacterId, u32, u32, EGG::Heap*, EGG::Heap*);
    const LoadKartArchiveHolder2Fn original = reinterpret_cast<LoadKartArchiveHolder2Fn>(kmRuntimeAddr(0x80540f90));
    ScopedCharacterPostfixSwap swap(playerId, character);
    ArchivesHolder* const holder = original(archiveMgr, playerId, kart, character, color, type, decompressedHeap, archiveHeap);
    return holder;
}
kmCall(0x80554198, LoadKartArchiveHolder2Hook);

kmRuntimeUse(0x805419c8);
static const char* GetMenuDriverBRRESNameHook(u32 character) {
    const CharacterId characterId = static_cast<CharacterId>(character);
    typedef const char* (*GetCharacterNameFn)(u32);
    const GetCharacterNameFn original = reinterpret_cast<GetCharacterNameFn>(kmRuntimeAddr(0x805419c8));

    u8 tableIdx = CUSTOM_CHARACTER_TABLE_DEFAULT;
    if (buildingMenuDriverModelTable < CUSTOM_CHARACTER_TABLE_COUNT) {
        const CharacterId buildingMenuCharacter = GetMenuDriverBRRESCharacter(buildingMenuDriverModelCharacter);
        if (characterId == buildingMenuCharacter) tableIdx = buildingMenuDriverModelTable;
    } else {
        tableIdx = ResolveMenuDriverModelTable(characterId);
    }

    const char* driverName = GetDriverBRRESName(characterId, tableIdx);
    if (tableIdx < CUSTOM_CHARACTER_TABLE_COUNT && characterId >= 0 && characterId < CUSTOM_CHARACTER_COUNT &&
        rawMenuDriverBRRESLoadFailed[tableIdx][characterId]) {
        return original(character);
    }
    return driverName != nullptr ? driverName : original(character);
}
kmCall(0x8081e4a0, GetMenuDriverBRRESNameHook);

static void ResetCustomCharacterMenuState() {
    if (!IsOnlineRoom(RKNet::Controller::sInstance)) ResetOnlineCustomCharacterFlags();
    ResetMenuDriverModelCache();
    EnsureActiveCustomCharacterTable();
}
static SectionLoadHook ResetCustomCharacterMenuStateHook(ResetCustomCharacterMenuState);

kmRuntimeUse(0x8083e5f4);
static void CharacterSelectHoverHook(Pages::CharacterSelect* page, CtrlMenuCharacterSelect::ButtonDriver* button, u32 buttonId, u8 hudSlotId) {
    if (hudSlotId < MAX_LOCAL_PLAYERS) {
        hoveredCharacterByHud[hudSlotId] = static_cast<CharacterId>(buttonId);
    }
    typedef void (*CharacterSelectHoverFn)(Pages::CharacterSelect*, CtrlMenuCharacterSelect::ButtonDriver*, u32, u8);
    const CharacterSelectHoverFn original = reinterpret_cast<CharacterSelectHoverFn>(kmRuntimeAddr(0x8083e5f4));
    original(page, button, buttonId, hudSlotId);
}
kmCall(0x807e2cf0, CharacterSelectHoverHook);
kmCall(0x807e304c, CharacterSelectHoverHook);
kmCall(0x807e34d0, CharacterSelectHoverHook);
kmCall(0x807e37b0, CharacterSelectHoverHook);
kmCall(0x807e3a88, CharacterSelectHoverHook);

static ControllerType GetControllerTypeForHudSlot(const SectionMgr& sectionMgr, u8 hudSlotId) {
    if (hudSlotId >= MAX_LOCAL_PLAYERS) return GCN;

    const Input::RealControllerHolder* holder = sectionMgr.pad.padInfos[hudSlotId].controllerHolder;
    if (holder == nullptr || holder->curController == nullptr) return GCN;

    const ControllerType type = holder->curController->GetType();
    if (type == WHEEL || type == NUNCHUCK || type == CLASSIC || type == GCN) return type;
    return GCN;
}

static void HideAllCustomCharacterHintPanes(CharaName& nameUi) {
    nameUi.SetPaneVisibility("cc_prev_gc", false);
    nameUi.SetPaneVisibility("cc_next_gc", false);
    nameUi.SetPaneVisibility("cc_prev_cls", false);
    nameUi.SetPaneVisibility("cc_next_cls", false);
    nameUi.SetPaneVisibility("cc_prev_nc", false);
    nameUi.SetPaneVisibility("cc_next_nc", false);
    nameUi.SetPaneVisibility("cc_prev_wh", false);
    nameUi.SetPaneVisibility("cc_next_wh", false);
}

static void ApplyCustomCharacterHintPanesForType(CharaName& nameUi, ControllerType type) {
    HideAllCustomCharacterHintPanes(nameUi);
    switch (type) {
        case WHEEL:
            nameUi.SetPaneVisibility("cc_prev_wh", true);
            nameUi.SetPaneVisibility("cc_next_wh", true);
            break;
        case NUNCHUCK:
            nameUi.SetPaneVisibility("cc_prev_nc", true);
            nameUi.SetPaneVisibility("cc_next_nc", true);
            break;
        case CLASSIC:
            nameUi.SetPaneVisibility("cc_prev_cls", true);
            nameUi.SetPaneVisibility("cc_next_cls", true);
            break;
        case GCN:
        default:
            nameUi.SetPaneVisibility("cc_prev_gc", true);
            nameUi.SetPaneVisibility("cc_next_gc", true);
            break;
    }
}

static void UpdateCustomCharacterSelectNamePaneIcons() {
    if (!IsCharacterSelectPageActive()) return;

    SectionMgr* const sectionMgr = SectionMgr::sInstance;
    if (sectionMgr == nullptr || sectionMgr->sectionParams == nullptr) return;

    Pages::CharacterSelect* const characterSelect = sectionMgr->curSection->Get<Pages::CharacterSelect>();
    if (characterSelect == nullptr || characterSelect->names == nullptr) return;

    const u8 localPlayerCount = GetSectionLocalPlayerCount(*sectionMgr);
    if (IsLocalMultiplayerActive()) {
        for (u8 hudSlotId = 0; hudSlotId < localPlayerCount; ++hudSlotId) {
            HideAllCustomCharacterHintPanes(characterSelect->names[hudSlotId]);
        }
        return;
    }

    for (u8 hudSlotId = 0; hudSlotId < localPlayerCount; ++hudSlotId) {
        const ControllerType type = GetControllerTypeForHudSlot(*sectionMgr, hudSlotId);
        ApplyCustomCharacterHintPanesForType(characterSelect->names[hudSlotId], type);
    }
}

static void GetCustomCharacterToggleInputs(ControllerType type, u16& previousButton, u16& nextButton, u16& previousAction, u16& nextAction) {
    previousAction = 0;
    nextAction = 0;

    switch (type) {
        case WHEEL:
            previousButton = WPAD::WPAD_BUTTON_B;
            nextButton = WPAD::WPAD_BUTTON_A;
            previousAction = static_cast<u16>(1 << BACK_PRESS);
            nextAction = static_cast<u16>(1 << FORWARD_PRESS);
            break;
        case NUNCHUCK:
            previousButton = WPAD::WPAD_BUTTON_1;
            nextButton = WPAD::WPAD_BUTTON_2;
            previousAction = static_cast<u16>(1 << BACK_PRESS);
            nextAction = static_cast<u16>(1 << FORWARD_PRESS);
            break;
        case CLASSIC:
            previousButton = WPAD::WPAD_CL_TRIGGER_L;
            nextButton = WPAD::WPAD_CL_TRIGGER_R;
            break;
        case GCN:
        default:
            previousButton = PAD::PAD_BUTTON_L;
            nextButton = PAD::PAD_BUTTON_R;
            break;
    }
}

static void ConsumeCustomCharacterToggleInput(Input::RealControllerHolder& controllerHolder, u16 rawButton, u16 uiActionMask) {
    controllerHolder.inputStates[0].buttonRaw &= static_cast<u16>(~rawButton);
    controllerHolder.uiinputStates[0].rawButtons &= static_cast<u16>(~rawButton);
    controllerHolder.uiinputStates[0].buttonActions &= static_cast<u16>(~uiActionMask);
}

static bool ShouldProcessCustomCharacterInput() {
    if (GetLocalPlayerCount() != 1) {
        heldCustomCharacterToggleButtons = 0;
        return false;
    }
    if (!IsCharacterSelectPageActive()) {
        heldCustomCharacterToggleButtons = 0;
        return false;
    }

    const SectionMgr* sectionMgr = SectionMgr::sInstance;
    if (sectionMgr == nullptr || sectionMgr->pad.padInfos[0].controllerHolder == nullptr) {
        heldCustomCharacterToggleButtons = 0;
        return false;
    }

    Input::RealControllerHolder* controllerHolder = sectionMgr->pad.padInfos[0].controllerHolder;
    const Input::Controller* controller = controllerHolder->curController;
    if (controller == nullptr) {
        heldCustomCharacterToggleButtons = 0;
        return false;
    }

    // Eat the toggle press here so it does not also navigate the stock menu.
    const u16 inputs = controllerHolder->inputStates[0].buttonRaw;

    u16 previousButton = 0;
    u16 nextButton = 0;
    u16 previousAction = 0;
    u16 nextAction = 0;
    GetCustomCharacterToggleInputs(GetControllerTypeForHudSlot(*sectionMgr, 0), previousButton, nextButton, previousAction, nextAction);

    const bool isPreviousHeld = (inputs & previousButton) != 0;
    const bool isNextHeld = (inputs & nextButton) != 0;
    const u16 toggleButtons = static_cast<u16>(previousButton | nextButton);
    const u16 heldToggleButtons = static_cast<u16>(inputs & toggleButtons);
    const u16 newToggleButtons = static_cast<u16>(heldToggleButtons & ~heldCustomCharacterToggleButtons);
    heldCustomCharacterToggleButtons = heldToggleButtons;

    if (isPreviousHeld) ConsumeCustomCharacterToggleInput(*controllerHolder, previousButton, previousAction);
    if (isNextHeld) ConsumeCustomCharacterToggleInput(*controllerHolder, nextButton, nextAction);

    if ((newToggleButtons & previousButton) != 0) {
        if (CycleCustomCharacterTable(GetPreviewCharacterForHud(0), -1)) {
            Audio::RSARPlayer::PlaySoundById(SOUND_ID_LEFT_ARROW_PRESS, 0, 0);
            return true;
        }
    }
    if ((newToggleButtons & nextButton) != 0) {
        if (CycleCustomCharacterTable(GetPreviewCharacterForHud(0), 1)) {
            Audio::RSARPlayer::PlaySoundById(SOUND_ID_RIGHT_ARROW_PRESS, 0, 0);
            return true;
        }
    }
    return false;
}

kmRuntimeUse(0x8063583c);
// MenuScene_vf30 / GlobeScene_vf30 -> SectionMgr::MenuUpdate call sites
static void MenuSceneSectionUpdateHook(SectionMgr* sectionMgr) {
    ApplyDeferredMenuDriverReinitsAtFrameStart();
    UpdateCustomCharacterSelectNamePaneIcons();
    ShouldProcessCustomCharacterInput();
    typedef void (*SectionMgrUpdateFn)(SectionMgr*);
    const SectionMgrUpdateFn original = reinterpret_cast<SectionMgrUpdateFn>(kmRuntimeAddr(0x8063583c));
    original(sectionMgr);
    ProcessVotingVRMenuDriverModelReinit();
    ProcessVotingVRMenuDriverStateRefresh();
    ProcessPendingMenuDriverModelReinit();
}
kmCall(0x805552e8, MenuSceneSectionUpdateHook);
kmCall(0x80553b30, MenuSceneSectionUpdateHook);

}  // namespace CustomCharacters
}  // namespace Pulsar
