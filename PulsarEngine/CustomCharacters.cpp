#include <runtimeWrite.hpp>
#include <CustomCharacters.hpp>
#include <Settings/Settings.hpp>
#include <MarioKartWii/Archive/ArchiveMgr.hpp>
#include <MarioKartWii/UI/Page/Menu/CharacterSelect.hpp>
#include <MarioKartWii/UI/Page/Menu/KartSelect.hpp>
#include <MarioKartWii/3D/Model/Menu/MenuDriverModel.hpp>
#include <MarioKartWii/3D/Model/Menu/MenuModelMgr.hpp>
#include <MarioKartWii/Driver/Toadette.hpp>
#include <MarioKartWii/Kart/KartLink.hpp>
#include <MarioKartWii/Audio/Actors/RaceActor.hpp>
#include <MarioKartWii/Mii/MiiHeadsModel.hpp>
#include <MarioKartWii/System/Identifiers.hpp>
#include <MarioKartWii/GlobalFunctions.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>
#include <MarioKartWii/Scene/GameScene.hpp>
#include <MarioKartWii/Audio/RSARPlayer.hpp>
#include <MarioKartWii/Input/Controller.hpp>

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

struct CharacterAssetNames {
    CharacterId character;
    CharacterId stateCharacter;
    const char* defaultPostfix;
    const char* defaultDriverBrresName;
};

struct CharacterOverride {
    CharacterId character;
    const char* postfix;
    const char* driverBrresName;
};

struct CharacterTable {
    const CharacterOverride* overrides;
    u8 overrideCount;
};

static const CharacterAssetNames defaultCharacterAssets[] = {
    {MARIO, MARIO, "mr", nullptr},
    {BABY_PEACH, BABY_PEACH, "bpc", nullptr},
    {WALUIGI, WALUIGI, "wl", nullptr},
    {BOWSER, BOWSER, "kp", nullptr},
    {BABY_DAISY, BABY_DAISY, "bds", nullptr},
    {DRY_BONES, DRY_BONES, "ka", nullptr},
    {BABY_MARIO, BABY_MARIO, "bmr", nullptr},
    {LUIGI, LUIGI, "lg", nullptr},
    {TOAD, TOAD, "ko", nullptr},
    {DONKEY_KONG, DONKEY_KONG, "dk", nullptr},
    {YOSHI, YOSHI, "ys", nullptr},
    {WARIO, WARIO, "wr", nullptr},
    {BABY_LUIGI, BABY_LUIGI, "blg", nullptr},
    {TOADETTE, TOADETTE, "kk", nullptr},
    {KOOPA_TROOPA, KOOPA_TROOPA, "nk", nullptr},
    {DAISY, DAISY, "ds", nullptr},
    {PEACH, PEACH, "pc", nullptr},
    {BIRDO, BIRDO, "ca", nullptr},
    {DIDDY_KONG, DIDDY_KONG, "dd", nullptr},
    {KING_BOO, KING_BOO, "kt", nullptr},
    {BOWSER_JR, BOWSER_JR, "jr", nullptr},
    {DRY_BOWSER, DRY_BOWSER, "bk", nullptr},
    {FUNKY_KONG, FUNKY_KONG, "fk", nullptr},
    {ROSALINA, ROSALINA, "rs", nullptr},
    {PEACH_BIKER, PEACH, "pc", "pc_menu"},
    {DAISY_BIKER, DAISY, "ds", "ds_menu"},
    {ROSALINA_BIKER, ROSALINA, "rs", "rs_menu"},
};

static const CharacterOverride skin1CharacterOverrides[] = {
    {MARIO, "mr-1", nullptr},
    {BABY_PEACH, "bpc-1", nullptr},
    {WALUIGI, "wl-1", nullptr},
    {BOWSER, "kp-1", nullptr},
    {BABY_DAISY, "bds-1", nullptr},
    {DRY_BONES, "ka-1", nullptr},
    {BABY_MARIO, "bmr-1", nullptr},
    {LUIGI, "lg-1", nullptr},
    {TOAD, "ko-1", nullptr},
    {DONKEY_KONG, "dk-1", nullptr},
    {YOSHI, "ys-1", nullptr},
    {WARIO, "wr-1", nullptr},
    {BABY_LUIGI, "blg-1", nullptr},
    {TOADETTE, "kk-1", nullptr},
    {KOOPA_TROOPA, "nk-1", nullptr},
    {DAISY, "ds-1", nullptr},
    {PEACH, "pc-1", nullptr},
    {BIRDO, "ca-1", nullptr},
    {DIDDY_KONG, "dd-1", nullptr},
    {KING_BOO, "kt-1", nullptr},
    {BOWSER_JR, "jr-1", nullptr},
    {DRY_BOWSER, "bk-1", nullptr},
    {FUNKY_KONG, "fk-1", nullptr},
    {ROSALINA, "rs-1", nullptr},
    {PEACH_BIKER, "pc-1", "pc-1_menu"},
    {DAISY_BIKER, "ds-1", "ds-1_menu"},
    {ROSALINA_BIKER, "rs-1", "rs-1_menu"},
};

static const CharacterOverride skin2CharacterOverrides[] = {
    {MARIO, "mr-2", nullptr},
    {BABY_PEACH, "bpc-2", nullptr},
    {WALUIGI, "wl-2", nullptr},
    {BOWSER, "kp-2", nullptr},
    {BABY_DAISY, "bds-2", nullptr},
    {DRY_BONES, "ka-2", nullptr},
    {BABY_MARIO, "bmr-2", nullptr},
    {LUIGI, "lg-2", nullptr},
    {TOAD, "ko-2", nullptr},
    {DONKEY_KONG, "dk-2", nullptr},
    {YOSHI, "ys-2", nullptr},
    {WARIO, "wr-2", nullptr},
    {KOOPA_TROOPA, "nk-2", nullptr},
    {DIDDY_KONG, "dd-2", nullptr},
    {BOWSER_JR, "jr-2", nullptr},
    {FUNKY_KONG, "fk-2", nullptr},
    {PEACH, "pc-2", nullptr},
    {DAISY, "ds-2", nullptr},
    {ROSALINA, "rs-2", nullptr},
    {PEACH_BIKER, "pc-2", "pc-2_menu"},
    {DAISY_BIKER, "ds-2", "ds-2_menu"},
    {ROSALINA_BIKER, "rs-2", "rs-2_menu"},
};

