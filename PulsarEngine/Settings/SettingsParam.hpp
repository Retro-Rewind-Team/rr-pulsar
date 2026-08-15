#ifndef _SETTINGS_PARAMS_
#define _SETTINGS_PARAMS_

#include <kamek.hpp>

namespace Pulsar {
namespace Settings {

// Each SettingId is the setting title's BMG ID.
enum SettingId {
    SETTING_BRAKEDRIFT = 0x63000,
    SETTING_HARDAI = 0x63001,
    SETTING_INPUTDISPLAY = 0x63002,
    SETTING_MIIHEADS = 0x63003,
    SETTING_SPEEDOMETER = 0x63004,
    SETTING_BATTLEGLITCH = 0x64000,
    SETTING_FPS = 0x64001,
    SETTING_BLOOM = 0x64002,
    SETTING_FOV = 0x64003,
    SETTING_NAMETAG = 0x64004,
    SETTING_FASTMENUS = 0x65000,
    SETTING_LAYOUT = 0x65001,
    SETTING_BOOT = 0x65A00,
    SETTING_HUDCOLOR = 0x65A01,
    SETTING_ONLINERANDOMBUTTON = 0x66000,
    SETTING_PREDICTIONREMOVAL = 0x66001,
    SETTING_STREAMERMODE = 0x66002,
    SETTING_INFINITEMATCHMAKINGTIMEOUT = 0x66003,
    SETTING_DISPLAYCUSTOMSKINS = 0x66004,
    SETTING_CTMUSIC = 0x67000,
    SETTING_MUSIC = 0x67001,
    SETTING_MUSICSPEEDUP = 0x67002,
    SETTING_SPECIALITEMRECEIVE = 0x67003,
    SETTING_BATTLETEAMS = 0x68000,
    SETTING_BATTLEELIMINATION = 0x68001,
    SETTING_FROOMCC = 0x69000,
    SETTING_KARTSELECT = 0x69001,
    SETTING_CHARSELECT = 0x69002,
    SETTING_RANKINGS = 0x69003,
    SETTING_VR = 0x69004,
    SETTING_RACECOUNT = 0x69A00,
    SETTING_ITEMMODE = 0x69A01,
    SETTING_TRACKSELECTION = 0x69A02,
    SETTING_HOSTWINS = 0x6A000,
    SETTING_ALLOWMIIHEADS = 0x6A001,
    SETTING_ITEMBOXRESPAWN = 0x6A002,
    SETTING_THUNDERCLOUD = 0x6A003,
    SETTING_FORCETRANSMISSION = 0x6A004,
    SETTING_ALLITEMSCANLAND = 0x6A005,
    SETTING_VANILLAMODE = 0x6A006,
    SETTING_OTTONLINE = 0x6B000,
    SETTING_OTTOFFLINE = 0x6B001,
    SETTING_OTTALLOWCHANGECOMBO = 0x6B002,
    SETTING_OTTALLOWUMTS = 0x6B003,
    SETTING_OTTMUTEPTANDPLAYERS = 0x6B004,
    SETTING_KOENABLED = 0x6C000,
    SETTING_KOFINAL = 0x6C001,
    SETTING_KO1V1FINALE = 0x6C002,
    SETTING_KOPERRACE = 0x6CA00,
    SETTING_RACESPERKO = 0x6CA01,
    SETTING_KOELIMTHRESHOLD = 0x6CA02,
    SETTING_KOELIMCHANGE = 0x6CA03,
    SETTING_KOROYALEENABLED = 0x70000,
    SETTING_KOROYALEBALLOONS = 0x70A00,
    SETTING_KOROYALELAPMULTIPLIER = 0x70A01,
    SETTING_EXTENDEDTEAMSENABLED = 0x6D000,
    SETTING_EXTENDEDTEAMSLINE = 0x6D001,
    SETTING_EXTENDEDTEAMSPLAYERS = 0x6DA00,
    SETTING_LOOSEARCHIVEOVERRIDES = 0x6E000,
    SETTING_LANGUAGE = 0x6EA00,
    SETTING_WWMODE = 0x5AD000,
    SETTING_COUNT = 61
};

enum SettingsPageId {
    SETTINGS_PAGE_RACE1,
    SETTINGS_PAGE_RACE2,
    SETTINGS_PAGE_MENU,
    SETTINGS_PAGE_SOUND,
    SETTINGS_PAGE_ONLINE,
    SETTINGS_PAGE_BATTLE,
    SETTINGS_PAGE_FROOM1,
    SETTINGS_PAGE_FROOM2,
    SETTINGS_PAGE_OTT_OFFLINE,
    SETTINGS_PAGE_OTT_ONLINE,
    SETTINGS_PAGE_KO,
    SETTINGS_PAGE_ROYALE,
    SETTINGS_PAGE_EXTENDED_TEAMS,
    SETTINGS_PAGE_MISC,
    SETTINGS_PAGE_ITEMS,
    SETTINGS_PAGE_RACE1_OFFLINE,
    SETTINGS_PAGE_RACE2_OFFLINE,
    SETTINGS_PAGE_COUNT
};

enum SettingsContext {
    SETTINGS_CONTEXT_OFFLINE,
    SETTINGS_CONTEXT_ONLINE,
    SETTINGS_CONTEXT_VOTING,
    SETTINGS_CONTEXT_COUNT
};

struct SettingDef {
    SettingId id;
    u8 optionCount;
};

struct SettingsPageDef {
    u32 nameBmg;
    const SettingId *radioSettings;
    u8 radioCount;
    const SettingId *scrollerSettings;
    u8 scrollerCount;
    bool isSpecial;
};

struct SettingsContextDef {
    const SettingsPageId *pages;
    u8 pageCount;
};

class Params {
public:
    static const int maxRadioCount = 8;
    static const int maxScrollerCount = 5;
    static const int maxContextPageCount = 13;

