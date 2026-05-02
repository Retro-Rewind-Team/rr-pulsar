#include <hooks.hpp>
#include <runtimeWrite.hpp>
#include <CustomCharacters.hpp>
#include <Settings/Settings.hpp>
#include <MarioKartWii/Archive/ArchiveMgr.hpp>
#include <MarioKartWii/UI/Page/Menu/CharacterSelect.hpp>
#include <MarioKartWii/UI/Page/Other/ModelRenderer.hpp>
#include <MarioKartWii/3D/Model/Menu/MenuDriverModel.hpp>
#include <MarioKartWii/3D/Model/Menu/MenuKartModel.hpp>
#include <MarioKartWii/3D/Model/Menu/MenuModelMgr.hpp>
#include <MarioKartWii/Driver/DriverController.hpp>
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
#include <MarioKartWii/UI/Ctrl/CtrlRace/CtrlRace2DMap.hpp>
#include <MarioKartWii/UI/Ctrl/CtrlRace/CtrlRaceResult.hpp>
#include <core/egg/DVD/DvdRipper.hpp>
#include <core/rvl/dvd/dvd.hpp>
#include <core/nw4r/ut/List.hpp>
#include <MarioKartWii/3D/Scn/ScnMgr.hpp>
#include <UI/UI.hpp>
#include <UI/ChangeCombo/ChangeCombo.hpp>

namespace Pulsar {
namespace CustomCharacters {

#define ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))

enum {
    TABLE_DEFAULT = 0,
    TABLE_SKIN1,
    TABLE_SKIN2,
    TABLE_SKIN3,
    TABLE_SKIN4,
    TABLE_SKIN5,
    TABLE_COUNT,
    TABLE_INVALID = TABLE_COUNT,