static const CharacterOverride skin3CharacterOverrides[] = {
    {BOWSER, "kp-3", nullptr},
    {DRY_BONES, "ka-3", nullptr},
    {BABY_MARIO, "bmr-3", nullptr},
    {LUIGI, "lg-3", nullptr},
    {TOAD, "ko-3", nullptr},
    {DONKEY_KONG, "dk-3", nullptr},
    {YOSHI, "ys-3", nullptr},
    {WARIO, "wr-3", nullptr},
    {BOWSER_JR, "jr-3", nullptr},
    {FUNKY_KONG, "fk-3", nullptr},
    {PEACH, "pc-3", nullptr},
    {DAISY, "ds-3", nullptr},
    {ROSALINA, "rs-3", nullptr},
    {PEACH_BIKER, "pc-3", "pc-3_menu"},
    {DAISY_BIKER, "ds-3", "ds-3_menu"},
    {ROSALINA_BIKER, "rs-3", "rs-3_menu"},
};

static const CharacterOverride skin4CharacterOverrides[] = {
    {DRY_BONES, "ka-4", nullptr},
    {BABY_MARIO, "bmr-4", nullptr},
    {LUIGI, "lg-4", nullptr},
    {DONKEY_KONG, "dk-4", nullptr},
    {BOWSER_JR, "jr-4", nullptr},
};

static const CharacterOverride skin5CharacterOverrides[] = {
    {BOWSER_JR, "jr-5", nullptr},
};

// To add another table, add an enum value above CUSTOM_CHARACTER_TABLE_COUNT,
// add a CharacterOverride array, and register it here.
static const CharacterTable customCharacterTables[CUSTOM_CHARACTER_TABLE_COUNT] = {
    {nullptr, 0},
    {skin1CharacterOverrides, static_cast<u8>(ARRAY_COUNT(skin1CharacterOverrides))},
    {skin2CharacterOverrides, static_cast<u8>(ARRAY_COUNT(skin2CharacterOverrides))},
    {skin3CharacterOverrides, static_cast<u8>(ARRAY_COUNT(skin3CharacterOverrides))},
    {skin4CharacterOverrides, static_cast<u8>(ARRAY_COUNT(skin4CharacterOverrides))},
    {skin5CharacterOverrides, static_cast<u8>(ARRAY_COUNT(skin5CharacterOverrides))},
};

static const u8 CUSTOM_CHARACTER_COUNT = 0x30;
static const u8 CUSTOM_CHARACTER_TABLE_PACKET_BITS = 3;
static const u8 CUSTOM_CHARACTER_TABLE_PACKET_MASK = (1 << CUSTOM_CHARACTER_TABLE_PACKET_BITS) - 1;
static const u8 CUSTOM_CHARACTER_TABLE_PACKET_COUNT = 1 << CUSTOM_CHARACTER_TABLE_PACKET_BITS;
static u8 selectedCharacterTableByCharacter[CUSTOM_CHARACTER_COUNT];
static u8 pendingMenuDriverReinitFrames = 0;
static MenuDriverModelMgr* cachedMenuDriverModels[CUSTOM_CHARACTER_TABLE_COUNT] = {nullptr};
static MenuModelMgr* cachedMenuModelMgr = nullptr;
static u8 cachedMenuDriverModelPlayerCount = 0;
static u8 currentMenuDriverModelTable = CUSTOM_CHARACTER_TABLE_INVALID;
static u8 buildingMenuDriverModelTable = CUSTOM_CHARACTER_TABLE_INVALID;
static CharacterId hoveredCharacterByHud[4] = {MARIO, MARIO, MARIO, MARIO};
static u16 heldCustomCharacterToggleButtons = 0;
static_assert(CUSTOM_CHARACTER_TABLE_COUNT <= CUSTOM_CHARACTER_TABLE_PACKET_COUNT, "character table packet encoding cannot fit every table");
static_assert(CUSTOM_CHARACTER_TABLE_PACKET_BITS * 2 <= 8, "character table packet field is one byte");

static void ApplyCharacterPostfixes();
void RefreshLocalOnlineCustomCharacterFlags();
const char* GetCustomCharacterPostfix(CharacterId character);
const char* GetDefaultCharacterPostfix(CharacterId character);
static void ApplyCharacterPostfix(CharacterId character, u8 tableIdx);
static const char** GetCharacterPostfixEntry(CharacterId character);
static bool IsCustomCharacterEnabled(CharacterId character);
static bool IsCharacterSelectPageActive();
static void ReinitializeMenuDriverModels();
static void ProcessPendingMenuDriverModelReinit();
static void ResetMenuDriverModelCache();
static void SyncMenuDriverModelCache();
static u8 GetMenuDriverModelTableForCharacter(CharacterId character);
static void ScheduleMenuDriverModelReinitForPreview();
static void RefreshCharacterSelectModels();
static void RefreshKartSelectModel();
static CharacterId GetPreviewCharacterForHud(u8 hudSlotId);
static CharacterId GetSelectedCharacterForHud(u8 hudSlotId);

kmRuntimeUse(0x808b3a90);
static const u32 CHARACTER_POSTFIX_TABLE_ADDRESS = kmRuntimeAddr(0x808b3a90);

static const CharacterAssetNames* GetCharacterAssets(CharacterId character) {
    for (u32 i = 0; i < ARRAY_COUNT(defaultCharacterAssets); ++i) {
        if (defaultCharacterAssets[i].character == character) return &defaultCharacterAssets[i];
    }
    return nullptr;
}

static const CharacterOverride* GetCharacterOverride(CharacterId character, u8 tableIdx) {
    if (tableIdx == CUSTOM_CHARACTER_TABLE_DEFAULT || tableIdx >= CUSTOM_CHARACTER_TABLE_COUNT) return nullptr;

    const CharacterTable& table = customCharacterTables[tableIdx];
    for (u8 i = 0; i < table.overrideCount; ++i) {
        if (table.overrides[i].character == character) return &table.overrides[i];
    }
    return nullptr;
}

static bool IsCharacterTableValidForCharacter(CharacterId character, u8 tableIdx) {
    if (GetCharacterAssets(character) == nullptr) return false;
    if (tableIdx == CUSTOM_CHARACTER_TABLE_DEFAULT) return true;
    return GetCharacterOverride(character, tableIdx) != nullptr;
}

static u8 NormalizeCharacterTable(CharacterId character, u8 tableIdx) {
    if (IsCharacterTableValidForCharacter(character, tableIdx)) return tableIdx;
    return CUSTOM_CHARACTER_TABLE_DEFAULT;
}

static CharacterId GetCustomCharacterStateId(CharacterId character) {
    const CharacterAssetNames* assets = GetCharacterAssets(character);
    if (assets == nullptr) return CHARACTER_NONE;
    return assets->stateCharacter;
}

static bool IsCustomCharacterStateIdValid(CharacterId character) {
    const CharacterAssetNames* assets = GetCharacterAssets(character);
    if (assets == nullptr) return false;
    return assets->stateCharacter >= 0 && assets->stateCharacter < CUSTOM_CHARACTER_COUNT;
}