    static const SettingDef settingDefs[SETTING_COUNT];
    static const SettingsPageDef pageDefs[SETTINGS_PAGE_COUNT];
    static const SettingsContextDef contextDefs[SETTINGS_CONTEXT_COUNT];

    static inline bool IsValidSettingId(SettingId id) {
        return GetSettingIndex(id) < SETTING_COUNT;
    }

    static inline u32 GetSettingIndex(SettingId id) {
        for (u32 i = 0; i < SETTING_COUNT; ++i) {
            if (settingDefs[i].id == id) return i;
        }
        return SETTING_COUNT;
    }

    static inline const SettingDef &GetSettingDef(SettingId id) {
        return settingDefs[GetSettingIndex(id)];
    }

    static inline u32 GetOptionBmg(SettingId id, u32 option) {
        const u32 bmg = static_cast<u32>(id);
        return (bmg & ~0xF) + ((bmg & 0xF) + 1) * 0x10 + option;
    }

    static inline u32 GetDescriptionBmg(SettingId id, u32 option) {
        const u32 bmg = static_cast<u32>(id);
        return (bmg & ~0xF) + ((bmg & 0xF) + 1) * 0x100 + option;
    }

    static inline const SettingsPageDef &GetPageDef(SettingsPageId id) {
        return pageDefs[id];
    }

    static inline const SettingsContextDef &GetContextDef(SettingsContext context) {
        return contextDefs[context];
    }

