#include <kamek.hpp>
#include <MarioKartWii/UI/Page/RaceMenu/RaceMenu.hpp>
#include <MarioKartWii/UI/Page/Other/TTSplits.hpp>
#include <MarioKartWii/UI/Page/Menu/MultiDriftSelect.hpp>
#include <MarioKartWii/UI/Page/Other/VR.hpp>
#include <MarioKartWii/GlobalFunctions.hpp>
#include <PulsarSystem.hpp>
#include <Gamemodes/OnlineTT/OnlineTT.hpp>
#include <Gamemodes/KO/KOMgr.hpp>
#include <Gamemodes/KO/KORaceEndPage.hpp>
#include <Network/PulSELECT.hpp>
#include <Network/PacketExpansion.hpp>
#include <AutoTrackSelect/ChooseNextTrack.hpp>
#include <Settings/UI/SettingsPanel.hpp>

namespace Pulsar {
// For hooks which are shared by different things
namespace UI {

PageId TTSplitsGetNextPage(const Pages::TTSplits& splits) {
    const RacedataScenario& scenario = Racedata::sInstance->racesScenario;
    const GameMode mode = scenario.settings.gamemode;
    const bool isOTTF = System::sInstance->IsContext(PULSAR_MODE_OTT) &&
                            (RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_HOST) ||
                        (RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_NONHOST) || (RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_NONE && mode != MODE_TIME_TRIAL);
    const bool isOTTW = System::sInstance->IsContext(PULSAR_MODE_OTT) &&
                            (RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_VS_REGIONAL) ||
                        (RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_JOINING_REGIONAL);

    const SectionId sectionId = SectionMgr::sInstance->curSection->sectionId;
    if (isOTTW)
        return PAGE_WW_LEADERBOARDS_UPDATE;
    else if (isOTTF || sectionId == SECTION_GP)
        return PAGE_GPVS_LEADERBOARD_UPDATE;
    else if (sectionId == SECTION_TT || sectionId == SECTION_GHOST_RACE_1 || sectionId == SECTION_GHOST_RACE_2)
        return PAGE_TT_LEADERBOARDS;
    else if (sectionId >= SECTION_WATCH_GHOST_FROM_CHANNEL && sectionId <= SECTION_WATCH_GHOST_FROM_MENU)
        return PAGE_GHOST_REPLAY_PAUSE_MENU;  // Enhanced Replay
    return PAGE_NONE;
}
kmBranch(0x808561dc, TTSplitsGetNextPage);

void LoadCorrectPageAfterMultiDrift(Pages::MultiDriftSelect* page, u32 animDirection, float animLength) {
    page->EndStateAnimated(animLength, animDirection);
    System* system = System::sInstance;
    SectionMgr* sectionMgr = SectionMgr::sInstance;
    if (system->ottMgr.voteState == OTT::COMBO_SELECTION) {
        system->ottMgr.voteState = OTT::COMBO_SELECTED;
        Network::ExpSELECTHandler& handler = Network::ExpSELECTHandler::Get();
        for (int i = 0; i < 2; ++i) {
            handler.toSendPacket.playersData[i].character = sectionMgr->sectionParams->characters[i];
            handler.toSendPacket.playersData[i].kart = sectionMgr->sectionParams->karts[i];
        }
        handler.toSendPacket.allowChangeComboStatus = Network::SELECT_COMBO_SELECTED;
    } else if (sectionMgr->curSection->Get<Pages::VR>() != nullptr && page->nextPageId == static_cast<PageId>(PULPAGE_SETTINGS)) {
        ExpSection::GetSection()->CreateAndInitPage(*ExpSection::GetSection(), PULPAGE_SETTINGS);
        return;
    } else if (sectionMgr->curSection->Get<Pages::VR>() != nullptr) {
        page->nextPageId = IsBattle() ? PAGE_BATTLE_CUP_SELECT : PAGE_CUP_SELECT;
    }
}
kmCall(0x8084b68c, LoadCorrectPageAfterMultiDrift);

// MultiDriftSelect::CreateControl writes drift state while controls are being created.
// In 2P online this can leave both locals in manual before voting starts.
kmWrite32(0x8084b120, 0x60000000);

// if more custom racemenus added in pulsar, make the func virtual
void RaceMenuExtraControls(Pages::RaceMenu& page, u32 gameControlCount) {
    const SectionId curSectionId = SectionMgr::sInstance->curSection->sectionId;
    if (curSectionId >= SECTION_P1_WIFI_FRIEND_VS || curSectionId >= SECTION_P2_WIFI_FRIEND_COIN) {
        const ExpSection* section = ExpSection::GetSection();
        if (page.pageId == ChooseNextTrack::fakeId && section->GetPulPage<ChooseNextTrack>() == &page)
            return static_cast<ChooseNextTrack&>(page).InitExtraControls(gameControlCount);
        else if (page.pageId == KO::RaceEndPage::fakeId && section->GetPulPage<KO::RaceEndPage>() == &page)
            return static_cast<KO::RaceEndPage&>(page).InitExtraControls(gameControlCount);
    }
    page.InitControlGroup(gameControlCount);
}
kmCall(0x80858ebc, RaceMenuExtraControls);
}  // namespace UI

}  // namespace Pulsar