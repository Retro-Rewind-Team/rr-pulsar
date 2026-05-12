#include <hooks.hpp>
#include <runtimeWrite.hpp>
#include <CustomCharacters.hpp>
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
    PACKET_BITS = 3,
    PACKET_MASK = (1 << PACKET_BITS) - 1,
    TABLE_DEFAULT = 0,
    TABLE_COUNT = 1 << PACKET_BITS,
    TABLE_INVALID = TABLE_COUNT,

    CHARACTER_COUNT = 0x30,
    MENU_DRIVER_MODEL_COUNT = 0x18,
    LOCAL_PLAYER_COUNT = 4,
    ONLINE_PLAYER_COUNT = 12,
    MII_C_COUNT = 6
};

extern "C" const char* characterNames[];

static_assert(TABLE_COUNT <= (1 << PACKET_BITS), "SELECT packet skin table field is too small");
static_assert(PACKET_BITS * 2 <= 8, "SELECT packet skin table fields must fit in one byte");

struct CharacterOverride {
    CharacterId character;
    const char* postfix;
    bool silentVoice;
    u32 nameBmgId;
    const wchar_t* fallbackName;
    const char* authorText;
    u32 voiceBaseGroupId;
};

#define CUSTOM_CHARACTER_NAME_BMG(index) (UI::BMG_CUSTOM_CHARACTER_NAME_START + (index))

static const CharacterOverride customCharacterAssets[] = {
    {MARIO, "mr-1", false, CUSTOM_CHARACTER_NAME_BMG(0), L"Mario (Fire)", "LTC 91"},
    {MARIO, "mr-2", false, CUSTOM_CHARACTER_NAME_BMG(1), L"Mario (Builder)", "UltraWario"},
    {MARIO, "mr-3", true, CUSTOM_CHARACTER_NAME_BMG(2), L"Snowman", "GioMacGrillin"},
    {BABY_PEACH, "bpc-1", false, CUSTOM_CHARACTER_NAME_BMG(3), L"Baby Peach (Cherub)", "UltraWario"},
    {BABY_PEACH, "bpc-2", true, CUSTOM_CHARACTER_NAME_BMG(4), L"Pikachu", "ALE XD"},
    {WALUIGI, "wl-1", false, CUSTOM_CHARACTER_NAME_BMG(5), L"Waluigi (Athletic)", "TheBeefBai"},
    {WALUIGI, "wl-2", false, CUSTOM_CHARACTER_NAME_BMG(6), L"Waluigi (Vampire)", "UltraWario"},
    {WALUIGI, "wl-3", true, CUSTOM_CHARACTER_NAME_BMG(7), L"Wiggler", "UltraWario"},
    {BOWSER, "kp-1", false, CUSTOM_CHARACTER_NAME_BMG(8), L"Dark Bowser", "UltraWario"},
    {BOWSER, "kp-2", false, CUSTOM_CHARACTER_NAME_BMG(9), L"Bowser (Santa)", "UltraWario"},
    {BOWSER, "kp-3", true, CUSTOM_CHARACTER_NAME_BMG(10), L"Chargin Chuck", "UltraWario, Porko"},
    {BOWSER, "kp-4", true, CUSTOM_CHARACTER_NAME_BMG(11), L"Boom Boom", "Numerosity"},
    {BABY_DAISY, "bds-1", false, CUSTOM_CHARACTER_NAME_BMG(12), L"Baby Daisy (Coat)", "Poobah"},
    {BABY_DAISY, "bds-2", true, CUSTOM_CHARACTER_NAME_BMG(13), L"Rocky Wrench", "Porko, UltraWario"},
    {DRY_BONES, "ka-1", false, CUSTOM_CHARACTER_NAME_BMG(14), L"Dry Bones (Dark)", "MiningBoy"},
    {DRY_BONES, "ka-2", false, CUSTOM_CHARACTER_NAME_BMG(15), L"Dry Bones (Gold)", "LTC 91"},
    {DRY_BONES, "ka-3", true, CUSTOM_CHARACTER_NAME_BMG(16), L"Goomba", "UltraWario"},
    {DRY_BONES, "ka-4", true, CUSTOM_CHARACTER_NAME_BMG(17), L"Peepa", "GioMacGrillin"},
    {BABY_MARIO, "bmr-1", false, CUSTOM_CHARACTER_NAME_BMG(18), L"Baby Wario", "Whipinsnapper"},
    {BABY_MARIO, "bmr-2", false, CUSTOM_CHARACTER_NAME_BMG(19), L"Baby Mario (Koala)", "LTC 91"},
    {BABY_MARIO, "bmr-3", true, CUSTOM_CHARACTER_NAME_BMG(20), L"Coin Coffer", "GioMacGrillin"},
    {BABY_MARIO, "bmr-4", true, CUSTOM_CHARACTER_NAME_BMG(21), L"Shy Guy", "UltraWario"},
    {BABY_MARIO, "bmr-5", true, CUSTOM_CHARACTER_NAME_BMG(84), L"Paper Mario", "dutchmvp26"},
    {LUIGI, "lg-1", false, CUSTOM_CHARACTER_NAME_BMG(22), L"Luigi (Classic)", "UltraWario"},
    {LUIGI, "lg-2", false, CUSTOM_CHARACTER_NAME_BMG(23), L"Luigi (Vacation)", "PlayersPurity"},
    {LUIGI, "lg-3", true, CUSTOM_CHARACTER_NAME_BMG(24), L"Monty Mole", "JTG", BRSAR_GROUP_LUIGI},
    {LUIGI, "lg-4", true, CUSTOM_CHARACTER_NAME_BMG(25), L"Sonic", "ALE XD"},
    {TOAD, "ko-1", false, CUSTOM_CHARACTER_NAME_BMG(26), L"Toad (Captain)", "UltraWario"},
    {TOAD, "ko-2", false, CUSTOM_CHARACTER_NAME_BMG(27), L"Toad (Builder)", "UltraWario"},
    {TOAD, "ko-3", true, CUSTOM_CHARACTER_NAME_BMG(28), L"Swoop", "Monxy"},
    {DONKEY_KONG, "dk-1", false, CUSTOM_CHARACTER_NAME_BMG(29), L"DK (White)", "Whipinsnapper"},
    {DONKEY_KONG, "dk-2", false, CUSTOM_CHARACTER_NAME_BMG(30), L"DK (Gladiator)", "LTC 91"},
    {DONKEY_KONG, "dk-3", true, CUSTOM_CHARACTER_NAME_BMG(31), L"Kritter", "Monxy"},
    {DONKEY_KONG, "dk-4", true, CUSTOM_CHARACTER_NAME_BMG(66), L"Pokey", "Cillow that Willow", BRSAR_GROUP_FUNKY_KONG},
    {DONKEY_KONG, "dk-5", true, CUSTOM_CHARACTER_NAME_BMG(80), L"King K. Rool", "ordatz"},
    {YOSHI, "ys-1", false, CUSTOM_CHARACTER_NAME_BMG(33), L"Yoshi (Black)", "LTC 91"},
    {YOSHI, "ys-2", false, CUSTOM_CHARACTER_NAME_BMG(34), L"Yoshi (Kangaroo)", "ordartz"},
    {YOSHI, "ys-3", true, CUSTOM_CHARACTER_NAME_BMG(35), L"Piranha Plant", "UltraWario, Porko"},
    {WARIO, "wr-1", false, CUSTOM_CHARACTER_NAME_BMG(36), L"Wario (Cowboy)", "UltraWario"},
    {WARIO, "wr-2", false, CUSTOM_CHARACTER_NAME_BMG(37), L"Wario (Hiker)", "UltraWario"},
    {WARIO, "wr-3", true, CUSTOM_CHARACTER_NAME_BMG(38), L"King Bob-omb", "UltraWario"},
    {BABY_LUIGI, "blg-1", false, CUSTOM_CHARACTER_NAME_BMG(39), L"Baby Waluigi", "Tadhger"},
    {BABY_LUIGI, "blg-2", true, CUSTOM_CHARACTER_NAME_BMG(81), L"E. Gadd", "UltraWario"},
    {TOADETTE, "kk-1", false, CUSTOM_CHARACTER_NAME_BMG(40), L"Toadette (Bubble)", "Toadette Hack Fan"},
    {KOOPA_TROOPA, "nk-1", false, CUSTOM_CHARACTER_NAME_BMG(41), L"Koopa Paratroopa", "UltraWario"},
    {KOOPA_TROOPA, "nk-2", true, CUSTOM_CHARACTER_NAME_BMG(42), L"Lakitu", "UltraWario"},
    {DAISY, "ds-1", false, CUSTOM_CHARACTER_NAME_BMG(43), L"Daisy (Black/Teal)", "TheBeefBai"},
    {DAISY, "ds-2", false, CUSTOM_CHARACTER_NAME_BMG(44), L"Daisy (Farmer)", "ALE XD"},
    {DAISY, "ds-3", false, CUSTOM_CHARACTER_NAME_BMG(45), L"Daisy (Swimwear)", "UltraWario"},
    {PEACH, "pc-1", false, CUSTOM_CHARACTER_NAME_BMG(46), L"Peach (Ice)", "Whipinsnapper"},
    {PEACH, "pc-2", false, CUSTOM_CHARACTER_NAME_BMG(47), L"Peach (Explorer)", "GVRIMZ"},
    {PEACH, "pc-3", false, CUSTOM_CHARACTER_NAME_BMG(48), L"Peach (Halloween)", "UltraWario"},
    {BIRDO, "ca-1", false, CUSTOM_CHARACTER_NAME_BMG(49), L"Birdo (Red)", "JTG"},
    {BIRDO, "ca-2", true, CUSTOM_CHARACTER_NAME_BMG(50), L"Mega Man", "DJ Lowgey"},
    {DIDDY_KONG, "dd-1", false, CUSTOM_CHARACTER_NAME_BMG(51), L"Diddy Kong (All-Star)", "Cazzyboy360"},
    {DIDDY_KONG, "dd-2", true, CUSTOM_CHARACTER_NAME_BMG(52), L"Dixie Kong", "UltraWario"},
    {DIDDY_KONG, "dd-3", true, CUSTOM_CHARACTER_NAME_BMG(53), L"The Chimp", "ZoroCarlos"},
    {KING_BOO, "kt-1", false, CUSTOM_CHARACTER_NAME_BMG(54), L"King Boo (LM)", "UltraWario"},
    {KING_BOO, "kt-2", true, CUSTOM_CHARACTER_NAME_BMG(79), L"Gooper Blooper", "UltraWario"},
    {KING_BOO, "kt-3", true, CUSTOM_CHARACTER_NAME_BMG(82), L"Goku", "Chiller7", BRSAR_GROUP_WALUIGI},
    {BOWSER_JR, "jr-1", false, CUSTOM_CHARACTER_NAME_BMG(55), L"Bowser Jr. (Pirate)", "UltraWario"},
    {BOWSER_JR, "jr-2", true, CUSTOM_CHARACTER_NAME_BMG(56), L"Hammer Bro", "UltraWario", BRSAR_GROUP_MARIO},
    {BOWSER_JR, "jr-3", true, CUSTOM_CHARACTER_NAME_BMG(57), L"Kamek", "DJ Lowgey"},
    {BOWSER_JR, "jr-4", false, CUSTOM_CHARACTER_NAME_BMG(58), L"Bowser Jr. (Dark)", "Whipinsnapper"},
    {BOWSER_JR, "jr-5", true, CUSTOM_CHARACTER_NAME_BMG(59), L"Nabbit", "UltraWario"},
    {DRY_BOWSER, "bk-1", false, CUSTOM_CHARACTER_NAME_BMG(60), L"Dry Bowser (Dark)", "Kracken"},
    {DRY_BOWSER, "bk-2", true, CUSTOM_CHARACTER_NAME_BMG(61), L"Lubba", "Ricoxemani, Cillow that Willow", BRSAR_GROUP_WALUIGI},
    {DRY_BOWSER, "bk-3", true, CUSTOM_CHARACTER_NAME_BMG(83), L"Petey Piranha", "UltraWario", BRSAR_GROUP_FUNKY_KONG},
    {FUNKY_KONG, "fk-1", false, CUSTOM_CHARACTER_NAME_BMG(62), L"Zapple Kong", "ZPL"},
    {FUNKY_KONG, "fk-2", true, CUSTOM_CHARACTER_NAME_BMG(63), L"Chain Chomp", "UltraWario"},
    {FUNKY_KONG, "fk-3", false, CUSTOM_CHARACTER_NAME_BMG(64), L"Funky Kong (Link)", "ordartz"},
    {FUNKY_KONG, "fk-4", false, CUSTOM_CHARACTER_NAME_BMG(65), L"Funky Kong (Dripped Up)", "Whipinsnapper"},
    {FUNKY_KONG, "fk-5", true, CUSTOM_CHARACTER_NAME_BMG(32), L"R.O.B", "Chiller7", BRSAR_GROUP_DONKEY_KONG},
    {ROSALINA, "rs-1", false, CUSTOM_CHARACTER_NAME_BMG(67), L"Rosalina (Aurora)", "Chiller7"},
    {ROSALINA, "rs-2", false, CUSTOM_CHARACTER_NAME_BMG(68), L"Rosalina (Touring)", "Eydra"},
    {ROSALINA, "rs-3", true, CUSTOM_CHARACTER_NAME_BMG(69), L"Pauline", "UltraWario"},
    {PEACH_BIKER, "pc-1", false, CUSTOM_CHARACTER_NAME_BMG(70), L"Peach (Ice)", "TheBeefBai"},
    {PEACH_BIKER, "pc-2", false, CUSTOM_CHARACTER_NAME_BMG(71), L"Peach (Explorer)", "ALE XD"},
    {PEACH_BIKER, "pc-3", false, CUSTOM_CHARACTER_NAME_BMG(72), L"Peach (Halloween)", "UltraWario"},
    {DAISY_BIKER, "ds-1", false, CUSTOM_CHARACTER_NAME_BMG(73), L"Daisy (Green/Black)", "Whipinsnapper"},
    {DAISY_BIKER, "ds-2", false, CUSTOM_CHARACTER_NAME_BMG(74), L"Daisy (Farmer)", "GVRIMZ"},
    {DAISY_BIKER, "ds-3", false, CUSTOM_CHARACTER_NAME_BMG(75), L"Daisy (Swimwear)", "UltraWario"},
    {ROSALINA_BIKER, "rs-1", false, CUSTOM_CHARACTER_NAME_BMG(76), L"Rosalina (Aurora)", "Chiller7"},
    {ROSALINA_BIKER, "rs-2", false, CUSTOM_CHARACTER_NAME_BMG(77), L"Rosalina (Touring)", "Eydra"},
    {ROSALINA_BIKER, "rs-3", true, CUSTOM_CHARACTER_NAME_BMG(78), L"Pauline", "UltraWario"},
};

