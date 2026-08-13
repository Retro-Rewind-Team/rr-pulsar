#include <Settings/SettingsParam.hpp>

namespace Pulsar {
namespace Settings {

#define ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))

const SettingDef Params::settingDefs[SETTING_COUNT] = {
    {SETTING_BRAKEDRIFT, 2},
    {SETTING_HARDAI, 2},
    {SETTING_INPUTDISPLAY, 3},
    {SETTING_MIIHEADS, 2},
    {SETTING_SPEEDOMETER, 4},
    {SETTING_BATTLEGLITCH, 2},
    {SETTING_FPS, 2},
    {SETTING_BLOOM, 2},
    {SETTING_FOV, 3},
    {SETTING_NAMETAG, 2},
    {SETTING_FASTMENUS, 2},
    {SETTING_LAYOUT, 2},
    {SETTING_BOOT, 5},
    {SETTING_HUDCOLOR, 12},
    {SETTING_ONLINERANDOMBUTTON, 2},
    {SETTING_PREDICTIONREMOVAL, 2},
    {SETTING_STREAMERMODE, 2},
    {SETTING_INFINITEMATCHMAKINGTIMEOUT, 2},
    {SETTING_DISPLAYCUSTOMSKINS, 2},
    {SETTING_CTMUSIC, 2},
    {SETTING_MUSIC, 3},
    {SETTING_MUSICSPEEDUP, 2},
    {SETTING_SPECIALITEMRECEIVE, 2},
    {SETTING_BATTLETEAMS, 2},
    {SETTING_BATTLEELIMINATION, 2},
    {SETTING_FROOMCC, 4},
    {SETTING_KARTSELECT, 3},
    {SETTING_CHARSELECT, 4},
    {SETTING_RANKINGS, 2},
    {SETTING_VR, 2},
    {SETTING_RACECOUNT, 7},
    {SETTING_ITEMMODE, 6},
    {SETTING_TRACKSELECTION, 4},
    {SETTING_HOSTWINS, 2},
    {SETTING_ALLOWMIIHEADS, 2},
    {SETTING_ITEMBOXRESPAWN, 2},
    {SETTING_THUNDERCLOUD, 2},
    {SETTING_FORCETRANSMISSION, 4},
    {SETTING_ALLITEMSCANLAND, 2},
    {SETTING_VANILLAMODE, 2},
    {SETTING_OTTONLINE, 2},
    {SETTING_OTTOFFLINE, 2},
    {SETTING_OTTALLOWCHANGECOMBO, 2},
    {SETTING_OTTALLOWUMTS, 2},
    {SETTING_OTTMUTEPTANDPLAYERS, 2},
    {SETTING_KOENABLED, 3},
    {SETTING_KOFINAL, 2},
    {SETTING_KO1V1FINALE, 2},
    {SETTING_KOPERRACE, 4},
    {SETTING_RACESPERKO, 5},
    {SETTING_KOELIMTHRESHOLD, 7},
    {SETTING_KOELIMCHANGE, 4},
    {SETTING_KOROYALEENABLED, 2},
    {SETTING_KOROYALEBALLOONS, 4},
    {SETTING_KOROYALELAPMULTIPLIER, 3},
    {SETTING_EXTENDEDTEAMSENABLED, 2},
    {SETTING_EXTENDEDTEAMSLINE, 2},
    {SETTING_EXTENDEDTEAMSPLAYERS, 4},
    {SETTING_LOOSEARCHIVEOVERRIDES, 2},
    {SETTING_LANGUAGE, 13}};

static const SettingId race1Radios[] = {
    SETTING_BRAKEDRIFT,
    SETTING_HARDAI,
    SETTING_INPUTDISPLAY,
    SETTING_MIIHEADS,
    SETTING_SPEEDOMETER};

static const SettingId race2Radios[] = {
    SETTING_BATTLEGLITCH,
    SETTING_FPS,
    SETTING_BLOOM,
    SETTING_FOV,
    SETTING_NAMETAG};

static const SettingId race1OfflineRadios[] = {
    SETTING_BRAKEDRIFT,
    SETTING_HARDAI,
    SETTING_INPUTDISPLAY,
    SETTING_MIIHEADS,
    SETTING_SPEEDOMETER,
    SETTING_KARTSELECT,
    SETTING_CHARSELECT};

static const SettingId race1OfflineScrollers[] = {
    SETTING_TRACKSELECTION};

static const SettingId race2OfflineRadios[] = {
    SETTING_BATTLEGLITCH,
    SETTING_FPS,
    SETTING_BLOOM,
    SETTING_FOV,
    SETTING_NAMETAG,
    SETTING_ITEMBOXRESPAWN,
    SETTING_THUNDERCLOUD,
    SETTING_ALLITEMSCANLAND};

static const SettingId race2OfflineScrollers[] = {
    SETTING_ITEMMODE};

static const SettingId menuRadios[] = {
    SETTING_FASTMENUS,
    SETTING_LAYOUT};

static const SettingId menuScrollers[] = {
    SETTING_BOOT,
    SETTING_HUDCOLOR};

static const SettingId soundRadios[] = {
    SETTING_CTMUSIC,
    SETTING_MUSIC,
    SETTING_MUSICSPEEDUP,
    SETTING_SPECIALITEMRECEIVE};

static const SettingId onlineRadios[] = {
    SETTING_ONLINERANDOMBUTTON,
    SETTING_PREDICTIONREMOVAL,
    SETTING_STREAMERMODE,
    SETTING_INFINITEMATCHMAKINGTIMEOUT,
    SETTING_DISPLAYCUSTOMSKINS};

static const SettingId battleRadios[] = {
    SETTING_BATTLETEAMS,
    SETTING_BATTLEELIMINATION};

static const SettingId froom1Radios[] = {
    SETTING_FROOMCC,
    SETTING_KARTSELECT,
    SETTING_CHARSELECT,
    SETTING_RANKINGS,
    SETTING_VR};

static const SettingId froom1Scrollers[] = {
    SETTING_RACECOUNT,
    SETTING_ITEMMODE,
    SETTING_TRACKSELECTION};

static const SettingId froom2Radios[] = {
    SETTING_HOSTWINS,
    SETTING_ALLOWMIIHEADS,
    SETTING_ITEMBOXRESPAWN,
    SETTING_THUNDERCLOUD,
    SETTING_FORCETRANSMISSION,
    SETTING_ALLITEMSCANLAND,
    SETTING_VANILLAMODE};

static const SettingId ottOfflineRadios[] = {
    SETTING_OTTOFFLINE,
    SETTING_OTTALLOWCHANGECOMBO,
    SETTING_OTTALLOWUMTS,
    SETTING_OTTMUTEPTANDPLAYERS};

static const SettingId ottOnlineRadios[] = {
    SETTING_OTTONLINE,
    SETTING_OTTALLOWCHANGECOMBO,
    SETTING_OTTALLOWUMTS,
    SETTING_OTTMUTEPTANDPLAYERS};

static const SettingId koRadios[] = {
    SETTING_KOENABLED,
    SETTING_KOFINAL,
    SETTING_KO1V1FINALE};

static const SettingId koScrollers[] = {
    SETTING_KOPERRACE,
    SETTING_RACESPERKO,
    SETTING_KOELIMTHRESHOLD,
    SETTING_KOELIMCHANGE};

static const SettingId royaleRadios[] = {
    SETTING_KOROYALEENABLED,
    SETTING_ALLITEMSCANLAND};

static const SettingId royaleScrollers[] = {
    SETTING_KOROYALEBALLOONS,
    SETTING_KOROYALELAPMULTIPLIER};

static const SettingId extendedTeamsRadios[] = {
    SETTING_EXTENDEDTEAMSENABLED,
    SETTING_EXTENDEDTEAMSLINE};

static const SettingId extendedTeamsScrollers[] = {
    SETTING_EXTENDEDTEAMSPLAYERS};

static const SettingId miscRadios[] = {
    SETTING_LOOSEARCHIVEOVERRIDES};

static const SettingId miscScrollers[] = {
    SETTING_LANGUAGE};

#define PAGE_DEF(bmg, radios, scrollers) \
    {bmg, radios, ARRAY_COUNT(radios), scrollers, ARRAY_COUNT(scrollers), false}
#define PAGE_DEF_RADIOS_ONLY(bmg, radios) \
    {bmg, radios, ARRAY_COUNT(radios), 0, 0, false}

const SettingsPageDef Params::pageDefs[SETTINGS_PAGE_COUNT] = {
    PAGE_DEF_RADIOS_ONLY(0x62F01, race1Radios),
    PAGE_DEF_RADIOS_ONLY(0x62F02, race2Radios),
    PAGE_DEF(0x62F03, menuRadios, menuScrollers),
    PAGE_DEF_RADIOS_ONLY(0x62F05, soundRadios),
    PAGE_DEF_RADIOS_ONLY(0x62F04, onlineRadios),
    PAGE_DEF_RADIOS_ONLY(0x62F06, battleRadios),
    PAGE_DEF(0x62F07, froom1Radios, froom1Scrollers),
    PAGE_DEF_RADIOS_ONLY(0x62F08, froom2Radios),
    PAGE_DEF_RADIOS_ONLY(0x62F09, ottOfflineRadios),
    PAGE_DEF_RADIOS_ONLY(0x62F09, ottOnlineRadios),
    PAGE_DEF(0x62F0A, koRadios, koScrollers),
    PAGE_DEF(0x62F0E, royaleRadios, royaleScrollers),
    PAGE_DEF(0x62F0B, extendedTeamsRadios, extendedTeamsScrollers),
    PAGE_DEF(0x62F0C, miscRadios, miscScrollers),
    {0x62F0D, 0, 0, 0, 0, true},
    {0x62F01,
     race1OfflineRadios, ARRAY_COUNT(race1OfflineRadios), race1OfflineScrollers,
     ARRAY_COUNT(race1OfflineScrollers), false},
    {0x62F02,
     race2OfflineRadios, ARRAY_COUNT(race2OfflineRadios), race2OfflineScrollers,
     ARRAY_COUNT(race2OfflineScrollers), false},
};

static const SettingsPageId offlinePages[] = {
    SETTINGS_PAGE_RACE1_OFFLINE, SETTINGS_PAGE_RACE2_OFFLINE, SETTINGS_PAGE_MENU, SETTINGS_PAGE_SOUND,
    SETTINGS_PAGE_KO, SETTINGS_PAGE_BATTLE, SETTINGS_PAGE_OTT_OFFLINE, SETTINGS_PAGE_ROYALE,
    SETTINGS_PAGE_EXTENDED_TEAMS, SETTINGS_PAGE_MISC, SETTINGS_PAGE_ITEMS};
static const SettingsPageId onlinePages[] = {
    SETTINGS_PAGE_RACE1, SETTINGS_PAGE_RACE2, SETTINGS_PAGE_MENU, SETTINGS_PAGE_SOUND,
    SETTINGS_PAGE_ONLINE, SETTINGS_PAGE_BATTLE, SETTINGS_PAGE_FROOM1, SETTINGS_PAGE_FROOM2,
    SETTINGS_PAGE_OTT_ONLINE, SETTINGS_PAGE_KO, SETTINGS_PAGE_ROYALE,
    SETTINGS_PAGE_EXTENDED_TEAMS, SETTINGS_PAGE_ITEMS};
static const SettingsPageId votingPages[] = {
    SETTINGS_PAGE_RACE1, SETTINGS_PAGE_RACE2, SETTINGS_PAGE_MENU, SETTINGS_PAGE_ONLINE, SETTINGS_PAGE_SOUND};

const SettingsContextDef Params::contextDefs[SETTINGS_CONTEXT_COUNT] = {
    {offlinePages, ARRAY_COUNT(offlinePages)},
    {onlinePages, ARRAY_COUNT(onlinePages)},
    {votingPages, ARRAY_COUNT(votingPages)},
};

u32 Params::BuildHostRulePages(SettingsPageId *dest, bool isBattle, bool isKO,
                               bool isOTT, bool isRoyale, bool isExtendedTeams) {
    u32 count = 0;
    dest[count++] = SETTINGS_PAGE_FROOM1;
    dest[count++] = SETTINGS_PAGE_FROOM2;
    if (isBattle) {
        dest[count++] = SETTINGS_PAGE_BATTLE;
        if (isExtendedTeams) dest[count++] = SETTINGS_PAGE_EXTENDED_TEAMS;
    } else {
        if (isExtendedTeams)
            dest[count++] = SETTINGS_PAGE_EXTENDED_TEAMS;
        else if (isKO)
            dest[count++] = SETTINGS_PAGE_KO;
        if (isOTT)
            dest[count++] = SETTINGS_PAGE_OTT_ONLINE;
        else if (isRoyale)
            dest[count++] = SETTINGS_PAGE_ROYALE;
    }
    return count;
}

#undef PAGE_DEF_RADIOS_ONLY
#undef PAGE_DEF
#undef ARRAY_COUNT

}  // namespace Settings
}  // namespace Pulsar
