#include <kamek.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/RKNet/Select.hpp>
#include <MarioKartWii/System/random.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>
#include <Settings/Settings.hpp>
#include <Network/Network.hpp>
#include <Network/PulSELECT.hpp>
#include <PulsarSystem.hpp>
#include <AutoTrackSelect/ChooseNextTrack.hpp>
#include <Gamemodes/KO/KOMgr.hpp>

namespace Pulsar {
namespace Network {

static bool IsWiFiMenuSection(SectionId id) {
    switch (id) {
        case SECTION_P1_WIFI:
        case SECTION_P1_WIFI_FROM_FROOM_RACE:
        case SECTION_P1_WIFI_FROM_FIND_FRIEND:
        case SECTION_P2_WIFI:
        case SECTION_P2_WIFI_FROM_FROOM_RACE:
        case SECTION_P2_WIFI_FROM_FIND_FRIEND:
        case SECTION_WIFI_DISCONNECT_ERROR:
        case SECTION_WIFI_DISCONNECT_GENERAL:
            return true;
        default:
            return false;
    }
}

static void ResetTrackBlockingOnRoomEnd() {
    SectionMgr* sectionMgr = SectionMgr::sInstance;
    if (sectionMgr == nullptr || sectionMgr->curSection == nullptr) return;

    const SectionId sectionId = sectionMgr->curSection->sectionId;
    if (!IsWiFiMenuSection(sectionId)) return;

    System* system = System::sInstance;
    if (system == nullptr) return;

    Mgr& netMgr = system->netMgr;
    const u32 blockingCount = system->GetInfo().GetTrackBlocking();

    if (netMgr.lastTracks != nullptr && blockingCount > 0) {
        for (u32 i = 0; i < blockingCount; ++i) {
            netMgr.lastTracks[i] = PULSARID_NONE;
        }
        netMgr.curBlockingArrayIdx = 0;
        netMgr.lastGroupedTrackPlayed = false;
    }
}
static SectionLoadHook resetTrackBlockingHook(ResetTrackBlockingOnRoomEnd);

static void CalcSectionAfterRace(SectionMgr* sectionMgr, SectionId id) {
    UI::ChooseNextTrack* choosePage = reinterpret_cast<UI::ExpSection*>(sectionMgr->curSection)->GetPulPage<UI::ChooseNextTrack>();
    const System* system = System::sInstance;
    // if(choosePage != nullptr) id = choosePage->ProcessHAW(id);
    if (id != SECTION_NONE) {
        if (system->IsContext(PULSAR_MODE_KO)) id = system->koMgr->GetSectionAfterKO(id);
        sectionMgr->SetNextSection(id, 0);
        register Pages::WWRaceEndWait* wait;
        asm(mr wait, r31);
        wait->EndStateAnimated(0.0f, 0);
        sectionMgr->RequestSceneChange(0, 0xFF);
    }
}
kmCall(0x8064f5fc, CalcSectionAfterRace);
kmPatchExitPoint(CalcSectionAfterRace, 0x8064f648);

}  // namespace Network
}  // namespace Pulsar