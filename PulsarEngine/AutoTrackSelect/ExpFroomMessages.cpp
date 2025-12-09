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

// void ExpFroomMessages::OnModeButtonClick(PushButton& button, u32 hudSlotId) {
//     this->clickedButtonIdx = button.buttonId;
//     this->OnActivate();
// }

// void ExpFroomMessages::OnCourseButtonClick(PushButton& button, u32 hudSlotId) {
//     CupsConfig* cupsConfig = CupsConfig::sInstance;
//     u32 clickedIdx = clickedButtonIdx;
//     s32 id = button.buttonId;
//     PulsarId pulsarId = static_cast<PulsarId>(id);
//     if (clickedIdx < 2) {
//         if (id == this->msgCount - 1) {
//             pulsarId = cupsConfig->RandomizeTrack();
//         }
//         else {
//             PulsarCupId cupId =  static_cast<PulsarCupId>(cupsConfig->ConvertTrack_IdxToPulsarId(id) / 4);
//             pulsarId = cupsConfig->ConvertTrack_PulsarCupToTrack(cupId, id % 4);
//             //pulsarId = cupsConfig->ConvertTrack_IdxToPulsarId(id); //vs or teamvs
//         }
//     }
//     else pulsarId = static_cast<PulsarId>(pulsarId + 0x20U); //Battle
//     cupsConfig->SetWinning(pulsarId);
//     PushButton& clickedButton = this->messages[0].buttons[clickedIdx];
//     clickedButton.buttonId = clickedIdx;
//     Pages::FriendRoomMessages::OnModeButtonClick(clickedButton, 0); //hudslot is unused
// }

// //kmWrite32(0x805dc47c, 0x7FE3FB78); //Get Page in r3
// static void OnStartButtonFroomMsgActivate() {
//     register ExpFroomMessages* msg;
//     asm(mr msg, r31;);

//     if (!Settings::Mgr::Get().GetUserSettingValue(Settings::SETTINGSTYPE_FROOM2, RADIO_HOSTWINS)) {
//         msg->onModeButtonClickHandler.ptmf = &Pages::FriendRoomMessages::OnModeButtonClick;
//         msg->msgCount = 4;
//     }
//     else {
//         for (int i = 0; i < 4; ++i) msg->messages[0].buttons[i].HandleDeselect(0, -1);
//         if (msg->isOnModeSelection) {
//             msg->isOnModeSelection = false;
//             if (msg->clickedButtonIdx >= 2) msg->msgCount = 10;
//             else msg->msgCount = CupsConfig::sInstance->GetEffectiveTrackCount() + 1;
//             msg->onModeButtonClickHandler.ptmf = &ExpFroomMessages::OnCourseButtonClick;

//         }
//         else {
//             msg->isOnModeSelection = true;
//             msg->msgCount = 4;
//             msg->onModeButtonClickHandler.ptmf = &ExpFroomMessages::OnModeButtonClick;
//         }
//     }
// }
// kmCall(0x805dc480, OnStartButtonFroomMsgActivate);
// //kmWrite32(0x805dc498, 0x60000000);
// //kmWrite32(0x805dc4c0, 0x60000000);

// static void OnBackPress(ExpFroomMessages& msg) {
//     if (Settings::Mgr::Get().GetUserSettingValue(Settings::SETTINGSTYPE_FROOM2, RADIO_HOSTWINS) && msg.location == 1) {
//         if (!msg.isOnModeSelection) {
//             msg.isEnding = false;
//             msg.OnActivate();
//         }
//         else msg.isOnModeSelection = false;
//     }
// }
// kmBranch(0x805dd32c, OnBackPress);

// static void OnBackButtonClick() {
//     OnBackPress(*SectionMgr::sInstance->curSection->Get<ExpFroomMessages>());
// }
// kmBranch(0x805dd314, OnBackButtonClick);

