#include <kamek.hpp>
#include <runtimeWrite.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/UI/Page/Page.hpp>
#include <MarioKartWii/UI/Ctrl/UIControl.hpp>
#include <MarioKartWii/Input/InputManager.hpp>
#include <MarioKartWii/UI/Section/Section.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/RKSYS/RKSYSMgr.hpp>
#include <Network/Rating/PlayerRating.hpp>
#include <hooks.hpp>

namespace Pulsar {
namespace UI {

// Allow pausing online in RaceHUD::initInputs
kmWrite32(0x808567d4, 0x60000000);

kmRuntimeUse(0x8051e85c);
void SetInputPaused(bool paused) {
    Input::Manager* sInstance = Input::Manager::sInstance;
    if (sInstance) {
        sInstance->isPaused = paused;
        for (int i = 0; i < 4; i++) {
            sInstance->realControllerHolders[i].blockInputs = paused;
            if (paused) {
                reinterpret_cast<void (*)(Input::State*)>(kmRuntimeAddr(0x8051e85c))(&sInstance->realControllerHolders[i].inputStates[0]);
            }
        }
    }
}

// Prevent race freeze when pausing online
kmRuntimeUse(0x809c4680);
void SetRaceHUDVisibility(bool visible) {
    Page* raceHUD = *reinterpret_cast<Page**>(kmRuntimeAddr(0x809c4680));
    if (!raceHUD) return;
    
    // ControlGroup is at page + 0x24
    // controlArray is at group + 0x0
    // controlCount is at group + 0x10
    u8* groupPtr = reinterpret_cast<u8*>(raceHUD) + 0x24;
    UIControl** controlArray = *reinterpret_cast<UIControl***>(groupPtr + 0x0);
    u32 controlCount = *reinterpret_cast<u32*>(groupPtr + 0x10);
    
    if (controlArray) {
        for (u32 i = 0; i < controlCount; ++i) {
            UIControl* ctrl = controlArray[i];
            if (ctrl) {
                ctrl->isHidden = !visible;
            }
        }
    }
}

// Add Pause Pages to online sections
void AddOnlinePausePages() {
    SetInputPaused(false); // Reset pause state on section load
    Section* section = SectionMgr::sInstance->curSection;
    SectionId sid = section->sectionId;
    
    // Online Race Sections (including Live View)
    if (sid == SECTION_P1_WIFI_VS || sid == SECTION_P2_WIFI_VS || 
        sid == SECTION_P1_WIFI_FRIEND_VS || sid == SECTION_P1_WIFI_FRIEND_TEAMVS ||
        sid == SECTION_P2_WIFI_FRIEND_VS || sid == SECTION_P2_WIFI_FRIEND_TEAMVS ||
        sid == SECTION_P1_WIFI_VS_LIVEVIEW || sid == SECTION_P2_WIFI_VS_LIVEVIEW) {
        section->CreateAndInitPage(PAGE_VS_RACE_PAUSE_MENU);
        section->CreateAndInitPage(PAGE_QUIT_CONFIRMATION);
    }
    // Online Battle Sections (including Live View)
    else if (sid == SECTION_P1_WIFI_BT || sid == SECTION_P2_WIFI_BT ||
             sid == SECTION_P1_WIFI_FRIEND_BALLOON || sid == SECTION_P1_WIFI_FRIEND_COIN ||
             sid == SECTION_P2_WIFI_FRIEND_BALLOON || sid == SECTION_P2_WIFI_FRIEND_COIN ||
             sid == SECTION_P1_WIFI_BT_LIVEVIEW || sid == SECTION_P2_WIFI_BT_LIVEVIEW) {
        section->CreateAndInitPage(PAGE_BATTLE_PAUSE_MENU);
        section->CreateAndInitPage(PAGE_QUIT_CONFIRMATION);
    }
}
static SectionLoadHook AddOnlinePausePagesHook(AddOnlinePausePages);

// NOP ScheduleDisconnection in RaceMenuPage::onButtonFront
kmWrite32(0x8085a2a4, 0x60000000);
kmWrite32(0x8085b774, 0x60000000);

// Redirect "Quit" to Lobby in ChangeSectionBySceneChange
SectionId GetOnlineQuitSection() {
    const Racedata* racedata = Racedata::sInstance;
    GameMode mode = racedata->menusScenario.settings.gamemode;
    bool isPrivate = (mode == MODE_PRIVATE_VS || mode == MODE_PRIVATE_BATTLE);
    if (racedata->racesScenario.localPlayerCount > 1) {
        return isPrivate ? SECTION_P2_WIFI_FROM_FROOM_RACE : SECTION_P2_WIFI;
    }
    return isPrivate ? SECTION_P1_WIFI_FROM_FROOM_RACE : SECTION_P1_WIFI;
}

void RedirectOnlineQuit(SectionMgr* mgr, SectionId sectionId, u32 anim) {
    SectionId next = sectionId;
    if (sectionId == SECTION_MAIN_MENU_FROM_MENU) {
        const Racedata* racedata = Racedata::sInstance;
        GameMode mode = racedata->menusScenario.settings.gamemode;
        // Check if we are in an online mode
        if (mode >= MODE_PRIVATE_VS && mode <= MODE_PRIVATE_BATTLE) {
            // Apply 200 VR/BR penalty for leaving through the pause menu
            const Raceinfo* raceInfo = Raceinfo::sInstance;
            const RKSYS::Mgr* rksys = RKSYS::Mgr::sInstance;
            if (raceInfo && rksys && rksys->curLicenseId < 4) {
                const u32 licenseId = rksys->curLicenseId;
                Section* section = SectionMgr::sInstance->curSection;
                SectionId sid = section->sectionId;
                if (mode == MODE_PUBLIC_VS && sid != SECTION_P1_WIFI_VS_LIVEVIEW && sid != SECTION_P2_WIFI_VS_LIVEVIEW) {
                    const float vr = PointRating::GetUserVR(licenseId);
                    PointRating::SetUserVR(licenseId, vr - 2.10f);
                }
                else if (mode == MODE_PUBLIC_BATTLE && sid != SECTION_P1_WIFI_BT_LIVEVIEW && sid != SECTION_P2_WIFI_BT_LIVEVIEW) {
                    const float br = PointRating::GetUserBR(licenseId);
                    PointRating::SetUserBR(licenseId, br - 2.10f);
                }
            }

            SetInputPaused(false);
            SetRaceHUDVisibility(true);
            next = GetOnlineQuitSection();

            RKNet::Controller* controller = RKNet::Controller::sInstance;
            if (controller) {
                OS::LockMutex(&controller->mutex);
                bool isPrivate = (mode == MODE_PRIVATE_VS || mode == MODE_PRIVATE_BATTLE);
                if (isPrivate) {
                    const RKNet::ControllerSub& sub = controller->subs[controller->currentSub];
                    if (sub.localAid == sub.hostAid) {
                        controller->localStatusData.status = RKNet::FRIEND_STATUS_FROOM_OPEN;
                    }
                    else {
                        controller->localStatusData.status = RKNet::FRIEND_STATUS_FROOM_NON_HOST;
                    }
                }
                else {
                    controller->localStatusData.status = RKNet::FRIEND_STATUS_IDLE;
                }
                controller->localStatusData.playerCount = controller->subs[controller->currentSub].localPlayerCount;
                controller->localStatusData.curRace = 0;
                OS::UnlockMutex(&controller->mutex);
                controller->ResetRH1andROOM();
            }
        }
    }
    mgr->SetNextSection(next, anim);
}
kmCall(0x806024dc, RedirectOnlineQuit);

void OnlineHUDVisibilityHook() {
    const Racedata* racedata = Racedata::sInstance;
    GameMode mode = racedata->menusScenario.settings.gamemode;
    if (mode >= MODE_PRIVATE_VS && mode <= MODE_PRIVATE_BATTLE) {
        const Raceinfo* raceInfo = Raceinfo::sInstance;
        if (raceInfo && raceInfo->IsAtLeastStage(RACESTAGE_IS_FINISHING)) {
            Section* section = 0;
            if (SectionMgr::sInstance) section = SectionMgr::sInstance->curSection;
            if (section) {
                // If the player finishes while paused, the underlying race UI can transition/dispose.
                // Force-close pause layers to prevent stale pages from being updated during section layer processing.
                Page* vsPause = section->pages[PAGE_VS_RACE_PAUSE_MENU];
                Page* btPause = section->pages[PAGE_BATTLE_PAUSE_MENU];
                Page* quitConf = section->pages[PAGE_QUIT_CONFIRMATION];
                if (vsPause && vsPause->currentState != STATE_DEACTIVATED) vsPause->EndState();
                if (btPause && btPause->currentState != STATE_DEACTIVATED) btPause->EndState();
                if (quitConf && quitConf->currentState != STATE_DEACTIVATED) quitConf->EndState();
            }

            // Always release input pause at race finish; avoid touching RaceHUD pointers here.
            SetInputPaused(false);
            return;
        }

        Input::Manager* input = Input::Manager::sInstance;
        if (input && input->isPaused) {
            Section* section = SectionMgr::sInstance->curSection;
            bool isPauseOpen = false;
            
            // Check if pause menu or quit confirmation is open
            Page* vsPause = section->pages[PAGE_VS_RACE_PAUSE_MENU];
            Page* btPause = section->pages[PAGE_BATTLE_PAUSE_MENU];
            Page* quitConf = section->pages[PAGE_QUIT_CONFIRMATION];
            
            if ((vsPause && vsPause->currentState != STATE_DEACTIVATED) ||
                (btPause && btPause->currentState != STATE_DEACTIVATED) ||
                (quitConf && quitConf->currentState != STATE_DEACTIVATED)) {
                isPauseOpen = true;
            }
            
            if (!isPauseOpen) {
                SetInputPaused(false);
                SetRaceHUDVisibility(true);
            } else {
                SetRaceHUDVisibility(false);
            }
        }
    }
}
static RaceFrameHook OnlineHUDVisibility(OnlineHUDVisibilityHook);

kmRuntimeUse(0x808600dc);
void OnlinePauseControl(void* r3) {
    const Racedata* racedata = Racedata::sInstance;
    GameMode mode = racedata->menusScenario.settings.gamemode;
    if (mode >= MODE_PRIVATE_VS && mode <= MODE_PRIVATE_BATTLE) {
        const Raceinfo* raceInfo = Raceinfo::sInstance;
        if (raceInfo && raceInfo->IsAtLeastStage(RACESTAGE_RACE)) {
            SetRaceHUDVisibility(false);
            SetInputPaused(true);
        }
        return;
    }
    reinterpret_cast<void (*)(void*)>(kmRuntimeAddr(0x808600dc))(r3);
}
kmCall(0x80856b38, OnlinePauseControl);

kmRuntimeUse(0x80860100);
void OnlineUnpauseControl(void* r3) {
    const Racedata* racedata = Racedata::sInstance;
    GameMode mode = racedata->menusScenario.settings.gamemode;
    if (mode >= MODE_PRIVATE_VS && mode <= MODE_PRIVATE_BATTLE) {
        const Raceinfo* raceInfo = Raceinfo::sInstance;
        if (raceInfo && raceInfo->IsAtLeastStage(RACESTAGE_IS_FINISHING)) {
            // Race is ending; ensure we don't keep any pause state alive.
            SetInputPaused(false);
            return;
        }
        SetRaceHUDVisibility(true);
        SetInputPaused(false);
        return;
    }
    reinterpret_cast<void (*)(void*)>(kmRuntimeAddr(0x80860100))(r3);
}
kmCall(0x8085a080, OnlineUnpauseControl);
kmCall(0x8085a0dc, OnlineUnpauseControl);
kmCall(0x8085a200, OnlineUnpauseControl);
kmCall(0x8085a260, OnlineUnpauseControl);

int GetOnlineVSPausePageId() {
    const Raceinfo* raceInfo = Raceinfo::sInstance;
    if (raceInfo && !raceInfo->IsAtLeastStage(RACESTAGE_RACE)) {
        return -1;
    }
    return PAGE_VS_RACE_PAUSE_MENU;
}

int GetOnlineBTPausePageId() {
    const Raceinfo* raceInfo = Raceinfo::sInstance;
    if (raceInfo && !raceInfo->IsAtLeastStage(RACESTAGE_RACE)) {
        return -1;
    }
    return PAGE_BATTLE_PAUSE_MENU;
}

kmBranch(0x806335b8, GetOnlineVSPausePageId);
kmBranch(0x806337f8, GetOnlineVSPausePageId);
kmBranch(0x80633768, GetOnlineVSPausePageId);
kmBranch(0x80633948, GetOnlineBTPausePageId);
kmBranch(0x80633888, GetOnlineBTPausePageId);
kmBranch(0x806336d8, GetOnlineVSPausePageId);
kmBranch(0x80633648, GetOnlineVSPausePageId);

} // namespace UI
} // namespace Pulsar