struct RawBRRES {
    EGG::ExpHeap* heap;
    void* file;
    bool failed;
    bool bound;
};

struct RawTPL {
    bool failed;
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
static u8 looseVoiceFiles[TABLE_COUNT][CHARACTER_COUNT];
static const GameScene* rawCacheSceneOwner;
static u32 offlineCpuSkinSignature;
static u8 offlineCpuSkinRaceNumber;
static bool offlineCpuSkinTablesValid;
static u16 heldToggleButtons;
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
static SectionId votingMenuTableSection = SECTION_NONE;
static bool votingMenuTablesRestored;
static bool voteRandomMessageBoxKartStateApplied;
static EGG::ExpHeap* reloadedMenuDriverModelHeaps[MENU_DRIVER_MODEL_COUNT];
static ModelDirector* reloadedMenuDriverModels[MENU_DRIVER_MODEL_COUNT];
static ToadetteHair* reloadedMenuDriverModelHairs[MENU_DRIVER_MODEL_COUNT];
static const GameScene* reloadedMenuDriverModelSceneOwner;
static MenuDriverModel* reloadedMenuDriverModelOwner;
static bool forceDefaultMenuDriverBRRES;

static u8 MinLocalPlayers(u32 count) {
    return count > LOCAL_PLAYER_COUNT ? LOCAL_PLAYER_COUNT : static_cast<u8>(count);
}

static bool IsCharacter(CharacterId character) {
    return character >= 0 && character < CHARACTER_COUNT;
}

static bool HasLooseCustomVoiceFiles(CharacterId character, u8 table);

static bool IsMiiCharacter(CharacterId character) {
    return (character >= MII_S_A_MALE && character <= MII_L_C_FEMALE) || character == MII_M || character == MII_S || character == MII_L;
}

static const char** CharacterNameEntry(CharacterId character) {
    if (!IsCharacter(character)) return nullptr;
    return characterNames + character;
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
    u8 tableIdx = 1;
    for (u32 i = 0; i < ARRAY_COUNT(customCharacterAssets); ++i) {
        const CharacterOverride& characterOverride = customCharacterAssets[i];
        if (characterOverride.character != character) continue;
        if (tableIdx == table) return &characterOverride;
        ++tableIdx;
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

static const wchar_t* CustomCharacterNameFallback(u32 bmgId) {
    for (u32 i = 0; i < ARRAY_COUNT(customCharacterAssets); ++i) {
        const CharacterOverride& characterOverride = customCharacterAssets[i];
        if (characterOverride.nameBmgId == bmgId) return characterOverride.fallbackName;
    }
    return static_cast<const wchar_t*>(0);
}

static bool HasBmgMessage(const BMGHolder& holder, u32 bmgId) {
    if (holder.messageIds == nullptr) return false;
    const BMGMessageIds& msgIds = *holder.messageIds;
    for (u32 i = 0; i < msgIds.msgCount; ++i) {
        const u32 curBmgId = msgIds.messageIds[i];
        if (curBmgId == bmgId) return true;
        if (curBmgId > bmgId) return false;
    }
    return false;
}

static bool HasCustomCharacterNameMessage(const LayoutUIControl& control, u32 bmgId) {
    return UI::GetCustomMsg(static_cast<s32>(bmgId)) != nullptr || HasBmgMessage(control.curFileBmgs, bmgId) ||
           HasBmgMessage(control.commonBmgs, bmgId);
}

static const wchar_t* MissingCustomCharacterNameFallback(const LayoutUIControl& control, u32 bmgId) {
    const wchar_t* fallback = CustomCharacterNameFallback(bmgId);
    if (fallback == nullptr || HasCustomCharacterNameMessage(control, bmgId)) return static_cast<const wchar_t*>(0);
    return fallback;
}

static void FillTextInfo(Text::Info& info, const wchar_t* text) {
    info.strings[0] = const_cast<wchar_t*>(text);
}

static bool SetCustomCharacterNameMessage(LayoutUIControl& control, const char* paneName, u32 bmgId) {
    const wchar_t* fallback = MissingCustomCharacterNameFallback(control, bmgId);
    if (fallback == nullptr) {
        control.SetTextBoxMessage(paneName, bmgId, nullptr);
    } else {
        Text::Info info;
        FillTextInfo(info, fallback);
        control.SetTextBoxMessage(paneName, UI::BMG_TEXT, &info);
    }
    return true;
}

static bool SetCustomCharacterNameMessage(LayoutUIControl& control, u32 bmgId) {
    const wchar_t* fallback = MissingCustomCharacterNameFallback(control, bmgId);
    if (fallback == nullptr) {
        control.SetMessage(bmgId, nullptr);
    } else {
        Text::Info info;
        FillTextInfo(info, fallback);
        control.SetMessage(UI::BMG_TEXT, &info);
    }
    return true;
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

static bool IsVotingSection(SectionId section) {
    return section == SECTION_P1_WIFI_VS_VOTING || section == SECTION_P1_WIFI_BATTLE_VOTING || section == SECTION_P2_WIFI_VS_VOTING ||
           section == SECTION_P2_WIFI_BATTLE_VOTING ||
           (section >= SECTION_P1_WIFI_FROOM_VS_VOTING && section <= SECTION_P2_WIFI_FROOM_COIN_VOTING);
}

static SectionId CurrentSectionId() {
    const SectionMgr* mgr = SectionMgr::sInstance;
    if (mgr == nullptr || mgr->curSection == nullptr) return SECTION_NONE;
    return mgr->curSection->sectionId;
}

static bool ShouldForceDefaultVotingMenuTable() {
    const SectionId section = CurrentSectionId();
    if (!IsVotingSection(section)) {
        votingMenuTableSection = SECTION_NONE;
        votingMenuTablesRestored = false;
        return false;
    }
    if (votingMenuTableSection != section) {
        votingMenuTableSection = section;
        votingMenuTablesRestored = false;
    }
    return !votingMenuTablesRestored;
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
    return characterOverride != nullptr && characterOverride->silentVoice && !HasLooseCustomVoiceFiles(character, table);
}

static TicoModel* CreateTicoModelHook(void* memory, DriverController* controller) {
    const Racedata* racedata = Racedata::sInstance;
    const u8 playerId = controller->GetPlayerIdx();
    const CharacterId character = racedata->racesScenario.players[playerId].characterId;
    const u8 table = RaceSkinTable(playerId, character);
    const CharacterOverride* characterOverride = GetCharacterOverride(character, table);
    if (table == TABLE_DEFAULT) return new (memory) TicoModel(controller);
    if (controller != nullptr && characterOverride->silentVoice == true) {
        if (memory != nullptr) ::operator delete(memory);
        return nullptr;
    }
    if (memory == nullptr) return nullptr;
    return new (memory) TicoModel(controller);
}
kmCall(0x807c8994, CreateTicoModelHook);

static const char** BeginNameSwap(u8 playerId, CharacterId character, const char*& oldName) {
    const char** entry = CharacterNameEntry(character);
    oldName = nullptr;
    if (entry == nullptr) return nullptr;
    oldName = *entry;
    const char* name = SkinName(character, RaceSkinTable(playerId, character));
    if (name == nullptr) name = GetDefaultCharacterPostfix(character);
    *entry = name;
    return entry;
}

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
    return SetCustomCharacterNameMessage(control, paneName, bmgId);
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

kmRuntimeUse(0x807f4e68);
static void LoadRaceResultHook(CtrlRaceResult* result) {
    reinterpret_cast<void (*)(CtrlRaceResult*)>(kmRuntimeAddr(0x807f4e68))(result);

    nw4r::lyt::TextBox* name = static_cast<nw4r::lyt::TextBox*>(result->layout.GetPaneByName("player_name"));
    if (name == nullptr) return;

    name->AllocStringBuffer(32);
    Text::PaneHandler* handler = result->layout.GetTextPaneHandlerByName("player_name");
    if (handler != nullptr) {
        handler->~PaneHandler();
        new (handler) Text::PaneHandler;
        handler->Init(name);
    }
}
kmWritePointer(0x808d3f24, LoadRaceResultHook);

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
        SetCustomCharacterNameMessage(name, bmgId);
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

static CharaName* ConstructCharaName(CharaName* name) {
    return new (name) CharaName;
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

static void CharacterSelectNameLoadHook(ControlLoader* loader, const char* folderName, const char* ctrName, const char* variant, const char** animNames) {
    loader->Load(folderName, ctrName, variant, animNames);
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
    ScnMgr* const* mgrs = ScnMgr::sInstance;
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

static bool IsGameplaySectionLoading() {
    const SectionMgr* mgr = SectionMgr::sInstance;
    if (mgr == nullptr) return false;
    if (mgr->curSection != nullptr && IsGameplaySection(mgr->curSection->sectionId)) return true;
    return mgr->nextSectionId != SECTION_NONE && IsGameplaySection(mgr->nextSectionId);
}

static void ClearRawCache(RawBRRES& cache, bool destroyHeap) {
    if (destroyHeap)
        DestroyHeap(cache.heap);
    else
        cache.heap = nullptr;
    cache.file = nullptr;
    cache.failed = false;
    cache.bound = false;
}

static void ClearRawCaches(bool destroyHeap) {
    if (destroyHeap && IsGameplaySectionLoading()) destroyHeap = false;
    for (u32 table = 0; table < TABLE_COUNT; ++table) {
        for (u32 character = 0; character < CHARACTER_COUNT; ++character) ClearRawCache(rawBRRES[table][character], destroyHeap);
    }
    for (u32 i = 0; i < MII_C_COUNT; ++i) ClearRawCache(looseMiiCBRRES[i], destroyHeap);
}

static void SyncRawCachesToCurrentScene() {
    const GameScene* const scene = GameScene::GetCurrent();
    ClearRawCaches(false);
    rawCacheSceneOwner = scene;
}

static u8 ResolveMenuTable(CharacterId character) {
    if (forceDefaultMenuDriverBRRES) return TABLE_DEFAULT;
    if (ShouldForceDefaultVotingMenuTable()) return TABLE_DEFAULT;
    return SelectedTable(character);
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

static void CopyUpperPostfix(char* dest, u32 destSize, const char* postfix) {
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

static bool BuildLooseVoicePath(const char* postfix, const char* suffix, const char* extension, char* path, u32 pathSize) {
    char upperPostfix[32];
    CopyUpperPostfix(upperPostfix, sizeof(upperPostfix), postfix);
    if (upperPostfix[0] == '\0' || suffix == nullptr || extension == nullptr) return false;
    const int written = snprintf(path, pathSize, "/sound/GRP_VO_%s_%s.%s", upperPostfix, suffix, extension);
    return written > 0 && static_cast<u32>(written) < pathSize;
}

static bool LooseVoiceFileExists(const char* postfix, const char* suffix, const char* extension) {
    char path[0x80];
    if (!BuildLooseVoicePath(postfix, suffix, extension, path, sizeof(path))) return false;
    u32 fileSize = 0;
    return DiscFileSize(path, fileSize);
}

static bool LooseVoiceStemExists(const char* postfix, const char* suffix) {
    return LooseVoiceFileExists(postfix, suffix, "brwsd") || LooseVoiceFileExists(postfix, suffix, "brbnk");
}

static const char* const looseVoiceGroupSuffixes[] = {
    "PC",      "NPC",      "CAN_PC",  "CAN_NPC", "GOL_TOP", "GOL_TOP2", "GOL_TOP3",
    "GOL_GOD", "GOL_GOD2", "GOL_GOD3", "GOL_BAD", "GOL_BAD2", "GOL_BAD3",
};

static const char* const looseVoiceTimeAttackGroupSuffixAliases[] = {
    "GOL_TOP", "GOL_TOP2", "GOL_TOP3", "GOL_BAD", "GOL_BAD2", "GOL_BAD3", "GOL_BAD3",
};

static const char* LooseVoiceSuffixForGroupOffset(u32 offset) {
    if (offset < ARRAY_COUNT(looseVoiceGroupSuffixes)) return looseVoiceGroupSuffixes[offset];
    const u32 taOffset = offset - ARRAY_COUNT(looseVoiceGroupSuffixes);
    if (taOffset < ARRAY_COUNT(looseVoiceTimeAttackGroupSuffixAliases)) return looseVoiceTimeAttackGroupSuffixAliases[taOffset];
    return nullptr;
}

static bool HasLooseCustomVoiceFiles(CharacterId character, u8 table) {
    if (table == TABLE_DEFAULT || table >= TABLE_COUNT || !IsCharacter(character)) return false;
    u8& cached = looseVoiceFiles[table][character];
    if (cached != 0) return cached == 2;

    const CharacterOverride* characterOverride = GetCharacterOverride(character, table);
    bool exists = false;
    if (characterOverride != nullptr && characterOverride->postfix != nullptr) {
        for (u32 i = 0; i < ARRAY_COUNT(looseVoiceGroupSuffixes); ++i) {
            if (LooseVoiceStemExists(characterOverride->postfix, looseVoiceGroupSuffixes[i])) {
                exists = true;
                break;
            }
        }
    }
    cached = exists ? 2 : 1;
    return exists;
}

struct VoiceGroupBase {
    CharacterId character;
    u32 groupId;
};

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

static bool CharacterHasOnlyBaseVoiceGroup(CharacterId character) {
    return character == DRY_BONES || character == KOOPA_TROOPA || character == KING_BOO;
}

static bool FindVoiceGroupBaseCharacter(u32 groupId, CharacterId& character) {
    for (u32 i = 0; i < ARRAY_COUNT(voiceGroupBases); ++i) {
        if (voiceGroupBases[i].groupId != groupId) continue;
        character = voiceGroupBases[i].character;
        return true;
    }
    return false;
}

static bool FindVoiceGroupBase(CharacterId character, u32& groupId) {
    for (u32 i = 0; i < ARRAY_COUNT(voiceGroupBases); ++i) {
        if (voiceGroupBases[i].character != character) continue;
        groupId = voiceGroupBases[i].groupId;
        return true;
    }
    return false;
}

static bool VoiceBaseGroupForTable(CharacterId character, u8 table, u32& groupId) {
    const CharacterOverride* characterOverride = GetCharacterOverride(character, table);
    if (characterOverride != nullptr && characterOverride->voiceBaseGroupId != 0) {
        CharacterId groupCharacter = CHARACTER_NONE;
        if (!FindVoiceGroupBaseCharacter(characterOverride->voiceBaseGroupId, groupCharacter)) return false;
        groupId = characterOverride->voiceBaseGroupId;
        return true;
    }
    return FindVoiceGroupBase(character, groupId);
}

static bool ActorRaceCharacter(const Audio::CharacterActor* actor, CharacterId& character) {
    const Racedata* racedata = Racedata::sInstance;
    if (actor == nullptr || racedata == nullptr) return false;
    const u8 playerId = actor->playerId;
    if (playerId >= racedata->racesScenario.playerCount) return false;
    character = racedata->racesScenario.players[playerId].characterId;
    return IsCharacter(character) && !IsMiiCharacter(character);
}

static bool VoiceBaseGroupForActor(const Audio::CharacterActor* actor, CharacterId& character, u32& groupId, CharacterId& groupCharacter) {
    if (!ActorRaceCharacter(actor, character)) return false;
    const u8 table = RaceSkinTable(actor->playerId, character);
    if (!VoiceBaseGroupForTable(character, table, groupId)) return false;
    if (!FindVoiceGroupBaseCharacter(groupId, groupCharacter)) return false;
    return groupCharacter != character;
}

static CharacterId VoiceBaseCharacterForActor(const Audio::CharacterActor* actor) {
    CharacterId character = CHARACTER_NONE;
    CharacterId groupCharacter = CHARACTER_NONE;
    u32 groupId = 0;
    return VoiceBaseGroupForActor(actor, character, groupId, groupCharacter) ? groupCharacter : CHARACTER_NONE;
}

static Audio::CharacterActor* voiceInitActor;

static Audio::CharacterVoiceActionTable VoiceActionTable(CharacterId character) {
    if (!IsCharacter(character)) return nullptr;
    return Audio::CharacterActor::voiceActionTables[character];
}

static Audio::CharacterVoiceActionTable& CharacterActorVoiceActionTableSlot(Audio::CharacterActor& actor) {
    return *reinterpret_cast<Audio::CharacterVoiceActionTable*>(reinterpret_cast<u8*>(&actor) + 0x134);
}

static u16& CharacterActorCharacterSlot(Audio::CharacterActor& actor) {
    return *reinterpret_cast<u16*>(reinterpret_cast<u8*>(&actor) + 0x9c);
}

static bool ApplyVoiceBaseActionTable(Audio::CharacterActor* actor) {
    const CharacterId voiceCharacter = VoiceBaseCharacterForActor(actor);
    Audio::CharacterVoiceActionTable table = VoiceActionTable(voiceCharacter);
    if (table == nullptr) return false;
    CharacterActorVoiceActionTableSlot(*actor) = table;
    return true;
}

static void InitCharacterVoiceRangesHook(Audio::CharacterActor* actor) {
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

static void* DriverSoundSetForLinkHook(void* manager, CharacterId character, u32 type) {
    ApplyVoiceBaseActionTable(voiceInitActor);
    const CharacterId voiceCharacter = VoiceBaseCharacterForActor(voiceInitActor);
    if (IsCharacter(voiceCharacter)) {
        CharacterId actorCharacter = CHARACTER_NONE;
        if (ActorRaceCharacter(voiceInitActor, actorCharacter) && character == actorCharacter) character = voiceCharacter;
    }
    return static_cast<Audio::DriverSoundManager*>(manager)->GetCharacterVoiceSoundSet(character, type);
}
kmCall(0x80863dd8, DriverSoundSetForLinkHook);

static u32 CharacterVoiceGroupHook(Audio::CharacterActor* actor) {
    ApplyVoiceBaseActionTable(actor);
    CharacterId character = CHARACTER_NONE;
    CharacterId groupCharacter = CHARACTER_NONE;
    u32 groupId = 0;
    if (VoiceBaseGroupForActor(actor, character, groupId, groupCharacter)) {
        if (!actor->isLocal && !CharacterHasOnlyBaseVoiceGroup(groupCharacter)) ++groupId;
        return groupId;
    }
    return actor->GetCharacterGroupId();
}
kmCall(0x80716224, CharacterVoiceGroupHook);

static u32 CharacterCannonVoiceGroupHook(Audio::CharacterActor* actor) {
    ApplyVoiceBaseActionTable(actor);
    CharacterId character = CHARACTER_NONE;
    CharacterId groupCharacter = CHARACTER_NONE;
    u32 groupId = 0;
    if (VoiceBaseGroupForActor(actor, character, groupId, groupCharacter)) {
        if (CharacterHasOnlyBaseVoiceGroup(groupCharacter)) return 0xffffffff;
        return groupId + (actor->isLocal ? 2 : 3);
    }
    return actor->GetCharacterCannonGroupId();
}
kmCall(0x80716280, CharacterCannonVoiceGroupHook);

static u32 CharacterGoalVoiceGroupHook(Audio::CharacterActor* actor, u32 type) {
    ApplyVoiceBaseActionTable(actor);
    CharacterId character = CHARACTER_NONE;
    CharacterId groupCharacter = CHARACTER_NONE;
    u32 groupId = 0;
    if (!VoiceBaseGroupForActor(actor, character, groupId, groupCharacter)) {
        return actor->GetCharacterGoalGroupId(type);
    }
    u16& actorCharacter = CharacterActorCharacterSlot(*actor);
    const u16 oldCharacter = actorCharacter;
    actorCharacter = static_cast<u16>(groupCharacter);
    const u32 group = actor->GetCharacterGoalGroupId(type);
    actorCharacter = oldCharacter;
    return group;
}
kmCall(0x80716254, CharacterGoalVoiceGroupHook);

static bool FindVoiceGroup(u32 groupId, CharacterId& character, u32& offset) {
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

static bool PlayerMatchesVoiceGroupOffset(PlayerType playerType, u32 offset) {
    if (playerType == PLAYER_GHOST || playerType == PLAYER_NONE) return false;
    const bool npcGroup = offset == 1 || offset == 3;
    if (npcGroup) return playerType != PLAYER_REAL_LOCAL;
    return playerType == PLAYER_REAL_LOCAL;
}

const char* GetLooseVoicePostfixForGroup(u32 groupId, const char*& groupSuffix) {
    groupSuffix = nullptr;
    CharacterId groupCharacter = CHARACTER_NONE;
    u32 groupOffset = 0;
    if (!FindVoiceGroup(groupId, groupCharacter, groupOffset)) return nullptr;
    groupSuffix = LooseVoiceSuffixForGroupOffset(groupOffset);
    if (groupSuffix == nullptr) return nullptr;

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
        if (!HasLooseCustomVoiceFiles(character, table)) continue;
        const CharacterOverride* characterOverride = GetCharacterOverride(character, table);
        if (characterOverride != nullptr && characterOverride->postfix != nullptr && LooseVoiceStemExists(characterOverride->postfix, groupSuffix)) {
            return characterOverride->postfix;
        }
    }
    return nullptr;
}

static EGG::Heap* RawParentHeap(GameScene& scene, u32 heapSize) {
    static const u32 PARENT_RESERVE = 0x200000;
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

static bool BindRawBRRES(nw4r::g3d::ResFile& resFile, const char* path) {
    return ModelDirector::BindBRRESImpl(resFile, path, nullptr, 0);
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
    if (!cache.bound) {
        BindRawBRRES(resFile, path);
        cache.bound = true;
    }
    return true;
}

static RawBRRES* RawCacheForModel(const ModelDirector* model) {
    if (model == nullptr || model->rawMdl.data == nullptr) return nullptr;
    for (u32 table = 0; table < TABLE_COUNT; ++table) {
        for (u32 character = 0; character < CHARACTER_COUNT; ++character) {
            if (IsInHeap(rawBRRES[table][character].heap, model->rawMdl.data)) return &rawBRRES[table][character];
        }
    }
    for (u32 i = 0; i < MII_C_COUNT; ++i) {
        if (IsInHeap(looseMiiCBRRES[i].heap, model->rawMdl.data)) return &looseMiiCBRRES[i];
    }
    return nullptr;
}

static bool LoadRawBRRESIntoHeap(void* holder, EGG::ExpHeap* heap, const char* path, u32 fileSize) {
    if (holder == nullptr || heap == nullptr || path == nullptr || fileSize == 0) return false;
    u32 loadedSize = 0;
    void* file = EGG::DvdRipper::LoadToMainRAM(path, nullptr, heap, EGG::DvdRipper::ALLOC_FROM_HEAD, 0, nullptr, &loadedSize);
    if (file == nullptr || loadedSize == 0) return false;
    if ((reinterpret_cast<u32>(file) & 0x1f) != 0) {
        heap->free(file);
        return false;
    }

    nw4r::g3d::ResFile& resFile = *reinterpret_cast<nw4r::g3d::ResFile*>(reinterpret_cast<u8*>(holder) + 4);
    resFile.data = reinterpret_cast<nw4r::g3d::ResFileData*>(file);
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

static bool TryLoadCustomMenuBRRESIntoHeap(void* holder, CharacterId character, EGG::ExpHeap* heap, bool& fileExists) {
    fileExists = false;
    const CharacterId menuCharacter = MenuBRRESCharacter(character);
    const u8 table = ResolveMenuTable(menuCharacter);
    if (table == TABLE_DEFAULT || table >= TABLE_COUNT || !HasSkin(menuCharacter, table)) return false;
    char path[0x60];
    if (!BuildDriverPath(menuCharacter, table, path, sizeof(path))) return false;
    u32 fileSize = 0;
    if (!DiscFileSize(path, fileSize)) {
        rawBRRES[table][menuCharacter].failed = true;
        return false;
    }
    fileExists = true;
    return LoadRawBRRESIntoHeap(holder, heap, path, fileSize);
}

static bool TryLoadLooseMiiCBRRESIntoHeap(void* holder, CharacterId character, EGG::ExpHeap* heap, bool& fileExists) {
    fileExists = false;
    const u8 idx = MiiCIndex(character);
    if (idx >= MII_C_COUNT) return false;
    const char* name = GetDefaultCharacterPostfix(character);
    if (name == nullptr) return false;
    char path[0x60];
    const int written = snprintf(path, sizeof(path), "/Scene/Model/Driver/%s.brres", name);
    if (written <= 0 || static_cast<u32>(written) >= sizeof(path)) return false;
    u32 fileSize = 0;
    if (!DiscFileSize(path, fileSize)) {
        looseMiiCBRRES[idx].failed = true;
        return false;
    }
    fileExists = true;
    return LoadRawBRRESIntoHeap(holder, heap, path, fileSize);
}

static u32 LoadMenuDriverBRRESHook(void* holder, CharacterId character) {
    if (TryLoadCustomMenuBRRES(holder, character) || TryLoadLooseMiiCBRRES(holder, character)) return 1;
    return static_cast<MenuModelBRRESHandle*>(holder)->BindDriverBRRES(character);
}
kmCall(0x80830368, LoadMenuDriverBRRESHook);
kmCall(0x80831234, LoadMenuDriverBRRESHook);
kmCall(0x8083183c, LoadMenuDriverBRRESHook);

static bool LoadMenuDriverBRRESForReload(void* holder, CharacterId character, EGG::ExpHeap* heap) {
    bool fileExists = false;
    if (TryLoadCustomMenuBRRESIntoHeap(holder, character, heap, fileExists)) return true;
    if (fileExists) return false;
    if (TryLoadLooseMiiCBRRESIntoHeap(holder, character, heap, fileExists)) return true;
    if (fileExists) return false;
    return static_cast<MenuModelBRRESHandle*>(holder)->BindDriverBRRES(character);
}

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

static void InitMinimapCharacterHook(CtrlRace2DMapCharacter* control) {
    ApplyLooseMinimapTPL(control);
    control->CtrlRaceBase::InitSelf();
}
kmCall(0x807eb22c, InitMinimapCharacterHook);

static ArchivesHolder* LoadKartArchiveHook(ArchiveMgr* archiveMgr, u8 playerId, KartId kart, CharacterId character, u32 color, u32 type,
                                           EGG::Heap* decompressedHeap, EGG::Heap* archiveHeap) {
    const char* oldName;
    const char** entry = BeginNameSwap(playerId, character, oldName);
    ArchivesHolder* holder = archiveMgr->LoadKartArchive(playerId, kart, character, color, type, decompressedHeap, archiveHeap);
    if (entry != nullptr) *entry = oldName;
    return holder;
}
kmCall(0x805540f4, LoadKartArchiveHook);

struct MenuKartArchiveLoader {
    void* vtable;
    EGG::Heap* mountHeap;
    EGG::Heap* dumpHeap;
    u32 state;
    CharacterId character;
    u32 gamemode;
};

static bool RequestLoadKartArchivesImmediate(ArchiveMgr* archiveMgr, u8 hudSlotId, CharacterId character, u32 gamemode) {
    if (archiveMgr == nullptr || hudSlotId >= LOCAL_PLAYER_COUNT) return false;
    MenuKartArchiveLoader* loader = reinterpret_cast<MenuKartArchiveLoader*>(&archiveMgr->allkartsModelsLoaders[hudSlotId]);
    if (loader->mountHeap == nullptr || loader->state == (0 || 2 || 4)) return false;
    loader->character = character;
    loader->gamemode = gamemode;
    loader->state = 1;
    ArchiveMgr::LoadKartArchiveAsync(hudSlotId);
    return true;
}
kmCall(0x805f5658, RequestLoadKartArchivesImmediate);
kmCall(0x805f5798, RequestLoadKartArchivesImmediate);
kmCall(0x805f592c, RequestLoadKartArchivesImmediate);

static const char* GetMenuDriverBRRESNameHook(u32 character) {
    const CharacterId id = static_cast<CharacterId>(character);
    const u8 table = ResolveMenuTable(id);
    if (IsCharacter(id) && table < TABLE_COUNT && rawBRRES[table][id].failed) return ArchiveMgr::GetKartArchivePostfix(id);
    const char* name = DriverBRRESName(id, table);
    if (name != nullptr) return name;
    return ArchiveMgr::GetKartArchivePostfix(id);
}
kmCall(0x8081e4a0, GetMenuDriverBRRESNameHook);

static void DetachListNodeIfPresent(nw4r::ut::List* list, void* target) {
    if (list == nullptr || target == nullptr) return;
    for (void* node = nw4r::ut::List_GetNext(list, nullptr); node != nullptr;) {
        void* next = nw4r::ut::List_GetNext(list, node);
        if (node == target) nw4r::ut::List_Remove(list, node);
        node = next;
    }
}

static void DetachModelDirectorFromScnMgrs(ModelDirector* model) {
    if (model == nullptr) return;
    ScnMgr* const* mgrs = ScnMgr::sInstance;
    for (u32 i = 0; i < 2; ++i) {
        ScnMgr* mgr = mgrs[i];
        if (mgr == nullptr) continue;
        DetachListNodeIfPresent(&mgr->modelDirectors, model);
        DetachListNodeIfPresent(&mgr->screenSpecificModelDirectors, model);
        DetachListNodeIfPresent(&mgr->hardcodedMatNamesModelDirectors, model);
    }
}

static void RemoveInitializedModelDirector(ModelDirector* model) {
    if (model == nullptr) return;
    if ((model->bitfield & 0x100000) != 0) model->ToggleVisible(false);
    DetachModelDirectorFromScnMgrs(model);
}

static void DestroyModelDirector(ModelDirector* model) {
    if (model == nullptr) return;
    RemoveInitializedModelDirector(model);
    delete model;
}

static void ForgetReloadedMenuDriverModelHeaps() {
    for (u32 i = 0; i < MENU_DRIVER_MODEL_COUNT; ++i) {
        reloadedMenuDriverModelHeaps[i] = nullptr;
        reloadedMenuDriverModels[i] = nullptr;
        reloadedMenuDriverModelHairs[i] = nullptr;
    }
    reloadedMenuDriverModelOwner = nullptr;
}

static void DestroyReloadedMenuDriverModel(u8 idx, ModelDirector** modelSlot) {
    if (idx >= MENU_DRIVER_MODEL_COUNT) return;
    EGG::ExpHeap*& heap = reloadedMenuDriverModelHeaps[idx];
    ModelDirector* model = reloadedMenuDriverModels[idx];
    if (model == nullptr && modelSlot != nullptr) model = *modelSlot;
    ToadetteHair* hair = reloadedMenuDriverModelHairs[idx];

    if (heap == nullptr) {
        DestroyModelDirector(hair);
        DestroyModelDirector(model);
        if (modelSlot != nullptr && *modelSlot == model) *modelSlot = nullptr;
        reloadedMenuDriverModelHairs[idx] = nullptr;
        reloadedMenuDriverModels[idx] = nullptr;
        return;
    }

    if (hair != nullptr && IsInHeap(heap, hair)) DestroyModelDirector(hair);
    if (model != nullptr && IsInHeap(heap, model)) DestroyModelDirector(model);
    if (modelSlot != nullptr && *modelSlot == model) *modelSlot = nullptr;
    reloadedMenuDriverModelHairs[idx] = nullptr;
    reloadedMenuDriverModels[idx] = nullptr;
    DestroyHeap(heap);
}

static void DestroyAllReloadedMenuDriverModels() {
    for (u8 i = 0; i < MENU_DRIVER_MODEL_COUNT; ++i) DestroyReloadedMenuDriverModel(i, nullptr);
}

static void SyncReloadedMenuDriverModelHeaps(MenuDriverModel* models) {
    const GameScene* scene = GameScene::GetCurrent();
    if (reloadedMenuDriverModelSceneOwner != scene) {
        ForgetReloadedMenuDriverModelHeaps();
        reloadedMenuDriverModelSceneOwner = scene;
    }

    if (reloadedMenuDriverModelOwner != models) {
        if (reloadedMenuDriverModelOwner != nullptr) DestroyAllReloadedMenuDriverModels();
        ForgetReloadedMenuDriverModelHeaps();
        reloadedMenuDriverModelOwner = models;
    }
}

static void CleanupReloadedMenuDriverModels() {
    DestroyAllReloadedMenuDriverModels();
    ForgetReloadedMenuDriverModelHeaps();
    reloadedMenuDriverModelSceneOwner = nullptr;
}

static void DestroyMenuModelMgrInstanceHook() {
    MenuModelMgr* mgr = MenuModelMgr::sInstance;
    if (mgr == nullptr) return;
    CleanupReloadedMenuDriverModels();
    MenuModelMgr::sInstance = nullptr;

    const u32 vtable = *reinterpret_cast<u32*>(reinterpret_cast<u8*>(mgr) + 0x10);
    typedef void (*Dtor)(MenuModelMgr*, s32);
    reinterpret_cast<Dtor>(*reinterpret_cast<u32*>(vtable + 0x8))(mgr, 1);
}
kmBranch(0x8059e04c, DestroyMenuModelMgrInstanceHook);

static EGG::Allocator** MenuAllocatorSlot() { return &menuAllocator; }

static EGG::Allocator* CreateScnObjAllocator(EGG::Heap* parent) {
    if (parent == nullptr) return nullptr;
    void* buf = operator new(0x1c, parent, 4);
    if (buf == nullptr) return nullptr;
    return new (buf) EGG::Allocator(parent, 0x20);
}

static EGG::ExpHeap* CreateMenuDriverModelHeap(GameScene& scene) {
    static const u32 MIN_HEAP_SIZE = 0x60000;
    static const u32 PARENT_RESERVE = 0x20000;
    EGG::Heap* parents[] = {scene.structsHeaps.heaps[0], scene.structsHeaps.heaps[1]};
    EGG::Heap* best = nullptr;
    u32 bestSize = 0;
    for (u32 i = 0; i < ARRAY_COUNT(parents); ++i) {
        EGG::Heap* parent = parents[i];
        if (parent == nullptr) continue;
        UnlockHeap(parent);
        const u32 freeSize = parent->getAllocatableSize(0x20);
        if (freeSize > bestSize) {
            best = parent;
            bestSize = freeSize;
        }
    }
    if (best == nullptr || bestSize <= MIN_HEAP_SIZE + PARENT_RESERVE) return nullptr;
    return EGG::ExpHeap::Create(static_cast<int>(bestSize - PARENT_RESERVE), best, 0);
}

static void UnlockMenuModelHeaps(MenuModelMgr& modelMgr) {
    UnlockHeap(modelMgr.otherHeap);
    UnlockHeap(modelMgr.heap);

    GameScene* scene = const_cast<GameScene*>(GameScene::GetCurrent());
    if (scene == nullptr) return;
    for (u32 i = 0; i < ARRAY_COUNT(scene->structsHeaps.heaps); ++i) UnlockHeap(scene->structsHeaps.heaps[i]);
    UnlockHeap(scene->mainMEMHeap);
    UnlockHeap(scene->otherMEMHeap);
}

static ModelDirector** MenuDriverModelDirectorSlot(MenuDriverModel* models, u8 idx) {
    static const u32 MENU_DRIVER_MODEL_SIZE = 0x28;
    static const u32 MENU_MODEL_DIRECTOR_OFFSET = 0x4;
    return reinterpret_cast<ModelDirector**>(reinterpret_cast<u8*>(models) + idx * MENU_DRIVER_MODEL_SIZE + MENU_MODEL_DIRECTOR_OFFSET);
}

static MenuDriverModel* MenuDriverModelSlot(MenuDriverModel* models, u8 idx) {
    static const u32 MENU_DRIVER_MODEL_SIZE = 0x28;
    return reinterpret_cast<MenuDriverModel*>(reinterpret_cast<u8*>(models) + idx * MENU_DRIVER_MODEL_SIZE);
}

static u32& MenuDriverModelStateSlot(MenuDriverModel& model) {
    static const u32 MENU_DRIVER_MODEL_STATE_OFFSET = 0x8;
    return *reinterpret_cast<u32*>(reinterpret_cast<u8*>(&model) + MENU_DRIVER_MODEL_STATE_OFFSET);
}

static ModelTransformator** MenuDriverModelCharSelTransformatorSlot(MenuDriverModel& model) {
    static const u32 MENU_DRIVER_MODEL_CHAR_SEL_TRANSFORMATOR_OFFSET = 0xc;
    return reinterpret_cast<ModelTransformator**>(reinterpret_cast<u8*>(&model) + MENU_DRIVER_MODEL_CHAR_SEL_TRANSFORMATOR_OFFSET);
}

static ModelTransformator** MenuDriverModelOnKartTransformatorSlot(MenuDriverModel& model) {
    static const u32 MENU_DRIVER_MODEL_ON_KART_TRANSFORMATOR_OFFSET = 0x10;
    return reinterpret_cast<ModelTransformator**>(reinterpret_cast<u8*>(&model) + MENU_DRIVER_MODEL_ON_KART_TRANSFORMATOR_OFFSET);
}

static u32& MenuDriverModelIdSlot(MenuDriverModel& model) {
    static const u32 MENU_DRIVER_MODEL_ID_OFFSET = 0x18;
    return *reinterpret_cast<u32*>(reinterpret_cast<u8*>(&model) + MENU_DRIVER_MODEL_ID_OFFSET);
}

static KartId& MenuDriverModelVehicleSlot(MenuDriverModel& model) {
    static const u32 MENU_DRIVER_MODEL_VEHICLE_OFFSET = 0x1c;
    return *reinterpret_cast<KartId*>(reinterpret_cast<u8*>(&model) + MENU_DRIVER_MODEL_VEHICLE_OFFSET);
}

static void ConstructMenuModelBRRESHandle(void* handle) {
    new (handle) MenuModelBRRESHandle;
}

static void DestroyMenuModelBRRESHandle(void* handle) {
    static_cast<MenuModelBRRESHandle*>(handle)->~MenuModelBRRESHandle();
}

static bool LoadMenuDriverModel(void* handle, ModelDirector* model, CharacterId character) {
    return static_cast<MenuModelBRRESHandle*>(handle)->LoadDriverModel(*model, character);
}

static ToadetteHair* LoadMenuDriverToadetteHair(void* handle, EGG::ExpHeap* heap, ModelDirector* model) {
    if (heap == nullptr || model == nullptr) return nullptr;
    return new (heap, 4) ToadetteHair(static_cast<MenuModelBRRESHandle*>(handle)->menuModelBRRES, model, 1);
}

static bool IsLoadedMenuDriverModelReady(const ModelDirector* model) {
    return model != nullptr && (model->bitfield & 0x100000) != 0 && model->scnMdlEx[0] != nullptr &&
           model->scnMdlEx[0]->scnObj != nullptr && model->scnMdlEx[1] != nullptr && model->scnMdlEx[1]->scnObj != nullptr;
}

static bool LoadReloadedMenuDriverModel(GameScene& scene, ScnMgr& scnMgr, CharacterId character, ModelDirector*& newModel,
                                        ToadetteHair*& newHair, EGG::ExpHeap*& newHeap) {
    newModel = nullptr;
    newHair = nullptr;
    newHeap = nullptr;

    EGG::ExpHeap* modelHeap = CreateMenuDriverModelHeap(scene);
    if (modelHeap == nullptr) return false;

    u32 handle[2];
    ConstructMenuModelBRRESHandle(handle);
    const bool brresLoaded = LoadMenuDriverBRRESForReload(handle, character, modelHeap);
    if (!brresLoaded) {
        DestroyMenuModelBRRESHandle(handle);
        DestroyHeap(modelHeap);
        return false;
    }

    EGG::Allocator* freshAllocator = CreateScnObjAllocator(modelHeap);
    if (freshAllocator == nullptr) {
        DestroyMenuModelBRRESHandle(handle);
        DestroyHeap(modelHeap);
        return false;
    }

    EGG::Allocator** menuAllocSlot = MenuAllocatorSlot();
    EGG::Heap* savedHeap = scnMgr.curHeap;
    EGG::Allocator* savedAllocator = scnMgr.curAllocator;
    EGG::Allocator* savedMenuAllocator = *menuAllocSlot;
    scnMgr.curHeap = modelHeap;
    scnMgr.curAllocator = freshAllocator;
    *menuAllocSlot = freshAllocator;

    ModelDirector* model = new (modelHeap, 4) ModelDirector(2, 0);
    const bool loaded = model != nullptr && LoadMenuDriverModel(handle, model, character) && IsLoadedMenuDriverModelReady(model);
    ToadetteHair* hair = nullptr;
    bool hairLoaded = true;
    if (loaded && character == TOADETTE) {
        hair = LoadMenuDriverToadetteHair(handle, modelHeap, model);
        hairLoaded = IsLoadedMenuDriverModelReady(hair);
    }

    scnMgr.curHeap = savedHeap;
    scnMgr.curAllocator = savedAllocator;
    *menuAllocSlot = savedMenuAllocator;
    DestroyMenuModelBRRESHandle(handle);

    if (!loaded || !hairLoaded) {
        DestroyModelDirector(hair);
        DestroyModelDirector(model);
        DestroyHeap(modelHeap);
        return false;
    }

    UnlockHeap(modelHeap);
    modelHeap->adjust();
    newModel = model;
    newHair = hair;
    newHeap = modelHeap;
    return true;
}

static bool LoadDefaultReloadedMenuDriverModel(GameScene& scene, ScnMgr& scnMgr, CharacterId character, ModelDirector*& newModel,
                                               ToadetteHair*& newHair, EGG::ExpHeap*& newHeap) {
    forceDefaultMenuDriverBRRES = true;
    const bool loaded = LoadReloadedMenuDriverModel(scene, scnMgr, character, newModel, newHair, newHeap);
    forceDefaultMenuDriverBRRES = false;
    return loaded;
}

static void DestroyOldMenuDriverModelForReload(u8 idx, ModelDirector** modelSlot, ModelDirector* oldModel, ToadetteHair** hairSlot,
                                               ToadetteHair* oldHair) {
    if (reloadedMenuDriverModelHeaps[idx] != nullptr) {
        DestroyReloadedMenuDriverModel(idx, modelSlot);
        if (hairSlot != nullptr && *hairSlot == oldHair) *hairSlot = nullptr;
        return;
    }

    RawBRRES* rawCache = RawCacheForModel(oldModel);
    DestroyModelDirector(oldHair);
    DestroyModelDirector(oldModel);
    if (modelSlot != nullptr && *modelSlot == oldModel) *modelSlot = nullptr;
    if (hairSlot != nullptr && *hairSlot == oldHair) *hairSlot = nullptr;
    if (rawCache != nullptr) ClearRawCache(*rawCache, true);
}

static void ResetReloadedMenuDriverModel(MenuDriverModel& menuModel, CharacterId character) {
    *MenuDriverModelCharSelTransformatorSlot(menuModel) = menuModel.model->modelTransformator;
    *MenuDriverModelOnKartTransformatorSlot(menuModel) = nullptr;
    MenuDriverModelIdSlot(menuModel) = static_cast<u32>(character);
}

static bool ReloadMenuDriverModel(MenuDriverModelMgr& driverMgr, CharacterId character) {
    if (character < 0 || character >= MENU_DRIVER_MODEL_COUNT || driverMgr.models == nullptr) return false;
    const u8 idx = static_cast<u8>(character);
    MenuDriverModel* menuModel = MenuDriverModelSlot(driverMgr.models, idx);
    ModelDirector** modelSlot = MenuDriverModelDirectorSlot(driverMgr.models, idx);
    ToadetteHair** hairSlot = character == TOADETTE ? &driverMgr.bangs : static_cast<ToadetteHair**>(nullptr);

    GameScene* scene = const_cast<GameScene*>(GameScene::GetCurrent());
    if (scene == nullptr) return false;
    SyncReloadedMenuDriverModelHeaps(driverMgr.models);

    ScnMgr* const* mgrs = ScnMgr::sInstance;
    ScnMgr* scnMgr = mgrs[0];
    if (scnMgr == nullptr) return false;

    ModelDirector* oldModel = *modelSlot;
    ToadetteHair* oldHair = hairSlot != nullptr ? *hairSlot : static_cast<ToadetteHair*>(nullptr);
    bool oldModelDestroyed = false;

    ModelDirector* newModel = nullptr;
    ToadetteHair* newHair = nullptr;
    EGG::ExpHeap* newHeap = nullptr;
    if (!LoadReloadedMenuDriverModel(*scene, *scnMgr, character, newModel, newHair, newHeap)) {
        if (reloadedMenuDriverModelHeaps[idx] == nullptr) return false;
        DestroyOldMenuDriverModelForReload(idx, modelSlot, oldModel, hairSlot, oldHair);
        oldModelDestroyed = true;
        if (!LoadReloadedMenuDriverModel(*scene, *scnMgr, character, newModel, newHair, newHeap) &&
            !LoadDefaultReloadedMenuDriverModel(*scene, *scnMgr, character, newModel, newHair, newHeap)) {
            return false;
        }
    }

    if (!oldModelDestroyed) DestroyOldMenuDriverModelForReload(idx, modelSlot, oldModel, hairSlot, oldHair);

    *modelSlot = newModel;
    if (hairSlot != nullptr) *hairSlot = newHair;
    reloadedMenuDriverModelHeaps[idx] = newHeap;
    reloadedMenuDriverModels[idx] = newModel;
    reloadedMenuDriverModelHairs[idx] = newHair;
    ResetReloadedMenuDriverModel(*menuModel, character);
    menuModel->Init();
    return true;
}

static void ReinitMenuDriverModelMgr(u8 hud, CharacterId character) {
    MenuModelMgr* modelMgr = MenuModelMgr::sInstance;
    if (modelMgr == nullptr || !modelMgr->isActive || modelMgr->driverModels == nullptr) return;
    if (hud >= LOCAL_PLAYER_COUNT || hud >= modelMgr->playerCount) return;

    UnlockMenuModelHeaps(*modelMgr);
    if (!ReloadMenuDriverModel(*modelMgr->driverModels, character)) return;
    modelMgr->driverModels->SetPlayerCharacter(hud, character);

    SectionMgr* sectionMgr = SectionMgr::sInstance;
    if (sectionMgr == nullptr || sectionMgr->curSection == nullptr) return;
    Pages::CharacterSelect* page = sectionMgr->curSection->Get<Pages::CharacterSelect>();
    if (page != nullptr && page->models != nullptr) page->models[hud].RequestModel(character);
}

static KartId SelectedMenuKartForHud(u8 hud) {
    const SectionMgr* sectionMgr = SectionMgr::sInstance;
    if (sectionMgr != nullptr && sectionMgr->sectionParams != nullptr && hud < sectionMgr->sectionParams->localPlayerCount) {
        return sectionMgr->sectionParams->karts[hud];
    }
    const Racedata* racedata = Racedata::sInstance;
    if (racedata != nullptr && hud < racedata->menusScenario.localPlayerCount) {
        const u8 playerId = racedata->menusScenario.settings.hudPlayerIds[hud];
        if (playerId < ONLINE_PLAYER_COUNT) return racedata->menusScenario.players[playerId].kartId;
    }
    return STANDARD_KART_S;
}

static void ApplyVoteRandomMessageBoxKartState() {
    SectionMgr* sectionMgr = SectionMgr::sInstance;
    if (sectionMgr == nullptr || sectionMgr->curSection == nullptr) {
        voteRandomMessageBoxKartStateApplied = false;
        return;
    }
    const Page* topPage = sectionMgr->curSection->GetTopLayerPage();
    if (!IsVotingSection(sectionMgr->curSection->sectionId) || topPage == nullptr || topPage->pageId != PAGE_VOTERANDOM_MESSAGE_BOX) {
        voteRandomMessageBoxKartStateApplied = false;
        return;
    }
    if (voteRandomMessageBoxKartStateApplied) return;

    MenuModelMgr* modelMgr = MenuModelMgr::sInstance;
    if (modelMgr == nullptr || !modelMgr->isActive || modelMgr->driverModels == nullptr) return;
    const u8 count = SectionPlayerCount(sectionMgr);
    for (u8 hud = 0; hud < count && hud < modelMgr->playerCount; ++hud) {
        MenuDriverModel* model = modelMgr->driverModels->players[hud].playerModel;
        if (model == nullptr) continue;
        MenuDriverModelVehicleSlot(*model) = SelectedMenuKartForHud(hud);
        modelMgr->driverModels->PrepareDriverOnKartAnms(hud);
        model->SwitchState(hud, static_cast<MenuDriverModel::State>(3));
    }
    voteRandomMessageBoxKartStateApplied = true;
}

void RestoreVotingMenuDriverModels() {
    const SectionId section = CurrentSectionId();
    if (!IsVotingSection(section)) return;

    votingMenuTableSection = section;
    votingMenuTablesRestored = true;
    ApplySelectedNames();
    RefreshLocalOnlineCustomCharacterFlags();

    const SectionMgr* sectionMgr = SectionMgr::sInstance;
    if (sectionMgr == nullptr || sectionMgr->sectionParams == nullptr) return;
    const u8 count = SectionPlayerCount(sectionMgr);
    for (u8 hud = 0; hud < count; ++hud) {
        ReinitMenuDriverModelMgr(hud, sectionMgr->sectionParams->characters[hud]);
    }
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
    if (changed) ReinitMenuDriverModelMgr(0, character);
    return changed;
}

static void ResetCustomCharacterMenuState() {
    if (!IsVotingSection(CurrentSectionId())) {
        votingMenuTableSection = SECTION_NONE;
        votingMenuTablesRestored = false;
    }
    if (!IsOnlineRoom(RKNet::Controller::sInstance)) ResetOnlineCustomCharacterFlags();
    ResetOfflineCpuSkinTablesForSection();
    SyncRawCachesToCurrentScene();
    CacheHoveredFromSection();
    ApplySelectedNames();
}
static SectionLoadHook ResetCustomCharacterMenuStateHook(ResetCustomCharacterMenuState);

static void CharacterSelectHoverHook(Pages::CharacterSelect* page, CtrlMenuCharacterSelect::ButtonDriver* button, u32 buttonId, u8 hud) {
    if (hud < LOCAL_PLAYER_COUNT) hoveredCharacters[hud] = static_cast<CharacterId>(buttonId);
    page->OnButtonDriverSelect(button, buttonId, hud);
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
    if ((pressed & prevButton) != 0) {
        const CharacterId character = PreviewCharacter(0);
        if (CycleSkin(character, -1)) {
            ReinitMenuDriverModelMgr(0, character);
            Audio::RSARPlayer::PlaySoundById(SOUND_ID_LEFT_ARROW_PRESS, 0, 0);
            return true;
        }
    }
    if ((pressed & nextButton) != 0) {
        const CharacterId character = PreviewCharacter(0);
        if (CycleSkin(character, 1)) {
            ReinitMenuDriverModelMgr(0, character);
            Audio::RSARPlayer::PlaySoundById(SOUND_ID_RIGHT_ARROW_PRESS, 0, 0);
            return true;
        }
    }
    return false;
}

static void MenuSceneSectionUpdateHook(SectionMgr* mgr) {
    UpdateHintPanes();
    ProcessSkinInput();
    UpdateCurrentCharacterSelectAuthorText(0);
    ApplyVoteRandomMessageBoxKartState();
    mgr->MenuUpdate();
    ApplyVoteRandomMessageBoxKartState();
}
kmCall(0x805552e8, MenuSceneSectionUpdateHook);
kmCall(0x80553b30, MenuSceneSectionUpdateHook);

}  // namespace CustomCharacters
}  // namespace Pulsar
