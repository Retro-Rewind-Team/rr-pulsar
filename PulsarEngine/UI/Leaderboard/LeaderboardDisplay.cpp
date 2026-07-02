#include <UI/Leaderboard/LeaderboardDisplay.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/RKNet/USER.hpp>
#include <UI/UI.hpp>
#include <PulsarSystem.hpp>
#include <Gamemodes/KO/KOMgr.hpp>

namespace Pulsar {
namespace UI {

static LeaderboardDisplayType displayLeaderboardType = LEADERBOARD_DISPLAY_NAMES;
static bool hasLeaderboardDisplayToggled = false;

void setLeaderboardDisplayType(LeaderboardDisplayType type) {
    displayLeaderboardType = type;
    hasLeaderboardDisplayToggled = false;
}

LeaderboardDisplayType getLeaderboardDisplayType() {
    return displayLeaderboardType;
}

// In case we want to add more display types (FC, flags, whatever..)
void nextLeaderboardDisplayType() {
    hasLeaderboardDisplayToggled = true;
    if (displayLeaderboardType == LEADERBOARD_DISPLAY_NAMES) {
        displayLeaderboardType = LEADERBOARD_DISPLAY_TIMES;
    } else if (displayLeaderboardType == LEADERBOARD_DISPLAY_TIMES) {
        if (RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_NONE) {
            displayLeaderboardType = LEADERBOARD_DISPLAY_NAMES;
        } else {
            displayLeaderboardType = LEADERBOARD_DISPLAY_FC;
        }
    } else if (displayLeaderboardType == LEADERBOARD_DISPLAY_FC) {
        displayLeaderboardType = LEADERBOARD_DISPLAY_NAMES;
    }
}

bool isSectionSpectatorLiveView(SectionId id) {
    return id == SECTION_P1_WIFI_VS_LIVEVIEW || id == SECTION_P2_WIFI_VS_LIVEVIEW || id == SECTION_P1_WIFI_BT_LIVEVIEW || id == SECTION_P2_WIFI_BT_LIVEVIEW;
}

void fillLeaderboardResult(CtrlRaceResult& result, u8 playerId) {
    const System* system = System::sInstance;
    const bool isKO = system->IsContext(PULSAR_MODE_KO);
    KO::Status koStatus = KO::NORMAL;
    if (isKO) koStatus = system->koMgr->GetPlayerStatus(playerId);

    if (koStatus != KO::NORMAL && !hasLeaderboardDisplayToggled) {
        u32 bmgId = UI::BMG_KO_TIE;
        if (koStatus == KO::KOD) bmgId = UI::BMG_KO_OUT;
        result.SetTextBoxMessage("player_name", bmgId);
        result.ResetTextBoxMessage("time");
    } else if (displayLeaderboardType == LEADERBOARD_DISPLAY_TIMES) {
        result.FillFinishTime(playerId);
    } else if (displayLeaderboardType == LEADERBOARD_DISPLAY_NAMES) {
        result.FillName(playerId);
    } else if (displayLeaderboardType == LEADERBOARD_DISPLAY_FC) {
        if (playerId < 12) {
            u8 aid = RKNet::Controller::sInstance->aidsBelongingToPlayerIds[playerId];
            u32 hudSlot = Racedata::sInstance->GetHudSlotId(playerId);

            u32 pid = 0;
            for (int i = 0; i < 32; i++) {
                if (DWC::MatchControl::sInstance->nodes[i].aid == aid) {
                    pid = DWC::MatchControl::sInstance->nodes[i].pid;
                    break;
                }
            }

            DWC::AccUserData dwcUserData;
            dwcUserData.gamecode = 'RMCJ';
            dwcUserData.gsProfileId = pid;
            u64 fc = DWC::CreateFriendKey(&dwcUserData);

            u32 fcParts[3];
            for (int j = 0; j < 3; ++j) {
                fcParts[j] = fc % 10000;
                fc /= 10000;
            }

            wchar_t fcText[16];
            swprintf(fcText, 16, L"%04d-%04d-%04d", fcParts[2], fcParts[1], fcParts[0]);

            if (wcscmp(fcText, L"0000-0000-0000") == 0) {
                swprintf(fcText, 16, L"----");
            }

            Text::Info textInfo;
            textInfo.strings[0] = fcText;

            result.SetTextBoxMessage("player_name", UI::BMG_TEXT, &textInfo);
            result.ResetTextBoxMessage("time");
        }
    }

    if (koStatus != KO::NORMAL) {
        ut::Color color;
        if (koStatus == KO::KOD) {
            color = 0xff0000c0;
        }
        if (koStatus == KO::DISCONNECTED) {
            color = 0xff0f00c0;
        }
        if (koStatus == KO::TIE) {
            color = 0xff00f0c0;
        }
        result.animator.GetAnimationGroupById(4).PlayAnimationAtFrame(6, 0.0f);
        lyt::Picture* selectBase = static_cast<nw4r::lyt::Picture*>(result.layout.GetPaneByName("select_base"));
        UI::ResetMatColor(selectBase, color);
        UI::UnbindRLMC(selectBase->material);
        selectBase->vertexColours[0] = color;
        selectBase->vertexColours[1] = color;
        selectBase->vertexColours[2] = color;
        selectBase->vertexColours[3] = color;
    }
}

void fillLeaderboardResults(int count, CtrlRaceResult** results) {
    // Copy the way the game loops here
    for (int i = 0; i < (count & 0xff); ++i) {
        const int position = (i + 1) & 0xff;
        const u8 playerId = Raceinfo::sInstance->playerIdInEachPosition[position - 1];
        fillLeaderboardResult(*results[i], playerId);
    }
}

const u32 WIIMOTE_DPAD_BUTTONS = WPAD::WPAD_BUTTON_LEFT | WPAD::WPAD_BUTTON_RIGHT | WPAD::WPAD_BUTTON_DOWN | WPAD::WPAD_BUTTON_UP;
const u32 CLASSIC_DPAD_BUTTONS = WPAD::WPAD_CL_BUTTON_UP | WPAD::WPAD_CL_BUTTON_LEFT | WPAD::WPAD_CL_BUTTON_DOWN | WPAD::WPAD_CL_BUTTON_RIGHT;
const u32 GC_DPAD_BUTTONS = PAD::PAD_BUTTON_LEFT | PAD::PAD_BUTTON_RIGHT | PAD::PAD_BUTTON_DOWN | PAD::PAD_BUTTON_UP;

bool checkLeaderboardDisplaySwapInputs() {
    const Input::RealControllerHolder* controllerHolder = SectionMgr::sInstance->pad.padInfos[0].controllerHolder;
    const ControllerType controllerType = controllerHolder->curController->GetType();
    const u16 inputs = controllerHolder->inputStates[0].buttonRaw;
    const u16 newInputs = (inputs & ~controllerHolder->inputStates[1].buttonRaw);

    bool swapDisplayType = false;
    switch (controllerType) {
        case NUNCHUCK:
        case WHEEL:
            swapDisplayType = (newInputs & WIIMOTE_DPAD_BUTTONS) != 0;
            break;
        case CLASSIC:
            swapDisplayType = (newInputs & (WPAD::WPAD_CL_TRIGGER_L | WPAD::WPAD_CL_TRIGGER_R | CLASSIC_DPAD_BUTTONS)) != 0;
            break;
        default:
            swapDisplayType = (newInputs & (PAD::PAD_BUTTON_L | PAD::PAD_BUTTON_R | GC_DPAD_BUTTONS)) != 0;
            break;
    }
    return swapDisplayType;
}

}  // namespace UI
}  // namespace Pulsar