static u8 GetSelectedCharacterTable(CharacterId character) {
    if (!IsCustomCharacterStateIdValid(character)) return CUSTOM_CHARACTER_TABLE_DEFAULT;

    const u8 tableIdx = selectedCharacterTableByCharacter[GetCustomCharacterStateId(character)];
    return NormalizeCharacterTable(character, tableIdx);
}

static bool IsCustomCharacterEnabled(CharacterId character) {
    return GetSelectedCharacterTable(character) != CUSTOM_CHARACTER_TABLE_DEFAULT;
}

static bool IsAnyCustomCharacterEnabled() {
    for (u8 character = 0; character < CUSTOM_CHARACTER_COUNT; ++character) {
        if (selectedCharacterTableByCharacter[character] != CUSTOM_CHARACTER_TABLE_DEFAULT) return true;
    }
    return false;
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

    u8 tableIdx = GetSelectedCharacterTable(character);
    for (u8 i = 0; i < CUSTOM_CHARACTER_TABLE_COUNT; ++i) {
        if (direction < 0) {
            tableIdx = tableIdx == 0 ? CUSTOM_CHARACTER_TABLE_COUNT - 1 : tableIdx - 1;
        } else {
            tableIdx = tableIdx + 1 >= CUSTOM_CHARACTER_TABLE_COUNT ? CUSTOM_CHARACTER_TABLE_DEFAULT : tableIdx + 1;
        }

        if (!IsCharacterTableValidForCharacter(character, tableIdx)) continue;
        if (!SetCustomCharacterTable(character, tableIdx)) return false;
        pendingMenuDriverReinitFrames = 2;
        return true;
    }
    return false;
}

static const char* GetCharacterPostfix(CharacterId character, u8 tableIdx) {
    const CharacterOverride* characterOverride = GetCharacterOverride(character, tableIdx);
    if (characterOverride != nullptr && characterOverride->postfix != nullptr) return characterOverride->postfix;

    const CharacterAssetNames* assets = GetCharacterAssets(character);
    if (assets == nullptr) return nullptr;
    return assets->defaultPostfix;
}

static const char* GetDriverBRRESName(CharacterId character, u8 tableIdx) {
    const CharacterOverride* characterOverride = GetCharacterOverride(character, tableIdx);
    if (characterOverride != nullptr) {
        if (characterOverride->driverBrresName != nullptr) return characterOverride->driverBrresName;
        return characterOverride->postfix;
    }

    const CharacterAssetNames* assets = GetCharacterAssets(character);
    if (assets == nullptr) return nullptr;
    if (assets->defaultDriverBrresName != nullptr) return assets->defaultDriverBrresName;
    return assets->defaultPostfix;
}

static void EnsureActiveCustomCharacterTable() {
    ApplyCharacterPostfixes();
}

bool IsCustomCharacterTableActive() {
    return IsAnyCustomCharacterEnabled();
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
    return IsOnlineRoom(controller) && GetLocalPlayerCount() > 1;
}

bool isDisplayCustomSkinsEnabled() {
    return Pulsar::Settings::Mgr::Get().GetUserSettingValue(Pulsar::Settings::SETTINGSTYPE_ONLINE, Pulsar::RADIO_DISPLAYCUSTOMSKINS) == Pulsar::DISPLAYCUSTOMSKINS_ENABLED;
}

const char* GetCustomCharacterPostfix(CharacterId character) {
    return GetCharacterPostfix(character, CUSTOM_CHARACTER_TABLE_SKIN1);
}

const char* GetDefaultCharacterPostfix(CharacterId character) {
    return GetCharacterPostfix(character, CUSTOM_CHARACTER_TABLE_DEFAULT);
}

static void ApplyCharacterPostfix(CharacterId character, u8 tableIdx) {
    const char** entry = GetCharacterPostfixEntry(character);
    if (entry == nullptr) return;

    const char* postfix = GetCharacterPostfix(character, tableIdx);
    if (postfix != nullptr) *entry = postfix;
}

void ApplyCharacterTable(u8 tableIdx) {
    if (tableIdx >= CUSTOM_CHARACTER_TABLE_COUNT) tableIdx = CUSTOM_CHARACTER_TABLE_DEFAULT;
    for (u8 character = 0; character < CUSTOM_CHARACTER_COUNT; ++character) {
        selectedCharacterTableByCharacter[character] = CUSTOM_CHARACTER_TABLE_DEFAULT;
    }
    for (u32 i = 0; i < ARRAY_COUNT(defaultCharacterAssets); ++i) {
        const CharacterId character = defaultCharacterAssets[i].character;
        const CharacterId stateCharacter = defaultCharacterAssets[i].stateCharacter;
        if (stateCharacter >= 0 && stateCharacter < CUSTOM_CHARACTER_COUNT) {
            const u8 normalizedTable = NormalizeCharacterTable(character, tableIdx);
            if (normalizedTable != CUSTOM_CHARACTER_TABLE_DEFAULT) selectedCharacterTableByCharacter[stateCharacter] = normalizedTable;
        }
    }
    ApplyCharacterPostfixes();
    RefreshLocalOnlineCustomCharacterFlags();
}

static void ApplyCharacterPostfixes() {
    for (u32 i = 0; i < ARRAY_COUNT(defaultCharacterAssets); ++i) {
        const CharacterId character = defaultCharacterAssets[i].character;
        ApplyCharacterPostfix(character, GetSelectedCharacterTable(character));
    }
}

static void ApplyMenuDriverModelTablePostfixes(u8 tableIdx) {
    for (u32 i = 0; i < ARRAY_COUNT(defaultCharacterAssets); ++i) {
        const CharacterId character = defaultCharacterAssets[i].character;
        ApplyCharacterPostfix(character, NormalizeCharacterTable(character, tableIdx));
    }
}

bool IsRaceSectionActive() {
    const SectionMgr* sectionMgr = SectionMgr::sInstance;
    if (sectionMgr == nullptr || sectionMgr->curSection == nullptr) return false;
    return IsGameplaySection(sectionMgr->curSection->sectionId) == SCENE_ID_RACE;
}

bool IsLocalRacePlayer(u8 playerId) {
    const Racedata* racedata = Racedata::sInstance;
    if (racedata == nullptr) return false;

    const RacedataScenario& scenario = racedata->racesScenario;
    const u8 localPlayerCount = scenario.localPlayerCount > 4 ? 4 : scenario.localPlayerCount;

    for (u8 hudSlotId = 0; hudSlotId < localPlayerCount; ++hudSlotId) {
        const u32 localPlayerId = racedata->GetPlayerIdOfLocalPlayer(hudSlotId);
        if (localPlayerId == playerId) return true;
    }
    return false;
}