// kmWrite32(0x805dcb6c, 0x7EC4B378);  // Get the loop idx in r4
u32 CorrectModeButtonsBMG(const RKNet::ROOMPacket& packet) {
    register u32 rowIdx;
    asm(mr rowIdx, r22;);
    register const ExpFroomMessages* messages;
    asm(mr messages, r19;);
    u32 bmgId;
    bmgId = Pages::FriendRoomManager::GetMessageBmg(packet, 0);
    if (rowIdx == 0) {
        const u32 hostContext = System::sInstance->netMgr.hostContext;
        const u32 wwSetting = Settings::Mgr::Get().GetUserSettingValue(Settings::SETTINGSTYPE_FROOM2, SCROLLER_STARTWORLDWIDE);
        const u32 isOTT = Settings::Mgr::Get().GetUserSettingValue(Settings::SETTINGSTYPE_OTT, RADIO_OTTONLINE) == OTTSETTING_ONLINE_NORMAL;
        const u32 isKO = Settings::Mgr::Get().GetUserSettingValue(Settings::SETTINGSTYPE_KO, RADIO_KOENABLED) != KOSETTING_DISABLED;
        const u32 isExtendedTeam = Settings::Mgr::Get().GetUserSettingValue(Settings::SETTINGSTYPE_EXTENDEDTEAMS, RADIO_EXTENDEDTEAMSENABLED) == EXTENDEDTEAMS_ENABLED;
        const bool isStartRetro = wwSetting == START_WORLDWIDE_RETROS;
        const bool isStartCT = wwSetting == START_WORLDWIDE_CTS;
        const bool isStartRTS = wwSetting == START_WORLDWIDE_RTS;
        const bool isStart200 = wwSetting == START_WORLDWIDE_200;
        const bool isStartOTT = wwSetting == START_WORLDWIDE_OTT;
        const bool isStartItemRain = wwSetting == START_WORLDWIDE_ITEMRAIN;
        if (isOTT && !isStartCT && !isStartRetro && !isStartRTS && !isStart200 && !isStartOTT && !isStartItemRain) {
            bmgId = BMG_PLAY_OTT;
        } else if (isKO && !isStartCT && !isStartRetro && !isStartRTS && !isStart200 && !isStartOTT && !isStartItemRain) {
            bmgId = BMG_PLAY_KO;
        } else if (isOTT && isKO && !isStartCT && !isStartRetro && !isStartRTS && !isStart200 && !isStartOTT && !isStartItemRain) {
            bmgId = BMG_PLAY_OTTKO;
        } else if (isExtendedTeam && !isStartCT && !isStartRetro && !isStartRTS && !isStart200 && !isStartOTT && !isStartItemRain) {
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
        } else {
            bmgId = BMG_PLAY_GP;
        }
    }
    return bmgId;
}
kmCall(0x805dcb74, CorrectModeButtonsBMG);

void CorrectRoomStartButton(Pages::Globe::MessageWindow& control, u32 bmgId, Text::Info* info) {
    Network::SetGlobeMsgColor(control, -1);
    if (bmgId == BMG_PLAY_GP || bmgId == BMG_PLAY_TEAM_GP) {
        const u32 hostContext = System::sInstance->netMgr.hostContext;
        const bool isOTT = hostContext & (1 << PULSAR_MODE_OTT);
        const bool isKO = hostContext & (1 << PULSAR_MODE_KO) || hostContext & (1 << PULSAR_MODE_LAPKO);
        const bool isExtendedTeam = hostContext & (1 << PULSAR_EXTENDEDTEAMS);
        const bool isStartRetro = hostContext & (1 << PULSAR_STARTRETROS);
        const bool isStartCT = hostContext & (1 << PULSAR_STARTCTS);
        const bool isStartRTS = hostContext & (1 << PULSAR_STARTREGS);
        const bool isStart200 = hostContext & (1 << PULSAR_START200);
        const bool isStartOTT = hostContext & (1 << PULSAR_STARTOTT);
        const bool isStartItemRain = hostContext & (1 << PULSAR_STARTITEMRAIN);
        if (isOTT || isKO) {
            const bool isTeam = bmgId == BMG_PLAY_TEAM_GP;
            bmgId = (BMG_PLAY_OTT - 1) + isOTT + isKO * 2 + isTeam * 3;
        }

        if (isExtendedTeam && !isStartCT && !isStartRetro && !isStartRTS && !isStart200 && !isStartOTT && !isStartItemRain) {
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