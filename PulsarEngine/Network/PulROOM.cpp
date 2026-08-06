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

static_assert(HOST_SETTINGS_PREVIEW_COUNT == 27, "Update settings preview capacity checks with the ROOM payload");

static void ConvertROOMPacketToData(const PulROOM &packet) {
    System *system = System::sInstance;
    system->netMgr.hostContext = packet.hostSystemContext;
    system->netMgr.hostContext2 = packet.hostSystemContext2;
    system->netMgr.customItemsBitfield = packet.customItemsBitfield;
    system->netMgr.racesPerGP = packet.raceCount;
    memcpy(system->netMgr.hostSettingsPreview, packet.hostSettingsPreview, sizeof(system->netMgr.hostSettingsPreview));
    system->netMgr.hasHostSettingsPreview = true;
}

static void WriteHostSettingsPreviewToPacket(PulROOM *packet, const Settings::Mgr &settings) {
    const bool isBattle = packet->message == 2 || packet->message == 3;
    const bool isExtendedTeams = settings.GetSettingValue(Settings::SETTING_EXTENDEDTEAMSENABLED) == EXTENDEDTEAMS_ENABLED ||
                                 (Mogi::IsActive() && Mogi::IsTeamFormat());
    const bool isKO = !isBattle && !isExtendedTeams &&
                      settings.GetSettingValue(Settings::SETTING_KOENABLED) != KOSETTING_DISABLED;
    const bool isOTT = settings.GetSettingValue(Settings::SETTING_OTTONLINE) != OTTSETTING_ONLINE_DISABLED;
    const bool isRoyale = settings.GetSettingValue(Settings::SETTING_KOROYALEENABLED) == KOROYALESETTING_ENABLED;
    Settings::SettingsPageId pages[6];
    const u32 pageCount = Settings::Params::BuildHostRulePages(
        pages, isBattle, isKO, isOTT, isRoyale, isExtendedTeams);

    memset(packet->hostSettingsPreview, 0, sizeof(packet->hostSettingsPreview));
    u32 offset = 0;
    for (u32 page = 0; page < pageCount; ++page) {
        const Settings::SettingsPageDef &def = Settings::Params::GetPageDef(pages[page]);
        const u32 valueCount = def.radioCount + def.scrollerCount;
        if (offset + valueCount > HOST_SETTINGS_PREVIEW_COUNT) break;
        u8 *dest = packet->hostSettingsPreview + offset;

        for (u32 i = 0; i < def.radioCount; ++i) dest[i] = settings.GetSettingValue(def.radioSettings[i]);
        for (u32 i = 0; i < def.scrollerCount; ++i)
            dest[def.radioCount + i] = settings.GetSettingValue(def.scrollerSettings[i]);
        offset += valueCount;
    }
}

