#include <kamek.hpp>
#include <MarioKartWii/RKNet/ROOM.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <Settings/UI/SettingsPanel.hpp>
#include <Settings/Settings.hpp>
#include <Network/Network.hpp>
#include <Network/PacketExpansion.hpp>
#include <UI/ExtendedTeamSelect/ExtendedTeamSelect.hpp>

namespace Pulsar {
namespace Network {

// Implements the ability for a host to send a message, allowing for custom host settings

// If we are in a room, we are guaranteed to be in a situation where Pul packets are being sent
// however, no reason to send the settings outside of START packets and if we are not the host, this is easily changed by just editing the check

static void ConvertROOMPacketToData(const PulROOM& packet) {
    System* system = System::sInstance;
    system->netMgr.hostContext = packet.hostSystemContext;
    system->netMgr.hostContext2 = packet.hostSystemContext2;
    system->netMgr.racesPerGP = packet.raceCount;
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

static void BeforeROOMSend(RKNet::PacketHolder<PulROOM>* packetHolder, PulROOM* src, u32 len) {
    packetHolder->Copy(src, len);  // default

    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    const RKNet::ControllerSub& sub = controller->subs[controller->currentSub];
    Pulsar::System* system = Pulsar::System::sInstance;
    PulROOM* destPacket = packetHolder->packet;
    if (destPacket->messageType == 1 && sub.localAid == sub.hostAid) {
        packetHolder->packetSize = sizeof(PulROOM);  // this has been changed by copy so it's safe to do this
        const Settings::Mgr& settings = Settings::Mgr::Get();
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
        const u8 ottOnline = settings.GetUserSettingValue(Settings::SETTINGSTYPE_OTT, RADIO_OTTONLINE);
        const u8 miiHeads = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM2, RADIO_ALLOWMIIHEADS) == ALLOW_MIIHEADS_ENABLED;
        u8 charRestrictLight = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM1, RADIO_CHARSELECT) == CHAR_LIGHTONLY;
        u8 charRestrictMid = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM1, RADIO_CHARSELECT) == CHAR_MEDIUMONLY;
        u8 charRestrictHeavy = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM1, RADIO_CHARSELECT) == CHAR_HEAVYONLY;
        u8 kartRestrict = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM1, RADIO_KARTSELECT) == KART_KARTONLY;
        u8 bikeRestrict = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM1, RADIO_KARTSELECT) == KART_BIKEONLY;
        const u8 itemModeRandom = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM1, SCROLLER_ITEMMODE) == GAMEMODE_RANDOM && isNotPublic;
        const u8 itemModeBlast = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM1, SCROLLER_ITEMMODE) == GAMEMODE_BLAST && isNotPublic;
        const u8 itemModeNone = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM1, SCROLLER_ITEMMODE) == GAMEMODE_NONE;
        const u8 RegOnly = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM1, SCROLLER_TRACKSELECTION) == TRACKSELECTION_REGS;
        const u8 RetroOnly = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM1, SCROLLER_TRACKSELECTION) == TRACKSELECTION_RETROS && mode != MODE_PUBLIC_VS;
        const u8 CtsOnly = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM1, SCROLLER_TRACKSELECTION) == TRACKSELECTION_CTS && mode != MODE_PUBLIC_VS;
        const u8 koFinal = settings.GetUserSettingValue(Settings::SETTINGSTYPE_KO, RADIO_KOFINAL) == KOSETTING_FINAL_ALWAYS;
        const u8 changeCombo = settings.GetUserSettingValue(Settings::SETTINGSTYPE_OTT, RADIO_OTTALLOWCHANGECOMBO) == OTTSETTING_COMBO_ENABLED;
        const u8 itemBoxRepsawnFast = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM2, RADIO_ITEMBOXRESPAWN) == ITEMBOX_FASTRESPAWN;
        const u8 transmissionInside = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM2, RADIO_FORCETRANSMISSION) == FORCE_TRANSMISSION_INSIDE;
        const u8 transmissionOutside = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM2, RADIO_FORCETRANSMISSION) == FORCE_TRANSMISSION_OUTSIDE;
        const u8 transmissionVanilla = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM2, RADIO_FORCETRANSMISSION) == FORCE_TRANSMISSION_VANILLA;
        const u8 itemModeRain = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM1, SCROLLER_ITEMMODE) == GAMEMODE_ITEMRAIN;
        const u8 itemModeStorm = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM1, SCROLLER_ITEMMODE) == GAMEMODE_ITEMSTORM;
        const u8 extendedTeams = settings.GetUserSettingValue(Settings::SETTINGSTYPE_EXTENDEDTEAMS, RADIO_EXTENDEDTEAMSENABLED) == EXTENDEDTEAMS_ENABLED;
        const u8 normalTC = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM2, RADIO_THUNDERCLOUD) == THUNDERCLOUD_NORMAL && isNotPublic;
        const u8 isStartRetro = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM2, SCROLLER_STARTWORLDWIDE) == START_WORLDWIDE_RETROS;
        const u8 isStartCT = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM2, SCROLLER_STARTWORLDWIDE) == START_WORLDWIDE_CTS;
        const u8 isStartRTS = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM2, SCROLLER_STARTWORLDWIDE) == START_WORLDWIDE_RTS;
        const u8 isStart200 = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM2, SCROLLER_STARTWORLDWIDE) == START_WORLDWIDE_200;
        const u8 isStartOTT = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM2, SCROLLER_STARTWORLDWIDE) == START_WORLDWIDE_OTT;
        const u8 isStartItemRain = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM2, SCROLLER_STARTWORLDWIDE) == START_WORLDWIDE_ITEMRAIN;

        if (extendedTeams) {
            koSetting = KOSETTING_DISABLED;
            if (destPacket->message == 2 || destPacket->message == 3) {
                battleTeam = BATTLE_FFA_ENABLED;
            } else {
                battleTeam = BATTLE_FFA_DISABLED;
            }
        }

        if (isStartCT || isStartRetro || isStartRTS || isStart200 || isStartOTT || isStartItemRain) {
            Pulsar::System::sInstance->context &= ~(1 << PULSAR_EXTENDEDTEAMS);
            Pulsar::System::sInstance->context &= ~(1 << PULSAR_CHARRESTRICTHEAVY);
            Pulsar::System::sInstance->context &= ~(1 << PULSAR_CHARRESTRICTMID);
            Pulsar::System::sInstance->context &= ~(1 << PULSAR_CHARRESTRICTLIGHT);
            Pulsar::System::sInstance->context &= ~(1 << PULSAR_KARTRESTRICT);
            Pulsar::System::sInstance->context &= ~(1 << PULSAR_BIKERESTRICT);
        }

        destPacket->hostSystemContext |= (ottOnline != OTTSETTING_OFFLINE_DISABLED) << PULSAR_MODE_OTT  // ott
                                         | (ottOnline == OTTSETTING_ONLINE_FEATHER) << PULSAR_FEATHER  // ott feather
                                         | (settings.GetUserSettingValue(Settings::SETTINGSTYPE_OTT, RADIO_OTTALLOWUMTS) ^ true) << PULSAR_UMTS  // ott umts
                                         | koSetting << PULSAR_MODE_KO | lapKoSetting << PULSAR_MODE_LAPKO | charRestrictLight << PULSAR_CHARRESTRICTLIGHT | charRestrictMid << PULSAR_CHARRESTRICTMID | charRestrictHeavy << PULSAR_CHARRESTRICTHEAVY | kartRestrict << PULSAR_KARTRESTRICT | bikeRestrict << PULSAR_BIKERESTRICT | koFinal << PULSAR_KOFINAL | changeCombo << PULSAR_CHANGECOMBO | normalTC << PULSAR_THUNDERCLOUD | settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM1, RADIO_FROOMCC) << PULSAR_500 | RegOnly << PULSAR_REGS | RetroOnly << PULSAR_RETROS | CtsOnly << PULSAR_CTS | battleTeam << PULSAR_FFA | extendedTeams << PULSAR_EXTENDEDTEAMS | battleElim << PULSAR_ELIMINATION | isStartRetro << PULSAR_STARTRETROS | isStartCT << PULSAR_STARTCTS | isStartRTS << PULSAR_STARTREGS | isStart200 << PULSAR_START200 | isStartOTT << PULSAR_STARTOTT | isStartItemRain << PULSAR_STARTITEMRAIN;

        destPacket->hostSystemContext2 |= transmissionInside << PULSAR_TRANSMISSIONINSIDE | transmissionOutside << PULSAR_TRANSMISSIONOUTSIDE | transmissionVanilla << PULSAR_TRANSMISSIONVANILLA | miiHeads << PULSAR_MIIHEADS | itemModeRandom << PULSAR_ITEMMODERANDOM | itemModeBlast << PULSAR_ITEMMODEBLAST | itemModeRain << PULSAR_ITEMMODERAIN | itemModeStorm << PULSAR_ITEMMODESTORM | settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM2, RADIO_HOSTWINS) << PULSAR_HAW | itemBoxRepsawnFast << PULSAR_ITEMBOXRESPAWN;

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
        ConvertROOMPacketToData(*destPacket);

        if (extendedTeams) {
            UI::ExtendedTeamManager::sInstance->hasFriendRoomStarted = true;
        }
    }

    // if we're starting a Extended Team VS or we're the host updating the teams, write the new teams to the packet
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
        const Settings::Mgr& settings = Settings::Mgr::Get();

        bool isCharRestrictLight = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM1, RADIO_CHARSELECT) == CHAR_LIGHTONLY;
        bool isCharRestrictMid = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM1, RADIO_CHARSELECT) == CHAR_MEDIUMONLY;
        bool isCharRestrictHeavy = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM1, RADIO_CHARSELECT) == CHAR_HEAVYONLY;
        bool isKartRestrictKart = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM1, RADIO_KARTSELECT) == KART_KARTONLY;
        bool isKartRestrictBike = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM1, RADIO_KARTSELECT) == KART_BIKEONLY;
        bool isExtendedTeams = settings.GetUserSettingValue(Settings::SETTINGSTYPE_EXTENDEDTEAMS, RADIO_EXTENDEDTEAMSENABLED) == EXTENDEDTEAMS_ENABLED;
        bool isStartRetro = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM2, SCROLLER_STARTWORLDWIDE) == START_WORLDWIDE_RETROS;
        bool isStartCT = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM2, SCROLLER_STARTWORLDWIDE) == START_WORLDWIDE_CTS;
        bool isStartRTS = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM2, SCROLLER_STARTWORLDWIDE) == START_WORLDWIDE_RTS;
        bool isStart200 = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM2, SCROLLER_STARTWORLDWIDE) == START_WORLDWIDE_200;
        bool isStartOTT = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM2, SCROLLER_STARTWORLDWIDE) == START_WORLDWIDE_OTT;
        bool isStartItemRain = settings.GetUserSettingValue(Settings::SETTINGSTYPE_FROOM2, SCROLLER_STARTWORLDWIDE) == START_WORLDWIDE_ITEMRAIN;
        u32 newContext = 0;
        Network::Mgr& netMgr = Pulsar::System::sInstance->netMgr;
        newContext = netMgr.hostContext;
        isCharRestrictLight = newContext & (1 << PULSAR_CHARRESTRICTLIGHT);
        isCharRestrictMid = newContext & (1 << PULSAR_CHARRESTRICTMID);
        isCharRestrictHeavy = newContext & (1 << PULSAR_CHARRESTRICTHEAVY);
        isKartRestrictKart = newContext & (1 << PULSAR_KARTRESTRICT);
        isKartRestrictBike = newContext & (1 << PULSAR_BIKERESTRICT);
        isExtendedTeams = newContext & (1 << PULSAR_EXTENDEDTEAMS);
        isStartRetro = newContext & (1 << PULSAR_STARTRETROS);
        isStartCT = newContext & (1 << PULSAR_STARTCTS);
        isStartRTS = newContext & (1 << PULSAR_STARTREGS);
        isStart200 = newContext & (1 << PULSAR_START200);
        isStartOTT = newContext & (1 << PULSAR_STARTOTT);
        isStartItemRain = newContext & (1 << PULSAR_STARTITEMRAIN);
        bool isThunderCloud = newContext & (1 << PULSAR_THUNDERCLOUD);
        netMgr.hostContext = newContext;

        u32 context = (isStartRetro << PULSAR_STARTRETROS) | (isStartCT << PULSAR_STARTCTS) | (isStartRTS << PULSAR_STARTREGS) | (isStart200 << PULSAR_START200) | (isStartOTT << PULSAR_STARTOTT) | (isStartItemRain << PULSAR_STARTITEMRAIN) | (isCharRestrictLight << PULSAR_CHARRESTRICTLIGHT) | (isCharRestrictMid << PULSAR_CHARRESTRICTMID) | (isCharRestrictHeavy << PULSAR_CHARRESTRICTHEAVY) | (isKartRestrictKart << PULSAR_KARTRESTRICT) | (isKartRestrictBike << PULSAR_BIKERESTRICT) | (isExtendedTeams << PULSAR_EXTENDEDTEAMS);
        Pulsar::System::sInstance->context = context;

        if (isStartCT || isStartRetro || isStartRTS || isStart200 || isStartOTT || isStartItemRain) {
            Pulsar::System::sInstance->context &= ~(1 << PULSAR_EXTENDEDTEAMS);
            Pulsar::System::sInstance->context &= ~(1 << PULSAR_CHARRESTRICTHEAVY);
            Pulsar::System::sInstance->context &= ~(1 << PULSAR_CHARRESTRICTMID);
            Pulsar::System::sInstance->context &= ~(1 << PULSAR_CHARRESTRICTLIGHT);
            Pulsar::System::sInstance->context &= ~(1 << PULSAR_KARTRESTRICT);
            Pulsar::System::sInstance->context &= ~(1 << PULSAR_BIKERESTRICT);
        }

        // Also exit the settings page to prevent weird graphical artefacts
        Page* topPage = SectionMgr::sInstance->curSection->GetTopLayerPage();
        PageId topId = topPage->pageId;
        if (topId == UI::SettingsPanel::id) {
            UI::SettingsPanel* panel = static_cast<UI::SettingsPanel*>(topPage);
            panel->OnBackPress(0);
        }

        // Extended Team VS start
        if (isExtendedTeams) {
            HandleExtendedTeamUpdates(src);
            UI::ExtendedTeamManager::sInstance->hasFriendRoomStarted = true;
        }
    }

    if (((src.messageType == UI::ExtendedTeamManager::MSG_TYPE_UPDATE_TEAMS) || (src.messageType == UI::ExtendedTeamManager::MSG_TYPE_UPDATE_TEAMS)) &&
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

/*
//ROOMPacket bits arrangement: 0-4 GPraces
//u8 racesPerGP = 0;



//Adds the settings to the free bits of the packet, only called for the host, msgType1 has 14 free bits as the game only has 4 gamemodes
void SetAllToSendPackets(RKNet::ROOMHandler& roomHandler, u32 packetArg) {
    RKNet::ROOMPacketReg packetReg ={ packetArg };
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    const u8 localAid = controller->subs[controller->currentSub].localAid;
    Pulsar::System* system = Pulsar::System::sInstance;
    if((packetReg.packet.messageType) == 1 && localAid == controller->subs[controller->currentSub].hostAid) {
        const u8 hostParam = Settings::Mgr::GetUserSettingValue(Settings::SETTINGSTYPE_FROOM2, RADIO_HOSTWINS);
        packetReg.packet.message |= hostParam << 2; //uses bit 2 of message

        const u8 gpParam = Settings::Mgr::GetUserSettingValue(Settings::SETTINGSTYPE_FROOM, SCROLLER_RACECOUNT);
        const u8 disableMiiHeads = Settings::Mgr::GetUserSettingValue(Settings::SETTINGSTYPE_FROOM2, RADIO_ALLOWMIIHEADS);
        packetReg.packet.message |= gpParam << 3; //uses bits 3-5
        packetReg.packet.message |= disableMiiHeads << 6; //uses bit 6
        packetReg.packet.message |= Settings::Mgr::GetUserSettingValue(Settings::SETTINGSTYPE_OTT, RADIO_OTTONLINE) << 7; //7 for OTT
        packetReg.packet.message |= Settings::Mgr::GetUserSettingValue(Settings::SETTINGSTYPE_KO, RADIO_KOENABLED) << 8; //8 for KO

        ConvertROOMPacketToData(packetReg.packet.message >> 2); //5 right now (2-8) + 1 reserved (9)
        packetReg.packet.message |= (System::sInstance->SetPackROOMMsg() << 0xA & 0b1111110000000000); //6 bits for packs (10-15)
    }
    for(int i = 0; i < 12; ++i) if(i != localAid) roomHandler.toSendPackets[i] = packetReg.packet;
}
kmBranch(0x8065ae70, SetAllToSendPackets);
//kmCall(0x805dce34, SetAllToSendPackets);
//kmCall(0x805dcd2c, SetAllToSendPackets);
//kmCall(0x805d9fe8, SetAllToSendPackets);

//Non-hosts extract the setting, store it and then return the packet without these bits
RKNet::ROOMPacket GetParamFromPacket(u32 packetArg, u8 aidOfSender) {
    RKNet::ROOMPacketReg packetReg ={ packetArg };
    if(packetReg.packet.messageType == 1) {
        const RKNet::Controller* controller = RKNet::Controller::sInstance;
        //Seeky's code to prevent guests from start the GP
        if(controller->subs[controller->currentSub].hostAid != aidOfSender) packetReg.packet.messageType = 0;
        else {
            ConvertROOMPacketToData((packetReg.packet.message & 0b0000001111111100) >> 2);
            System::sInstance->ParsePackROOMMsg(packetReg.packet.message >> 0xA);
        }
        packetReg.packet.message &= 0x3;
        Page* topPage = SectionMgr::sInstance->curSection->GetTopLayerPage();
        PageId topId = topPage->pageId;
        if(topId == UI::SettingsPanel::id) {
            UI::SettingsPanel* panel = static_cast<UI::SettingsPanel*>(topPage);
            panel->OnBackPress(0);
        }
    }
    return packetReg.packet;
}
kmBranch(0x8065af70, GetParamFromPacket);
*/

// Implements that setting
kmCall(0x806460B8, System::GetRaceCount);
kmCall(0x8064f51c, System::GetRaceCount);
}  // namespace Network
}  // namespace Pulsar