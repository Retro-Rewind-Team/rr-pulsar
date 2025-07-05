#include <kamek.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <MarioKartWii/3D/Model/ModelDirector.hpp>
#include <MarioKartWii/Kart/KartValues.hpp>
#include <MarioKartWii/Kart/KartMovement.hpp>
#include <MarioKartWii/Item/Obj/ObjProperties.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <Race/200ccParams.hpp>
#include <PulsarSystem.hpp>
#include <RetroRewind.hpp>

namespace Pulsar {
namespace Race {
//Mostly a port of MrBean's version with better hooks and arguments documentation
RaceinfoPlayer* LoadCustomLapCount(RaceinfoPlayer* player, u8 id) {
    const u8 lapCount = KMP::Manager::sInstance->stgiSection->holdersArray[0]->raw->lapCount;
    Racedata::sInstance->racesScenario.settings.lapCount = lapCount;
    return new(player) RaceinfoPlayer(id, lapCount);
}
kmCall(0x805328d4, LoadCustomLapCount);

//kmWrite32(0x80723d64, 0x7FA4EB78);
void DisplayCorrectLap(AnmTexPatHolder* texPat) { //This Anm is held by a ModelDirector in a Lakitu::Player
    register u32 maxLap;
    asm(mr maxLap, r29;);
    texPat->UpdateRateAndSetFrame((float)(maxLap - 2));
    return;
}
kmCall(0x80723d70, DisplayCorrectLap);


//kmWrite32(0x808b5cd8, 0x3F800000); //change 100cc speed ratio to 1.0    
Kart::Stats* ApplySpeedModifier(KartId kartId, CharacterId characterId) {
    union SpeedModConv {
        float speedMod;
        u32 kmpValue;
    };

    Kart::Stats* stats = Kart::ComputeStats(kartId, characterId);
    const GameMode gameMode = Racedata::sInstance->menusScenario.settings.gamemode;
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    const RKNet::RoomType roomType = RKNet::Controller::sInstance->roomType;
    SpeedModConv speedModConv;
    bool is200 = Racedata::sInstance->racesScenario.settings.engineClass == CC_100 && RKNet::Controller::sInstance->roomType != RKNet::ROOMTYPE_VS_WW;
    speedModConv.kmpValue = (KMP::Manager::sInstance->stgiSection->holdersArray[0]->raw->speedMod << 16);
    if(speedModConv.speedMod == 0.0f) speedModConv.speedMod = 1.0f;
    float factor = 1.0f;
    if (is200 && System::sInstance->IsContext(Pulsar::PULSAR_500)){
        factor = 2.66f;
    }
    else if (is200){
        factor = speedFactor;
    }
    else if (RetroRewind::System::Is500cc() && (gameMode == MODE_PRIVATE_VS || gameMode == MODE_VS_RACE || gameMode == MODE_PUBLIC_VS || gameMode == MODE_GRAND_PRIX)){
        factor = 3.0f;
    }
    else if (RetroRewind::System::Is500cc() && (gameMode == MODE_BATTLE || gameMode == MODE_PUBLIC_BATTLE || gameMode == MODE_PRIVATE_BATTLE)){
        factor = 1.214;
    }
    else if (System::sInstance->IsContext(PULSAR_MODE_OTT) && gameMode == MODE_PUBLIC_VS) {
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
        stats->weight = 0x9C;
    }

    Kart::minDriftSpeedRatio = 0.55f * (factor > 1.0f ? (1.0f / factor) : 1.0f);
    Kart::unknown_70 = 70.0f * factor;
    Kart::regularBoostAccel = 3.0f * factor;

    return stats;
}
kmCall(0x8058f670, ApplySpeedModifier);

kmWrite32(0x805336B8, 0x60000000);
kmWrite32(0x80534350, 0x60000000);
kmWrite32(0x80534BBC, 0x60000000);
kmWrite32(0x80723D10, 0x281D0009);
kmWrite32(0x80723D40, 0x3BA00009);

kmWrite24(0x808AAA0C, 'PUL'); //time_number -> time_numPUL
}//namespace Race
}//namespace Pulsar