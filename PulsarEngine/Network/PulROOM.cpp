#include <kamek.hpp>
#include <MarioKartWii/RKNet/ROOM.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <Settings/UI/SettingsPanel.hpp>
#include <Settings/UI/SettingsPageSelect.hpp>
#include <Settings/Settings.hpp>
#include <Network/Network.hpp>
#include <Network/PacketExpansion.hpp>
#include <UI/ExtendedTeamSelect/ExtendedTeamSelect.hpp>

namespace Pulsar {
namespace Network {

static void ConvertROOMPacketToData(const PulROOM& packet) {
    System* system = System::sInstance;
    system->netMgr.hostContext = packet.hostSystemContext;
    system->netMgr.hostContext2 = packet.hostSystemContext2;
    system->netMgr.customItemsBitfield = packet.customItemsBitfield;
    system->netMgr.racesPerGP = packet.raceCount;
    memcpy(system->netMgr.hostSettingsPreview, packet.hostSettingsPreview, sizeof(system->netMgr.hostSettingsPreview));
    system->netMgr.hasHostSettingsPreview = true;
}

static void WriteHostSettingsPreviewToPacket(PulROOM* packet, const Settings::Mgr& settings) {
    static const Settings::UserType pages[] = {
        Settings::SETTINGSTYPE_FROOM1,
        Settings::SETTINGSTYPE_FROOM2,
        Settings::SETTINGSTYPE_KO,
        Settings::SETTINGSTYPE_KOROYALE,
        Settings::SETTINGSTYPE_OTT,
    };

    memset(packet->hostSettingsPreview, 0, sizeof(packet->hostSettingsPreview));
    u32 offset = 0;
    for (u32 page = 0; page < sizeof(pages) / sizeof(pages[0]); ++page) {
        const Settings::UserType type = pages[page];
        const u32 valueCount = Settings::Params::radioCount[type] + Settings::Params::scrollerCount[type];
        if (offset + valueCount > HOST_SETTINGS_PREVIEW_COUNT) break;
        u8* dest = packet->hostSettingsPreview + offset;

        for (u32 setting = 0; setting < Settings::Params::radioCount[type]; ++setting) {
            dest[setting] = settings.GetUserSettingValue(type, setting);
        }
        for (u32 setting = 0; setting < Settings::Params::scrollerCount[type]; ++setting) {
            dest[Settings::Params::radioCount[type] + setting] =
                settings.GetUserSettingValue(type, Settings::Params::maxRadioCount + setting);
        }
        offset += valueCount;
    }
}

static void WriteBlockedTracksToPacket(PulROOM* packet) {
    System* system = System::sInstance;
    if (!system) return;

    const Network::Mgr& netMgr = system->netMgr;
    const u32 blockingCount = system->GetInfo().GetTrackBlocking();

    const u32 writeCount = (blockingCount < MAX_TRACK_BLOCKING) ? blockingCount : MAX_TRACK_BLOCKING;
    packet->blockedTrackCount = static_cast<u8>(writeCount);
    packet->curBlockingArrayIdx = netMgr.curBlockingArrayIdx;
    packet->lastGroupedTrackPlayed = netMgr.lastGroupedTrackPlayed;

    for (u32 i = 0; i < writeCount; ++i) {
        packet->blockedTracks[i] = (netMgr.lastTracks != nullptr) ? static_cast<u16>(netMgr.lastTracks[i]) : 0xFFFF;
    }
    for (u32 i = writeCount; i < MAX_TRACK_BLOCKING; ++i) {
        packet->blockedTracks[i] = 0xFFFF;
    }
}

static void HandleExtendedTeamUpdates(const PulROOM& packet) {
    UI::ExtendedTeamSelect* ets = SectionMgr::sInstance->curSection->Get<UI::ExtendedTeamSelect>();
    for (int id = 0; id < 12; ++id) {
        const u8 byte = id / 2;
        const u8 shift = (id % 2) * 4;
        UI::ExtendedTeamID team = static_cast<UI::ExtendedTeamID>(packet.extendedTeams[byte] >> shift & 0x0F);
        if (team != 0x0F) {
            ets->UpdatePlayerTeam(id, static_cast<UI::ExtendedTeamID>(packet.extendedTeams[byte] >> shift & 0x0F));
        }
    }
}

static bool ApplyHostContextLocally(u32 hostContext, u32 hostContext2) {
    System* system = System::sInstance;

    const bool isCharRestrictLight = hostContext & (1 << PULSAR_CHARRESTRICTLIGHT);
    const bool isCharRestrictMid = hostContext & (1 << PULSAR_CHARRESTRICTMID);
    const bool isCharRestrictHeavy = hostContext & (1 << PULSAR_CHARRESTRICTHEAVY);
    const bool isKartRestrictKart = hostContext & (1 << PULSAR_KARTRESTRICT);
    const bool isKartRestrictBike = hostContext & (1 << PULSAR_BIKERESTRICT);
    const bool isInsideForced = hostContext2 & (1 << PULSAR_TRANSMISSIONINSIDE);
    const bool isOutsideForced = hostContext2 & (1 << PULSAR_TRANSMISSIONOUTSIDE);
    const bool isVanillaForced = hostContext2 & (1 << PULSAR_TRANSMISSIONVANILLA);
    const bool isExtendedTeams = hostContext & (1 << PULSAR_EXTENDEDTEAMS);
    const bool isStartRetro = hostContext & (1 << PULSAR_STARTRETROS);
    const bool isStartCT = hostContext & (1 << PULSAR_STARTCTS);
    const bool isStartRTS = hostContext & (1 << PULSAR_STARTREGS);
    const bool isStart200 = hostContext & (1 << PULSAR_START200);
    const bool isStartOTT = hostContext & (1 << PULSAR_STARTOTT);
    const bool isStartItemRain = hostContext & (1 << PULSAR_STARTITEMRAIN);
    const bool isVanillaMode = hostContext2 & (1 << PULSAR_VANILLAMODE);

    u32 context = (isStartRetro << PULSAR_STARTRETROS) | (isStartCT << PULSAR_STARTCTS) |
                  (isStartRTS << PULSAR_STARTREGS) | (isStart200 << PULSAR_START200) |
                  (isStartOTT << PULSAR_STARTOTT) | (isStartItemRain << PULSAR_STARTITEMRAIN) |
                  (isCharRestrictLight << PULSAR_CHARRESTRICTLIGHT) | (isCharRestrictMid << PULSAR_CHARRESTRICTMID) |
                  (isCharRestrictHeavy << PULSAR_CHARRESTRICTHEAVY) | (isKartRestrictKart << PULSAR_KARTRESTRICT) |
                  (isKartRestrictBike << PULSAR_BIKERESTRICT) | (isExtendedTeams << PULSAR_EXTENDEDTEAMS);
    u32 context2 = (isInsideForced << PULSAR_TRANSMISSIONINSIDE) | (isOutsideForced << PULSAR_TRANSMISSIONOUTSIDE) | (isVanillaForced << PULSAR_TRANSMISSIONVANILLA) | (isVanillaMode << PULSAR_VANILLAMODE);
    system->context = context;
    system->context2 = context2;

    if (isStartCT || isStartRetro || isStartRTS || isStart200 || isStartOTT || isStartItemRain) {
        system->context &= ~(1 << PULSAR_EXTENDEDTEAMS);
        system->context &= ~(1 << PULSAR_CHARRESTRICTHEAVY);
        system->context &= ~(1 << PULSAR_CHARRESTRICTMID);
        system->context &= ~(1 << PULSAR_CHARRESTRICTLIGHT);
        system->context &= ~(1 << PULSAR_KARTRESTRICT);
        system->context &= ~(1 << PULSAR_BIKERESTRICT);
    }

    return isExtendedTeams;
}

static void BeforeROOMSend(RKNet::PacketHolder<PulROOM>* packetHolder, PulROOM* src, u32 len) {
    packetHolder->Copy(src, len);  // default

    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    const RKNet::ControllerSub& sub = controller->subs[controller->currentSub];
    Pulsar::System* system = Pulsar::System::sInstance;
    PulROOM* destPacket = packetHolder->packet;
    if (destPacket->messageType == 1 && sub.localAid == sub.hostAid) {
        packetHolder->packetSize = sizeof(PulROOM);  // this has been changed by copy so it's safe to do this

        // Store original message index for worldwide option detection
        const u8 originalMessage = destPacket->message;
        if (originalMessage >= 4 && originalMessage <= 9) {
            destPacket->message = 0;
        }

        const Settings::Mgr& settings = Settings::Mgr::Get();
        WriteHostSettingsPreviewToPacket(destPacket, settings);
        const RacedataSettings& racedataSettings = Racedata::sInstance->menusScenario.settings;
        const GameMode mode = racedataSettings.gamemode;

        bool isFroom = controller->roomType == RKNet::ROOMTYPE_FROOM_HOST || controller->roomType == RKNet::ROOMTYPE_FROOM_NONHOST;
        bool isFroomStart = destPacket->message == 0;
        bool isBattle = destPacket->message == 2 || destPacket->message == 3;
        bool isBalloonBattle = destPacket->message == 2;
        bool isNotPublic = isFroom || controller->roomType == RKNet::ROOMTYPE_NONE;
        bool isTimeTrial = mode == MODE_TIME_TRIAL;

        u8 koSetting = settings.GetUserSettingValue(Settings::SETTINGSTYPE_KO, RADIO_KOENABLED) == KOSETTING_ENABLED;
        u8 lapKoSetting = settings.GetUserSettingValue(Settings::SETTINGSTYPE_KO, RADIO_KOENABLED) == KOSETTING_LAPBASED && isNotPublic && !isBattle && !isTimeTrial;
        u8 battleTeam = settings.GetUserSettingValue(Settings::SETTINGSTYPE_BATTLE, RADIO_BATTLETEAMS) == BATTLE_FFA_DISABLED && isBattle;
        u8 battleElim = settings.GetUserSettingValue(Settings::SETTINGSTYPE_BATTLE, RADIO_BATTLEELIMINATION) && isBalloonBattle;
        u8 ottOnline = settings.GetUserSettingValue(Settings::SETTINGSTYPE_OTT, RADIO_OTTONLINE);
        const u8 miiHeads = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM2, RADIO_ALLOWMIIHEADS) == ALLOW_MIIHEADS_ENABLED;
        u8 charRestrictLight = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM1, RADIO_CHARSELECT) == CHAR_LIGHTONLY;
        u8 charRestrictMid = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM1, RADIO_CHARSELECT) == CHAR_MEDIUMONLY;
        u8 charRestrictHeavy = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM1, RADIO_CHARSELECT) == CHAR_HEAVYONLY;
        u8 kartRestrict = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM1, RADIO_KARTSELECT) == KART_KARTONLY;
        u8 bikeRestrict = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM1, RADIO_KARTSELECT) == KART_BIKEONLY;
        u8 itemModeRandom = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM1, SCROLLER_ITEMMODE) == GAMEMODE_RANDOM && isNotPublic;
        u8 itemModeBlast = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM1, SCROLLER_ITEMMODE) == GAMEMODE_BLAST && isNotPublic;
        u8 itemModeNone = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM1, SCROLLER_ITEMMODE) == GAMEMODE_NONE;
        u8 regsOnly = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM1, SCROLLER_TRACKSELECTION) == TRACKSELECTION_REGS;
        u8 retrosOnly = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM1, SCROLLER_TRACKSELECTION) == TRACKSELECTION_RETROS && mode != MODE_PUBLIC_VS;
        u8 ctsOnly = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM1, SCROLLER_TRACKSELECTION) == TRACKSELECTION_CTS && mode != MODE_PUBLIC_VS;
        const u8 koFinal = settings.GetUserSettingValue(Settings::SETTINGSTYPE_KO, RADIO_KOFINAL) == KOSETTING_FINAL_ALWAYS;
        const u8 changeCombo = settings.GetUserSettingValue(Settings::SETTINGSTYPE_OTT, RADIO_OTTALLOWCHANGECOMBO) == OTTSETTING_COMBO_ENABLED;
        u8 itemBoxRespawnFast = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM2, RADIO_ITEMBOXRESPAWN) == ITEMBOX_FASTRESPAWN;
        u8 transmissionInside = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM2, RADIO_FORCETRANSMISSION) == FORCE_TRANSMISSION_INSIDE;
        u8 transmissionOutside = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM2, RADIO_FORCETRANSMISSION) == FORCE_TRANSMISSION_OUTSIDE;
        u8 transmissionVanilla = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM2, RADIO_FORCETRANSMISSION) == FORCE_TRANSMISSION_VANILLA;
        u8 itemModeRain = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM1, SCROLLER_ITEMMODE) == GAMEMODE_ITEMRAIN;
        u8 itemModeStorm = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM1, SCROLLER_ITEMMODE) == GAMEMODE_ITEMSTORM;
        u8 allItemsCanLand = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM2, RADIO_ALLITEMSCANLAND) == ALLITEMSCANLAND_ENABLED;
        const u8 vanillaMode = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM2, RADIO_VANILLAMODE) == VANILLAMODE_ENABLED;
        const u8 extendedTeams = settings.GetUserSettingValue(Settings::SETTINGSTYPE_EXTENDEDTEAMS, RADIO_EXTENDEDTEAMSENABLED) == EXTENDEDTEAMS_ENABLED;
        u8 normalTC = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM2, RADIO_THUNDERCLOUD) == THUNDERCLOUD_NORMAL && isNotPublic;
        u8 vr = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM1, RADIO_VR) == VR_ENABLED && isNotPublic;
        const u8 isStartRetro = (originalMessage == 4);
        const u8 isStartCT = (originalMessage == 5);
        const u8 isStartRTS = (originalMessage == 6);
        const u8 isStart200 = (originalMessage == 7);
        const u8 isStartOTT = (originalMessage == 8);
        const u8 isStartItemRain = (originalMessage == 9);
        const u8 rankings = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM1, RADIO_RANKINGS) == RANKINGS_ENABLED;
        const u8 battleRoyale = settings.GetUserSettingValue(Settings::SETTINGSTYPE_KOROYALE, RADIO_KOROYALEENABLED) == KOROYALESETTING_ENABLED;
        const u8 koRoyaleBalloons = settings.GetUserSettingValue(Settings::SETTINGSTYPE_KOROYALE, SCROLLER_KOROYALEBALLOONS);
        const u8 koPerRace2 = koRoyaleBalloons == KOROYALESETTING_BALLOONS_2;
        const u8 koPerRace3 = koRoyaleBalloons == KOROYALESETTING_BALLOONS_3;
        const u8 koPerRace4 = koRoyaleBalloons == KOROYALESETTING_BALLOONS_4;
        const u8 koRoyaleLapMultiplier = settings.GetUserSettingValue(Settings::SETTINGSTYPE_KOROYALE, SCROLLER_KOROYALELAPMULTIPLIER);
        const u8 koRoyaleLaps1_5x = koRoyaleLapMultiplier == KOROYALESETTING_LAPS_1_5X;
        const u8 koRoyaleLaps2_0x = koRoyaleLapMultiplier == KOROYALESETTING_LAPS_2_0X;

        if (extendedTeams) {
            koSetting = KOSETTING_DISABLED;
            vr = VR_DISABLED;
            if (destPacket->message == 2 || destPacket->message == 3) {
                battleTeam = BATTLE_FFA_ENABLED;
            } else {
                battleTeam = BATTLE_FFA_DISABLED;
            }
        }

        if (vanillaMode) {
            regsOnly = 1;
            retrosOnly = 0;
            ctsOnly = 0;
            transmissionInside = 0;
            transmissionOutside = 0;
            transmissionVanilla = 1;
            normalTC = 1;
            allItemsCanLand = 0;
            itemBoxRespawnFast = 0;
            destPacket->customItemsBitfield = 0x7FFFF;
        }

        destPacket->hostSystemContext |= (ottOnline != OTTSETTING_OFFLINE_DISABLED) << PULSAR_MODE_OTT |  // ott
                                         (ottOnline == OTTSETTING_ONLINE_FEATHER) << PULSAR_FEATHER |  // ott feather
                                         (settings.GetUserSettingValue(Settings::SETTINGSTYPE_OTT, RADIO_OTTALLOWUMTS) != OTTSETTING_UMTS_DISABLED) << PULSAR_UMTS |  // ott umts
                                         koSetting << PULSAR_MODE_KO | lapKoSetting << PULSAR_MODE_LAPKO |
                                         charRestrictLight << PULSAR_CHARRESTRICTLIGHT | charRestrictMid << PULSAR_CHARRESTRICTMID |
                                         charRestrictHeavy << PULSAR_CHARRESTRICTHEAVY | kartRestrict << PULSAR_KARTRESTRICT |
                                         bikeRestrict << PULSAR_BIKERESTRICT | koFinal << PULSAR_KOFINAL |
                                         changeCombo << PULSAR_CHANGECOMBO | normalTC << PULSAR_THUNDERCLOUD |
                                         (settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM1, RADIO_FROOMCC) == HOSTCC_500) << PULSAR_500 | regsOnly << PULSAR_REGS |
                                         retrosOnly << PULSAR_RETROS | ctsOnly << PULSAR_CTS |
                                         battleTeam << PULSAR_FFA | extendedTeams << PULSAR_EXTENDEDTEAMS |
                                         battleElim << PULSAR_ELIMINATION | isStartRetro << PULSAR_STARTRETROS |
                                         isStartCT << PULSAR_STARTCTS | isStartRTS << PULSAR_STARTREGS |
                                         isStart200 << PULSAR_START200 | isStartOTT << PULSAR_STARTOTT |
                                         isStartItemRain << PULSAR_STARTITEMRAIN;

        destPacket->hostSystemContext2 |= transmissionInside << PULSAR_TRANSMISSIONINSIDE | transmissionOutside << PULSAR_TRANSMISSIONOUTSIDE |
                                          transmissionVanilla << PULSAR_TRANSMISSIONVANILLA | miiHeads << PULSAR_MIIHEADS |
                                          itemModeRandom << PULSAR_ITEMMODERANDOM | itemModeBlast << PULSAR_ITEMMODEBLAST |
                                          itemModeRain << PULSAR_ITEMMODERAIN | itemModeStorm << PULSAR_ITEMMODESTORM |
                                          allItemsCanLand << PULSAR_ALLITEMSCANLAND |
                                          settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM2, RADIO_HOSTWINS) << PULSAR_HAW | itemBoxRespawnFast << PULSAR_ITEMBOXRESPAWN |
                                          rankings << PULSAR_RANKING | vr << PULSAR_VR | battleRoyale << PULSAR_MODE_BATTLEROYALE |
                                          itemModeNone << PULSAR_ITEMMODENONE |
                                          koPerRace2 << PULSAR_KOPERRACE_2 |
                                          koPerRace3 << PULSAR_KOPERRACE_3 |
                                          koPerRace4 << PULSAR_KOPERRACE_4 |
                                          koRoyaleLaps1_5x << PULSAR_KOROYALE_LAPS_1_5X |
                                          koRoyaleLaps2_0x << PULSAR_KOROYALE_LAPS_2_0X |
                                          vanillaMode << PULSAR_VANILLAMODE;

        if (!vanillaMode) {
            destPacket->customItemsBitfield = settings.GetCustomItems();
        }

        u8 raceCount;
        if (koSetting == KOSETTING_ENABLED)
            raceCount = 0xFE;
        else
            switch (settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM1, SCROLLER_RACECOUNT)) {
                case (1):
                    raceCount = 5;
                    break;
                case (2):
                    raceCount = 7;
                    break;
                case (3):
                    raceCount = 11;
                    break;
                case (4):
                    raceCount = 23;
                    break;
                case (5):
                    raceCount = 31;
                    break;
                case (6):
                    raceCount = 1;
                    break;
                default:
                    raceCount = 3;
            }
        destPacket->raceCount = raceCount;

        WriteBlockedTracksToPacket(destPacket);

        ConvertROOMPacketToData(*destPacket);
        (void)ApplyHostContextLocally(destPacket->hostSystemContext, destPacket->hostSystemContext2);

        if (extendedTeams) {
            UI::ExtendedTeamManager::sInstance->hasFriendRoomStarted = true;
        }
    }

    const bool isExtendedTeams = Settings::Mgr::Get().GetUserSettingValue(Settings::SETTINGSTYPE_EXTENDEDTEAMS, RADIO_EXTENDEDTEAMSENABLED) == EXTENDEDTEAMS_ENABLED;
    const bool isUpdateTeamMessage = destPacket->messageType == UI::ExtendedTeamManager::MSG_TYPE_UPDATE_TEAMS;
    const bool isStartVSRaceMessage = destPacket->messageType == 1 && (destPacket->message == 0 || destPacket->message == 2 || destPacket->message == 3);
    if ((isUpdateTeamMessage || (isStartVSRaceMessage && isExtendedTeams)) && sub.localAid == sub.hostAid) {
        packetHolder->packetSize = sizeof(PulROOM);
        const UI::ExtendedTeamPlayer* playerInfo = UI::ExtendedTeamManager::sInstance->GetPlayerInfo();

        memset(destPacket->extendedTeams, 0xff, sizeof(destPacket->extendedTeams));
        for (int i = 0; i < 12; ++i) {
            if (playerInfo[i].playerIdx >= 12)
                continue;

            const u8 byte = i / 2;
            const u8 shift = (i % 2) * 4;

            destPacket->extendedTeams[byte] &= ~(0x0F << shift);
            destPacket->extendedTeams[byte] |= (playerInfo[i].team & 0x0F) << shift;
        }
    }
}
kmCall(0x8065b15c, BeforeROOMSend);