    static u32 BuildHostRulePages(SettingsPageId *dest, bool isBattle, bool isKO,
                                  bool isOTT, bool isRoyale, bool isExtendedTeams);
};

}  // namespace Settings

enum LooseArchiveOverridesSetting {
    LOOSEARCHIVEOVERRIDES_ENABLED = 0,
    LOOSEARCHIVEOVERRIDES_DISABLED = 1
};

// 1) Race 1 values
enum Transmission {
    TRANSMISSION_DEFAULT,
    TRANSMISSION_INSIDE,
    TRANSMISSION_INSIDEBIKE = 2,
    TRANSMISSION_OUTSIDE = 3
};

enum BrakeDrift {
    BRAKEDRIFT_DISABLED,
    BRAKEDRIFT_ENABLED
};

enum HardAI {
    HARDAI_DISABLED,
    HARDAI_ENABLED
};

enum InputDisplay {
    INPUTDISPLAY_DISABLED,
    INPUTDISPLAY_ENABLED,
    INPUTDISPLAY_CHUK
};

enum RaceSettingMII {
    MII_DISABLED = 0x0,
    MII_ENABLED = 0x1
};

enum RaceSettingSOM {
    SOM_DISABLED,
    SOM_DIGITS_0,
    SOM_DIGITS_1,
    SOM_DIGITS_2,
};

// 2) Race 2 values
enum FPS {
    FPS_DEFAULT,
    FPS_HALF
};

enum RaceSettingBLOOM {
    BLOOM_DISABLED = 0x0,
    BLOOM_ENABLED = 0x1
};

enum FOVChange {
    FOV_CHANGE_DEFAULT,
    FOV_CHANGE_16_9,
    FOV_CHANGE_4_3
};

enum RaceSettingBATTLE {
    BATTLE_GLITCH_DISABLED = 0x0,
    BATTLE_GLITCH_ENABLED = 0x1
};

enum RaceSettingNAMETAG {
    NAMETAG_REGULAR = 0x0,
    NAMETAG_MII = 0x1
};

// 3) Menu values
enum MenuSettingFastMenus {
    FASTMENUS_DISABLED = 0x0,
    FASTMENUS_ENABLED = 0x1
};

enum MenuSettingLayout {
    LAYOUT_DEFAULT = 0x0,
    LAYOUT_ALPHABETICAL = 0x1
};

enum MenuSettingBoot {
    BOOT_DISABLED,
    BOOT_L1,
    BOOT_L2,
    BOOT_L3,
    BOOT_L4
};

enum MenuSettingHUDColor {
    HUDCOLOR_WHITE,
    HUDCOLOR_BLACK,
    HUDCOLOR_RED,
    HUDCOLOR_ORANGE,
    HUDCOLOR_YELLOW,
    HUDCOLOR_GREEN,
    HUDCOLOR_BLUE,
    HUDCOLOR_PURPLE,
    HUDCOLOR_PINK,
    HUDCOLOR_MAGENTA,
    HUDCOLOR_CYAN,
    HUDCOLOR_TEAL
};

// 4) Online values
enum RandomButton {
    RANDOMBUTTON_ENABLED,
    RANDOMBUTTON_DISABLED
};

enum MenuSettingPredictionRemoval {
    PREDICTIONREMOVAL_DISABLED = 0x0,
    PREDICTIONREMOVAL_ENABLED = 0x1
};

enum MenuSettingStreamerMode {
    STREAMERMODE_DISABLED = 0x0,
    STREAMERMODE_ENABLED = 0x1
};

enum MatchmakingTimeoutSetting {
    MATCHMAKINGTIMEOUT_DEFAULT = 0x0,
    MATCHMAKINGTIMEOUT_INFINITE = 0x1
};

enum DisplayCustomSkins {
    DISPLAYCUSTOMSKINS_ENABLED = 0x0,
    DISPLAYCUSTOMSKINS_DISABLED = 0x1
};

// 5) Sound values
enum CTMusic {
    CTMUSIC_ENABLED,
    CTMUSIC_DISABLED
};

enum MenuSettingMusic {
    MUSIC_DEFAULT = 0x0,
    MUSIC_DISABLE_ALL = 0x1,
    MUSIC_DISABLE_RACE = 0x2
};

enum RaceSettingSPEEDUP {
    SPEEDUP_DISABLED = 0x0,
    SPEEDUP_ENABLED = 0x1
};

enum SpecialItemReceive {
    SPECIALITEMRECEIVE_ENABLED = 0x0,
    SPECIALITEMRECEIVE_DISABLED = 0x1
};

// 6) Battle values
enum BattleFFA {
    BATTLE_FFA_DISABLED,
    BATTLE_FFA_ENABLED
};

enum HostSettingElimination {
    ELIMINATION_DISABLED = 0x0,
    ELIMINATION_ENABLED = 0x1
};

// 7) Friend Room 1 values
enum HostSettingHostCC {
    HOSTCC_NORMAL,
    HOSTCC_150,
    HOSTCC_100,
    HOSTCC_500
};

enum KartRestriction {
    KART_DEFAULTSELECTION,
    KART_KARTONLY,
    KART_BIKEONLY
};

enum CharacterRestriction {
    CHAR_DEFAULTSELECTION,
    CHAR_LIGHTONLY,
    CHAR_MEDIUMONLY,
    CHAR_HEAVYONLY
};

enum RankingSetting {
    RANKINGS_DISABLED,
    RANKINGS_ENABLED
};

enum VRSetting {
    VR_DISABLED,
    VR_ENABLED
};

enum HostSettingGPRACES {
    GP_RACES_4,
    GP_RACES_8,
    GP_RACES_12,
    GP_RACES_24,
    GP_RACES_32,
    GP_RACES_64,
    GP_RACES_2
};

enum ItemMode {
    GAMEMODE_DEFAULT,
    GAMEMODE_RANDOM,
    GAMEMODE_BLAST,
    GAMEMODE_ITEMRAIN,
    GAMEMODE_ITEMSTORM,
    GAMEMODE_NONE
};

enum TrackSelection {
    TRACKSELECTION_RETROS,
    TRACKSELECTION_CTS,
    TRACKSELECTION_ALL,
    TRACKSELECTION_REGS,
};

// 8) Friend Room 2 values
enum HostSettingHAW {
    HOSTWINS_DISABLED,
    HOSTWINS_ENABLED
};

enum HostSettingMiiHeads {
    ALLOW_MIIHEADS_ENABLED,
    ALLOW_MIIHEADS_DISABLED
};

enum ItemBoxRespawn {
    ITEMBOX_DEFAULTRESPAWN,
    ITEMBOX_FASTRESPAWN
};

enum ThunderCloud {
    THUNDERCLOUD_MEGA,
    THUNDERCLOUD_NORMAL
};

enum ForceTransmission {
    FORCE_TRANSMISSION_DEFAULT,
    FORCE_TRANSMISSION_INSIDE,
    FORCE_TRANSMISSION_OUTSIDE,
    FORCE_TRANSMISSION_VANILLA
};

enum AllItemsCanLandSetting {
    ALLITEMSCANLAND_DISABLED,
    ALLITEMSCANLAND_ENABLED
};

enum VanillaModeSetting {
    VANILLAMODE_DISABLED,
    VANILLAMODE_ENABLED
};

// 9) OTT values
enum OTTSettingOnline {
    OTTSETTING_ONLINE_DISABLED,
    OTTSETTING_ONLINE_NORMAL,
    OTTSETTING_ONLINE_FEATHER
};

enum OTTSettingOffline {
    OTTSETTING_OFFLINE_DISABLED,
    OTTSETTING_OFFLINE_NORMAL,
    OTTSETTING_OFFLINE_FEATHER
};

enum OTTSettingCombo {
    OTTSETTING_COMBO_DISABLED,
    OTTSETTING_COMBO_ENABLED
};

enum OTTSettingUMTs {
    OTTSETTING_UMTS_ENABLED,
    OTTSETTING_UMTS_DISABLED
};

// 10) KO values
enum KOSettingEnabled {
    KOSETTING_DISABLED,
    KOSETTING_ENABLED,
    KOSETTING_LAPBASED
};

enum KOSettingFINAL {
    KOSETTING_FINAL_DISABLED,
    KOSETTING_FINAL_ALWAYS
};

enum KOSettingKOPerRace {
    KOSETTING_KOPERRACE_1,
    KOSETTING_KOPERRACE_2,
    KOSETTING_KOPERRACE_3,
    KOSETTING_KOPERRACE_4
};

enum KOSettingRacesPerKO {
    KOSETTING_RACESPERKO_1,
    KOSETTING_RACESPERKO_2,
    KOSETTING_RACESPERKO_3,
    KOSETTING_RACESPERKO_4,
    KOSETTING_RACESPERKO_5
};

enum KOSetting1v1Finale {
    KOSETTING_1V1FINALE_SINGLE,
    KOSETTING_1V1FINALE_MULTIPLE
};

enum KOSettingElimThreshold {
    KOSETTING_ELIMTHRESHOLD_DISABLED,
    KOSETTING_ELIMTHRESHOLD_3,
    KOSETTING_ELIMTHRESHOLD_4,
    KOSETTING_ELIMTHRESHOLD_5,
    KOSETTING_ELIMTHRESHOLD_6,
    KOSETTING_ELIMTHRESHOLD_7,
    KOSETTING_ELIMTHRESHOLD_8
};

enum KOSettingElimChange {
    KOSETTING_ELIMCHANGE_1,
    KOSETTING_ELIMCHANGE_2,
    KOSETTING_ELIMCHANGE_3,
    KOSETTING_ELIMCHANGE_4
};

// 11) KO Royale values
enum KORoyaleSettingEnabled {
    KOROYALESETTING_DISABLED,
    KOROYALESETTING_ENABLED
};

enum KORoyaleSettingBalloons {
    KOROYALESETTING_BALLOONS_1,
    KOROYALESETTING_BALLOONS_2,
    KOROYALESETTING_BALLOONS_3,
    KOROYALESETTING_BALLOONS_4
};

enum KORoyaleSettingLapMultiplier {
    KOROYALESETTING_LAPS_DEFAULT,
    KOROYALESETTING_LAPS_1_5X,
    KOROYALESETTING_LAPS_2_0X
};

// 11) Extended Teams values
enum ExtendedTeams {
    EXTENDEDTEAMS_DISABLED,
    EXTENDEDTEAMS_ENABLED
};

enum ExtendedTeamsLine {
    EXTENDEDTEAMS_LINE_TEAM,
    EXTENDEDTEAMS_LINE_REGION
};

enum ExtendedTeamsPlayers {
    EXTENDEDTEAMS_PLAYERS_2,
    EXTENDEDTEAMS_PLAYERS_3,
    EXTENDEDTEAMS_PLAYERS_4,
    EXTENDEDTEAMS_PLAYERS_6
};

// 12) Misc values
enum Language {
    LANGUAGE_ENGLISH,
    LANGUAGE_JAPANESE,
    LANGUAGE_FRENCH,
    LANGUAGE_GERMAN,
    LANGUAGE_DUTCH,
    LANGUAGE_SPANISHUS,
    LANGUAGE_SPANISHEU,
    LANGUAGE_FINNISH,
    LANGUAGE_ITALIAN,
    LANGUAGE_KOREAN,
    LANGUAGE_RUSSIAN,
    LANGUAGE_TURKISH,
    LANGUAGE_CZECH,
};

enum WWMode {
    WWMODE_DEFAULT,
    WWMODE_OTT,
    WWMODE_200
};

}  // namespace Pulsar

#endif