bool ShouldUseCustomCharacterForArchivePlayer(u8 playerId, CharacterId character) {
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    if (IsOnlineRoom(controller)) {
        if (IsOnlineMultiLocal(controller)) return false;
        if (!isDisplayCustomSkinsEnabled()) return false;
        if (IsLocalRacePlayer(playerId)) return IsCustomCharacterEnabled(character);
        return playerId < 12 && IsCharacterTableValidForCharacter(character, onlineCharacterTables[playerId]) &&
               onlineCharacterTables[playerId] != CUSTOM_CHARACTER_TABLE_DEFAULT;
    }
    return IsCustomCharacterEnabled(character) && IsLocalRacePlayer(playerId);
}

static u8 GetRaceCharacterTable(u8 playerId, CharacterId character) {
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    if (IsOnlineRoom(controller) && !IsOnlineMultiLocal(controller) && isDisplayCustomSkinsEnabled()) {
        if (IsLocalRacePlayer(playerId)) return GetSelectedCharacterTable(character);
        return playerId < 12 ? NormalizeCharacterTable(character, onlineCharacterTables[playerId]) : CUSTOM_CHARACTER_TABLE_DEFAULT;
    }
    return IsLocalRacePlayer(playerId) ? GetSelectedCharacterTable(character) : CUSTOM_CHARACTER_TABLE_DEFAULT;
}

bool ShouldMuteCharacterVoice(const Kart::Link* link) {
    const Racedata* racedata = Racedata::sInstance;
    if (link == nullptr || racedata == nullptr) return false;

    const u8 playerId = link->GetPlayerIdx();
    if (playerId >= racedata->racesScenario.playerCount) return false;

    const CharacterId character = racedata->racesScenario.players[playerId].characterId;
    return GetRaceCharacterTable(playerId, character) == CUSTOM_CHARACTER_TABLE_SKIN2;
}

static bool ShouldMuteMenuCharacterVoice() {
    if (IsRaceSectionActive()) return false;
    if (buildingMenuDriverModelTable == CUSTOM_CHARACTER_TABLE_SKIN2) return true;
    if (currentMenuDriverModelTable == CUSTOM_CHARACTER_TABLE_SKIN2) return true;
    if (IsCharacterSelectPageActive()) return GetSelectedCharacterTable(GetPreviewCharacterForHud(0)) == CUSTOM_CHARACTER_TABLE_SKIN2;
    return false;
}

static const char** GetCharacterPostfixEntry(CharacterId character) {
    if (character >= 0x30) return nullptr;

    const char** table = reinterpret_cast<const char**>(CHARACTER_POSTFIX_TABLE_ADDRESS);
    return &table[character];
}

