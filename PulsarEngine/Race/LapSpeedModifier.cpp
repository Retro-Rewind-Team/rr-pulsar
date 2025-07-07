#include <kamek.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <MarioKartWii/3D/Model/ModelDirector.hpp>
#include <MarioKartWii/Kart/KartValues.hpp>
#include <MarioKartWii/Kart/KartMovement.hpp>
#include <MarioKartWii/Item/Obj/ObjProperties.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/File/StatsParam.hpp>
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

//Moved speed modifier to Race/StatChanges.cpp
kmWrite32(0x805336B8, 0x60000000);
kmWrite32(0x80534350, 0x60000000);
kmWrite32(0x80534BBC, 0x60000000);
kmWrite32(0x80723D10, 0x281D0009);
kmWrite32(0x80723D40, 0x3BA00009);

kmWrite24(0x808AAA0C, 'PUL'); //time_number -> time_numPUL
}//namespace Race
}//namespace Pulsar