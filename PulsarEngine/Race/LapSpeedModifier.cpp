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

static u8 GetBattleRoyaleLapCount(u8 baseLapCount, const System* system) {
    if (baseLapCount <= 1 || system == nullptr) return baseLapCount;
    if (system->IsContext(PULSAR_KOROYALE_LAPS_1_5X)) return static_cast<u8>((baseLapCount * 3 + 1) / 2);
    if (system->IsContext(PULSAR_KOROYALE_LAPS_2_0X)) return static_cast<u8>(baseLapCount * 2);
    return baseLapCount;
}

kmRuntimeUse(0x808a9cc7);  // lap_number.brctr
static void SetLapCounterResourceName(char first, char second) {
    volatile char* name = reinterpret_cast<volatile char*>(kmRuntimeAddr(0x808a9cc7));
    name[0] = first;
    name[1] = second;
}

RaceinfoPlayer* LoadCustomLapCount(RaceinfoPlayer* player, u8 id) {
    SetLapCounterResourceName('l', 'a');
    System* system = System::sInstance;
    Racedata* racedata = Racedata::sInstance;
    u8 lapCount = KMP::Manager::sInstance->stgiSection->holdersArray[0]->raw->lapCount;

    const bool lapKoActive = IsLapKOEnabled(system);
    if (lapKoActive) {
        // Base KO lap count (existing behaviour)
        const u8 basePlayers = GetLapKOTargetCount(system, racedata, 1);
        const RKNet::Controller* controller = RKNet::Controller::sInstance;
        const bool offlineLapKO = (controller->roomType == RKNet::ROOMTYPE_NONE);

        u8 koPerRace = 1;
        if (!offlineLapKO) {
            koPerRace = system->lapKoMgr->GetKoPerRace();
        } else {
            const Settings::Mgr& settings = Settings::Mgr::Get();
            koPerRace = static_cast<u8>(settings.GetUserSettingValue(Settings::SETTINGSTYPE_KO, SCROLLER_KOPERRACE) + 1);
            if (koPerRace == 0) koPerRace = 1;
            system->lapKoMgr->SetKoPerRace(koPerRace);
        }

        const u8 usualTrackLaps = KMP::Manager::sInstance->stgiSection->holdersArray[0]->raw->lapCount;

        // BuildPlan handles 1-lap tracks and 2-lap pacing adjustments internally
        const u8 totalRounds = LapKO::Mgr::BuildPlan(basePlayers, koPerRace, usualTrackLaps, nullptr, LapKO::Mgr::MaxRounds);
        lapCount = (totalRounds == 0) ? 1 : totalRounds;
    } else if (system != nullptr && system->IsContext(PULSAR_MODE_BATTLEROYALE)) {
        lapCount = GetBattleRoyaleLapCount(lapCount, system);
        if (lapCount > 12) lapCount = 12;
    }

    if (racedata != nullptr) {
        racedata->racesScenario.settings.lapCount = lapCount;
        if (lapKoActive) racedata->menusScenario.settings.lapCount = lapCount;
        if (lapCount > 9) {
            SetLapCounterResourceName('R', 'R');  // RRp_number.brctr
        }
    }
    return new (player) RaceinfoPlayer(id, lapCount);
}
kmCall(0x805328d4, LoadCustomLapCount);

void DisplayCorrectLap(AnmTexPatHolder* texPat) {  // This Anm is held by a ModelDirector in a Lakitu::Player
    register u32 maxLap;
    asm(mr maxLap, r29;);
    texPat->UpdateRateAndSetFrame((float)(maxLap - 2));
    return;
}
kmCall(0x80723d70, DisplayCorrectLap);

Kart::Stats* ApplyStatChanges(KartId kartId, CharacterId characterId, KartType kartType) {
    union SpeedModConv {
        float speedMod;
        u32 kmpValue;
    };

    Kart::Stats* stats = Kart::ComputeStats(kartId, characterId);
    const GameMode gameMode = Racedata::sInstance->menusScenario.settings.gamemode;
    const GameType gameType = Racedata::sInstance->menusScenario.settings.gametype;
    SpeedModConv speedModConv;
    bool is200 = Racedata::sInstance->racesScenario.settings.engineClass == CC_100 && RKNet::Controller::sInstance->roomType != RKNet::ROOMTYPE_VS_WW;
    speedModConv.kmpValue = (KMP::Manager::sInstance->stgiSection->holdersArray[0]->raw->speedMod << 16);
    if (speedModConv.speedMod == 0.0f) speedModConv.speedMod = 1.0f;
    float factor = 1.0f;
    if (gameType == GAMETYPE_ONLINE_SPECTATOR && System::sInstance->netMgr.region != 0x0C) {
        factor = 1.0f;
    } else if (is200 && System::sInstance->IsContext(Pulsar::PULSAR_500)) {
        factor = 2.66f;
    } else if (is200) {
        factor = Race::speedFactor;
    } else if (RetroRewind::System::Is500cc() && (gameMode == MODE_PRIVATE_VS || gameMode == MODE_VS_RACE || gameMode == MODE_PUBLIC_VS || gameMode == MODE_GRAND_PRIX)) {
        factor = 3.0f;
    } else if (RetroRewind::System::Is500cc() && (gameMode == MODE_BATTLE || gameMode == MODE_PUBLIC_BATTLE || gameMode == MODE_PRIVATE_BATTLE)) {
        factor = 1.214;
    } else if (System::sInstance->IsContext(PULSAR_MODE_OTT) && gameMode == MODE_PUBLIC_VS) {
        factor = 1.0f;
    }
    factor *= speedModConv.speedMod;

    Item::greenShellSpeed = 105.0f * factor;
    Item::redShellInitialSpeed = 75.0f * factor;
    Item::redShellSpeed = 130.0f * factor;
    Item::blueShellSpeed = 260.0f * factor;
    Item::blueShellMinimumDiveDistance = 640000.0f * factor;
    Item::blueShellHomingSpeed = 130.0f * factor;

    Kart::hardSpeedCap = 120.0f * factor;
    Kart::bulletSpeed = 145.0f * factor;
    Kart::starSpeed = 105.0f * factor;
    Kart::megaTCSpeed = 95.0f * factor;

    stats->baseSpeed *= factor;
    stats->standard_acceleration_as[0] *= factor;
    stats->standard_acceleration_as[1] *= factor;
    stats->standard_acceleration_as[2] *= factor;
    stats->standard_acceleration_as[3] *= factor;

    if (is200) {
        stats->weight = 6.0f;
    }

    Kart::minDriftSpeedRatio = 0.55f * (factor > 1.0f ? (1.0f / factor) : 1.0f);
    Kart::unknown_70 = 70.0f * factor;
    Kart::regularBoostAccel = 3.0f * factor;

    return stats;
}

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