class ScopedCharacterPostfixSwap {
   public:
    ScopedCharacterPostfixSwap(u8 playerId, CharacterId character) : entry(nullptr), previousValue(nullptr) {
        this->entry = GetCharacterPostfixEntry(character);
        if (this->entry == nullptr) return;

        this->previousValue = *this->entry;
        const RKNet::Controller* controller = RKNet::Controller::sInstance;
        u8 tableIdx = CUSTOM_CHARACTER_TABLE_DEFAULT;
        if (IsOnlineRoom(controller) && !IsOnlineMultiLocal(controller) && isDisplayCustomSkinsEnabled()) {
            if (IsLocalRacePlayer(playerId)) {
                tableIdx = GetSelectedCharacterTable(character);
            } else if (playerId < 12) {
                tableIdx = NormalizeCharacterTable(character, onlineCharacterTables[playerId]);
            }
        } else if (IsLocalRacePlayer(playerId)) {
            tableIdx = GetSelectedCharacterTable(character);
        }

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

void ResetOnlineCustomCharacterFlags() {
    for (int playerId = 0; playerId < 12; ++playerId) {
        onlineCharacterTables[playerId] = CUSTOM_CHARACTER_TABLE_DEFAULT;
    }
}

static u8 GetMenuModelPlayerCount() {
    MenuModelMgr* menuModelMgr = MenuModelMgr::sInstance;
    if (menuModelMgr == nullptr) return 0;
    return menuModelMgr->playerCount;
}

static void ResetMenuDriverModelCache() {
    pendingMenuDriverReinitFrames = 0;
    cachedMenuModelMgr = nullptr;
    cachedMenuDriverModelPlayerCount = 0;
    currentMenuDriverModelTable = IsAnyCustomCharacterEnabled() ? CUSTOM_CHARACTER_TABLE_INVALID : CUSTOM_CHARACTER_TABLE_DEFAULT;
    buildingMenuDriverModelTable = CUSTOM_CHARACTER_TABLE_INVALID;
    for (u32 tableIdx = 0; tableIdx < CUSTOM_CHARACTER_TABLE_COUNT; ++tableIdx) {
        cachedMenuDriverModels[tableIdx] = nullptr;
    }
    const SectionMgr* const sectionMgr = SectionMgr::sInstance;
    if (sectionMgr != nullptr && sectionMgr->sectionParams != nullptr) {
        const u8 localPlayerCount = sectionMgr->sectionParams->localPlayerCount > 4 ? 4 : sectionMgr->sectionParams->localPlayerCount;
        for (u8 hudSlotId = 0; hudSlotId < localPlayerCount; ++hudSlotId) {
            hoveredCharacterByHud[hudSlotId] = sectionMgr->sectionParams->characters[hudSlotId];
        }
    }
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
        for (u32 tableIdx = 0; tableIdx < CUSTOM_CHARACTER_TABLE_COUNT; ++tableIdx) {
            cachedMenuDriverModels[tableIdx] = nullptr;
        }
    }

    if (currentMenuDriverModelTable < CUSTOM_CHARACTER_TABLE_COUNT &&
        cachedMenuDriverModels[currentMenuDriverModelTable] == nullptr) {
        cachedMenuDriverModels[currentMenuDriverModelTable] = menuModelMgr->driverModels;
    }
}

static void UnlockHeap(EGG::Heap* heap) {
    if (heap != nullptr) heap->dameFlag &= ~0x1;
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

kmRuntimeUse(0x8059dfbc);
static MenuModelMgr* CreateMenuModelManager() {
    typedef MenuModelMgr* (*CreateMenuModelManagerFn)();
    const CreateMenuModelManagerFn original = reinterpret_cast<CreateMenuModelManagerFn>(kmRuntimeAddr(0x8059dfbc));
    return original();
}

kmRuntimeUse(0x8059e04c);
static void DestroyMenuModelManager() {
    typedef void (*DestroyMenuModelManagerFn)();
    const DestroyMenuModelManagerFn original = reinterpret_cast<DestroyMenuModelManagerFn>(kmRuntimeAddr(0x8059e04c));
    original();
}

static void ClearMenuDriverModels(MenuDriverModelMgr& driverModelMgr) {
    if (driverModelMgr.bangs != nullptr) driverModelMgr.bangs->ToggleVisible(false);

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

kmRuntimeUse(0x8059e250);
kmRuntimeUse(0x805f5c94);
static void InitMenuModelManager(MenuModelMgr& menuModelMgr, u8 playerCount) {
    typedef void (*InitMenuModelManagerFn)(MenuModelMgr*, u8, void*);
    const InitMenuModelManagerFn original = reinterpret_cast<InitMenuModelManagerFn>(kmRuntimeAddr(0x8059e250));
    original(&menuModelMgr, playerCount, reinterpret_cast<void*>(kmRuntimeAddr(0x805f5c94)));
}

kmRuntimeUse(0x8059e1fc);
static void StartMenuModelManager(MenuModelMgr& menuModelMgr) {
    typedef void (*StartMenuModelManagerFn)(MenuModelMgr*);
    const StartMenuModelManagerFn original = reinterpret_cast<StartMenuModelManagerFn>(kmRuntimeAddr(0x8059e1fc));
    original(&menuModelMgr);
}

kmRuntimeUse(0x80830180);
static MenuDriverModelMgr* CreateMenuDriverModelManager(u8 playerCount) {
    typedef MenuDriverModelMgr* (*CreateMenuDriverModelManagerFn)(MenuDriverModelMgr*, u8);
    GameScene* const currentScene = const_cast<GameScene*>(GameScene::GetCurrent());
    EGG::ExpHeap* modelHeap = nullptr;
    EGG::ExpHeap* originalStructMem1Heap = nullptr;
    EGG::ExpHeap* originalSceneMem1Heap = nullptr;
    EGG::ExpHeap* originalSceneMem2Heap = nullptr;
    EGG::Heap* previousHeap = nullptr;
    const bool shouldKeepStructHeapLayout = currentScene != nullptr && (currentScene->id == SCENE_ID_GLOBE || currentScene->id == SCENE_ID_MENU);
    if (currentScene != nullptr) {
        modelHeap = currentScene->structsHeaps.heaps[1];
        originalStructMem1Heap = currentScene->structsHeaps.heaps[0];
        originalSceneMem1Heap = currentScene->expHeapGroup.heaps[0];
        originalSceneMem2Heap = currentScene->expHeapGroup.heaps[1];
    }

    // The vanilla constructor hardcodes structsHeaps[0] for model/scn allocations.
    if (!shouldKeepStructHeapLayout && currentScene != nullptr && modelHeap != nullptr) {
        previousHeap = modelHeap->BecomeCurrentHeap();
        currentScene->structsHeaps.heaps[0] = modelHeap;
    }

    // Mii body/head setup uses the inherited scene heap group directly. Redirect it as well
    // so custom menu-driver caches do not exhaust the small globe MEM1 scene heap.
    if (currentScene != nullptr && modelHeap != nullptr) {
        currentScene->expHeapGroup.heaps[0] = modelHeap;
        currentScene->expHeapGroup.heaps[1] = modelHeap;
    }

    EGG::Heap* allocationHeap = modelHeap;
    if (shouldKeepStructHeapLayout) allocationHeap = originalStructMem1Heap;

    void* memory = allocationHeap != nullptr ? operator new(sizeof(MenuDriverModelMgr), allocationHeap) : operator new(sizeof(MenuDriverModelMgr));
    if (memory == nullptr) {
        if (currentScene != nullptr && modelHeap != nullptr) {
            currentScene->expHeapGroup.heaps[0] = originalSceneMem1Heap;
            currentScene->expHeapGroup.heaps[1] = originalSceneMem2Heap;
        }
        if (!shouldKeepStructHeapLayout && currentScene != nullptr && modelHeap != nullptr) {
            currentScene->structsHeaps.heaps[0] = originalStructMem1Heap;
            if (previousHeap != nullptr) previousHeap->BecomeCurrentHeap();
        }
        return nullptr;
    }

    const CreateMenuDriverModelManagerFn original = reinterpret_cast<CreateMenuDriverModelManagerFn>(kmRuntimeAddr(0x80830180));
    MenuDriverModelMgr* const manager = original(reinterpret_cast<MenuDriverModelMgr*>(memory), playerCount);

    if (currentScene != nullptr && modelHeap != nullptr) {
        currentScene->expHeapGroup.heaps[0] = originalSceneMem1Heap;
        currentScene->expHeapGroup.heaps[1] = originalSceneMem2Heap;
    }
    if (!shouldKeepStructHeapLayout && currentScene != nullptr && modelHeap != nullptr) {
        currentScene->structsHeaps.heaps[0] = originalStructMem1Heap;
        if (previousHeap != nullptr) previousHeap->BecomeCurrentHeap();
    }
    return manager;
}

kmRuntimeUse(0x80830748);
static void StartMenuDriverModelManager(MenuDriverModelMgr& driverModelMgr) {
    typedef void (*StartMenuDriverModelManagerFn)(MenuDriverModelMgr*);
    const StartMenuDriverModelManagerFn original = reinterpret_cast<StartMenuDriverModelManagerFn>(kmRuntimeAddr(0x80830748));
    original(&driverModelMgr);
}

kmRuntimeUse(0x807dbd80);
static MiiHeadsModel* CreateMenuMiiHeadModelHook(void* memory, u32 type, MiiDriverModel* driverModel, u32 miiId, Mii* mii, u32 r8) {
    const GameScene* const currentScene = GameScene::GetCurrent();
    if (currentScene != nullptr && (currentScene->id == SCENE_ID_GLOBE ||currentScene->id == SCENE_ID_MENU) &&
        buildingMenuDriverModelTable < CUSTOM_CHARACTER_TABLE_COUNT) {
        return nullptr;
    }

    typedef MiiHeadsModel* (*CreateMiiHeadModelFn)(void*, u32, MiiDriverModel*, u32, Mii*, u32);
    const CreateMiiHeadModelFn original = reinterpret_cast<CreateMiiHeadModelFn>(kmRuntimeAddr(0x807dbd80));
    return original(memory, type, driverModel, miiId, mii, r8);
}
kmCall(0x80830540, CreateMenuMiiHeadModelHook);

kmRuntimeUse(0x807da5c0);
kmRuntimeUse(0x8055ba64);
static ToadetteHair* CreateMenuToadetteHairHook(ToadetteHair* hair, g3d::ResFile& file, ModelDirector* toadette, u32 r6) {
    if (buildingMenuDriverModelTable == CUSTOM_CHARACTER_TABLE_SKIN2) return nullptr;

    typedef bool (*MdlExistsFn)(const char*, const g3d::ResFile&);
    const MdlExistsFn mdlExists = reinterpret_cast<MdlExistsFn>(kmRuntimeAddr(0x8055ba64));
    if (!mdlExists("hair", file)) return nullptr;

    typedef ToadetteHair* (*CreateMenuToadetteHairFn)(ToadetteHair*, g3d::ResFile&, ModelDirector*, u32);
    const CreateMenuToadetteHairFn original = reinterpret_cast<CreateMenuToadetteHairFn>(kmRuntimeAddr(0x807da5c0));
    return original(hair, file, toadette, r6);
}
kmCall(0x808303d0, CreateMenuToadetteHairHook);

kmRuntimeUse(0x807db028);
static void ToggleMenuToadetteHairLockHook(ToadetteHair* hair, bool isLocked) {
    if (hair == nullptr) return;

    typedef void (*ToggleMenuToadetteHairLockFn)(ToadetteHair*, bool);
    const ToggleMenuToadetteHairLockFn original = reinterpret_cast<ToggleMenuToadetteHairLockFn>(kmRuntimeAddr(0x807db028));
    original(hair, isLocked);
}
kmCall(0x808309e4, ToggleMenuToadetteHairLockHook);
kmCall(0x808309f4, ToggleMenuToadetteHairLockHook);

kmRuntimeUse(0x80590a5c);
extern "C" bool ShouldSkipRaceToadetteHairForController(DriverController* controller) {
    typedef u8 (*GetPlayerIdxFn)(const Kart::Link*);
    const GetPlayerIdxFn getPlayerIdx = reinterpret_cast<GetPlayerIdxFn>(kmRuntimeAddr(0x80590a5c));
    const u8 playerId = controller != nullptr ? getPlayerIdx(controller) : 0xff;
    return GetRaceCharacterTable(playerId, TOADETTE) == CUSTOM_CHARACTER_TABLE_SKIN2;
}

extern "C" void ToadetteFix1(void*);
extern "C" void ToadetteFix2(void*);
static asmFunc SkipRaceToadetteHairForSkin2Hook() {
    ASM(
        nofralloc;
        stwu r1, -0x10(r1);
        mflr r0;
        stw r0, 0x14(r1);
        mr r3, r29;
        bl ShouldSkipRaceToadetteHairForController;
        cmpwi r3, 0;
        lwz r0, 0x14(r1);
        mtlr r0;
        addi r1, r1, 0x10;
        bne skipHair;

        lwz r3, 0x0(r29);
        lwz r3, 0x0(r3);
        lwz r0, 0x8(r3);
        cmpwi r0, 0xd;
        bne skipHair;

        lis r12, ToadetteFix1 @h;
        ori r12, r12, ToadetteFix1 @l;
        mtctr r12;
        bctr;

        skipHair : lis r12, ToadetteFix2 @h;
        ori r12, r12, ToadetteFix2 @l;
        mtctr r12;
        bctr;)
}
kmBranch(0x807c89a8, SkipRaceToadetteHairForSkin2Hook);

static void SetupMenuCharacterBRASDHook(Audio::LinkedRaceActor* actor, nw4r::snd::detail::AnimSoundFile* rawBRASD) {
    if (ShouldMuteMenuCharacterVoice()) return;
    actor->SetupBRASD(rawBRASD);
}
kmCall(0x805572d4, SetupMenuCharacterBRASDHook);

kmRuntimeUse(0x80830d00);
static void RequestDriverModelHook(MenuModelMgr* menuModelMgr, u8 playerId, CharacterId character) {
    if (menuModelMgr == nullptr || !menuModelMgr->isActive) return;

    MenuDriverModelMgr* const driverModels = menuModelMgr->driverModels;
    if (driverModels == nullptr || playerId >= driverModels->playerCount) return;

    MenuDriverModel* const playerModel = driverModels->players[playerId].playerModel;
    const bool wasVisible = driverModels->players[playerId].isVisible;
    if (playerModel != nullptr) playerModel->ToggleVisible(false);

    typedef void (*SetPlayerCharacterFn)(MenuDriverModelMgr*, u8, CharacterId);
    const SetPlayerCharacterFn original = reinterpret_cast<SetPlayerCharacterFn>(kmRuntimeAddr(0x80830d00));
    original(driverModels, playerId, character);

    if (wasVisible) driverModels->TogglePlayerModel(playerId, true);
}
kmBranch(0x8059e568, RequestDriverModelHook);

static u8 GetMenuDriverModelTableForCharacter(CharacterId character) {
    return GetSelectedCharacterTable(character);
}

static void ScheduleMenuDriverModelReinitForPreview() {
    if (!IsCharacterSelectPageActive()) return;

    const u8 targetTable = GetMenuDriverModelTableForCharacter(GetPreviewCharacterForHud(0));
    if (targetTable != currentMenuDriverModelTable) pendingMenuDriverReinitFrames = 1;
}

static void ReinitializeMenuDriverModels() {
    MenuModelMgr* const menuModelMgr = MenuModelMgr::sInstance;
    GameScene* const currentScene = const_cast<GameScene*>(GameScene::GetCurrent());
    const SectionMgr* const sectionMgr = SectionMgr::sInstance;
    if (menuModelMgr == nullptr || currentScene == nullptr || sectionMgr == nullptr || sectionMgr->sectionParams == nullptr) return;
    if (menuModelMgr->heap == nullptr) return;

    SyncMenuDriverModelCache();

    const u8 playerCount = GetMenuModelPlayerCount();
    if (playerCount == 0) return;

    MenuDriverModelMgr* const oldDriverModels = menuModelMgr->driverModels;
    const u8 targetTable = GetMenuDriverModelTableForCharacter(GetPreviewCharacterForHud(0));
    if (targetTable == currentMenuDriverModelTable && oldDriverModels != nullptr) return;

    MenuDriverModelMgr* newDriverModels = cachedMenuDriverModels[targetTable];
    if (newDriverModels == nullptr) {
        UnlockMenuDriverModelHeaps(*currentScene, *menuModelMgr);
        currentScene->structsHeaps.SetHeapsGroupId(3);
        buildingMenuDriverModelTable = targetTable;
        ApplyMenuDriverModelTablePostfixes(targetTable);
        newDriverModels = CreateMenuDriverModelManager(playerCount);
        buildingMenuDriverModelTable = CUSTOM_CHARACTER_TABLE_INVALID;
        ApplyCharacterPostfixes();
        currentScene->structsHeaps.SetHeapsGroupId(0);
        if (newDriverModels == nullptr) {
            currentScene->structsHeaps.SetHeapsGroupId(6);
            return;
        }

        cachedMenuDriverModels[targetTable] = newDriverModels;
    }

    if (oldDriverModels != nullptr && oldDriverModels != newDriverModels) {
        ClearMenuDriverModels(*oldDriverModels);
    }

    menuModelMgr->driverModels = newDriverModels;
    StartMenuDriverModelManager(*newDriverModels);
    currentScene->structsHeaps.SetHeapsGroupId(6);

    const u8 localPlayerCount = sectionMgr->sectionParams->localPlayerCount > 4 ? 4 : sectionMgr->sectionParams->localPlayerCount;
    for (u8 hudSlotId = 0; hudSlotId < localPlayerCount; ++hudSlotId) {
        menuModelMgr->RequestDriverModel(hudSlotId, sectionMgr->sectionParams->characters[hudSlotId]);
    }

    if (oldDriverModels != nullptr && oldDriverModels != newDriverModels) {
        for (u8 playerId = 0; playerId < playerCount; ++playerId) {
            const bool isVisible = oldDriverModels->players[playerId].isVisible;
            newDriverModels->players[playerId].isVisible = isVisible;
            newDriverModels->TogglePlayerModel(playerId, isVisible);
        }
    }

    RefreshCharacterSelectModels();
    RefreshKartSelectModel();
    currentMenuDriverModelTable = targetTable;
}

static void ProcessPendingMenuDriverModelReinit() {
    if (pendingMenuDriverReinitFrames == 0) return;
    if (--pendingMenuDriverReinitFrames != 0) return;
    if (!IsCharacterSelectPageActive()) return;

    ReinitializeMenuDriverModels();
}

static void RefreshCharacterSelectModels() {
    SectionMgr* const sectionMgr = SectionMgr::sInstance;
    if (sectionMgr == nullptr || sectionMgr->sectionParams == nullptr || sectionMgr->curSection == nullptr) return;

    Pages::CharacterSelect* const characterSelect = sectionMgr->curSection->Get<Pages::CharacterSelect>();
    if (characterSelect == nullptr || characterSelect->models == nullptr) return;

    const u8 localPlayerCount = sectionMgr->sectionParams->localPlayerCount > 4 ? 4 : sectionMgr->sectionParams->localPlayerCount;
    for (u8 hudSlotId = 0; hudSlotId < localPlayerCount; ++hudSlotId) {
        characterSelect->models[hudSlotId].RequestModel(GetPreviewCharacterForHud(hudSlotId));
    }
}

static CharacterId GetPreviewCharacterForHud(u8 hudSlotId) {
    if (hudSlotId >= 4) return MARIO;

    const SectionMgr* const sectionMgr = SectionMgr::sInstance;
    if (sectionMgr == nullptr || sectionMgr->sectionParams == nullptr) {
        return hoveredCharacterByHud[hudSlotId];
    }

    CharacterId previewCharacter = hoveredCharacterByHud[hudSlotId];
    if (previewCharacter >= 0x30) {
        previewCharacter = sectionMgr->sectionParams->characters[hudSlotId];
    }
    return previewCharacter;
}

static CharacterId GetSelectedCharacterForHud(u8 hudSlotId) {
    if (hudSlotId >= 4) return MARIO;

    const SectionMgr* const sectionMgr = SectionMgr::sInstance;
    if (sectionMgr != nullptr && sectionMgr->sectionParams != nullptr &&
        hudSlotId < sectionMgr->sectionParams->localPlayerCount) {
        return sectionMgr->sectionParams->characters[hudSlotId];
    }

    const Racedata* const racedata = Racedata::sInstance;
    if (racedata != nullptr && hudSlotId < racedata->menusScenario.localPlayerCount) {
        const u8 playerId = racedata->menusScenario.settings.hudPlayerIds[hudSlotId];
        if (playerId < 12) return racedata->menusScenario.players[playerId].characterId;
    }

    return hoveredCharacterByHud[hudSlotId];
}

static void RefreshKartSelectModel() {
    SectionMgr* const sectionMgr = SectionMgr::sInstance;
    if (sectionMgr == nullptr || sectionMgr->sectionParams == nullptr || sectionMgr->curSection == nullptr) return;

    Pages::KartSelect* const kartSelect = sectionMgr->curSection->Get<Pages::KartSelect>();
    if (kartSelect == nullptr) return;
    if (sectionMgr->sectionParams->localPlayerCount == 0) return;

    kartSelect->vehicleModel.RequestModel(sectionMgr->sectionParams->karts[0]);
}

static bool IsCharacterSelectPageActive() {
    const SectionMgr* sectionMgr = SectionMgr::sInstance;
    if (sectionMgr == nullptr || sectionMgr->curSection == nullptr) return false;

    const Pages::CharacterSelect* characterSelect = sectionMgr->curSection->Get<Pages::CharacterSelect>();
    if (characterSelect == nullptr) return false;

    const Page* topPage = sectionMgr->curSection->GetTopLayerPage();
    return topPage == characterSelect && characterSelect->currentState == STATE_ACTIVE && !characterSelect->updateState;
}

void RefreshLocalOnlineCustomCharacterFlags() {
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    if (!IsOnlineRoom(controller)) return;
    if (IsOnlineMultiLocal(controller)) return;
    if (!isDisplayCustomSkinsEnabled()) return;

    const Racedata* racedata = Racedata::sInstance;
    if (racedata == nullptr) return;

    const RacedataScenario& scenario = racedata->racesScenario;
    const u8 localPlayerCount = scenario.localPlayerCount > 4 ? 4 : scenario.localPlayerCount;

    for (u8 hudSlotId = 0; hudSlotId < localPlayerCount; ++hudSlotId) {
        const u32 playerId = racedata->GetPlayerIdOfLocalPlayer(hudSlotId);
        if (playerId < 12) {
            onlineCharacterTables[playerId] = GetSelectedCharacterTable(scenario.players[playerId].characterId);
        }
    }
}

void UpdateOnlineCharacterTablesFromAid(u8 aid, const u8* playerIdToAid, u8 characterTables) {
    if (playerIdToAid == nullptr) return;

    u8 hudSlotId = 0;
    for (u8 playerId = 0; playerId < 12; ++playerId) {
        if (playerIdToAid[playerId] != aid) continue;

        const u8 shift = static_cast<u8>(hudSlotId * CUSTOM_CHARACTER_TABLE_PACKET_BITS);
        const u8 tableIdx = hudSlotId < 2 ? ((characterTables >> shift) & CUSTOM_CHARACTER_TABLE_PACKET_MASK) : CUSTOM_CHARACTER_TABLE_DEFAULT;
        onlineCharacterTables[playerId] = tableIdx < CUSTOM_CHARACTER_TABLE_COUNT ? tableIdx : CUSTOM_CHARACTER_TABLE_DEFAULT;
        ++hudSlotId;
    }
}

u8 GetLocalOnlineCharacterTables() {
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
    return playerId < 12 && onlineCharacterTables[playerId] != CUSTOM_CHARACTER_TABLE_DEFAULT;
}

kmRuntimeUse(0x80540e3c);
static ArchivesHolder* LoadKartArchiveHook(ArchiveMgr* archiveMgr, u8 playerId, KartId kart, CharacterId character, u32 color, u32 type, EGG::Heap* decompressedHeap, EGG::Heap* archiveHeap) {
    typedef ArchivesHolder* (*LoadKartArchiveFn)(ArchiveMgr*, u8, KartId, CharacterId, u32, u32, EGG::Heap*, EGG::Heap*);
    const LoadKartArchiveFn original = reinterpret_cast<LoadKartArchiveFn>(kmRuntimeAddr(0x80540e3c));
    ScopedCharacterPostfixSwap swap(playerId, character);
    return original(archiveMgr, playerId, kart, character, color, type, decompressedHeap, archiveHeap);
}
kmCall(0x805540f4, LoadKartArchiveHook);

kmRuntimeUse(0x80540f90);
static ArchivesHolder* LoadKartArchiveHolder2Hook(ArchiveMgr* archiveMgr, u8 playerId, KartId kart, CharacterId character, u32 color, u32 type, EGG::Heap* decompressedHeap, EGG::Heap* archiveHeap) {
    typedef ArchivesHolder* (*LoadKartArchiveHolder2Fn)(ArchiveMgr*, u8, KartId, CharacterId, u32, u32, EGG::Heap*, EGG::Heap*);
    const LoadKartArchiveHolder2Fn original = reinterpret_cast<LoadKartArchiveHolder2Fn>(kmRuntimeAddr(0x80540f90));
    ScopedCharacterPostfixSwap swap(playerId, character);
    return original(archiveMgr, playerId, kart, character, color, type, decompressedHeap, archiveHeap);
}
kmCall(0x80554198, LoadKartArchiveHolder2Hook);

kmRuntimeUse(0x805410e4);
static ArchivesHolder* LoadMenuKartArchiveHook(ArchiveMgr* archiveMgr, u8 playerId, CharacterId character, u32 type, EGG::Heap* archiveHeap, EGG::Heap* dumpHeap) {
    typedef ArchivesHolder* (*LoadMenuKartArchiveFn)(ArchiveMgr*, u8, CharacterId, u32, EGG::Heap*, EGG::Heap*);
    const LoadMenuKartArchiveFn original = reinterpret_cast<LoadMenuKartArchiveFn>(kmRuntimeAddr(0x805410e4));
    ScopedCharacterPostfixSwap swap(playerId, character);
    return original(archiveMgr, playerId, character, type, archiveHeap, dumpHeap);
}
kmBranch(0x805410e4, LoadMenuKartArchiveHook);

kmRuntimeUse(0x805419c8);
static const char* GetMenuDriverBRRESNameHook(u32 character) {
    const CharacterId characterId = static_cast<CharacterId>(character);
    const u8 tableIdx = buildingMenuDriverModelTable < CUSTOM_CHARACTER_TABLE_COUNT ? buildingMenuDriverModelTable : GetMenuDriverModelTableForCharacter(characterId);
    const char* overrideName = GetDriverBRRESName(characterId, tableIdx);
    if (overrideName != nullptr) return overrideName;

    typedef const char* (*GetCharacterNameFn)(u32);
    const GetCharacterNameFn original = reinterpret_cast<GetCharacterNameFn>(kmRuntimeAddr(0x805419c8));
    return original(character);
}
kmCall(0x8081e4a0, GetMenuDriverBRRESNameHook);

static void RefreshOnlineCustomCharacterFlagsOnRaceLoad() {
    RefreshLocalOnlineCustomCharacterFlags();
}
static RaceLoadHook RefreshOnlineCustomCharacterFlagsHook(RefreshOnlineCustomCharacterFlagsOnRaceLoad);

void SetCharacter() {
    if (!IsOnlineRoom(RKNet::Controller::sInstance)) ResetOnlineCustomCharacterFlags();
    ResetMenuDriverModelCache();
    EnsureActiveCustomCharacterTable();
}
static SectionLoadHook SetCharacterHook(SetCharacter);

kmRuntimeUse(0x8083e5f4);
static void CharacterSelectHoverHook(Pages::CharacterSelect* page, CtrlMenuCharacterSelect::ButtonDriver* button, u32 buttonId, u8 hudSlotId) {
    if (hudSlotId < 4) {
        hoveredCharacterByHud[hudSlotId] = static_cast<CharacterId>(buttonId);
        currentMenuDriverModelTable = CUSTOM_CHARACTER_TABLE_INVALID;
        ReinitializeMenuDriverModels();
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

kmRuntimeUse(0x8083dfa8);
static void CharacterSelectClickHook(Pages::CharacterSelect* page, PushButton* button, u32 buttonId, u8 hudSlotId) {
    if (hudSlotId < 4) {
        hoveredCharacterByHud[hudSlotId] = static_cast<CharacterId>(buttonId);
        if (hudSlotId == 0) ScheduleMenuDriverModelReinitForPreview();
        ReinitializeMenuDriverModels();
    }
    typedef void (*CharacterSelectClickFn)(Pages::CharacterSelect*, PushButton*, u32, u8);
    const CharacterSelectClickFn original = reinterpret_cast<CharacterSelectClickFn>(kmRuntimeAddr(0x8083dfa8));
    original(page, button, buttonId, hudSlotId);
}
kmCall(0x807e3570, CharacterSelectClickHook);
kmCall(0x807e36bc, CharacterSelectClickHook);
kmCall(0x807e39b4, CharacterSelectClickHook);
kmCall(0x807e3bd0, CharacterSelectClickHook);

static ControllerType GetControllerTypeForHudSlot(const SectionMgr& sectionMgr, u8 hudSlotId) {
    if (hudSlotId >= 4) return GCN;

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

    const u8 localPlayerCount = sectionMgr->sectionParams->localPlayerCount > 4 ? 4 : sectionMgr->sectionParams->localPlayerCount;
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

kmRuntimeUse(0x8063550c);
// RaceScene::calcSubsystems -> SectionMgr::Update call site
static void RaceSceneSectionUpdateHook(SectionMgr* sectionMgr) {
    UpdateCustomCharacterSelectNamePaneIcons();
    ShouldProcessCustomCharacterInput();
    typedef void (*SectionMgrUpdateFn)(SectionMgr*);
    const SectionMgrUpdateFn original = reinterpret_cast<SectionMgrUpdateFn>(kmRuntimeAddr(0x8063550c));
    original(sectionMgr);
    ProcessPendingMenuDriverModelReinit();
}
kmCall(0x80554dcc, RaceSceneSectionUpdateHook);

kmRuntimeUse(0x8063583c);
// MenuScene_vf30 / GlobeScene_vf30 -> SectionMgr::MenuUpdate call sites
static void MenuSceneSectionUpdateHook(SectionMgr* sectionMgr) {
    UpdateCustomCharacterSelectNamePaneIcons();
    ShouldProcessCustomCharacterInput();
    typedef void (*SectionMgrUpdateFn)(SectionMgr*);
    const SectionMgrUpdateFn original = reinterpret_cast<SectionMgrUpdateFn>(kmRuntimeAddr(0x8063583c));
    original(sectionMgr);
    ProcessPendingMenuDriverModelReinit();
}
kmCall(0x805552e8, MenuSceneSectionUpdateHook);
kmCall(0x80553b30, MenuSceneSectionUpdateHook);

}  // namespace CustomCharacters
}  // namespace Pulsar
