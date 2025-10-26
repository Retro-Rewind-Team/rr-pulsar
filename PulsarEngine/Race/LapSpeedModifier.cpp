#include <kamek.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <MarioKartWii/3D/Model/ModelDirector.hpp>
#include <MarioKartWii/Kart/KartValues.hpp>
#include <MarioKartWii/Kart/KartMovement.hpp>
#include <MarioKartWii/Item/Obj/ObjProperties.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/File/StatsParam.hpp>
#include <Race/200ccParams.hpp>
#include <Gamemodes/LapKO/LapKOMgr.hpp>
#include <PulsarSystem.hpp>
#include <RetroRewind.hpp>
#include <Settings/Settings.hpp>
#include <Settings/SettingsParam.hpp>
#include <runtimeWrite.hpp>

namespace Pulsar {
namespace Race {
// Mostly a port of MrBean's version with better hooks and arguments documentation
bool IsLapKOEnabled(const System* system) {
    if (system == nullptr) return false;
    if (system->IsContext(PULSAR_MODE_LAPKO)) return true;
    if (system->lapKoMgr != nullptr) return true;
    if (RKNet::Controller::sInstance->roomType != RKNet::ROOMTYPE_FROOM_NONHOST && RKNet::Controller::sInstance->roomType != RKNet::ROOMTYPE_FROOM_HOST && RKNet::Controller::sInstance->roomType != RKNet::ROOMTYPE_NONE) return false;
    return false;
}

u8 GetLapKOTargetCount(const System* system, const Racedata* racedata, u8 fallback) {
    u8 playerCount = 0;
    if (system != nullptr) playerCount = system->nonTTGhostPlayersCount;
    if (playerCount == 0 && racedata != nullptr) playerCount = racedata->racesScenario.playerCount;
    if (playerCount == 0) playerCount = fallback;
    if (playerCount < 2) playerCount = 2;
    if (playerCount > 12) playerCount = 12;
    return playerCount;
}

kmRuntimeUse(0x808a9cc7);  // lap_number.brctr
RaceinfoPlayer* LoadCustomLapCount(RaceinfoPlayer* player, u8 id) {
    kmRuntimeWrite16A(0x808a9cc7, 'la');
    System* system = System::sInstance;
    Racedata* racedata = Racedata::sInstance;
    u8 lapCount = KMP::Manager::sInstance->stgiSection->holdersArray[0]->raw->lapCount;

    const bool lapKoActive = IsLapKOEnabled(system);
    if (lapKoActive) {
        // Base KO lap count (existing behaviour)
        const u8 basePlayers = GetLapKOTargetCount(system, racedata, 1);
        const RKNet::Controller* controller = RKNet::Controller::sInstance;
        const bool offlineLapKO = (controller == nullptr || controller->roomType == RKNet::ROOMTYPE_NONE);

        u8 koPerRace = 1;
        if (system != nullptr && system->lapKoMgr != nullptr && !offlineLapKO) {
            koPerRace = system->lapKoMgr->GetKoPerRace();
        } else {
            const Settings::Mgr& settings = Settings::Mgr::Get();
            koPerRace = static_cast<u8>(settings.GetUserSettingValue(Settings::SETTINGSTYPE_KO, SCROLLER_KOPERRACE) + 1);
            if (koPerRace == 0) koPerRace = 1;
            if (system != nullptr && system->lapKoMgr != nullptr) {
                system->lapKoMgr->SetKoPerRace(koPerRace);
            }
        }

        const u8 usualTrackLaps = KMP::Manager::sInstance->stgiSection->holdersArray[0]->raw->lapCount;

        // BuildPlan handles 1-lap tracks and 2-lap pacing adjustments internally
        const u8 totalRounds = LapKO::Mgr::BuildPlan(basePlayers, koPerRace, usualTrackLaps, nullptr, LapKO::Mgr::MaxRounds);
        lapCount = (totalRounds == 0) ? 1 : totalRounds;
    }

    if (racedata != nullptr) {
        racedata->racesScenario.settings.lapCount = lapCount;
        if (lapKoActive) racedata->menusScenario.settings.lapCount = lapCount;
        if (lapCount > 9) {
            kmRuntimeWrite16A(0x808a9cc7, 'RR');  // RRp_number.brctr
        }
    }
    return new (player) RaceinfoPlayer(id, lapCount);
}
kmCall(0x805328d4, LoadCustomLapCount);

// kmWrite32(0x80723d64, 0x7FA4EB78);
void DisplayCorrectLap(AnmTexPatHolder* texPat) {  // This Anm is held by a ModelDirector in a Lakitu::Player
    register u32 maxLap;
    asm(mr maxLap, r29;);
    texPat->UpdateRateAndSetFrame((float)(maxLap - 2));
    return;
}
kmCall(0x80723d70, DisplayCorrectLap);

// Moved speed modifier to Race/StatChanges.cpp
kmWrite32(0x805336B8, 0x60000000);
kmWrite32(0x80534350, 0x60000000);
kmWrite32(0x80534BBC, 0x60000000);
kmWrite32(0x80723D10, 0x281D0063);
kmWrite32(0x80723D40, 0x3BA00063);

// Increase Lap Limit [Toadette Hack Fan]
kmWrite32(0x805328C0, 0x2800000C);
kmWrite32(0x805336c8, 0x2800000C);
kmWrite32(0x80534bcc, 0x2800000C);
kmWrite32(0x80534360, 0x2800000C);

kmWrite24(0x808AAA0C, 'PUL');  // time_number -> time_numPUL
}  // namespace Race
}  // namespace Pulsar
