#include <kamek.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/RKNet/Select.hpp>
#include <MarioKartWii/System/random.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>
#include <Settings/Settings.hpp>
#include <Network/PulSELECT.hpp>
#include <PulsarSystem.hpp>
#include <AutoTrackSelect/ChooseNextTrack.hpp>
#include <Gamemodes/KO/KOMgr.hpp>

namespace Pulsar
{
    namespace Network
    {

        static void CalcSectionAfterRace(SectionMgr *sectionMgr, SectionId id)
        {

            UI::ChooseNextTrack *choosePage = reinterpret_cast<UI::ExpSection *>(sectionMgr->curSection)->GetPulPage<UI::ChooseNextTrack>();
            const System *system = System::sInstance;
            // if(choosePage != nullptr) id = choosePage->ProcessHAW(id);
            if (id != SECTION_NONE)
            {
                if (system->IsContext(PULSAR_MODE_KO))
                    id = system->koMgr->GetSectionAfterKO(id);
                sectionMgr->SetNextSection(id, 0);
                register Pages::WWRaceEndWait *wait;
                asm(mr wait, r31);
                wait->EndStateAnimated(0.0f, 0);
                sectionMgr->RequestSceneChange(0, 0xFF);
            }
        }
        kmCall(0x8064f5fc, CalcSectionAfterRace);
        kmPatchExitPoint(CalcSectionAfterRace, 0x8064f648);

    } // namespace Network
} // namespace Pulsar