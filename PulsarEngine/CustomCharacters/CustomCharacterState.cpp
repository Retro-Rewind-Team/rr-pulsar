#include <CustomCharacters/CustomCharacters.hpp>
#include <Gamemodes/MissionMode/MissionMode.hpp>
#include <Gamemodes/MissionMode/MissionMusic.hpp>

namespace Pulsar {
namespace CustomCharacters {

// Process-wide skin, cache, and menu state shared by the custom character hooks.
u8 selectedTable[CHARACTER_COUNT];
u8 onlineCharacterTables[ONLINE_PLAYER_COUNT];
u8 offlineCpuCharacterTables[ONLINE_PLAYER_COUNT];
const char* defaultNames[CHARACTER_COUNT];
bool cachedDefaultNames;
char customPostfixes[CHARACTER_COUNT][TABLE_COUNT][16];
u8 customSkinExists[CHARACTER_COUNT][TABLE_COUNT];
CharacterId hoveredCharacters[LOCAL_PLAYER_COUNT] = {MARIO, MARIO, MARIO, MARIO};
RawBRRES rawBRRES[TABLE_COUNT][CHARACTER_COUNT];
RawBRRES looseMiiCBRRES[MII_C_COUNT];
RawTPL looseMinimapTPL[TABLE_COUNT][CHARACTER_COUNT];
const GameScene* rawCacheSceneOwner;
u32 offlineCpuSkinSignature;
u8 offlineCpuSkinRaceNumber;
bool offlineCpuSkinTablesValid;
u16 heldToggleButtons[LOCAL_PLAYER_COUNT];
u32 authorNameControlStorage[LOCAL_PLAYER_COUNT][AUTHOR_NAME_CONTROL_WORDS];
bool authorNameControlConstructed[LOCAL_PLAYER_COUNT];
bool authorNameControlLoaded[LOCAL_PLAYER_COUNT];
bool loadingAuthorNameControl;
CharaName* authorTextControl;
u32 authorTextValue;
CharaName* characterNameTextControl[LOCAL_PLAYER_COUNT];
u32 characterNameTextValue[LOCAL_PLAYER_COUNT];
bool characterNameTextOverridden[LOCAL_PLAYER_COUNT];
SectionId votingMenuTableSection = SECTION_NONE;
bool votingMenuTablesRestored;
bool voteRandomMessageBoxKartStateApplied;
EGG::ExpHeap* reloadedMenuDriverModelHeaps[MENU_DRIVER_MODEL_COUNT];
ModelDirector* reloadedMenuDriverModels[MENU_DRIVER_MODEL_COUNT];
ToadetteHair* reloadedMenuDriverModelHairs[MENU_DRIVER_MODEL_COUNT];
const GameScene* reloadedMenuDriverModelSceneOwner;
MenuDriverModel* reloadedMenuDriverModelOwner;
bool forceDefaultMenuDriverBRRES;

LooseVoiceInfo looseVoiceInfo[TABLE_COUNT][CHARACTER_COUNT];

// Clamp game-reported local player counts to the UI arrays this feature owns.
u8 MinLocalPlayers(u32 count) {
    return count > LOCAL_PLAYER_COUNT ? LOCAL_PLAYER_COUNT : static_cast<u8>(count);
}

bool IsCharacter(CharacterId character) {
    return character >= 0 && character < CHARACTER_COUNT;
}

bool IsMiiCharacter(CharacterId character) {
    return (character >= MII_S_A_MALE && character <= MII_L_C_FEMALE) || character == MII_M || character == MII_S || character == MII_L;
}

const char** CharacterNameEntry(CharacterId character) {
    if (!IsCharacter(character)) return nullptr;
    return characterNames + character;
}

// Cache vanilla postfix pointers before selected skins temporarily replace them.
void CacheDefaults() {
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

CharacterId StateCharacter(CharacterId character) {
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

const char* CustomPostfixBase(CharacterId character) {
    const CharacterId stateCharacter = StateCharacter(character);
    if (!IsCharacter(stateCharacter)) return nullptr;
    return GetDefaultCharacterPostfix(stateCharacter);
}

// Generated postfixes use the vanilla name plus a stable table suffix.
const char* GeneratedCustomPostfix(CharacterId character, u8 table) {
    if (!IsCharacter(character) || table == TABLE_DEFAULT || table > CUSTOM_TABLE_LIMIT) return nullptr;
    char* postfix = customPostfixes[character][table];
    if (postfix[0] != '\0') return postfix;
    const char* base = CustomPostfixBase(character);
    if (base == nullptr) return nullptr;
    const int written = snprintf(postfix, sizeof(customPostfixes[character][table]), "%s-%u", base, table);
    if (written <= 0 || static_cast<u32>(written) >= sizeof(customPostfixes[character][table])) {
        postfix[0] = '\0';
        return nullptr;
    }
    return postfix;
}

// Driver BRRES existence is cached because menu hooks query it repeatedly.
bool CustomDriverFileExists(CharacterId character, u8 table) {
    if (!IsCharacter(character) || table == TABLE_DEFAULT || table > CUSTOM_TABLE_LIMIT) return false;
    u8& cached = customSkinExists[character][table];
    if (cached != 0) return cached == 2;
    const char* postfix = GeneratedCustomPostfix(character, table);
    bool exists = false;
    if (postfix != nullptr) {
        char path[0x60];
        const int written = snprintf(path, sizeof(path), "/Scene/Model/Driver/%s.brres", postfix);
        if (written > 0 && static_cast<u32>(written) < sizeof(path)) {
            u32 fileSize = 0;
            exists = DiscFileSize(path, fileSize);
        }
    }
    cached = exists ? 2 : 1;
    return exists;
}

// Peach/Daisy/Rosalina menu BRRES files use biker ids for their standing models.
CharacterId MenuBRRESCharacter(CharacterId character) {
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

bool HasSkin(CharacterId character, u8 table) {
    return table == TABLE_DEFAULT || CustomDriverFileExists(character, table);
}

u8 NormalizeTable(CharacterId character, u8 table) {
    return HasSkin(character, table) ? table : TABLE_DEFAULT;
}

u32 SkinBmgId(u32 start, CharacterId character, u8 table) {
    if (!IsCharacter(character) || table == TABLE_DEFAULT || table > CUSTOM_TABLE_LIMIT || !HasSkin(character, table)) return 0;
    return (static_cast<u32>(character) << 16) | start | table;
}

// Custom BMG ids encode the character in the high half and table in the low half.
u32 SkinNameBmgId(CharacterId character, u8 table) {
    return SkinBmgId(CUSTOM_CHARACTER_NAME_BMG_START, character, table);
}

u32 SkinAuthorBmgId(CharacterId character, u8 table) {
    return SkinBmgId(CUSTOM_CHARACTER_AUTHOR_BMG_START, character, table);
}

u32 DefaultNameBmgIdForSkinBmgId(u32 bmgId) {
    CharacterId character = static_cast<CharacterId>(bmgId >> 16);
    if (!IsCharacter(character) || IsMiiCharacter(character)) return 0;
    const CharacterId displayCharacter = StateCharacter(character);
    if (IsCharacter(displayCharacter) && !IsMiiCharacter(displayCharacter)) character = displayCharacter;
    return GetCharacterBMGId(character, false);
}

BmgTextState GetBmgTextState(const BMGHolder& holder, u32 bmgId) {
    if (holder.bmgFile == nullptr) return BMG_TEXT_MISSING;
    const s32 msgId = holder.GetMsgId(static_cast<s32>(bmgId));
    if (msgId < 0) return BMG_TEXT_MISSING;
    const wchar_t* text = holder.GetMsgByMsgId(msgId);
    if (text == nullptr) return BMG_TEXT_MISSING;
    return text[0] == L'\0' ? BMG_TEXT_BLANK : BMG_TEXT_NONBLANK;
}

BmgTextState GetCustomCharacterBmgTextState(const LayoutUIControl& control, u32 bmgId) {
    const System* system = System::sInstance;
    if (system != nullptr) {
        BmgTextState state = GetBmgTextState(system->GetBMG(), bmgId);
        if (state != BMG_TEXT_MISSING) return state;
        state = GetBmgTextState(system->GetBMGCT(), bmgId);
        if (state != BMG_TEXT_MISSING) return state;
        state = GetBmgTextState(system->GetBMGBT(), bmgId);
        if (state != BMG_TEXT_MISSING) return state;
    }
    BmgTextState state = GetBmgTextState(control.curFileBmgs, bmgId);
    if (state != BMG_TEXT_MISSING) return state;
    return GetBmgTextState(control.commonBmgs, bmgId);
}

// Missing custom name text falls back to the vanilla character name.
u32 ResolveCustomCharacterNameBmgId(const LayoutUIControl& control, u32 bmgId) {
    if (GetCustomCharacterBmgTextState(control, bmgId) == BMG_TEXT_NONBLANK) return bmgId;
    const u32 defaultBmgId = DefaultNameBmgIdForSkinBmgId(bmgId);
    return defaultBmgId != 0 ? defaultBmgId : bmgId;
}

bool SetCustomCharacterNameMessage(LayoutUIControl& control, const char* paneName, u32 bmgId) {
    control.SetTextBoxMessage(paneName, ResolveCustomCharacterNameBmgId(control, bmgId), nullptr);
    return true;
}

bool SetCustomCharacterNameMessage(LayoutUIControl& control, u32 bmgId) {
    control.SetMessage(ResolveCustomCharacterNameBmgId(control, bmgId), nullptr);
    return true;
}

bool SetCustomCharacterAuthorMessage(LayoutUIControl& control, u32 bmgId) {
    if (GetCustomCharacterBmgTextState(control, bmgId) != BMG_TEXT_NONBLANK) return false;
    control.SetMessage(bmgId, nullptr);
    return true;
}

const char* DefaultMenuBRRESName(CharacterId character) {
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

const char* DriverBRRESName(CharacterId character, u8 table) {
    const char* generatedPostfix = GeneratedCustomPostfix(character, table);
    if (generatedPostfix != nullptr) return generatedPostfix;
    const char* menuName = DefaultMenuBRRESName(character);
    if (menuName != nullptr) return menuName;
    return GetDefaultCharacterPostfix(character);
}

// Apply selected skin names through the vanilla global character name table.
void ApplyName(CharacterId character, u8 table) {
    const char** entry = CharacterNameEntry(character);
    const char* name = GeneratedCustomPostfix(character, table);
    if (name == nullptr) name = GetDefaultCharacterPostfix(character);
    if (entry != nullptr && name != nullptr) *entry = name;
}

u8 SectionPlayerCount(const SectionMgr* mgr) {
    if (mgr == nullptr || mgr->sectionParams == nullptr) return 0;
    return MinLocalPlayers(mgr->sectionParams->localPlayerCount);
}

u8 RacePlayerCount(const RacedataScenario& scenario) {
    return MinLocalPlayers(scenario.localPlayerCount);
}

bool IsVotingSection(SectionId section) {
    return section == SECTION_P1_WIFI_VS_VOTING || section == SECTION_P1_WIFI_BATTLE_VOTING || section == SECTION_P2_WIFI_VS_VOTING ||
           section == SECTION_P2_WIFI_BATTLE_VOTING ||
           (section >= SECTION_P1_WIFI_FROOM_VS_VOTING && section <= SECTION_P2_WIFI_FROOM_COIN_VOTING);
}

SectionId CurrentSectionId() {
    const SectionMgr* mgr = SectionMgr::sInstance;
    if (mgr == nullptr || mgr->curSection == nullptr) return SECTION_NONE;
    return mgr->curSection->sectionId;
}

// Voting menus briefly need vanilla models until custom ones are restored.
bool ShouldForceDefaultVotingMenuTable() {
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

bool IsLocalMultiplayer() {
    const SectionMgr* sectionMgr = SectionMgr::sInstance;
    if (SectionPlayerCount(sectionMgr) > 1) return true;
    const Racedata* racedata = Racedata::sInstance;
    if (racedata != nullptr) {
        if (RacePlayerCount(racedata->racesScenario) > 1) return true;
        if (RacePlayerCount(racedata->menusScenario) > 1) return true;
    }
    return GetLocalPlayerCount() > 1;
}

u8 SelectedTable(CharacterId character) {
    const CharacterId stateCharacter = StateCharacter(character);
    if (!IsCharacter(stateCharacter)) return TABLE_DEFAULT;
    return NormalizeTable(character, selectedTable[stateCharacter]);
}

void ApplySelectedNames() {
    CacheDefaults();
    for (u32 i = 0; i < CHARACTER_COUNT; ++i) {
        ApplyName(static_cast<CharacterId>(i), SelectedTable(static_cast<CharacterId>(i)));
    }
}

bool AnyCustomSkin() {
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

bool DisplayOnlineSkins() {
    return Settings::Mgr::Get().GetUserSettingValue(Settings::SETTINGSTYPE_ONLINE, RADIO_DISPLAYCUSTOMSKINS) == DISPLAYCUSTOMSKINS_ENABLED;
}

void ResetOfflineCpuSkinTables() {
    offlineCpuSkinTablesValid = false;
    offlineCpuSkinRaceNumber = 0;
    offlineCpuSkinSignature = 0;
    for (u8 i = 0; i < ONLINE_PLAYER_COUNT; ++i) offlineCpuCharacterTables[i] = TABLE_DEFAULT;
}

void CompactOfflineCpuSkinTable(u8 targetPlayerId, u8 sourcePlayerId) {
    if (targetPlayerId >= ONLINE_PLAYER_COUNT || sourcePlayerId >= ONLINE_PLAYER_COUNT) return;
    offlineCpuCharacterTables[targetPlayerId] = offlineCpuCharacterTables[sourcePlayerId];
}

void ClearCustomCharacterFileCaches() {
    memset(customSkinExists, 0, sizeof(customSkinExists));
    memset(looseVoiceInfo, 0, sizeof(looseVoiceInfo));
}

void ResetAllCharacterTablesToDefault() {
    for (u32 i = 0; i < CHARACTER_COUNT; ++i) selectedTable[i] = TABLE_DEFAULT;
    ResetOnlineCustomCharacterFlags();
    ResetOfflineCpuSkinTables();
    ClearCustomCharacterFileCaches();
    ApplySelectedNames();
}

void ResetCharacterTablesOnLooseArchiveOverrideChange() {
    static bool initialized;
    static u8 lastValue;
    if (!Settings::Mgr::IsCreated()) return;

    const u8 value = Settings::Mgr::Get().GetUserSettingValue(Settings::SETTINGSTYPE_MISC, RADIO_LOOSEARCHIVEOVERRIDES);
    if (!initialized) {
        initialized = true;
        lastValue = value;
        return;
    }
    if (lastValue == value) return;

    lastValue = value;
    ResetAllCharacterTablesToDefault();
}
Settings::Hook ResetCharacterTablesOnLooseArchiveOverrideChangeHook(ResetCharacterTablesOnLooseArchiveOverrideChange);

// Offline CPU skins reset when leaving race flows that preserve CPU assignments.
bool IsOfflineCpuSkinResetSection(SectionId section) {
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

void ResetOfflineCpuSkinTablesForSection() {
    const SectionMgr* mgr = SectionMgr::sInstance;
    if (mgr == nullptr) return;
    if ((mgr->curSection != nullptr && IsOfflineCpuSkinResetSection(mgr->curSection->sectionId)) || IsOfflineCpuSkinResetSection(mgr->nextSectionId)) {
        ResetOfflineCpuSkinTables();
    }
}

bool IsLocalRacePlayer(u8 playerId) {
    const Racedata* racedata = Racedata::sInstance;
    if (racedata == nullptr) return false;
    const RacedataScenario& scenario = racedata->racesScenario;
    const u8 localCount = RacePlayerCount(scenario);
    for (u8 hud = 0; hud < localCount; ++hud) {
        if (racedata->GetPlayerIdOfLocalPlayer(hud) == playerId) return true;
    }
    return false;
}

// Online rooms advertise only the local player's selected table.
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

bool SetSelectedTable(CharacterId character, u8 table) {
    const CharacterId stateCharacter = StateCharacter(character);
    if (!IsCharacter(stateCharacter) || !HasSkin(character, table)) return false;
    if (selectedTable[stateCharacter] == table) return false;
    selectedTable[stateCharacter] = table;
    ApplySelectedNames();
    RefreshLocalOnlineCustomCharacterFlags();
    return true;
}

bool CycleSkin(CharacterId character, int step) {
    if (!IsCharacter(StateCharacter(character))) return false;
    u8 table = SelectedTable(character);
    for (u8 i = 1; i < TABLE_COUNT; ++i) {
        table = step < 0 ? (table == 0 ? TABLE_COUNT - 1 : table - 1) : (table + 1 >= TABLE_COUNT ? TABLE_DEFAULT : table + 1);
        if (HasSkin(character, table) && SetSelectedTable(character, table)) {
            return true;
        }
    }
    return false;
}

// Pick a stable random CPU skin table for the current offline race series.
u8 OfflineCpuSkinTable(const RacedataScenario& scenario, u8 playerId, CharacterId character) {
    u32 signature = 0x4343534b;
    signature = signature * 33 + static_cast<u32>(scenario.settings.gamemode);
    signature = signature * 33 + static_cast<u32>(scenario.settings.modeFlags);
    signature = signature * 33 + scenario.playerCount;
    for (u8 i = 0; i < scenario.playerCount && i < ONLINE_PLAYER_COUNT; ++i) {
        signature = signature * 33 + static_cast<u32>(scenario.players[i].characterId);
        signature = signature * 33 + static_cast<u32>(scenario.players[i].playerType);
    }

    const bool isOfflineKO = System::sInstance->IsContext(PULSAR_MODE_KO) && !IsOnlineRoom(RKNet::Controller::sInstance);
    const bool sameSeries = offlineCpuSkinTablesValid && (offlineCpuSkinSignature == signature || isOfflineKO);
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

// Race skin selection chooses local, remote, or stable offline CPU tables.
u8 RaceSkinTable(u8 playerId, CharacterId character) {
    const Racedata* racedata = Racedata::sInstance;
    if (racedata != nullptr) {
        const RacedataScenario* missionScenario = &racedata->racesScenario;
        if (!MissionMode::IsMissionScenario(*missionScenario)) {
            missionScenario = &racedata->menusScenario;
            if (!MissionMode::IsMissionScenario(*missionScenario)) missionScenario = nullptr;
        }
        if (missionScenario != nullptr && playerId < missionScenario->playerCount) {
            const u8 configuredTable = MissionMode::GetMissionCharacterTable(playerId);
            if (configuredTable != MissionMode::MISSION_CHARACTER_TABLE_UNSET)
                return NormalizeTable(character, configuredTable);
        }
    }
    if (racedata != nullptr && playerId < racedata->racesScenario.playerCount) {
        const RacedataScenario& scenario = racedata->racesScenario;
        const GameMode mode = scenario.settings.gamemode;
        const bool offlineCpuSkinMode = mode == MODE_GRAND_PRIX || mode == MODE_VS_RACE || mode == MODE_BATTLE;
        const bool offlineCpu = scenario.players[playerId].playerType == PLAYER_CPU;
        if (offlineCpuSkinMode && offlineCpu && !IsOnlineRoom(RKNet::Controller::sInstance) && DisplayOnlineSkins() && !IsLocalMultiplayer()) {
            return OfflineCpuSkinTable(scenario, playerId, character);
        }
    }

    if (IsLocalMultiplayer()) return IsLocalRacePlayer(playerId) ? SelectedTable(character) : TABLE_DEFAULT;
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    if (IsOnlineRoom(controller) && DisplayOnlineSkins()) {
        return IsLocalRacePlayer(playerId) ? SelectedTable(character) : NormalizeTable(character, onlineCharacterTables[playerId]);
    }
    return IsLocalRacePlayer(playerId) ? SelectedTable(character) : TABLE_DEFAULT;
}


}  // namespace CustomCharacters
}  // namespace Pulsar
