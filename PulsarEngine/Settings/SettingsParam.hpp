#ifndef _SETTINGS_PARAMS_
#define _SETTINGS_PARAMS_

#include <kamek.hpp>
#include <PulsarSystem.hpp>
#include <Config.hpp>

namespace Pulsar {
namespace UI {
class SettingsPanel;
}
namespace Settings {

class Params {
   public:
    static const int pulsarPageCount = 0;
    static const int userPageCount = 12;
    static const int pageCount = pulsarPageCount + userPageCount;

    static const int maxRadioCount = 8;  // per page, due to space
    static const int maxScrollerCount = 5;  // per page, due to space

    // Pulsar and User
    static u8 radioCount[pageCount];
    static u8 scrollerCount[pageCount];
    static u8 buttonsPerPagePerRow[pageCount][maxRadioCount];
    static u8 optionsPerPagePerScroller[pageCount][maxScrollerCount];
};

// Contains all the settings.
enum Type {
};

// If you want to add settings to your packs, they go in this enum, and GetUserSettingValue should be used to obtain the value of a given setting
enum UserType {
    SETTINGSTYPE_RACE1,
    SETTINGSTYPE_RACE2,
    SETTINGSTYPE_MENU,
    SETTINGSTYPE_ONLINE,
    SETTINGSTYPE_SOUND,
    SETTINGSTYPE_BATTLE,
    SETTINGSTYPE_FROOM1,
    SETTINGSTYPE_FROOM2,
    SETTINGSTYPE_OTT,
    SETTINGSTYPE_KO,
    SETTINGSTYPE_EXTENDEDTEAMS,
    SETTINGSTYPE_MISC,

};

}  // namespace Settings

// --------------------------------------------------------------------------------------
// Page order (User pages):
//  1) Race 1
//  2) Race 2
//  3) Menu
//  4) Online
//  5) Sound
//  6) Battle
//  7) Friend Room 1
//  8) Friend Room 2
//  9) OTT
// 10) KO
// 11) Extended Teams
// 12) Misc
// --------------------------------------------------------------------------------------

// 1) Race 1
enum Race1Settings {
    RADIO_TRANSMISSION = 0,
    RADIO_BRAKEDRIFT = 1,
    RADIO_HARDAI = 2,
    RADIO_INPUTDISPLAY = 3,
    RADIO_MIIHEADS = 4,
    RADIO_SPEEDOMETER = 5,
};

// 2) Race 2
enum Race2Settings {
    RADIO_BATTLEGLITCH = 0,
    RADIO_FPS = 1,
    RADIO_BLOOM = 2,
    RADIO_FOV = 3,
    RADIO_NAMETAG = 4,
};

// 3) Menu
enum MenuSettings {
    RADIO_FASTMENUS = 0,
    RADIO_LAYOUT = 1,
    SCROLL_BOOT = 0 + 8
};

// 4) Online
enum OnlineSettings {
    RADIO_ONLINERANDOMBUTTON = 0,
    RADIO_PREDICTIONREMOVAL = 1,
};

// 5) Sound
enum SoundSettings {
    RADIO_CTMUSIC = 0,
    RADIO_MUSIC = 1,
    RADIO_MUSICSPEEDUP = 2,
};

// 6) Battle
enum BattleSettings {
    RADIO_BATTLETEAMS = 0,
    RADIO_BATTLEELIMINATION = 1,
};

// 7) Friend Room 1
enum FriendRoom1Settings {
    RADIO_FROOMCC = 0,
    RADIO_KARTSELECT = 1,
    RADIO_CHARSELECT = 2,
    SCROLLER_RACECOUNT = 0 + 8,
    SCROLLER_ITEMMODE = 1 + 8,
    SCROLLER_TRACKSELECTION = 2 + 8,

};

// 8) Friend Room 2
enum FriendRoom2Settings {
    RADIO_HOSTWINS = 0,
    RADIO_ALLOWMIIHEADS = 1,
    RADIO_ITEMBOXRESPAWN = 2,
    RADIO_THUNDERCLOUD = 3,
    RADIO_FORCETRANSMISSION = 4,
};

// 9) OTT
enum OTTSettings {
    RADIO_OTTONLINE = 0,
    RADIO_OTTOFFLINE = 1,
    RADIO_OTTALLOWCHANGECOMBO = 2,
    RADIO_OTTALLOWUMTS = 3,
    RADIO_OTTMUTEPTANDPLAYERS = 4
};

// 10) KO
enum KOSettings {
    RADIO_KOENABLED = 0,
    RADIO_KOFINAL = 1,
    SCROLLER_KOPERRACE = 0 + 8,
    SCROLLER_RACESPERKO = 1 + 8
};

// 11) Extended Teams
enum ExtendedTeamsSettings {
    RADIO_EXTENDEDTEAMSENABLED = 0,
    RADIO_EXTENDEDTEAMSLINE = 1
};

// 12) Misc
enum MiscSettings {
    SCROLLER_CUSTOMCHARACTER = 0 + 8,
    SCROLLER_LANGUAGE = 1 + 8,
    SCROLLER_WWMODE = 2 + 8,
};

// --------------------------------------------------------------------------------------
// Value enums grouped by page order
// --------------------------------------------------------------------------------------

// 1) Race 1 values
enum Transmission {
    TRANSMISSION_DEFAULT,
    TRANSMISSION_INSIDEALL,
    TRANSMISSION_INSIDEBIKE,
    TRANSMISSION_OUTSIDE
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
    INPUTDISPLAY_ENABLED
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

// 4) Online values
enum RandomButton {
    RANDOMBUTTON_ENABLED,
    RANDOMBUTTON_DISABLED
};

enum MenuSettingPredictionRemoval {
    PREDICTIONREMOVAL_DISABLED = 0x0,
    PREDICTIONREMOVAL_ENABLED = 0x1
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

// 6) Battle values
enum BattleTeams {
    BATTLE_TEAMS_DISABLED,
    BATTLE_TEAMS_ENABLED
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
    OTTSETTING_UMTS_DISABLED,
    OTTSETTING_UMTS_ENABLED
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
    KOSETTING_RACESPERKO_4
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

// 12) Misc values
enum MenuSettingCustomCharacter {
    CUSTOMCHARACTER_DISABLED = 0x0,
    CUSTOMCHARACTER_ENABLED = 0x1
};

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