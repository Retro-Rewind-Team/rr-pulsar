#include <AutoTrackSelect/ExpFroomMessages.hpp>
#include <Settings/Settings.hpp>
#include <Settings/SettingsParam.hpp>
#include <SlotExpansion/CupsConfig.hpp>
#include <SlotExpansion/UI/ExpansionUIMisc.hpp>
#include <Gamemodes/OnlineTT/OTTRegional.hpp>

namespace Pulsar {
namespace UI {
bool ExpFroomMessages::isOnModeSelection = false;
s32 ExpFroomMessages::clickedButtonIdx = 0;

// Expand message count from 4 to 10 for worldwide start options
static void OnStartButtonFroomMsgActivate() {
    register ExpFroomMessages* msg;
    asm(mr msg, r31;);
    msg->msgCount = 10;  // 4 normal + 6 worldwide options
}
kmCall(0x805dc480, OnStartButtonFroomMsgActivate);

u32 CorrectModeButtonsBMG(const RKNet::ROOMPacket& packet) {
    register u32 rowIdx;
    asm(mr rowIdx, r24;);  // r24 contains the actual message index
    register const ExpFroomMessages* messages;
    asm(mr messages, r19;);
    u32 bmgId;
    bmgId = Pages::FriendRoomManager::GetMessageBmg(packet, 0);

    switch (rowIdx) {
        case 4:
            return BMG_RETRO_START_MESSAGE;
        case 5:
            return BMG_CUSTOM_START_MESSAGE;
        case 6:
            return BMG_REGS_START_MESSAGE;
        case 7:
            return BMG_200_START_MESSAGE;
        case 8:
            return BMG_OTT_START_MESSAGE;
        case 9:
            return BMG_ITEMRAIN_START_MESSAGE;
    }

    if (rowIdx == 0) {
        const bool isOTT = Settings::Mgr::Get().GetUserSettingValue(Settings::SETTINGSTYPE_OTT, RADIO_OTTONLINE) == OTTSETTING_ONLINE_NORMAL;
        const bool isKO = Settings::Mgr::Get().GetUserSettingValue(Settings::SETTINGSTYPE_KO, RADIO_KOENABLED) != KOSETTING_DISABLED;
        const bool isExtendedTeam = Settings::Mgr::Get().GetUserSettingValue(Settings::SETTINGSTYPE_EXTENDEDTEAMS, RADIO_EXTENDEDTEAMSENABLED) == EXTENDEDTEAMS_ENABLED;
        const bool isRoyale = Settings::Mgr::Get().GetUserSettingValue(Settings::SETTINGSTYPE_KOROYALE, RADIO_KOROYALEENABLED) == KOROYALESETTING_ENABLED;

        if (isOTT && isKO) {
            bmgId = BMG_PLAY_OTTKO;
        } else if (isKO && isRoyale && !isOTT) {
            bmgId = BMG_PLAY_KOROYALE;
        } else if (isOTT) {
            bmgId = BMG_PLAY_OTT;
        } else if (isRoyale) {
            bmgId = BMG_PLAY_ROYALE;
        } else if (isKO) {
            bmgId = BMG_PLAY_KO;
        } else if (isExtendedTeam) {
            bmgId = BMG_EXTENDEDTEAMS_PLAY;
        } else {
            bmgId = BMG_PLAY_GP;
        }
    }
    return bmgId;
}
kmCall(0x805dcb74, CorrectModeButtonsBMG);

static void RemapAndStoreSentMessage() {
    register u32 packet;
    register u32 manager;
    asm(mr packet, r30;);
    asm(mr manager, r28;);

    u32 message = (packet >> 8) & 0xFFFF;
    if (message >= 4 && message <= 9) {
        packet = packet & 0xFF0000FF;
    }

    *(volatile u32*)((u8*)manager + 0x2c60) = packet;
}
kmCall(0x805dce38, RemapAndStoreSentMessage);

void CorrectRoomStartButton(Pages::Globe::MessageWindow& control, u32 bmgId, Text::Info* info) {
    Network::SetGlobeMsgColor(control, -1);
    if (bmgId == BMG_PLAY_GP || bmgId == BMG_PLAY_TEAM_GP) {
        const u32 hostContext = System::sInstance->netMgr.hostContext;
        const u32 hostContext2 = System::sInstance->netMgr.hostContext2;
        const bool isOTT = hostContext & (1 << PULSAR_MODE_OTT);
        const bool isKO = hostContext & (1 << PULSAR_MODE_KO) || hostContext & (1 << PULSAR_MODE_LAPKO) || hostContext2 & (1 << PULSAR_MODE_BATTLEROYALE);
        const bool isExtendedTeam = hostContext & (1 << PULSAR_EXTENDEDTEAMS);
        const bool isStartRetro = hostContext & (1 << PULSAR_STARTRETROS);
        const bool isStartCT = hostContext & (1 << PULSAR_STARTCTS);
        const bool isStartRTS = hostContext & (1 << PULSAR_STARTREGS);
        const bool isStart200 = hostContext & (1 << PULSAR_START200);
        const bool isStartOTT = hostContext & (1 << PULSAR_STARTOTT);
        const bool isStartItemRain = hostContext & (1 << PULSAR_STARTITEMRAIN);
        const bool isRoyale = hostContext2 & (1 << PULSAR_MODE_BATTLEROYALE);

        if (isOTT && isKO) {
            bmgId = BMG_PLAY_OTTKO;
        } else if (isRoyale && isKO && !isOTT) {
            bmgId = BMG_PLAY_KOROYALE;
        } else if (isOTT) {
            bmgId = BMG_PLAY_OTT;
        } else if (isRoyale) {
            bmgId = BMG_PLAY_ROYALE;
        } else if (isKO) {
            bmgId = BMG_PLAY_KO;
        } else if (isExtendedTeam) {
            bmgId = BMG_EXTENDEDTEAMS_PLAY;
        } else if (isStartRetro) {
            bmgId = BMG_RETRO_START_MESSAGE;
        } else if (isStartCT) {
            bmgId = BMG_CUSTOM_START_MESSAGE;
        } else if (isStartRTS) {
            bmgId = BMG_REGS_START_MESSAGE;
        } else if (isStart200) {
            bmgId = BMG_200_START_MESSAGE;
        } else if (isStartOTT) {
            bmgId = BMG_OTT_START_MESSAGE;
        } else if (isStartItemRain) {
            bmgId = BMG_ITEMRAIN_START_MESSAGE;
        }
    }
    control.SetMessage(bmgId, info);
}
kmCall(0x805e4df4, CorrectRoomStartButton);

}  // namespace UI
}  // namespace Pulsar