    CHARACTER_COUNT = 0x30,
    LOCAL_PLAYER_COUNT = 4,
    ONLINE_PLAYER_COUNT = 12,
    MII_C_COUNT = 6,
    PACKET_BITS = 3,
    PACKET_MASK = (1 << PACKET_BITS) - 1,
    MENU_MII_SLOTS_PER_PLAYER = 3
};

static_assert(TABLE_COUNT <= (1 << PACKET_BITS), "SELECT packet skin table field is too small");
static_assert(PACKET_BITS * 2 <= 8, "SELECT packet skin table fields must fit in one byte");

struct CharacterOverride {
    CharacterId character;
    u8 tableIdx;
    const char* postfix;
    bool silentVoice;
    u32 nameBmgId;
    const char* authorText;
};

#define CUSTOM_CHARACTER_NAME_BMG(index) (UI::BMG_CUSTOM_CHARACTER_NAME_START + (index))

static const CharacterOverride customCharacterAssets[] = {
    {MARIO, TABLE_SKIN1, "mr-1", false, CUSTOM_CHARACTER_NAME_BMG(0), "LTC 91"},
    {MARIO, TABLE_SKIN2, "mr-2", false, CUSTOM_CHARACTER_NAME_BMG(1), "UltraWario"},
    {MARIO, TABLE_SKIN3, "mr-3", true, CUSTOM_CHARACTER_NAME_BMG(2), "GioMacGrillin"},
    {BABY_PEACH, TABLE_SKIN1, "bpc-1", false, CUSTOM_CHARACTER_NAME_BMG(3), "UltraWario"},
    {BABY_PEACH, TABLE_SKIN2, "bpc-2", true, CUSTOM_CHARACTER_NAME_BMG(4), "ALE XD"},
    {WALUIGI, TABLE_SKIN1, "wl-1", false, CUSTOM_CHARACTER_NAME_BMG(5), "TheBeefBai"},
    {WALUIGI, TABLE_SKIN2, "wl-2", false, CUSTOM_CHARACTER_NAME_BMG(6), "UltraWario"},
    {WALUIGI, TABLE_SKIN3, "wl-3", true, CUSTOM_CHARACTER_NAME_BMG(7), "UltraWario"},
    {BOWSER, TABLE_SKIN1, "kp-1", false, CUSTOM_CHARACTER_NAME_BMG(8), "UltraWario"},
    {BOWSER, TABLE_SKIN2, "kp-2", false, CUSTOM_CHARACTER_NAME_BMG(9), "UltraWario"},
    {BOWSER, TABLE_SKIN3, "kp-3", true, CUSTOM_CHARACTER_NAME_BMG(10), "UltraWario, Porko"},
    {BOWSER, TABLE_SKIN4, "kp-4", true, CUSTOM_CHARACTER_NAME_BMG(11), "Numerosity"},
    {BABY_DAISY, TABLE_SKIN1, "bds-1", false, CUSTOM_CHARACTER_NAME_BMG(12), "Poobah"},
    {BABY_DAISY, TABLE_SKIN2, "bds-2", true, CUSTOM_CHARACTER_NAME_BMG(13), "Porko, UltraWario"},
    {DRY_BONES, TABLE_SKIN1, "ka-1", false, CUSTOM_CHARACTER_NAME_BMG(14), "Tank2go"},
    {DRY_BONES, TABLE_SKIN2, "ka-2", false, CUSTOM_CHARACTER_NAME_BMG(15), "LTC 91"},
    {DRY_BONES, TABLE_SKIN3, "ka-3", true, CUSTOM_CHARACTER_NAME_BMG(16), "UltraWario"},
    {DRY_BONES, TABLE_SKIN4, "ka-4", true, CUSTOM_CHARACTER_NAME_BMG(17), "GioMacGrillin"},
    {BABY_MARIO, TABLE_SKIN1, "bmr-1", false, CUSTOM_CHARACTER_NAME_BMG(18), "Whipinsnapper"},
    {BABY_MARIO, TABLE_SKIN2, "bmr-2", false, CUSTOM_CHARACTER_NAME_BMG(19), "LTC 91"},
    {BABY_MARIO, TABLE_SKIN3, "bmr-3", true, CUSTOM_CHARACTER_NAME_BMG(20), "GioMacGrillin"},
    {BABY_MARIO, TABLE_SKIN4, "bmr-4", true, CUSTOM_CHARACTER_NAME_BMG(21), "UltraWario"},
    {LUIGI, TABLE_SKIN1, "lg-1", false, CUSTOM_CHARACTER_NAME_BMG(22), "UltraWario"},
    {LUIGI, TABLE_SKIN2, "lg-2", false, CUSTOM_CHARACTER_NAME_BMG(23), "PlayersPurity"},
    {LUIGI, TABLE_SKIN3, "lg-3", true, CUSTOM_CHARACTER_NAME_BMG(24), "JTG"},
    {LUIGI, TABLE_SKIN4, "lg-4", true, CUSTOM_CHARACTER_NAME_BMG(25), "ALE XD"},
    {TOAD, TABLE_SKIN1, "ko-1", false, CUSTOM_CHARACTER_NAME_BMG(26), "UltraWario"},
    {TOAD, TABLE_SKIN2, "ko-2", false, CUSTOM_CHARACTER_NAME_BMG(27), "UltraWario"},
    {TOAD, TABLE_SKIN3, "ko-3", true, CUSTOM_CHARACTER_NAME_BMG(28), "Monxy"},
    {DONKEY_KONG, TABLE_SKIN1, "dk-1", false, CUSTOM_CHARACTER_NAME_BMG(29), "Whipinsnapper"},
    {DONKEY_KONG, TABLE_SKIN2, "dk-2", false, CUSTOM_CHARACTER_NAME_BMG(30), "LTC 91"},
    {DONKEY_KONG, TABLE_SKIN3, "dk-3", true, CUSTOM_CHARACTER_NAME_BMG(31), "Monxy"},
    {DONKEY_KONG, TABLE_SKIN4, "dk-4", true, CUSTOM_CHARACTER_NAME_BMG(32), "Chiller7"},
    {YOSHI, TABLE_SKIN1, "ys-1", false, CUSTOM_CHARACTER_NAME_BMG(33), "LTC 91"},
    {YOSHI, TABLE_SKIN2, "ys-2", false, CUSTOM_CHARACTER_NAME_BMG(34), "ordartz"},
    {YOSHI, TABLE_SKIN3, "ys-3", true, CUSTOM_CHARACTER_NAME_BMG(35), "UltraWario, Porko"},
    {WARIO, TABLE_SKIN1, "wr-1", false, CUSTOM_CHARACTER_NAME_BMG(36), "UltraWario"},
    {WARIO, TABLE_SKIN2, "wr-2", false, CUSTOM_CHARACTER_NAME_BMG(37), "UltraWario"},
    {WARIO, TABLE_SKIN3, "wr-3", true, CUSTOM_CHARACTER_NAME_BMG(38), "UltraWario"},
    {BABY_LUIGI, TABLE_SKIN1, "blg-1", false, CUSTOM_CHARACTER_NAME_BMG(39), "Tadhger"},
    {TOADETTE, TABLE_SKIN1, "kk-1", false, CUSTOM_CHARACTER_NAME_BMG(40), "Toadette Hack Fan"},
    {KOOPA_TROOPA, TABLE_SKIN1, "nk-1", false, CUSTOM_CHARACTER_NAME_BMG(41), "Jordi6304"},
    {KOOPA_TROOPA, TABLE_SKIN2, "nk-2", true, CUSTOM_CHARACTER_NAME_BMG(42), "UltraWario"},
    {DAISY, TABLE_SKIN1, "ds-1", false, CUSTOM_CHARACTER_NAME_BMG(43), "TheBeefBai"},
    {DAISY, TABLE_SKIN2, "ds-2", false, CUSTOM_CHARACTER_NAME_BMG(44), "ALE XD"},
    {DAISY, TABLE_SKIN3, "ds-3", false, CUSTOM_CHARACTER_NAME_BMG(45), "UltraWario"},
    {PEACH, TABLE_SKIN1, "pc-1", false, CUSTOM_CHARACTER_NAME_BMG(46), "Whipinsnapper"},
    {PEACH, TABLE_SKIN2, "pc-2", false, CUSTOM_CHARACTER_NAME_BMG(47), "GVRIMZ"},
    {PEACH, TABLE_SKIN3, "pc-3", false, CUSTOM_CHARACTER_NAME_BMG(48), "UltraWario"},
    {BIRDO, TABLE_SKIN1, "ca-1", false, CUSTOM_CHARACTER_NAME_BMG(49), "JTG"},
    {BIRDO, TABLE_SKIN2, "ca-2", true, CUSTOM_CHARACTER_NAME_BMG(50), "DJ Lowgey"},
    {DIDDY_KONG, TABLE_SKIN1, "dd-1", false, CUSTOM_CHARACTER_NAME_BMG(51), "Cazzyboy360"},
    {DIDDY_KONG, TABLE_SKIN2, "dd-2", true, CUSTOM_CHARACTER_NAME_BMG(52), "UltraWario"},
    {DIDDY_KONG, TABLE_SKIN3, "dd-3", true, CUSTOM_CHARACTER_NAME_BMG(53), "ZoroCarlos"},
    {KING_BOO, TABLE_SKIN1, "kt-1", false, CUSTOM_CHARACTER_NAME_BMG(54), "UltraWario"},
    {BOWSER_JR, TABLE_SKIN1, "jr-1", false, CUSTOM_CHARACTER_NAME_BMG(55), "UltraWario"},
    {BOWSER_JR, TABLE_SKIN2, "jr-2", true, CUSTOM_CHARACTER_NAME_BMG(56), "JuniorMBW"},
    {BOWSER_JR, TABLE_SKIN3, "jr-3", true, CUSTOM_CHARACTER_NAME_BMG(57), "DJ Lowgey"},
    {BOWSER_JR, TABLE_SKIN4, "jr-4", false, CUSTOM_CHARACTER_NAME_BMG(58), "Whipinsnapper"},
    {BOWSER_JR, TABLE_SKIN5, "jr-5", true, CUSTOM_CHARACTER_NAME_BMG(59), "UltraWario"},
    {DRY_BOWSER, TABLE_SKIN1, "bk-1", false, CUSTOM_CHARACTER_NAME_BMG(60), "Kracken"},
    {DRY_BOWSER, TABLE_SKIN2, "bk-2", true, CUSTOM_CHARACTER_NAME_BMG(61), "Ricoxemani, Cillow that Willow"},
    {FUNKY_KONG, TABLE_SKIN1, "fk-1", false, CUSTOM_CHARACTER_NAME_BMG(62), "ZPL"},
    {FUNKY_KONG, TABLE_SKIN2, "fk-2", true, CUSTOM_CHARACTER_NAME_BMG(63), "UltraWario"},
    {FUNKY_KONG, TABLE_SKIN3, "fk-3", false, CUSTOM_CHARACTER_NAME_BMG(64), "ordartz"},
    {FUNKY_KONG, TABLE_SKIN4, "fk-4", false, CUSTOM_CHARACTER_NAME_BMG(65), "Whipinsnapper"},
    {FUNKY_KONG, TABLE_SKIN5, "fk-5", true, CUSTOM_CHARACTER_NAME_BMG(66), "Cillow that Willow"},
    {ROSALINA, TABLE_SKIN1, "rs-1", false, CUSTOM_CHARACTER_NAME_BMG(67), "Chiller7"},
    {ROSALINA, TABLE_SKIN2, "rs-2", false, CUSTOM_CHARACTER_NAME_BMG(68), "Eydra"},
    {ROSALINA, TABLE_SKIN3, "rs-3", true, CUSTOM_CHARACTER_NAME_BMG(69), "UltraWario"},
    {PEACH_BIKER, TABLE_SKIN1, "pc-1", false, CUSTOM_CHARACTER_NAME_BMG(70), "TheBeefBai"},
    {PEACH_BIKER, TABLE_SKIN2, "pc-2", false, CUSTOM_CHARACTER_NAME_BMG(71), "ALE XD"},
    {PEACH_BIKER, TABLE_SKIN3, "pc-3", false, CUSTOM_CHARACTER_NAME_BMG(72), "UltraWario"},
    {DAISY_BIKER, TABLE_SKIN1, "ds-1", false, CUSTOM_CHARACTER_NAME_BMG(73), "Whipinsnapper"},
    {DAISY_BIKER, TABLE_SKIN2, "ds-2", false, CUSTOM_CHARACTER_NAME_BMG(74), "GVRIMZ"},
    {DAISY_BIKER, TABLE_SKIN3, "ds-3", false, CUSTOM_CHARACTER_NAME_BMG(75), "UltraWario"},
    {ROSALINA_BIKER, TABLE_SKIN1, "rs-1", false, CUSTOM_CHARACTER_NAME_BMG(76), "Chiller7"},
    {ROSALINA_BIKER, TABLE_SKIN2, "rs-2", false, CUSTOM_CHARACTER_NAME_BMG(77), "Eydra"},
    {ROSALINA_BIKER, TABLE_SKIN3, "rs-3", true, CUSTOM_CHARACTER_NAME_BMG(78), "UltraWario"},
};

struct RawBRRES {
    EGG::ExpHeap* heap;
    void* file;
    bool failed;
};

struct RawTPL {
    bool failed;
};

struct DriverModelCache {
    MenuModelMgr* mgr;
    EGG::ExpHeap* activeHeap;
    EGG::ExpHeap* createdHeap;
    EGG::ExpHeap* buildingHeap;
    u8 playerCount;
    u8 currentTable;
    u8 buildingTable;
    CharacterId currentCharacter;
    CharacterId currentHud0Character;
    CharacterId buildingCharacter;
    bool resetting;
};

struct VotingVRState {
    const Section* section;
    bool sawVR;
    bool exitedVR;
    bool reinitialized;
    bool modelRendererPrimed;
    u8 reinitFrames;
    u8 refreshFrames;
};

static u8 selectedTable[CHARACTER_COUNT];
static u8 onlineCharacterTables[ONLINE_PLAYER_COUNT];
static u8 offlineCpuCharacterTables[ONLINE_PLAYER_COUNT];
static const char* defaultNames[CHARACTER_COUNT];
static bool cachedDefaultNames;
static CharacterId hoveredCharacters[LOCAL_PLAYER_COUNT] = {MARIO, MARIO, MARIO, MARIO};
static RawBRRES rawBRRES[TABLE_COUNT][CHARACTER_COUNT];
static RawBRRES looseMiiCBRRES[MII_C_COUNT];
static RawTPL looseMinimapTPL[TABLE_COUNT][CHARACTER_COUNT];
static u32 offlineCpuSkinSignature;
static u8 offlineCpuSkinRaceNumber;
static bool offlineCpuSkinTablesValid;
static DriverModelCache driverCache = {0, 0, 0, 0, 0, TABLE_INVALID, TABLE_INVALID, CHARACTER_NONE, CHARACTER_NONE, CHARACTER_NONE, false};
static VotingVRState votingVR = {0, false, false, false, false, 0, 0};
static u8 pendingReinitFrames;
static u16 pendingReinitWaits;
static u16 heldToggleButtons;
static bool applyMenuDriverReinit;
static bool applyVotingVRReinit;
enum { AUTHOR_NAME_CONTROL_WORDS = (sizeof(CharaName) + sizeof(u32) - 1) / sizeof(u32) };
static u32 authorNameControlStorage[LOCAL_PLAYER_COUNT][AUTHOR_NAME_CONTROL_WORDS];
static bool authorNameControlConstructed[LOCAL_PLAYER_COUNT];
static bool authorNameControlLoaded[LOCAL_PLAYER_COUNT];
static bool loadingAuthorNameControl;
static CharaName* authorTextControl;
static const char* authorTextValue;
static wchar_t authorTextBuffer[0x100];
static CharaName* characterNameTextControl[LOCAL_PLAYER_COUNT];
static u32 characterNameTextValue[LOCAL_PLAYER_COUNT];
static bool characterNameTextOverridden[LOCAL_PLAYER_COUNT];

static const MenuDriverModel::State VEHICLE_SELECTED_STATE = static_cast<MenuDriverModel::State>(3);
static const u16 MENU_REINIT_MAX_WAITS = 600;

kmRuntimeUse(0x808b3a90);
static const u32 CHARACTER_NAMES_ADDRESS = kmRuntimeAddr(0x808b3a90);
kmRuntimeUse(0x809c1850);
static const u32 SCNMGR_INSTANCES_ADDRESS = kmRuntimeAddr(0x809c1850);

static u8 MinLocalPlayers(u32 count) {
    return count > LOCAL_PLAYER_COUNT ? LOCAL_PLAYER_COUNT : static_cast<u8>(count);
}

static bool IsCharacter(CharacterId character) {
    return character >= 0 && character < CHARACTER_COUNT;
}

static bool IsMiiCharacter(CharacterId character) {
    return (character >= MII_S_A_MALE && character <= MII_L_C_FEMALE) || character == MII_M || character == MII_S || character == MII_L;
}

static const char** CharacterNameEntry(CharacterId character) {
    if (!IsCharacter(character)) return nullptr;
    return reinterpret_cast<const char**>(CHARACTER_NAMES_ADDRESS) + character;
}

static void CacheDefaults() {
    if (cachedDefaultNames) return;
    for (u32 i = 0; i < CHARACTER_COUNT; ++i) {
        const char** entry = CharacterNameEntry(static_cast<CharacterId>(i));
        defaultNames[i] = nullptr;
        if (entry != nullptr) defaultNames[i] = *entry;
    }
    cachedDefaultNames = true;
}

const char* GetDefaultCharacterPostfix(CharacterId character) {
    if (!IsCharacter(character)) return nullptr;
    CacheDefaults();
    return defaultNames[character];
}

static CharacterId StateCharacter(CharacterId character) {
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
            return IsCharacter(character) ? character : CHARACTER_NONE;
    }
}

static CharacterId MenuBRRESCharacter(CharacterId character) {
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

static const CharacterOverride* GetCharacterOverride(CharacterId character, u8 table) {
    if (table == TABLE_DEFAULT || table >= TABLE_COUNT) return nullptr;
    for (u32 i = 0; i < ARRAY_COUNT(customCharacterAssets); ++i) {
        const CharacterOverride& characterOverride = customCharacterAssets[i];
        if (characterOverride.character == character && characterOverride.tableIdx == table) return &characterOverride;
    }
    return nullptr;
}

static bool HasSkin(CharacterId character, u8 table) {
    return table == TABLE_DEFAULT || GetCharacterOverride(character, table) != nullptr;
}

static u8 NormalizeTable(CharacterId character, u8 table) {
    return HasSkin(character, table) ? table : TABLE_DEFAULT;
}

static const char* SkinName(CharacterId character, u8 table) {
    const CharacterOverride* characterOverride = GetCharacterOverride(character, table);
    if (characterOverride != nullptr && characterOverride->postfix != nullptr) return characterOverride->postfix;
    return GetDefaultCharacterPostfix(character);
}

static const char* SkinAuthorText(CharacterId character, u8 table) {
    const CharacterOverride* characterOverride = GetCharacterOverride(character, table);
    return characterOverride != nullptr ? characterOverride->authorText : static_cast<const char*>(0);
}

static u32 SkinNameBmgId(CharacterId character, u8 table) {
    const CharacterOverride* characterOverride = GetCharacterOverride(character, table);
    return characterOverride != nullptr ? characterOverride->nameBmgId : 0;
}

static const char* DefaultMenuBRRESName(CharacterId character) {
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

static const char* DriverBRRESName(CharacterId character, u8 table) {
    const CharacterOverride* characterOverride = GetCharacterOverride(character, table);
    if (characterOverride != nullptr && characterOverride->postfix != nullptr) return characterOverride->postfix;
    const char* menuName = DefaultMenuBRRESName(character);
    if (menuName != nullptr) return menuName;
    return GetDefaultCharacterPostfix(character);
}

static void ApplyName(CharacterId character, u8 table) {
    const char** entry = CharacterNameEntry(character);
    const char* name = SkinName(character, table);
    if (entry != nullptr && name != nullptr) *entry = name;
}

static u8 SectionPlayerCount(const SectionMgr* mgr) {
    if (mgr == nullptr || mgr->sectionParams == nullptr) return 0;
    return MinLocalPlayers(mgr->sectionParams->localPlayerCount);
}

static u8 RacePlayerCount(const RacedataScenario& scenario) {
    return MinLocalPlayers(scenario.localPlayerCount);
}

static bool IsLocalMultiplayer() {
    const SectionMgr* sectionMgr = SectionMgr::sInstance;
    if (SectionPlayerCount(sectionMgr) > 1) return true;
    const Racedata* racedata = Racedata::sInstance;
    if (racedata != nullptr) {
        if (RacePlayerCount(racedata->racesScenario) > 1) return true;
        if (RacePlayerCount(racedata->menusScenario) > 1) return true;
    }
    return GetLocalPlayerCount() > 1;
}

static u8 SelectedTable(CharacterId character) {
    const CharacterId stateCharacter = StateCharacter(character);
    if (IsLocalMultiplayer() || !IsCharacter(stateCharacter)) return TABLE_DEFAULT;
    return NormalizeTable(character, selectedTable[stateCharacter]);
}

static void ApplySelectedNames() {
    CacheDefaults();
    for (u32 i = 0; i < CHARACTER_COUNT; ++i) {
        ApplyName(static_cast<CharacterId>(i), SelectedTable(static_cast<CharacterId>(i)));
    }
}

static bool AnyCustomSkin() {
    if (IsLocalMultiplayer()) return false;
    for (u32 i = 0; i < CHARACTER_COUNT; ++i) {
        if (selectedTable[i] != TABLE_DEFAULT) return true;
    }
    return false;
}

bool IsCustomCharacterTableActive() {
    return AnyCustomSkin();
}

void ResetOnlineCustomCharacterFlags() {
    for (u32 i = 0; i < ONLINE_PLAYER_COUNT; ++i) onlineCharacterTables[i] = TABLE_DEFAULT;
}

static bool IsOnlineRoom(const RKNet::Controller* controller) {
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

static bool DisplayOnlineSkins() {
    return Settings::Mgr::Get().GetUserSettingValue(Settings::SETTINGSTYPE_ONLINE, RADIO_DISPLAYCUSTOMSKINS) == DISPLAYCUSTOMSKINS_ENABLED;
}

static bool IsOnlineMultiLocal(const RKNet::Controller* controller) {
    return IsOnlineRoom(controller) && IsLocalMultiplayer();
}

static void ResetOfflineCpuSkinTables() {
    offlineCpuSkinTablesValid = false;
    offlineCpuSkinRaceNumber = 0;
    offlineCpuSkinSignature = 0;
    for (u8 i = 0; i < ONLINE_PLAYER_COUNT; ++i) offlineCpuCharacterTables[i] = TABLE_DEFAULT;
}

static bool IsOfflineCpuSkinResetSection(SectionId section) {
    switch (section) {
        case SECTION_GP_AWARD:
        case SECTION_VS_RACE_AWARD:
        case SECTION_AWARD_37:
        case SECTION_AWARD_38:
        case SECTION_MAIN_MENU_FROM_BOOT:
        case SECTION_MAIN_MENU_FROM_RESET:
        case SECTION_MAIN_MENU_FROM_MENU:
        case SECTION_MAIN_MENU_FROM_NEW_LICENSE:
        case SECTION_MAIN_MENU_FROM_LICENSE:
        case SECTION_SINGLE_P_FROM_MENU:
        case SECTION_LOCAL_MULTIPLAYER:
            return true;
        default:
            return false;
    }
}

static void ResetOfflineCpuSkinTablesForSection() {
    const SectionMgr* mgr = SectionMgr::sInstance;
    if (mgr == nullptr) return;
    if ((mgr->curSection != nullptr && IsOfflineCpuSkinResetSection(mgr->curSection->sectionId)) || IsOfflineCpuSkinResetSection(mgr->nextSectionId)) {
        ResetOfflineCpuSkinTables();
    }
}

static bool IsLocalRacePlayer(u8 playerId) {
    const Racedata* racedata = Racedata::sInstance;
    if (racedata == nullptr) return false;
    const RacedataScenario& scenario = racedata->racesScenario;
    const u8 localCount = RacePlayerCount(scenario);
    for (u8 hud = 0; hud < localCount; ++hud) {
        if (racedata->GetPlayerIdOfLocalPlayer(hud) == playerId) return true;
    }
    return false;
}

void RefreshLocalOnlineCustomCharacterFlags() {
    if (!IsOnlineRoom(RKNet::Controller::sInstance)) return;
    if (IsLocalMultiplayer()) {
        ResetOnlineCustomCharacterFlags();
        return;
    }
    if (!DisplayOnlineSkins()) return;
    const Racedata* racedata = Racedata::sInstance;
    if (racedata == nullptr) return;
    const RacedataScenario& scenario = racedata->racesScenario;
    const u8 localCount = RacePlayerCount(scenario);
    for (u8 hud = 0; hud < localCount; ++hud) {
        const u32 playerId = racedata->GetPlayerIdOfLocalPlayer(hud);
        if (playerId < ONLINE_PLAYER_COUNT) onlineCharacterTables[playerId] = SelectedTable(scenario.players[playerId].characterId);
    }
}

static bool SetSelectedTable(CharacterId character, u8 table) {
    const CharacterId stateCharacter = StateCharacter(character);
    if (!IsCharacter(stateCharacter) || !HasSkin(character, table)) return false;
    if (selectedTable[stateCharacter] == table) return false;
    selectedTable[stateCharacter] = table;
    ApplySelectedNames();
    RefreshLocalOnlineCustomCharacterFlags();
    return true;
}

static void QueueMenuDriverReinit(u8 frames) {
    if (pendingReinitFrames == 0 || pendingReinitFrames > frames) pendingReinitFrames = frames;
    pendingReinitWaits = 0;
}

bool RandomizeSelectedCharacterTable(CharacterId character) {
    if (IsLocalMultiplayer() || !IsCharacter(StateCharacter(character))) return false;
    u8 valid[TABLE_COUNT];
    u8 count = 0;
    for (u8 table = 0; table < TABLE_COUNT; ++table) {
        if (HasSkin(character, table)) valid[count++] = table;
    }
    if (count == 0) return false;
    Random random;
    const bool changed = SetSelectedTable(character, valid[random.NextLimited<u8>(count)]);
    if (changed) QueueMenuDriverReinit(2);
    return changed;
}

static bool CycleSkin(CharacterId character, int step) {
    if (GetLocalPlayerCount() != 1 || !IsCharacter(StateCharacter(character))) return false;
    u8 table = SelectedTable(character);
    for (u8 i = 1; i < TABLE_COUNT; ++i) {
        table = step < 0 ? (table == 0 ? TABLE_COUNT - 1 : table - 1) : (table + 1 >= TABLE_COUNT ? TABLE_DEFAULT : table + 1);
        if (HasSkin(character, table) && SetSelectedTable(character, table)) {
            return true;
        }
    }
    return false;
}

static u8 RaceSkinTable(u8 playerId, CharacterId character) {
    const Racedata* racedata = Racedata::sInstance;
    if (racedata != nullptr && playerId < racedata->racesScenario.playerCount) {
        const RacedataScenario& scenario = racedata->racesScenario;
        const GameMode mode = scenario.settings.gamemode;
        const bool offlineCpuSkinMode = mode == MODE_GRAND_PRIX || mode == MODE_VS_RACE || mode == MODE_BATTLE;
        const bool offlineCpu = scenario.players[playerId].playerType == PLAYER_CPU;
        if (offlineCpuSkinMode && offlineCpu && !IsOnlineRoom(RKNet::Controller::sInstance) && DisplayOnlineSkins() && !IsLocalMultiplayer()) {
            u32 signature = 0x4343534b;
            signature = signature * 33 + static_cast<u32>(scenario.settings.gamemode);
            signature = signature * 33 + static_cast<u32>(scenario.settings.modeFlags);
            signature = signature * 33 + scenario.playerCount;
            for (u8 i = 0; i < scenario.playerCount && i < ONLINE_PLAYER_COUNT; ++i) {
                signature = signature * 33 + static_cast<u32>(scenario.players[i].characterId);
                signature = signature * 33 + static_cast<u32>(scenario.players[i].playerType);
            }

            const bool sameSeries = offlineCpuSkinTablesValid && offlineCpuSkinSignature == signature;
            const bool newSeriesStart = sameSeries && scenario.settings.raceNumber == 0 && offlineCpuSkinRaceNumber != 0;
            if (!sameSeries || newSeriesStart) {
                offlineCpuSkinTablesValid = true;
                offlineCpuSkinSignature = signature;
                for (u8 i = 0; i < ONLINE_PLAYER_COUNT; ++i) offlineCpuCharacterTables[i] = TABLE_DEFAULT;
                for (u8 i = 0; i < scenario.playerCount && i < ONLINE_PLAYER_COUNT; ++i) {
                    if (scenario.players[i].playerType != PLAYER_CPU) continue;
                    const CharacterId cpuCharacter = scenario.players[i].characterId;
                    u8 valid[TABLE_COUNT];
                    u8 count = 0;
                    for (u8 table = 0; table < TABLE_COUNT; ++table) {
                        if (HasSkin(cpuCharacter, table)) valid[count++] = table;
                    }
                    if (count == 0) continue;
                    Random random(static_cast<s32>(signature ^ (static_cast<u32>(i) * 0x1f123bb5)));
                    offlineCpuCharacterTables[i] = valid[random.NextLimited<u8>(count)];
                }
            }
            offlineCpuSkinRaceNumber = scenario.settings.raceNumber;
            return NormalizeTable(character, offlineCpuCharacterTables[playerId]);
        }
    }

    if (IsLocalMultiplayer()) return TABLE_DEFAULT;
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    if (IsOnlineRoom(controller) && !IsOnlineMultiLocal(controller) && DisplayOnlineSkins()) {
        return IsLocalRacePlayer(playerId) ? SelectedTable(character) : NormalizeTable(character, onlineCharacterTables[playerId]);
    }
    return IsLocalRacePlayer(playerId) ? SelectedTable(character) : TABLE_DEFAULT;
}

bool ShouldMuteCharacterVoice(const Kart::Link* link) {
    const Racedata* racedata = Racedata::sInstance;
    if (link == nullptr || racedata == nullptr) return false;
    const u8 playerId = link->GetPlayerIdx();
    if (playerId >= racedata->racesScenario.playerCount) return false;
    const CharacterId character = racedata->racesScenario.players[playerId].characterId;
    const u8 table = RaceSkinTable(playerId, character);
    const CharacterOverride* characterOverride = GetCharacterOverride(character, table);
    return characterOverride != nullptr && characterOverride->silentVoice;
}

kmRuntimeUse(0x807d9b98);
static TicoModel* CreateTicoModelHook(void* memory, DriverController* controller) {
    if (controller != nullptr && ShouldMuteCharacterVoice(controller)) {
        if (memory != nullptr) ::operator delete(memory);
        return nullptr;
    }
    typedef TicoModel* (*Ctor)(void*, DriverController*);
    return reinterpret_cast<Ctor>(kmRuntimeAddr(0x807d9b98))(memory, controller);
}
kmCall(0x807c8994, CreateTicoModelHook);

class ScopedNameSwap {
   public:
    ScopedNameSwap(u8 playerId, CharacterId character) : entry(CharacterNameEntry(character)), oldName(nullptr) {
        if (entry == nullptr) return;
        oldName = *entry;
        const char* name = SkinName(character, RaceSkinTable(playerId, character));
        if (name == nullptr) name = GetDefaultCharacterPostfix(character);
        *entry = name;
    }
    ~ScopedNameSwap() {
        if (entry != nullptr) *entry = oldName;
    }

   private:
    const char** entry;
    const char* oldName;
};

static CharacterId PreviewCharacter(u8 hud) {
    if (hud >= LOCAL_PLAYER_COUNT) return MARIO;
    const SectionMgr* mgr = SectionMgr::sInstance;
    CharacterId character = hoveredCharacters[hud];
    if (mgr != nullptr && mgr->sectionParams != nullptr && !IsCharacter(character)) character = mgr->sectionParams->characters[hud];
    return character;
}

static CharacterId SelectedCharacterForHud(u8 hud) {
    if (hud >= LOCAL_PLAYER_COUNT) return MARIO;
    const SectionMgr* mgr = SectionMgr::sInstance;
    if (mgr != nullptr && mgr->sectionParams != nullptr && hud < mgr->sectionParams->localPlayerCount) {
        return mgr->sectionParams->characters[hud];
    }
    const Racedata* racedata = Racedata::sInstance;
    if (racedata != nullptr && hud < racedata->menusScenario.localPlayerCount) {
        const u8 playerId = racedata->menusScenario.settings.hudPlayerIds[hud];
        if (playerId < ONLINE_PLAYER_COUNT) return racedata->menusScenario.players[playerId].characterId;
    }
    return hoveredCharacters[hud];
}

void UpdateOnlineCharacterTablesFromAid(u8 aid, const u8* playerIdToAid, u8 characterTables) {
    if (playerIdToAid == nullptr) return;
    if (IsLocalMultiplayer()) {
        ResetOnlineCustomCharacterFlags();
        return;
    }
    u8 hud = 0;
    for (u8 playerId = 0; playerId < ONLINE_PLAYER_COUNT; ++playerId) {
        if (playerIdToAid[playerId] != aid) continue;
        const u8 table = hud < 2 ? static_cast<u8>((characterTables >> (hud * PACKET_BITS)) & PACKET_MASK) : TABLE_DEFAULT;
        onlineCharacterTables[playerId] = table < TABLE_COUNT ? table : TABLE_DEFAULT;
        ++hud;
    }
}

u8 GetLocalOnlineCharacterTables() {
    if (IsLocalMultiplayer()) return 0;
    u8 localCount = GetLocalPlayerCount();
    const SectionMgr* mgr = SectionMgr::sInstance;
    if (mgr != nullptr && mgr->sectionParams != nullptr) {
        localCount = static_cast<u8>(mgr->sectionParams->localPlayerCount);
    } else if (Racedata::sInstance != nullptr) {
        localCount = Racedata::sInstance->menusScenario.localPlayerCount;
    }
    if (localCount > 2) localCount = 2;

    u8 packed = 0;
    for (u8 hud = 0; hud < localCount; ++hud) {
        packed |= static_cast<u8>((SelectedTable(SelectedCharacterForHud(hud)) & PACKET_MASK) << (hud * PACKET_BITS));
    }
    return packed;
}

bool ShouldUseCustomCharacterForPlayer(u8 playerId) {
    return !IsLocalMultiplayer() && playerId < ONLINE_PLAYER_COUNT && onlineCharacterTables[playerId] != TABLE_DEFAULT;
}

static bool IsCharacterSelectActive() {
    const SectionMgr* mgr = SectionMgr::sInstance;
    if (mgr == nullptr || mgr->curSection == nullptr) return false;
    const Pages::CharacterSelect* page = mgr->curSection->Get<Pages::CharacterSelect>();
    return page != nullptr && mgr->curSection->GetTopLayerPage() == page && page->currentState == STATE_ACTIVE && !page->updateState;
}

static bool MenuPreviewStable() {
    const SectionMgr* mgr = SectionMgr::sInstance;
    if (mgr == nullptr || mgr->curSection == nullptr) return false;
    const Page* top = mgr->curSection->GetTopLayerPage();
    if (top == nullptr || top->currentState != STATE_ACTIVE || top->updateState) return false;
    const Pages::CharacterSelect* chars = mgr->curSection->Get<Pages::CharacterSelect>();
    const UI::ExpCharacterSelect* expChars = mgr->curSection->Get<UI::ExpCharacterSelect>();
    return top == chars && (expChars == nullptr || expChars->rouletteCounter == 0);
}

static void RefreshCharacterSelectModels() {
    SectionMgr* mgr = SectionMgr::sInstance;
    if (mgr == nullptr || mgr->curSection == nullptr || mgr->sectionParams == nullptr) return;
    Pages::CharacterSelect* page = mgr->curSection->Get<Pages::CharacterSelect>();
    if (page == nullptr || page->models == nullptr) return;
    const u8 count = SectionPlayerCount(mgr);
    for (u8 hud = 0; hud < count; ++hud) page->models[hud].RequestModel(PreviewCharacter(hud));
}

static void CacheHoveredFromSection() {
    const SectionMgr* mgr = SectionMgr::sInstance;
    if (mgr == nullptr || mgr->sectionParams == nullptr) return;
    const u8 count = SectionPlayerCount(mgr);
    for (u8 hud = 0; hud < count; ++hud) hoveredCharacters[hud] = mgr->sectionParams->characters[hud];
}

static const char* PreviewAuthorText(u8 hud) {
    if (hud >= LOCAL_PLAYER_COUNT || IsLocalMultiplayer()) return static_cast<const char*>(0);
    const CharacterId character = PreviewCharacter(hud);
    return SkinAuthorText(character, SelectedTable(character));
}

static u32 PreviewNameBmgId(u8 hud) {
    if (hud >= LOCAL_PLAYER_COUNT || IsLocalMultiplayer()) return 0;
    const CharacterId character = PreviewCharacter(hud);
    return SkinNameBmgId(character, SelectedTable(character));
}

static void CopyAsciiToWide(wchar_t* dest, const char* src, u32 maxLen) {
    if (dest == nullptr || maxLen == 0) return;
    u32 i = 0;
    if (src != nullptr) {
        for (; i + 1 < maxLen && src[i] != '\0'; ++i) dest[i] = static_cast<unsigned char>(src[i]);
    }
    dest[i] = 0;
}

static bool IsOnlineRaceMode(GameMode mode) {
    return mode >= MODE_PRIVATE_VS && mode <= MODE_PRIVATE_BATTLE;
}

static u32 RaceNameBmgId(u8 playerId) {
    const Racedata* racedata = Racedata::sInstance;
    if (racedata == nullptr || playerId >= racedata->racesScenario.playerCount || playerId >= ONLINE_PLAYER_COUNT) return 0;
    const CharacterId character = racedata->racesScenario.players[playerId].characterId;
    if (IsMiiCharacter(character)) return 0;
    return SkinNameBmgId(character, RaceSkinTable(playerId, character));
}

bool SetRaceNameTextIfCustom(LayoutUIControl& control, const char* paneName, u8 playerId) {
    const u32 bmgId = RaceNameBmgId(playerId);
    if (bmgId == 0) return false;
    control.SetTextBoxMessage(paneName, bmgId, nullptr);
    return true;
}

static void SetRaceCharacterNameHook(LayoutUIControl* control, const char* paneName, u32 bmgId, const Text::Info* info) {
    if (control == nullptr) return;
    static const u32 PLAYER_ID_OFFSET = 0x178;
    const u32 playerId = *reinterpret_cast<const u32*>(reinterpret_cast<const u8*>(control) + PLAYER_ID_OFFSET);
    if (playerId >= ONLINE_PLAYER_COUNT) {
        control->SetTextBoxMessage(paneName, bmgId, info);
        return;
    }
    if (!SetRaceNameTextIfCustom(*control, paneName, static_cast<u8>(playerId))) {
        control->SetTextBoxMessage(paneName, bmgId, info);
    }
}
kmCall(0x807f0580, SetRaceCharacterNameHook);
kmCall(0x807f06b0, SetRaceCharacterNameHook);

static bool RaceResultUsesMiiName(const RacedataScenario& scenario, u8 playerId) {
    const RacedataPlayer& player = scenario.players[playerId];
    if (IsMiiCharacter(player.characterId)) return true;
    return (IsOnlineRaceMode(scenario.settings.gamemode) || scenario.localPlayerCount > 1) && player.playerType != PLAYER_CPU;
}

static void FillRaceResultNameHook(CtrlRaceResult* result, u8 playerId) {
    const Racedata* racedata = Racedata::sInstance;
    SectionMgr* sectionMgr = SectionMgr::sInstance;
    if (result == nullptr || racedata == nullptr || sectionMgr == nullptr || sectionMgr->sectionParams == nullptr || playerId >= racedata->racesScenario.playerCount) {
        return;
    }
    const RacedataScenario& scenario = racedata->racesScenario;
    const RacedataPlayer& player = scenario.players[playerId];
    if (RaceResultUsesMiiName(scenario, playerId)) {
        Text::Info info;
        info.miis[0] = sectionMgr->sectionParams->playerMiis.GetMii(playerId);
        result->SetTextBoxMessage("player_name", UI::BMG_MII_NAME, &info);
    } else if (!SetRaceNameTextIfCustom(*result, "player_name", playerId)) {
        result->SetTextBoxMessage("player_name", GetCharacterBMGId(player.characterId, true), nullptr);
    }
    result->ResetTextBoxMessage("time");
}
kmBranch(0x807f52f4, FillRaceResultNameHook);

static void ResetCharacterSelectNameTextCache() {
    for (u8 hud = 0; hud < LOCAL_PLAYER_COUNT; ++hud) {
        characterNameTextControl[hud] = nullptr;
        characterNameTextValue[hud] = 0;
        characterNameTextOverridden[hud] = false;
    }
}

static void RestoreCharacterSelectNameText(CharaName& name, CharacterId character) {
    if (IsMiiCharacter(character)) return;
    CharacterId displayCharacter = StateCharacter(character);
    if (!IsCharacter(displayCharacter)) displayCharacter = character;
    if (!IsCharacter(displayCharacter) || IsMiiCharacter(displayCharacter)) return;
    name.SetMessage(GetCharacterBMGId(displayCharacter, false), nullptr);
}

static void UpdateCharacterSelectNameText(Pages::CharacterSelect* page, u8 hud) {
    if (page == nullptr || page->names == nullptr || hud >= LOCAL_PLAYER_COUNT) return;
    CharaName& name = page->names[hud];
    const u32 bmgId = PreviewNameBmgId(hud);
    if (characterNameTextControl[hud] == &name && characterNameTextValue[hud] == bmgId) return;
    if (bmgId != 0) {
        name.SetMessage(bmgId, nullptr);
        characterNameTextOverridden[hud] = true;
    } else if (characterNameTextOverridden[hud]) {
        RestoreCharacterSelectNameText(name, PreviewCharacter(hud));
        characterNameTextOverridden[hud] = false;
    }
    characterNameTextControl[hud] = &name;
    characterNameTextValue[hud] = bmgId;
}

static CharaName* GetAuthorNameControl(u8 hud) {
    if (hud >= LOCAL_PLAYER_COUNT || !authorNameControlLoaded[hud]) return nullptr;
    return reinterpret_cast<CharaName*>(&authorNameControlStorage[hud][0]);
}

static void UpdateCharacterSelectAuthorText(Pages::CharacterSelect* page, u8 hud) {
    if (page == nullptr || page->names == nullptr) return;
    CharaName* authorControl = GetAuthorNameControl(hud);
    if (authorControl == nullptr) return;
    const char* text = PreviewAuthorText(hud);
    if (authorTextControl == authorControl && authorTextValue == text) return;
    if (text == static_cast<const char*>(0)) {
        authorControl->isHidden = true;
    } else {
        CopyAsciiToWide(authorTextBuffer, text, ARRAY_COUNT(authorTextBuffer));
        Text::Info info;
        info.strings[0] = authorTextBuffer;
        authorControl->isHidden = false;
        authorControl->SetMessage(UI::BMG_TEXT, &info);
    }
    authorTextControl = authorControl;
    authorTextValue = text;
}

static void UpdateCurrentCharacterSelectAuthorText(u8 hud) {
    if (!IsCharacterSelectActive()) {
        authorTextControl = nullptr;
        authorTextValue = static_cast<const char*>(0);
        ResetCharacterSelectNameTextCache();
        return;
    }
    SectionMgr* mgr = SectionMgr::sInstance;
    if (mgr == nullptr || mgr->curSection == nullptr) return;
    UpdateCharacterSelectNameText(mgr->curSection->Get<Pages::CharacterSelect>(), hud);
    UpdateCharacterSelectAuthorText(mgr->curSection->Get<Pages::CharacterSelect>(), hud);
}

static void SetPaneVisibleIfPresent(LayoutUIControl& control, const char* paneName, bool visible) {
    if (control.layout.GetPaneByName(paneName) != nullptr) control.SetPaneVisibility(paneName, visible);
}

static void HideAuthorNameDecoration(LayoutUIControl& control) {
    const char* panes[] = {"Window_00",
                           "black_parts_t_00",
                           "black_parts_t_01",
                           "select_base",
                           "border",
                           "cc_prev_wh",
                           "cc_next_wh",
                           "cc_prev_nc",
                           "cc_next_nc",
                           "cc_prev_cls",
                           "cc_next_cls",
                           "cc_prev_gc",
                           "cc_next_gc"};
    for (u32 i = 0; i < ARRAY_COUNT(panes); ++i) SetPaneVisibleIfPresent(control, panes[i], false);
}

static void PositionAuthorNameControl(LayoutUIControl& control) {
    for (u32 i = 0; i < ARRAY_COUNT(control.positionAndscale); ++i) {
        control.positionAndscale[i].position.x += 152.5f;
        control.positionAndscale[i].position.y += 40.0f;
        control.positionAndscale[i].scale.x *= 1.1f;
    }
}

kmRuntimeUse(0x8083d840);
static CharaName* ConstructCharaName(CharaName* name) {
    typedef CharaName* (*Fn)(CharaName*);
    return reinterpret_cast<Fn>(kmRuntimeAddr(0x8083d840))(name);
}

static void AttachAuthorNameControl(CharaName& name, const char* folderName, const char* ctrName, const char* variant) {
    const u32 hud = name.unknown_0x178;
    if (hud >= LOCAL_PLAYER_COUNT) return;
    characterNameTextControl[hud] = nullptr;
    characterNameTextValue[hud] = 0;
    characterNameTextOverridden[hud] = false;

    CharaName* author = reinterpret_cast<CharaName*>(&authorNameControlStorage[hud][0]);
    if (authorNameControlConstructed[hud]) {
        authorNameControlLoaded[hud] = false;
        if (authorTextControl == author) {
            authorTextControl = nullptr;
            authorTextValue = static_cast<const char*>(0);
        }
    }
    ConstructCharaName(author);
    authorNameControlConstructed[hud] = true;
    author->unknown_0x178 = hud;
    name.InitControlGroup(1);
    name.AddControl(0, author);
    loadingAuthorNameControl = true;
    ControlLoader loader(author);
    loader.Load(folderName, ctrName, variant, nullptr);
    loadingAuthorNameControl = false;

    HideAuthorNameDecoration(*author);
    PositionAuthorNameControl(*author);
    author->isHidden = true;
    authorNameControlLoaded[hud] = true;
}

kmRuntimeUse(0x805c2c60);
static void CharacterSelectNameLoadHook(ControlLoader* loader, const char* folderName, const char* ctrName, const char* variant, const char** animNames) {
    typedef void (*Fn)(ControlLoader*, const char*, const char*, const char*, const char**);
    reinterpret_cast<Fn>(kmRuntimeAddr(0x805c2c60))(loader, folderName, ctrName, variant, animNames);
    if (loadingAuthorNameControl || loader == nullptr || loader->layoutUIControl == nullptr) return;
    AttachAuthorNameControl(*static_cast<CharaName*>(loader->layoutUIControl), folderName, ctrName, variant);
}
kmCall(0x8083d9dc, CharacterSelectNameLoadHook);

static void UnlockHeap(EGG::Heap* heap) {
    if (heap != nullptr) heap->dameFlag &= ~0x1;
}

static bool IsInHeap(const EGG::ExpHeap* heap, const void* ptr) {
    if (heap == nullptr || ptr == nullptr || heap->rvlHeap == nullptr) return false;
    const u8* start = reinterpret_cast<const u8*>(heap->rvlHeap->startAddr);
    const u8* end = reinterpret_cast<const u8*>(heap->rvlHeap->endAddr);
    const u8* address = reinterpret_cast<const u8*>(ptr);
    return start != nullptr && end != nullptr && start < end && address >= start && address < end;
}

static void DetachHeapListNodes(nw4r::ut::List* list, const EGG::ExpHeap* heap) {
    for (void* node = nw4r::ut::List_GetNext(list, nullptr); node != nullptr;) {
        void* next = nw4r::ut::List_GetNext(list, node);
        if (IsInHeap(heap, node)) nw4r::ut::List_Remove(list, node);
        node = next;
    }
}

static void DetachHeapFromScnMgrs(const EGG::ExpHeap* heap) {
    if (heap == nullptr) return;
    ScnMgr* const* mgrs = reinterpret_cast<ScnMgr* const*>(SCNMGR_INSTANCES_ADDRESS);
    for (u32 i = 0; i < 2; ++i) {
        ScnMgr* mgr = mgrs[i];
        if (mgr == nullptr) continue;
        DetachHeapListNodes(&mgr->modelDirectors, heap);
        DetachHeapListNodes(&mgr->screenSpecificModelDirectors, heap);
        DetachHeapListNodes(&mgr->scnGroupExHolderList, heap);
        DetachHeapListNodes(&mgr->hardcodedMatNamesModelDirectors, heap);
    }
}

static void DestroyHeap(EGG::ExpHeap*& heap) {
    if (heap == nullptr) return;
    DetachHeapFromScnMgrs(heap);
    UnlockHeap(heap);
    heap->destroy();
    heap = nullptr;
}

static void ClearRawCache(RawBRRES& cache, bool destroyHeap) {
    if (destroyHeap)
        DestroyHeap(cache.heap);
    else
        cache.heap = nullptr;
    cache.file = nullptr;
    cache.failed = false;
}

static void ClearRawCaches(bool destroyHeap) {
    for (u32 table = 0; table < TABLE_COUNT; ++table) {
        for (u32 character = 0; character < CHARACTER_COUNT; ++character) ClearRawCache(rawBRRES[table][character], destroyHeap);
    }
    for (u32 i = 0; i < MII_C_COUNT; ++i) ClearRawCache(looseMiiCBRRES[i], destroyHeap);
}

static bool DriverMgrUsable(const MenuDriverModelMgr* mgr, u8 playerCount) {
    return mgr != nullptr && mgr->models != nullptr && mgr->playerCount != 0 && mgr->playerCount <= LOCAL_PLAYER_COUNT &&
           (playerCount == 0 || mgr->playerCount == playerCount) &&
           mgr->modelCount == 0x18 + mgr->playerCount * MENU_MII_SLOTS_PER_PLAYER;
}

static void ClearDriverModels(MenuDriverModelMgr& mgr) {
    if (mgr.bangs != nullptr) mgr.bangs->ToggleVisible(false);
    MiiHeadsModel* const* miiHeads = reinterpret_cast<MiiHeadsModel* const*>(mgr.miiHeads);
    for (u8 i = 0; i < mgr.playerCount; ++i) {
        mgr.players[i].isVisible = false;
        if (mgr.players[i].playerModel != nullptr && mgr.players[i].playerModel->model != nullptr) {
            mgr.players[i].playerModel->ToggleVisible(false);
        }
        if (miiHeads != nullptr && miiHeads[i] != nullptr && miiHeads[i]->curScnMdlEx != nullptr) {
            miiHeads[i]->ToggleVisible(false);
        }
    }
}

static void DetachMenuMgrHeap(MenuModelMgr* menuMgr, EGG::ExpHeap* heap) {
    if (menuMgr == nullptr || heap == nullptr || !IsInHeap(heap, menuMgr->driverModels)) return;
    ClearDriverModels(*menuMgr->driverModels);
    menuMgr->driverModels = nullptr;
}

static void ResetVotingVR(bool keepSection) {
    const Section* section = nullptr;
    if (keepSection) section = votingVR.section;
    votingVR.section = section;
    votingVR.sawVR = false;
    votingVR.exitedVR = false;
    votingVR.reinitialized = false;
    votingVR.modelRendererPrimed = false;
    votingVR.reinitFrames = 0;
    votingVR.refreshFrames = 0;
    if (!keepSection) applyVotingVRReinit = false;
}

static void ResetDriverCache() {
    pendingReinitFrames = 0;
    pendingReinitWaits = 0;
    applyMenuDriverReinit = false;
    driverCache.mgr = nullptr;
    driverCache.playerCount = 0;
    driverCache.currentTable = AnyCustomSkin() ? TABLE_INVALID : TABLE_DEFAULT;
    driverCache.currentCharacter = CHARACTER_NONE;
    driverCache.currentHud0Character = CHARACTER_NONE;
    driverCache.buildingTable = TABLE_INVALID;
    driverCache.buildingCharacter = CHARACTER_NONE;
    driverCache.activeHeap = nullptr;
    driverCache.createdHeap = nullptr;
    driverCache.buildingHeap = nullptr;
    ResetVotingVR(false);
    ClearRawCaches(false);
    CacheHoveredFromSection();
}

static void ResetDriverHeaps() {
    if (driverCache.resetting) return;
    driverCache.resetting = true;
    MenuModelMgr* menuMgr = MenuModelMgr::sInstance;
    DetachMenuMgrHeap(menuMgr, driverCache.activeHeap);
    DetachMenuMgrHeap(menuMgr, driverCache.createdHeap);
    ClearRawCaches(true);
    DestroyHeap(driverCache.activeHeap);
    DestroyHeap(driverCache.createdHeap);
    driverCache.buildingHeap = nullptr;
    ResetDriverCache();
    driverCache.resetting = false;
}

kmRuntimeUse(0x8059e04c);
static void DestroyMenuModelMgrInstanceHook() {
    ResetDriverHeaps();
    typedef void (*Fn)();
    reinterpret_cast<Fn>(kmRuntimeAddr(0x8059e04c))();
}
kmCall(0x80553ad4, DestroyMenuModelMgrInstanceHook);
kmCall(0x805552b0, DestroyMenuModelMgrInstanceHook);

static void SyncDriverCache() {
    MenuModelMgr* menuMgr = MenuModelMgr::sInstance;
    if (menuMgr == nullptr) {
        ResetDriverCache();
        return;
    }
    if (driverCache.mgr != menuMgr || driverCache.playerCount != menuMgr->playerCount) {
        driverCache.mgr = menuMgr;
        driverCache.playerCount = menuMgr->playerCount;
        driverCache.currentTable = AnyCustomSkin() ? TABLE_INVALID : TABLE_DEFAULT;
        driverCache.currentCharacter = CHARACTER_NONE;
        driverCache.currentHud0Character = CHARACTER_NONE;
        ClearRawCaches(false);
    }
}

static void UnlockMenuHeaps(GameScene& scene, MenuModelMgr& menuMgr) {
    UnlockHeap(scene.parentHeap);
    UnlockHeap(scene.otherMEMHeap);
    UnlockHeap(scene.mainMEMHeap);
    UnlockHeap(scene.debugHeap);
    for (u32 i = 0; i < 3; ++i) {
        UnlockHeap(scene.structsHeaps.heaps[i]);
        UnlockHeap(scene.archiveHeaps.heaps[i]);
    }
    UnlockHeap(menuMgr.heap);
    UnlockHeap(menuMgr.otherHeap);
}

static EGG::ExpHeap* BestParentHeap(GameScene& scene) {
    EGG::ExpHeap* heaps[] = {
        scene.structsHeaps.heaps[1],
        scene.mainMEMHeap,
        scene.expHeapGroup.heaps[1],
        scene.structsHeaps.heaps[0],
        scene.otherMEMHeap,
        scene.expHeapGroup.heaps[0],
    };
    EGG::ExpHeap* best = nullptr;
    u32 bestSize = 0;
    for (u32 i = 0; i < ARRAY_COUNT(heaps); ++i) {
        if (heaps[i] == nullptr) continue;
        UnlockHeap(heaps[i]);
        const u32 size = heaps[i]->getAllocatableSize(0x20);
        if (size > bestSize) {
            best = heaps[i];
            bestSize = size;
        }
    }
    return best;
}

kmRuntimeUse(0x80830180);
static MenuDriverModelMgr* CreateDriverMgr(u8 playerCount) {
    typedef MenuDriverModelMgr* (*Ctor)(MenuDriverModelMgr*, u8);
    GameScene* scene = const_cast<GameScene*>(GameScene::GetCurrent());
    EGG::ExpHeap* heap = nullptr;
    if (scene != nullptr) heap = BestParentHeap(*scene);
    EGG::ExpHeap* oldStruct0 = nullptr;
    EGG::ExpHeap* oldStruct1 = nullptr;
    EGG::ExpHeap* oldScene0 = nullptr;
    EGG::ExpHeap* oldScene1 = nullptr;
    EGG::Heap* oldCurrent = nullptr;

    driverCache.createdHeap = nullptr;
    if (heap != nullptr) {
        const u32 size = heap->getAllocatableSize(0x20);
        const u32 subHeapSize = size > 0x2000 ? size - 0x2000 : size;
        if (subHeapSize >= 0x80000) {
            driverCache.createdHeap = EGG::ExpHeap::Create(static_cast<int>(subHeapSize), heap, 0);
            if (driverCache.createdHeap != nullptr) heap = driverCache.createdHeap;
        }
    }

    if (scene != nullptr && heap != nullptr) {
        oldCurrent = heap->BecomeCurrentHeap();
        oldStruct0 = scene->structsHeaps.heaps[0];
        oldStruct1 = scene->structsHeaps.heaps[1];
        oldScene0 = scene->expHeapGroup.heaps[0];
        oldScene1 = scene->expHeapGroup.heaps[1];
        scene->structsHeaps.heaps[0] = heap;
        scene->structsHeaps.heaps[1] = heap;
        scene->expHeapGroup.heaps[0] = heap;
        scene->expHeapGroup.heaps[1] = heap;
    }

    driverCache.buildingHeap = driverCache.createdHeap;
    void* memory = nullptr;
    if (heap != nullptr)
        memory = operator new(sizeof(MenuDriverModelMgr), heap);
    else
        memory = operator new(sizeof(MenuDriverModelMgr));
    MenuDriverModelMgr* mgr = nullptr;
    if (memory != nullptr) {
        mgr = reinterpret_cast<Ctor>(kmRuntimeAddr(0x80830180))(reinterpret_cast<MenuDriverModelMgr*>(memory), playerCount);
    }

    if (scene != nullptr && heap != nullptr) {
        scene->structsHeaps.heaps[0] = oldStruct0;
        scene->structsHeaps.heaps[1] = oldStruct1;
        scene->expHeapGroup.heaps[0] = oldScene0;
        scene->expHeapGroup.heaps[1] = oldScene1;
        if (oldCurrent != nullptr) oldCurrent->BecomeCurrentHeap();
    }
    if (mgr == nullptr) DestroyHeap(driverCache.createdHeap);
    driverCache.buildingHeap = nullptr;
    return mgr;
}

static bool IsVotingSection() {
    const SectionMgr* mgr = SectionMgr::sInstance;
    if (mgr == nullptr || mgr->curSection == nullptr) return false;
    switch (mgr->curSection->sectionId) {
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

static bool PageIsActive(PageId pageId) {
    const SectionMgr* mgr = SectionMgr::sInstance;
    if (mgr == nullptr || mgr->curSection == nullptr) return false;
    const Section* section = mgr->curSection;
    Page* page = section->pages[pageId];
    if (page == nullptr) return false;
    for (u32 i = 0; i < section->layerCount; ++i) {
        if (section->activePages[i] == page) return true;
    }
    return false;
}

static bool SyncVotingVR() {
    const SectionMgr* mgr = SectionMgr::sInstance;
    if (mgr == nullptr || mgr->curSection == nullptr || !IsVotingSection()) {
        ResetVotingVR(false);
        return false;
    }
    if (votingVR.section != mgr->curSection) {
        votingVR.section = mgr->curSection;
        ResetVotingVR(true);
    }
    return true;
}

static bool UseDefaultTableForVotingVR() {
    if (!SyncVotingVR()) return false;
    if (PageIsActive(PAGE_VR)) votingVR.sawVR = true;
    return !votingVR.exitedVR;
}

static u8 ResolveMenuTable(CharacterId character) {
    return UseDefaultTableForVotingVR() ? TABLE_DEFAULT : SelectedTable(character);
}

static CharacterId MenuCacheCharacter(CharacterId character, u8 table) {
    return table != TABLE_DEFAULT && table < TABLE_COUNT ? MenuBRRESCharacter(character) : CHARACTER_NONE;
}

static u32 AlignUp(u32 value, u32 alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

static bool BuildDriverPath(CharacterId character, u8 table, char* path, u32 pathSize) {
    const char* name = DriverBRRESName(character, table);
    if (name == nullptr) return false;
    const int written = snprintf(path, pathSize, "/Scene/Model/Driver/%s.brres", name);
    return written > 0 && static_cast<u32>(written) < pathSize;
}

static bool DiscFileSize(const char* path, u32& size) {
    DVD::FileInfo info;
    if (!DVD::Open(path, &info)) {
        size = 0;
        return false;
    }
    size = info.length;
    DVD::Close(&info);
    return size != 0;
}

static EGG::Heap* RawParentHeap(GameScene& scene, u32 heapSize) {
    static const u32 PARENT_RESERVE = 0x200000;
    static const u32 CUSTOM_RESERVE = 0x1000;
    EGG::ExpHeap* customHeap = driverCache.activeHeap;
    if (driverCache.buildingHeap != nullptr) customHeap = driverCache.buildingHeap;
    if (customHeap != nullptr) {
        UnlockHeap(customHeap);
        if (customHeap->getAllocatableSize(0x20) >= heapSize + CUSTOM_RESERVE) return customHeap;
    }

    EGG::Heap* heaps[] = {scene.structsHeaps.heaps[1], scene.structsHeaps.heaps[0]};
    EGG::Heap* best = nullptr;
    u32 bestSize = 0;
    for (u32 i = 0; i < ARRAY_COUNT(heaps); ++i) {
        if (heaps[i] == nullptr) continue;
        UnlockHeap(heaps[i]);
        const u32 size = heaps[i]->getAllocatableSize(0x20);
        if (size >= heapSize + PARENT_RESERVE) return heaps[i];
        if (size > bestSize) {
            best = heaps[i];
            bestSize = size;
        }
    }
    if (bestSize >= heapSize) return best;
    return nullptr;
}

kmRuntimeUse(0x8055b7f8);
static void BindRawBRRES(nw4r::g3d::ResFile& resFile, const char* path) {
    typedef void (*Fn)(nw4r::g3d::ResFile*, const char*, const nw4r::g3d::ResFile*, u32);
    reinterpret_cast<Fn>(kmRuntimeAddr(0x8055b7f8))(&resFile, path, nullptr, 0);
}

static bool LoadRawBRRES(void* holder, RawBRRES& cache, const char* path) {
    if (cache.failed) return false;
    if (cache.file == nullptr) {
        u32 fileSize = 0;
        if (!DiscFileSize(path, fileSize)) {
            cache.failed = true;
            return false;
        }
        GameScene* scene = const_cast<GameScene*>(GameScene::GetCurrent());
        if (scene == nullptr) {
            cache.failed = true;
            return false;
        }
        const u32 heapSize = AlignUp(fileSize, 0x20) + 0x2000;
        EGG::Heap* parent = RawParentHeap(*scene, heapSize);
        if (parent == nullptr) {
            cache.failed = true;
            return false;
        }
        cache.heap = EGG::ExpHeap::Create(static_cast<int>(heapSize), parent, 0);
        if (cache.heap == nullptr) {
            cache.failed = true;
            return false;
        }
        u32 loadedSize = 0;
        cache.file = EGG::DvdRipper::LoadToMainRAM(path, nullptr, cache.heap, EGG::DvdRipper::ALLOC_FROM_HEAD, 0, nullptr, &loadedSize);
        if (cache.file == nullptr || loadedSize == 0) {
            ClearRawCache(cache, true);
            cache.failed = true;
            return false;
        }
    }
    if ((reinterpret_cast<u32>(cache.file) & 0x1f) != 0) {
        cache.failed = true;
        return false;
    }
    nw4r::g3d::ResFile& resFile = *reinterpret_cast<nw4r::g3d::ResFile*>(reinterpret_cast<u8*>(holder) + 4);
    resFile.data = reinterpret_cast<nw4r::g3d::ResFileData*>(cache.file);
    BindRawBRRES(resFile, path);
    return true;
}

static u8 MiiCIndex(CharacterId character) {
    switch (character) {
        case MII_S_C_MALE:
            return 0;
        case MII_S_C_FEMALE:
            return 1;
        case MII_M_C_MALE:
            return 2;
        case MII_M_C_FEMALE:
            return 3;
        case MII_L_C_MALE:
            return 4;
        case MII_L_C_FEMALE:
            return 5;
        default:
            return MII_C_COUNT;
    }
}

static bool TryLoadCustomMenuBRRES(void* holder, CharacterId character) {
    const CharacterId menuCharacter = MenuBRRESCharacter(character);
    const u8 table = ResolveMenuTable(menuCharacter);
    if (table == TABLE_DEFAULT || table >= TABLE_COUNT || !HasSkin(menuCharacter, table)) return false;
    char path[0x60];
    if (!BuildDriverPath(menuCharacter, table, path, sizeof(path))) return false;
    return LoadRawBRRES(holder, rawBRRES[table][menuCharacter], path);
}

static bool TryLoadLooseMiiCBRRES(void* holder, CharacterId character) {
    const u8 idx = MiiCIndex(character);
    if (idx >= MII_C_COUNT) return false;
    const char* name = GetDefaultCharacterPostfix(character);
    if (name == nullptr) return false;
    char path[0x60];
    const int written = snprintf(path, sizeof(path), "/Scene/Model/Driver/%s.brres", name);
    if (written <= 0 || static_cast<u32>(written) >= sizeof(path)) return false;
    return LoadRawBRRES(holder, looseMiiCBRRES[idx], path);
}

kmRuntimeUse(0x8081e358);
static u32 LoadMenuDriverBRRESHook(void* holder, CharacterId character) {
    if (TryLoadCustomMenuBRRES(holder, character) || TryLoadLooseMiiCBRRES(holder, character)) return 1;
    typedef u32 (*Fn)(void*, CharacterId);
    return reinterpret_cast<Fn>(kmRuntimeAddr(0x8081e358))(holder, character);
}
kmCall(0x80830368, LoadMenuDriverBRRESHook);
kmCall(0x80831234, LoadMenuDriverBRRESHook);
kmCall(0x8083183c, LoadMenuDriverBRRESHook);

static bool BuildMinimapTPLPath(CharacterId character, u8 table, char* path, u32 pathSize) {
    const CharacterOverride* characterOverride = GetCharacterOverride(character, table);
    if (characterOverride == nullptr || characterOverride->postfix == nullptr) return false;
    const int written = snprintf(path, pathSize, "/Race/Map/%s.tpl", characterOverride->postfix);
    return written > 0 && static_cast<u32>(written) < pathSize;
}

static EGG::Heap* MinimapTPLHeap(GameScene& scene, u32 fileSize) {
    EGG::Heap* heaps[] = {scene.structsHeaps.heaps[0], scene.structsHeaps.heaps[1], scene.mainMEMHeap, scene.otherMEMHeap};
    for (u32 i = 0; i < ARRAY_COUNT(heaps); ++i) {
        EGG::Heap* heap = heaps[i];
        if (heap == nullptr) continue;
        UnlockHeap(heap);
        if (heap->getAllocatableSize(0x20) >= fileSize + 0x1000) return heap;
    }
    return nullptr;
}

static TPLPalettePtr LoadLooseMinimapTPL(CharacterId character, u8 table) {
    static const u32 TPL_VERSION_NUMBER = 0x0020af30;
    if (table == TABLE_DEFAULT || table >= TABLE_COUNT || !IsCharacter(character)) return nullptr;
    RawTPL& cache = looseMinimapTPL[table][character];
    if (cache.failed) return nullptr;

    char path[0x60];
    if (!BuildMinimapTPLPath(character, table, path, sizeof(path))) {
        cache.failed = true;
        return nullptr;
    }
    u32 fileSize = 0;
    if (!DiscFileSize(path, fileSize)) {
        cache.failed = true;
        return nullptr;
    }
    GameScene* scene = const_cast<GameScene*>(GameScene::GetCurrent());
    if (scene == nullptr) {
        cache.failed = true;
        return nullptr;
    }
    EGG::Heap* heap = MinimapTPLHeap(*scene, fileSize);
    if (heap == nullptr) {
        cache.failed = true;
        return nullptr;
    }
    u32 loadedSize = 0;
    TPLPalettePtr palette = static_cast<TPLPalettePtr>(
        EGG::DvdRipper::LoadToMainRAM(path, nullptr, heap, EGG::DvdRipper::ALLOC_FROM_HEAD, 0, nullptr, &loadedSize));
    if (palette == nullptr || loadedSize == 0 || palette->versionNumber != TPL_VERSION_NUMBER) {
        cache.failed = true;
        return nullptr;
    }
    return palette;
}

static void ReplacePaneTPL(nw4r::lyt::Pane* pane, TPLPalettePtr tpl) {
    if (pane == nullptr || tpl == nullptr) return;
    nw4r::lyt::Material* material = pane->GetMaterial();
    if (material == nullptr) return;
    material->GetTexMapAry()->ReplaceImage(tpl);
}

static void ApplyLooseMinimapTPL(CtrlRace2DMapCharacter* control) {
    const Racedata* racedata = Racedata::sInstance;
    if (control == nullptr || racedata == nullptr) return;
    const u8 playerId = control->playerId;
    if (playerId >= racedata->racesScenario.playerCount) return;
    const CharacterId character = racedata->racesScenario.players[playerId].characterId;
    const u8 table = RaceSkinTable(playerId, character);
    TPLPalettePtr tpl = LoadLooseMinimapTPL(character, table);
    if (tpl == nullptr) return;
    ReplacePaneTPL(control->charaPane, tpl);
    ReplacePaneTPL(control->charaShadow0Pane, tpl);
    ReplacePaneTPL(control->charaShadow1Pane, tpl);
}

kmRuntimeUse(0x807ec6e8);
static void InitMinimapCharacterHook(CtrlRace2DMapCharacter* control) {
    ApplyLooseMinimapTPL(control);
    typedef void (*Fn)(CtrlRaceBase*);
    reinterpret_cast<Fn>(kmRuntimeAddr(0x807ec6e8))(control);
}
kmCall(0x807eb22c, InitMinimapCharacterHook);

kmRuntimeUse(0x807dbd80);
static MiiHeadsModel* CreateMenuMiiHeadModelHook(void* memory, u32 type, MiiDriverModel* driverModel, u32 miiId, Mii* mii, u32 r8) {
    const GameScene* scene = GameScene::GetCurrent();
    if (scene != nullptr && (scene->id == SCENE_ID_GLOBE || scene->id == SCENE_ID_MENU) && driverCache.buildingTable < TABLE_COUNT) {
        return nullptr;
    }
    typedef MiiHeadsModel* (*Fn)(void*, u32, MiiDriverModel*, u32, Mii*, u32);
    return reinterpret_cast<Fn>(kmRuntimeAddr(0x807dbd80))(memory, type, driverModel, miiId, mii, r8);
}
kmCall(0x80830540, CreateMenuMiiHeadModelHook);

kmRuntimeUse(0x80830d00);
static void RequestDriverModelHook(MenuModelMgr* menuMgr, u8 playerId, CharacterId character) {
    if (menuMgr == nullptr || !menuMgr->isActive) return;
    MenuDriverModelMgr* drivers = menuMgr->driverModels;
    if (!DriverMgrUsable(drivers, menuMgr->playerCount) || playerId >= drivers->playerCount) return;
    typedef void (*Fn)(MenuDriverModelMgr*, u8, CharacterId);
    reinterpret_cast<Fn>(kmRuntimeAddr(0x80830d00))(drivers, playerId, character);
}
kmBranch(0x8059e568, RequestDriverModelHook);

static void ApplyMenuDriverNames() {
    CacheDefaults();
    for (u32 i = 0; i < CHARACTER_COUNT; ++i) ApplyName(static_cast<CharacterId>(i), TABLE_DEFAULT);
    if (UseDefaultTableForVotingVR()) return;
    for (u32 i = 0; i < CHARACTER_COUNT; ++i) {
        const CharacterId character = static_cast<CharacterId>(i);
        const u8 table = SelectedTable(character);
        if (table != TABLE_DEFAULT) ApplyName(MenuBRRESCharacter(character), NormalizeTable(MenuBRRESCharacter(character), table));
    }
}

kmRuntimeUse(0x80830748);
static void StartDriverMgr(MenuDriverModelMgr& mgr) {
    typedef void (*Fn)(MenuDriverModelMgr*);
    reinterpret_cast<Fn>(kmRuntimeAddr(0x80830748))(&mgr);
}

static void SnapshotMiiHeads(const MenuDriverModelMgr* mgr, MiiHeadsModel** heads) {
    if (mgr == nullptr || heads == nullptr || mgr->miiHeads == nullptr) return;
    MiiHeadsModel* const* src = reinterpret_cast<MiiHeadsModel* const*>(mgr->miiHeads);
    const u8 count = MinLocalPlayers(mgr->playerCount);
    for (u8 i = 0; i < count; ++i) heads[i] = src[i];
}

static void RestoreMiiHeads(MenuDriverModelMgr* mgr, MiiHeadsModel* const* heads) {
    if (mgr == nullptr || heads == nullptr || mgr->miiHeads == nullptr) return;
    MiiHeadsModel** dst = reinterpret_cast<MiiHeadsModel**>(mgr->miiHeads);
    const u8 count = MinLocalPlayers(mgr->playerCount);
    for (u8 i = 0; i < count; ++i) {
        if (dst[i] == nullptr) dst[i] = heads[i];
    }
}

static bool ReinitDriverModels() {
    MenuModelMgr* menuMgr = MenuModelMgr::sInstance;
    GameScene* scene = const_cast<GameScene*>(GameScene::GetCurrent());
    const SectionMgr* sectionMgr = SectionMgr::sInstance;
    if (menuMgr == nullptr || scene == nullptr || sectionMgr == nullptr || sectionMgr->sectionParams == nullptr || menuMgr->heap == nullptr) {
        return false;
    }
    SyncDriverCache();
    const u8 playerCount = menuMgr->playerCount;
    if (playerCount == 0) return false;

    MenuDriverModelMgr* oldDrivers = menuMgr->driverModels;
    bool visible[LOCAL_PLAYER_COUNT] = {false, false, false, false};
    MiiHeadsModel* heads[LOCAL_PLAYER_COUNT] = {nullptr, nullptr, nullptr, nullptr};
    MenuDriverModel::State states[LOCAL_PLAYER_COUNT] = {
        MenuDriverModel::MENUDRIVERMODEL_STATE_ONCHARSELECT,
        MenuDriverModel::MENUDRIVERMODEL_STATE_ONCHARSELECT,
        MenuDriverModel::MENUDRIVERMODEL_STATE_ONCHARSELECT,
        MenuDriverModel::MENUDRIVERMODEL_STATE_ONCHARSELECT};

    if (oldDrivers != nullptr) {
        SnapshotMiiHeads(oldDrivers, heads);
        const u8 count = MinLocalPlayers(oldDrivers->playerCount);
        for (u8 i = 0; i < count; ++i) {
            visible[i] = oldDrivers->players[i].isVisible;
            MenuDriverModel* model = oldDrivers->players[i].playerModel;
            if (model != nullptr && (model->state == MenuDriverModel::MENUDRIVERMODEL_STATE_ONCHARSELECT ||
                                     model->state == MenuDriverModel::MENUDRIVERMODEL_STATE_ONKARTSELECT ||
                                     model->state == VEHICLE_SELECTED_STATE)) {
                states[i] = model->state;
            }
        }
    }

    const CharacterId target = PreviewCharacter(0);
    const u8 table = ResolveMenuTable(target);
    const CharacterId cacheCharacter = MenuCacheCharacter(target, table);
    if (oldDrivers != nullptr && table == driverCache.currentTable && target == driverCache.currentHud0Character &&
        (table == TABLE_DEFAULT || cacheCharacter == driverCache.currentCharacter)) {
        return true;
    }

    EGG::ExpHeap* oldHeap = driverCache.activeHeap;
    menuMgr->driverModels = nullptr;
    if (oldDrivers != nullptr) ClearDriverModels(*oldDrivers);
    ClearRawCaches(true);
    DestroyHeap(oldHeap);
    driverCache.activeHeap = nullptr;

    UnlockMenuHeaps(*scene, *menuMgr);
    scene->structsHeaps.SetHeapsGroupId(3);
    driverCache.buildingTable = table;
    driverCache.buildingCharacter = target;
    ApplyMenuDriverNames();
    MenuDriverModelMgr* newDrivers = CreateDriverMgr(playerCount);
    driverCache.buildingTable = TABLE_INVALID;
    driverCache.buildingCharacter = CHARACTER_NONE;
    ApplySelectedNames();
    scene->structsHeaps.SetHeapsGroupId(0);

    if (!DriverMgrUsable(newDrivers, playerCount)) {
        scene->structsHeaps.SetHeapsGroupId(6);
        return false;
    }

    RestoreMiiHeads(newDrivers, heads);
    menuMgr->driverModels = newDrivers;
    driverCache.activeHeap = driverCache.createdHeap;
    driverCache.createdHeap = nullptr;
    StartDriverMgr(*newDrivers);
    scene->structsHeaps.SetHeapsGroupId(6);

    const u8 localCount = SectionPlayerCount(sectionMgr);
    for (u8 hud = 0; hud < localCount; ++hud) {
        menuMgr->RequestDriverModel(hud, sectionMgr->sectionParams->characters[hud]);
    }
    for (u8 i = 0; i < playerCount && i < LOCAL_PLAYER_COUNT; ++i) {
        MenuDriverModel* model = newDrivers->players[i].playerModel;
        if (model != nullptr && model->model != nullptr) {
            if (states[i] != MenuDriverModel::MENUDRIVERMODEL_STATE_ONCHARSELECT) model->id = sectionMgr->sectionParams->karts[i];
            model->state = static_cast<MenuDriverModel::State>(0);
            model->SwitchState(i, states[i]);
        }
        newDrivers->players[i].isVisible = visible[i];
        newDrivers->TogglePlayerModel(i, visible[i]);
    }
    RefreshCharacterSelectModels();
    driverCache.currentTable = table;
    driverCache.currentCharacter = cacheCharacter;
    driverCache.currentHud0Character = target;
    return true;
}

void OnVotingVRPageExit() {
    if (!SyncVotingVR() || votingVR.reinitialized || votingVR.exitedVR) return;
    votingVR.exitedVR = true;
    votingVR.reinitFrames = 1;
}

kmRuntimeUse(0x805f59ac);
static void SetModelRendererVehicleVisible(Pages::ModelRenderer& renderer, u8 hud) {
    typedef void (*Fn)(Pages::ModelRenderer*, u8, u32, bool);
    reinterpret_cast<Fn>(kmRuntimeAddr(0x805f59ac))(&renderer, hud, 1, true);
}

static bool PrimeVotingRenderer() {
    SectionMgr* mgr = SectionMgr::sInstance;
    if (mgr == nullptr || mgr->curSection == nullptr || mgr->sectionParams == nullptr) return false;
    Pages::ModelRenderer* renderer = mgr->curSection->Get<Pages::ModelRenderer>();
    if (renderer == nullptr) return false;
    const u8 count = SectionPlayerCount(mgr);
    for (u8 hud = 0; hud < count; ++hud) {
        const CharacterId character = mgr->sectionParams->characters[hud];
        renderer->RequestCharacterModel(hud, character);
        renderer->LoadKartModelsByCharacter(hud, character);
        Pages::ModelRenderer::PrepareParams(hud);
        SetModelRendererVehicleVisible(*renderer, hud);
        renderer->RequestKartModel(hud, mgr->sectionParams->karts[hud]);
    }
    votingVR.modelRendererPrimed = true;
    return true;
}

static MenuDriverModel::State VotingDriverState(bool kartVisible) {
    if (!kartVisible) return MenuDriverModel::MENUDRIVERMODEL_STATE_ONCHARSELECT;
    if (PageIsActive(PAGE_KART_SELECT) || PageIsActive(PAGE_BATTLE_KART_SELECT) || PageIsActive(PAGE_MULTIPLAYER_KART_SELECT)) {
        return MenuDriverModel::MENUDRIVERMODEL_STATE_ONKARTSELECT;
    }
    return VEHICLE_SELECTED_STATE;
}

static void ReinitVotingDriverModels() {
    if (!SyncVotingVR() || votingVR.reinitialized) return;
    driverCache.currentTable = TABLE_INVALID;
    driverCache.currentCharacter = CHARACTER_NONE;
    driverCache.currentHud0Character = CHARACTER_NONE;
    if (ReinitDriverModels()) {
        votingVR.reinitialized = true;
        PrimeVotingRenderer();
        votingVR.refreshFrames = 20;
    } else {
        votingVR.reinitFrames = 1;
    }
}

static void ProcessVotingReinit() {
    if (!SyncVotingVR()) return;
    if (PageIsActive(PAGE_VR)) {
        votingVR.sawVR = true;
        return;
    }
    if (!votingVR.exitedVR && !votingVR.sawVR) return;
    OnVotingVRPageExit();
    if (votingVR.reinitFrames != 0) {
        --votingVR.reinitFrames;
        return;
    }
    applyVotingVRReinit = true;
}

static bool RefreshVotingStates() {
    MenuModelMgr* menuMgr = MenuModelMgr::sInstance;
    const SectionMgr* sectionMgr = SectionMgr::sInstance;
    if (menuMgr == nullptr || sectionMgr == nullptr || sectionMgr->sectionParams == nullptr) return false;
    if (!votingVR.modelRendererPrimed && !PrimeVotingRenderer()) return false;
    MenuDriverModelMgr* drivers = menuMgr->driverModels;
    MenuKartModelMgr* karts = menuMgr->kartModels;
    if (!DriverMgrUsable(drivers, menuMgr->playerCount) || karts == nullptr) return false;

    bool ready = true;
    const u8 count = SectionPlayerCount(sectionMgr);
    for (u8 hud = 0; hud < count; ++hud) {
        if (hud >= drivers->playerCount || hud >= karts->playerCount) continue;
        MenuDriverModel* model = drivers->players[hud].playerModel;
        if (model == nullptr || model->model == nullptr) {
            ready = false;
            continue;
        }
        const MenuDriverModel::State state = VotingDriverState(karts->players[hud].isModelVisible);
        if (state != MenuDriverModel::MENUDRIVERMODEL_STATE_ONCHARSELECT && model->onKartTransformator == nullptr) {
            menuMgr->RequestKartModel(hud, sectionMgr->sectionParams->karts[hud]);
            ready = false;
            continue;
        }
        if (model->state != state) {
            if (state != MenuDriverModel::MENUDRIVERMODEL_STATE_ONCHARSELECT) model->id = sectionMgr->sectionParams->karts[hud];
            model->SwitchState(hud, state);
        }
    }
    return ready;
}

static void ProcessVotingStateRefresh() {
    if (votingVR.refreshFrames == 0 || !SyncVotingVR() || PageIsActive(PAGE_VR)) return;
    if (RefreshVotingStates())
        votingVR.refreshFrames = 0;
    else
        --votingVR.refreshFrames;
}

static void ProcessPendingReinit() {
    if (pendingReinitFrames == 0 || --pendingReinitFrames != 0) return;
    if (!MenuPreviewStable()) {
        if (pendingReinitWaits < MENU_REINIT_MAX_WAITS) {
            ++pendingReinitWaits;
            pendingReinitFrames = 2;
        } else {
            pendingReinitWaits = 0;
        }
        return;
    }
    pendingReinitWaits = 0;
    applyMenuDriverReinit = true;
}

static void ApplyDeferredReinits() {
    if (applyMenuDriverReinit) {
        applyMenuDriverReinit = false;
        if (MenuPreviewStable()) ReinitDriverModels();
    }
    if (applyVotingVRReinit) {
        applyVotingVRReinit = false;
        ReinitVotingDriverModels();
    }
}

kmRuntimeUse(0x80540e3c);
static ArchivesHolder* LoadKartArchiveHook(ArchiveMgr* archiveMgr, u8 playerId, KartId kart, CharacterId character, u32 color, u32 type,
                                           EGG::Heap* decompressedHeap, EGG::Heap* archiveHeap) {
    typedef ArchivesHolder* (*Fn)(ArchiveMgr*, u8, KartId, CharacterId, u32, u32, EGG::Heap*, EGG::Heap*);
    ScopedNameSwap swap(playerId, character);
    return reinterpret_cast<Fn>(kmRuntimeAddr(0x80540e3c))(archiveMgr, playerId, kart, character, color, type, decompressedHeap, archiveHeap);
}
kmCall(0x805540f4, LoadKartArchiveHook);

kmRuntimeUse(0x80540f90);
static ArchivesHolder* LoadBackupKartArchiveHook(ArchiveMgr* archiveMgr, u8 playerId, KartId kart, CharacterId character, u32 color, u32 type,
                                                 EGG::Heap* decompressedHeap, EGG::Heap* archiveHeap) {
    typedef ArchivesHolder* (*Fn)(ArchiveMgr*, u8, KartId, CharacterId, u32, u32, EGG::Heap*, EGG::Heap*);
    ScopedNameSwap swap(playerId, character);
    return reinterpret_cast<Fn>(kmRuntimeAddr(0x80540f90))(archiveMgr, playerId, kart, character, color, type, decompressedHeap, archiveHeap);
}
kmCall(0x80554198, LoadBackupKartArchiveHook);

kmRuntimeUse(0x805419c8);
static const char* GetMenuDriverBRRESNameHook(u32 character) {
    typedef const char* (*Fn)(u32);
    const CharacterId id = static_cast<CharacterId>(character);
    const u8 table = ResolveMenuTable(id);
    if (IsCharacter(id) && table < TABLE_COUNT && rawBRRES[table][id].failed) return reinterpret_cast<Fn>(kmRuntimeAddr(0x805419c8))(character);
    const char* name = DriverBRRESName(id, table);
    if (name != nullptr) return name;
    return reinterpret_cast<Fn>(kmRuntimeAddr(0x805419c8))(character);
}
kmCall(0x8081e4a0, GetMenuDriverBRRESNameHook);

static void ResetCustomCharacterMenuState() {
    if (!IsOnlineRoom(RKNet::Controller::sInstance)) ResetOnlineCustomCharacterFlags();
    ResetOfflineCpuSkinTablesForSection();
    ResetDriverHeaps();
    ApplySelectedNames();
}
static SectionLoadHook ResetCustomCharacterMenuStateHook(ResetCustomCharacterMenuState);

kmRuntimeUse(0x8083e5f4);
static void CharacterSelectHoverHook(Pages::CharacterSelect* page, CtrlMenuCharacterSelect::ButtonDriver* button, u32 buttonId, u8 hud) {
    if (hud < LOCAL_PLAYER_COUNT) hoveredCharacters[hud] = static_cast<CharacterId>(buttonId);
    typedef void (*Fn)(Pages::CharacterSelect*, CtrlMenuCharacterSelect::ButtonDriver*, u32, u8);
    reinterpret_cast<Fn>(kmRuntimeAddr(0x8083e5f4))(page, button, buttonId, hud);
    if (hud < LOCAL_PLAYER_COUNT) characterNameTextControl[hud] = nullptr;
    UpdateCharacterSelectNameText(page, hud);
    UpdateCharacterSelectAuthorText(page, hud);
}
kmCall(0x807e2cf0, CharacterSelectHoverHook);
kmCall(0x807e304c, CharacterSelectHoverHook);
kmCall(0x807e34d0, CharacterSelectHoverHook);
kmCall(0x807e37b0, CharacterSelectHoverHook);
kmCall(0x807e3a88, CharacterSelectHoverHook);

static ControllerType ControllerForHud(const SectionMgr& mgr, u8 hud) {
    if (hud >= LOCAL_PLAYER_COUNT) return GCN;
    const Input::RealControllerHolder* holder = mgr.pad.padInfos[hud].controllerHolder;
    if (holder == nullptr || holder->curController == nullptr) return GCN;
    const ControllerType type = holder->curController->GetType();
    return type == WHEEL || type == NUNCHUCK || type == CLASSIC || type == GCN ? type : GCN;
}

static void SetHintPanes(CharaName& name, ControllerType type, bool visible) {
    const char* panes[] = {"cc_prev_wh", "cc_next_wh", "cc_prev_nc", "cc_next_nc", "cc_prev_cls", "cc_next_cls", "cc_prev_gc", "cc_next_gc"};
    for (u32 i = 0; i < ARRAY_COUNT(panes); ++i) name.SetPaneVisibility(panes[i], false);
    if (!visible) return;
    u32 offset = 6;
    if (type == WHEEL)
        offset = 0;
    else if (type == NUNCHUCK)
        offset = 2;
    else if (type == CLASSIC)
        offset = 4;
    name.SetPaneVisibility(panes[offset], true);
    name.SetPaneVisibility(panes[offset + 1], true);
}

static void UpdateHintPanes() {
    if (!IsCharacterSelectActive()) return;
    SectionMgr* mgr = SectionMgr::sInstance;
    if (mgr == nullptr || mgr->sectionParams == nullptr) return;
    Pages::CharacterSelect* page = mgr->curSection->Get<Pages::CharacterSelect>();
    if (page == nullptr || page->names == nullptr) return;
    const bool visible = !IsLocalMultiplayer();
    const u8 count = SectionPlayerCount(mgr);
    for (u8 hud = 0; hud < count; ++hud) SetHintPanes(page->names[hud], ControllerForHud(*mgr, hud), visible);
}

static void ToggleInputs(ControllerType type, u16& prevButton, u16& nextButton, u16& prevAction, u16& nextAction) {
    prevAction = 0;
    nextAction = 0;
    switch (type) {
        case WHEEL:
            prevButton = WPAD::WPAD_BUTTON_B;
            nextButton = WPAD::WPAD_BUTTON_A;
            prevAction = static_cast<u16>(1 << BACK_PRESS);
            nextAction = static_cast<u16>(1 << FORWARD_PRESS);
            break;
        case NUNCHUCK:
            prevButton = WPAD::WPAD_BUTTON_1;
            nextButton = WPAD::WPAD_BUTTON_2;
            prevAction = static_cast<u16>(1 << BACK_PRESS);
            nextAction = static_cast<u16>(1 << FORWARD_PRESS);
            break;
        case CLASSIC:
            prevButton = WPAD::WPAD_CL_TRIGGER_L;
            nextButton = WPAD::WPAD_CL_TRIGGER_R;
            break;
        case GCN:
        default:
            prevButton = PAD::PAD_BUTTON_L;
            nextButton = PAD::PAD_BUTTON_R;
            break;
    }
}

static void EatButton(Input::RealControllerHolder& holder, u16 button, u16 action) {
    holder.inputStates[0].buttonRaw &= static_cast<u16>(~button);
    holder.uiinputStates[0].rawButtons &= static_cast<u16>(~button);
    holder.uiinputStates[0].buttonActions &= static_cast<u16>(~action);
}

static void ReinitMenuDriverModelsNow() {
    pendingReinitFrames = 0;
    pendingReinitWaits = 0;
    applyMenuDriverReinit = false;
    driverCache.currentTable = TABLE_INVALID;
    driverCache.currentCharacter = CHARACTER_NONE;
    driverCache.currentHud0Character = CHARACTER_NONE;
    if (!ReinitDriverModels()) QueueMenuDriverReinit(1);
}

static bool ProcessSkinInput() {
    if (GetLocalPlayerCount() != 1 || !IsCharacterSelectActive()) {
        heldToggleButtons = 0;
        return false;
    }
    SectionMgr* mgr = SectionMgr::sInstance;
    if (mgr == nullptr || mgr->pad.padInfos[0].controllerHolder == nullptr) {
        heldToggleButtons = 0;
        return false;
    }
    Input::RealControllerHolder* holder = mgr->pad.padInfos[0].controllerHolder;
    if (holder->curController == nullptr) {
        heldToggleButtons = 0;
        return false;
    }

    u16 prevButton = 0;
    u16 nextButton = 0;
    u16 prevAction = 0;
    u16 nextAction = 0;
    ToggleInputs(ControllerForHud(*mgr, 0), prevButton, nextButton, prevAction, nextAction);
    const u16 inputs = holder->inputStates[0].buttonRaw;
    const u16 pressed = static_cast<u16>((inputs & (prevButton | nextButton)) & ~heldToggleButtons);
    heldToggleButtons = static_cast<u16>(inputs & (prevButton | nextButton));
    if ((inputs & prevButton) != 0) EatButton(*holder, prevButton, prevAction);
    if ((inputs & nextButton) != 0) EatButton(*holder, nextButton, nextAction);
    if ((pressed & prevButton) != 0 && CycleSkin(PreviewCharacter(0), -1)) {
        ReinitMenuDriverModelsNow();
        Audio::RSARPlayer::PlaySoundById(SOUND_ID_LEFT_ARROW_PRESS, 0, 0);
        return true;
    }
    if ((pressed & nextButton) != 0 && CycleSkin(PreviewCharacter(0), 1)) {
        ReinitMenuDriverModelsNow();
        Audio::RSARPlayer::PlaySoundById(SOUND_ID_RIGHT_ARROW_PRESS, 0, 0);
        return true;
    }
    return false;
}

kmRuntimeUse(0x8063583c);
static void MenuSceneSectionUpdateHook(SectionMgr* mgr) {
    ApplyDeferredReinits();
    UpdateHintPanes();
    ProcessSkinInput();
    UpdateCurrentCharacterSelectAuthorText(0);
    typedef void (*Fn)(SectionMgr*);
    reinterpret_cast<Fn>(kmRuntimeAddr(0x8063583c))(mgr);
    ProcessVotingReinit();
    ProcessVotingStateRefresh();
    ProcessPendingReinit();
}
kmCall(0x805552e8, MenuSceneSectionUpdateHook);
kmCall(0x80553b30, MenuSceneSectionUpdateHook);

}  // namespace CustomCharacters
}  // namespace Pulsar