static void WriteBlockedTracksToPacket(PulROOM *packet) {
    System *system = System::sInstance;
    if (!system) return;

    const Network::Mgr &netMgr = system->netMgr;
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

static void HandleExtendedTeamUpdates(const PulROOM &packet) {
    SectionMgr *sectionMgr = SectionMgr::sInstance;
    if (sectionMgr == nullptr || sectionMgr->curSection == nullptr) return;
    UI::ExtendedTeamSelect *ets = sectionMgr->curSection->Get<UI::ExtendedTeamSelect>();
    if (ets == nullptr) return;
    for (int id = 0; id < 12; ++id) {
        const u8 byte = id / 2;
        const u8 shift = (id % 2) * 4;
        UI::ExtendedTeamID team = static_cast<UI::ExtendedTeamID>(packet.extendedTeams[byte] >> shift & 0x0F);
        if (team < UI::TEAM_COUNT) ets->UpdatePlayerTeam(id, team);
    }
}

static bool ApplyHostContextLocally(u32 hostContext, u32 hostContext2) {
    System *system = System::sInstance;

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

static void BeforeROOMSend(RKNet::PacketHolder<PulROOM> *packetHolder, PulROOM *src, u32 len) {
    packetHolder->Copy(src, len);  // default

    const RKNet::Controller *controller = RKNet::Controller::sInstance;
    const RKNet::ControllerSub &sub = controller->subs[controller->currentSub];
    Pulsar::System *system = Pulsar::System::sInstance;
    PulROOM *destPacket = packetHolder->packet;
    if (destPacket->messageType == 1 && sub.localAid == sub.hostAid) {
        packetHolder->packetSize = sizeof(PulROOM);  // this has been changed by copy so it's safe to do this

        // Store original message index for worldwide option detection
        const u8 originalMessage = destPacket->message;
        if (originalMessage >= 4 && originalMessage <= 9) {
            destPacket->message = 0;
        }

        const Settings::Mgr &settings = Settings::Mgr::Get();
        WriteHostSettingsPreviewToPacket(destPacket, settings);
        const RacedataSettings &racedataSettings = Racedata::sInstance->menusScenario.settings;
        const GameMode mode = racedataSettings.gamemode;

        bool isFroom = controller->roomType == RKNet::ROOMTYPE_FROOM_HOST || controller->roomType == RKNet::ROOMTYPE_FROOM_NONHOST;
        bool isFroomStart = destPacket->message == 0;
        bool isBattle = destPacket->message == 2 || destPacket->message == 3;
        bool isBalloonBattle = destPacket->message == 2;
        bool isNotPublic = isFroom || controller->roomType == RKNet::ROOMTYPE_NONE;
        bool isTimeTrial = mode == MODE_TIME_TRIAL;

        u8 koSetting = settings.GetSettingValue(Pulsar::Settings::SETTING_KOENABLED) == KOSETTING_ENABLED;
        u8 lapKoSetting = settings.GetSettingValue(Pulsar::Settings::SETTING_KOENABLED) == KOSETTING_LAPBASED && isNotPublic && !isBattle && !isTimeTrial;
        u8 battleTeam = settings.GetSettingValue(Pulsar::Settings::SETTING_BATTLETEAMS) == BATTLE_FFA_DISABLED && isBattle;
        u8 battleElim = settings.GetSettingValue(Pulsar::Settings::SETTING_BATTLEELIMINATION) && isBalloonBattle;
        u8 ottOnline = settings.GetSettingValue(Pulsar::Settings::SETTING_OTTONLINE);
        const u8 miiHeads = settings.GetSettingValue(Pulsar::Settings::SETTING_ALLOWMIIHEADS) == ALLOW_MIIHEADS_ENABLED;
        u8 charRestrictLight = settings.GetSettingValue(Pulsar::Settings::SETTING_CHARSELECT) == CHAR_LIGHTONLY;
        u8 charRestrictMid = settings.GetSettingValue(Pulsar::Settings::SETTING_CHARSELECT) == CHAR_MEDIUMONLY;
        u8 charRestrictHeavy = settings.GetSettingValue(Pulsar::Settings::SETTING_CHARSELECT) == CHAR_HEAVYONLY;
        u8 kartRestrict = settings.GetSettingValue(Pulsar::Settings::SETTING_KARTSELECT) == KART_KARTONLY;
        u8 bikeRestrict = settings.GetSettingValue(Pulsar::Settings::SETTING_KARTSELECT) == KART_BIKEONLY;
        u8 itemModeRandom = settings.GetSettingValue(Pulsar::Settings::SETTING_ITEMMODE) == GAMEMODE_RANDOM && isNotPublic;
        u8 itemModeBlast = settings.GetSettingValue(Pulsar::Settings::SETTING_ITEMMODE) == GAMEMODE_BLAST && isNotPublic;
        u8 itemModeNone = settings.GetSettingValue(Pulsar::Settings::SETTING_ITEMMODE) == GAMEMODE_NONE;
        u8 regsOnly = settings.GetSettingValue(Pulsar::Settings::SETTING_TRACKSELECTION) == TRACKSELECTION_REGS;
        u8 retrosOnly = settings.GetSettingValue(Pulsar::Settings::SETTING_TRACKSELECTION) == TRACKSELECTION_RETROS && mode != MODE_PUBLIC_VS;
        u8 ctsOnly = settings.GetSettingValue(Pulsar::Settings::SETTING_TRACKSELECTION) == TRACKSELECTION_CTS && mode != MODE_PUBLIC_VS;
        const u8 koFinal = settings.GetSettingValue(Pulsar::Settings::SETTING_KOFINAL) == KOSETTING_FINAL_ALWAYS;
        const u8 changeCombo = settings.GetSettingValue(Pulsar::Settings::SETTING_OTTALLOWCHANGECOMBO) == OTTSETTING_COMBO_ENABLED;
        u8 itemBoxRespawnFast = settings.GetSettingValue(Pulsar::Settings::SETTING_ITEMBOXRESPAWN) == ITEMBOX_FASTRESPAWN;
        u8 transmissionInside = settings.GetSettingValue(Pulsar::Settings::SETTING_FORCETRANSMISSION) == FORCE_TRANSMISSION_INSIDE;
        u8 transmissionOutside = settings.GetSettingValue(Pulsar::Settings::SETTING_FORCETRANSMISSION) == FORCE_TRANSMISSION_OUTSIDE;
        u8 transmissionVanilla = settings.GetSettingValue(Pulsar::Settings::SETTING_FORCETRANSMISSION) == FORCE_TRANSMISSION_VANILLA;
        u8 itemModeRain = settings.GetSettingValue(Pulsar::Settings::SETTING_ITEMMODE) == GAMEMODE_ITEMRAIN;
        u8 itemModeStorm = settings.GetSettingValue(Pulsar::Settings::SETTING_ITEMMODE) == GAMEMODE_ITEMSTORM;
        u8 allItemsCanLand = settings.GetSettingValue(Pulsar::Settings::SETTING_ALLITEMSCANLAND) == ALLITEMSCANLAND_ENABLED;
        const u8 vanillaMode = settings.GetSettingValue(Pulsar::Settings::SETTING_VANILLAMODE) == VANILLAMODE_ENABLED;
        const u8 extendedTeams = settings.GetSettingValue(Pulsar::Settings::SETTING_EXTENDEDTEAMSENABLED) == EXTENDEDTEAMS_ENABLED;
        u8 normalTC = settings.GetSettingValue(Pulsar::Settings::SETTING_THUNDERCLOUD) == THUNDERCLOUD_NORMAL && isNotPublic;
        u8 vr = settings.GetSettingValue(Pulsar::Settings::SETTING_VR) == VR_ENABLED && isNotPublic;
        const u8 isStartRetro = (originalMessage == 4);
        const u8 isStartCT = (originalMessage == 5);
        const u8 isStartRTS = (originalMessage == 6);
        const u8 isStart200 = (originalMessage == 7);
        const u8 isStartOTT = (originalMessage == 8);
        const u8 isStartItemRain = (originalMessage == 9);
        const u8 rankings = settings.GetSettingValue(Pulsar::Settings::SETTING_RANKINGS) == RANKINGS_ENABLED;
        const u8 battleRoyale = settings.GetSettingValue(Pulsar::Settings::SETTING_KOROYALEENABLED) == KOROYALESETTING_ENABLED;
        const u8 koRoyaleBalloons = settings.GetSettingValue(Pulsar::Settings::SETTING_KOROYALEBALLOONS);
        const u8 koPerRace2 = koRoyaleBalloons == KOROYALESETTING_BALLOONS_2;
        const u8 koPerRace3 = koRoyaleBalloons == KOROYALESETTING_BALLOONS_3;
        const u8 koPerRace4 = koRoyaleBalloons == KOROYALESETTING_BALLOONS_4;
        const u8 koRoyaleLapMultiplier = settings.GetSettingValue(Pulsar::Settings::SETTING_KOROYALELAPMULTIPLIER);
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
                                         (settings.GetSettingValue(Pulsar::Settings::SETTING_OTTALLOWUMTS) != OTTSETTING_UMTS_DISABLED) << PULSAR_UMTS |  // ott umts
                                         koSetting << PULSAR_MODE_KO | lapKoSetting << PULSAR_MODE_LAPKO |
                                         charRestrictLight << PULSAR_CHARRESTRICTLIGHT | charRestrictMid << PULSAR_CHARRESTRICTMID |
                                         charRestrictHeavy << PULSAR_CHARRESTRICTHEAVY | kartRestrict << PULSAR_KARTRESTRICT |
                                         bikeRestrict << PULSAR_BIKERESTRICT | koFinal << PULSAR_KOFINAL |
                                         changeCombo << PULSAR_CHANGECOMBO | normalTC << PULSAR_THUNDERCLOUD |
                                         (settings.GetSettingValue(Pulsar::Settings::SETTING_FROOMCC) == HOSTCC_500) << PULSAR_500 | regsOnly << PULSAR_REGS |
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
                                          settings.GetSettingValue(Pulsar::Settings::SETTING_HOSTWINS) << PULSAR_HAW | itemBoxRespawnFast << PULSAR_ITEMBOXRESPAWN |
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
            switch (settings.GetSettingValue(Pulsar::Settings::SETTING_RACECOUNT)) {
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

    const bool isExtendedTeams = Settings::Mgr::Get().GetSettingValue(Pulsar::Settings::SETTING_EXTENDEDTEAMSENABLED) == EXTENDEDTEAMS_ENABLED ||
                                 (Mogi::IsActive() && Mogi::IsTeamFormat());
    const bool isUpdateTeamMessage = destPacket->messageType == UI::ExtendedTeamManager::MSG_TYPE_UPDATE_TEAMS;
    const bool isStartVSRaceMessage = destPacket->messageType == 1 && (destPacket->message == 0 || destPacket->message == 2 || destPacket->message == 3);
    if ((isUpdateTeamMessage || (isStartVSRaceMessage && isExtendedTeams)) && sub.localAid == sub.hostAid) {
        packetHolder->packetSize = sizeof(PulROOM);
        const UI::ExtendedTeamPlayer *playerInfo = UI::ExtendedTeamManager::sInstance->GetPlayerInfo();

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
static void AfterROOMReception(const RKNet::PacketHolder<PulROOM> *packetHolder, const PulROOM &src, u32 len) {
    register RKNet::ROOMPacket *packet;
    register u32 aid;
    asm(mr packet, r28;);
    asm(mr aid, r29;);

    const RKNet::Controller *controller = RKNet::Controller::sInstance;
    if (controller == nullptr || packetHolder == nullptr) {
        if (packet != nullptr) memcpy(packet, &src, sizeof(RKNet::ROOMPacket));
        return;
    }
    const RKNet::ControllerSub &sub = controller->subs[controller->currentSub];

    const bool isHost = sub.localAid == sub.hostAid;
    const bool isFromHost = aid < 12 && aid == sub.hostAid;
    const bool isFromConnectedPeer = aid < 12 && ((sub.availableAids >> aid) & 1) != 0;

    // START msg sent by the host, size check should always be guaranteed in theory
    if (src.messageType == 1 && !isHost && isFromHost && packetHolder->packetSize == sizeof(PulROOM)) {
        ConvertROOMPacketToData(src);

        // Get context from host packet (no need to read local settings - host values take precedence)
        Network::Mgr &netMgr = Pulsar::System::sInstance->netMgr;
        const bool isExtendedTeams = ApplyHostContextLocally(netMgr.hostContext, netMgr.hostContext2);

        // Also exit the settings page to prevent weird graphical artefacts
        SectionMgr *sectionMgr = SectionMgr::sInstance;
        Page *topPage = nullptr;
        if (sectionMgr != nullptr && sectionMgr->curSection != nullptr)
            topPage = sectionMgr->curSection->GetTopLayerPage();
        if (topPage != nullptr) {
            PageId topId = topPage->pageId;
            if (topId == UI::SettingsPanel::id) {
                UI::SettingsPanel *panel = static_cast<UI::SettingsPanel *>(topPage);
                panel->OnBackPress(0);
            } else if (topId == UI::SettingsPageSelect::id) {
                UI::SettingsPageSelect *pageSelect = static_cast<UI::SettingsPageSelect *>(topPage);
                pageSelect->OnBackPress(0);
            }
        }

        // Extended Team VS start
        if (isExtendedTeams) {
            HandleExtendedTeamUpdates(src);
            if (UI::ExtendedTeamManager::sInstance != nullptr)
                UI::ExtendedTeamManager::sInstance->hasFriendRoomStarted = true;
        }
    }

    if (src.messageType == UI::ExtendedTeamManager::MSG_TYPE_UPDATE_TEAMS &&
        !isHost &&
        isFromHost &&
        packetHolder->packetSize == sizeof(PulROOM)) {
        HandleExtendedTeamUpdates(src);
    }

    UI::ExtendedTeamManager *extendedTeamManager = UI::ExtendedTeamManager::sInstance;
    if (extendedTeamManager != nullptr && isHost && isFromConnectedPeer) {
        if (src.messageType == UI::ExtendedTeamManager::MSG_TYPE_PING)
            extendedTeamManager->SetActiveStatusForAID(aid);
        else if (src.messageType == UI::ExtendedTeamManager::MSG_TYPE_ACK_START_RACE)
            extendedTeamManager->SetDoneStatusForAID(aid);
    }

    if (packet != nullptr) memcpy(packet, &src, sizeof(RKNet::ROOMPacket));  // default
}
kmCall(0x8065add8, AfterROOMReception);

// Use the synced host race count when the game reads GP length.
kmCall(0x806460B8, System::GetRaceCount);
kmCall(0x8064f51c, System::GetRaceCount);
}  // namespace Network
}  // namespace Pulsar