kmWrite32(0x8065add0, 0x60000000);
static void AfterROOMReception(const RKNet::PacketHolder<PulROOM>* packetHolder, const PulROOM& src, u32 len) {
    register RKNet::ROOMPacket* packet;
    register u32 aid;
    asm(mr packet, r28;);
    asm(mr aid, r29;);

    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    const RKNet::ControllerSub& sub = controller->subs[controller->currentSub];

    const bool isHost = sub.localAid == sub.hostAid;

    // START msg sent by the host, size check should always be guaranteed in theory
    if (src.messageType == 1 && !isHost && packetHolder->packetSize == sizeof(PulROOM)) {
        ConvertROOMPacketToData(src);

        // Get context from host packet (no need to read local settings - host values take precedence)
        Network::Mgr& netMgr = Pulsar::System::sInstance->netMgr;
        const bool isExtendedTeams = ApplyHostContextLocally(netMgr.hostContext, netMgr.hostContext2);

        // Also exit the settings page to prevent weird graphical artefacts
        Page* topPage = SectionMgr::sInstance->curSection->GetTopLayerPage();
        PageId topId = topPage->pageId;
        if (topId == UI::SettingsPanel::id) {
            UI::SettingsPanel* panel = static_cast<UI::SettingsPanel*>(topPage);
            panel->OnBackPress(0);
        } else if (topId == UI::SettingsPageSelect::id) {
            UI::SettingsPageSelect* pageSelect = static_cast<UI::SettingsPageSelect*>(topPage);
            pageSelect->OnBackPress(0);
        }

        // Extended Team VS start
        if (isExtendedTeams) {
            HandleExtendedTeamUpdates(src);
            UI::ExtendedTeamManager::sInstance->hasFriendRoomStarted = true;
        }
    }

    if (src.messageType == UI::ExtendedTeamManager::MSG_TYPE_UPDATE_TEAMS &&
        !isHost &&
        packetHolder->packetSize == sizeof(PulROOM)) {
        HandleExtendedTeamUpdates(src);
    }

    if (isHost && src.messageType == UI::ExtendedTeamManager::MSG_TYPE_PING) {
        UI::ExtendedTeamManager::sInstance->SetActiveStatusForAID(aid);
    } else if (!isHost && src.messageType == UI::ExtendedTeamManager::MSG_TYPE_ACK_START_RACE) {
        UI::ExtendedTeamManager::sInstance->SetDoneStatusForAID(aid);
    }

    memcpy(packet, &src, sizeof(RKNet::ROOMPacket));  // default
}
kmCall(0x8065add8, AfterROOMReception);

// Use the synced host race count when the game reads GP length.
kmCall(0x806460B8, System::GetRaceCount);
kmCall(0x8064f51c, System::GetRaceCount);
}  // namespace Network
}  // namespace Pulsar
