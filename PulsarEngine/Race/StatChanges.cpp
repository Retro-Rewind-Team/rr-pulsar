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
#include <MarioKartWii/File/RKG.hpp>

namespace Pulsar {

    
Kart::Stats* ApplyStatChanges(KartId kartId, CharacterId characterId, KartType kartType) {
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
        factor = Race::speedFactor;
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
    
    bool isLocalPlayer = false;
    const RacedataScenario& scenario = Racedata::sInstance->racesScenario;
    bool isGhostPlayer = false;
    for (int playerId = 0; playerId < scenario.playerCount; ++playerId) {
        if (scenario.players[playerId].kartId == kartId && 
            scenario.players[playerId].characterId == characterId) {
            if(scenario.players[playerId].playerType == PLAYER_REAL_LOCAL) {
                isLocalPlayer = true;
            }
            else if (scenario.players[playerId].playerType == PLAYER_GHOST) {
                isGhostPlayer = true;
            }
        }
    }

    int ghostPlayerIdx = -1;
    if(isGhostPlayer) {
        for(int pid = 0; pid < scenario.playerCount; ++pid) {
            if(scenario.players[pid].kartId == kartId &&
               scenario.players[pid].characterId == characterId &&
               scenario.players[pid].playerType == PLAYER_GHOST) {
                ghostPlayerIdx = pid;
                break;
            }
        }
    }

    bool insideAll = Pulsar::FORCE_TRANSMISSION_DEFAULT;
    bool outsideAll = Pulsar::FORCE_TRANSMISSION_DEFAULT;
    bool vanilla = Pulsar::FORCE_TRANSMISSION_DEFAULT;
    if (RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_HOST || RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_NONHOST) {
        insideAll = System::sInstance->IsContext(Pulsar::PULSAR_TRANSMISSIONINSIDE) ? Pulsar::FORCE_TRANSMISSION_INSIDE : Pulsar::FORCE_TRANSMISSION_DEFAULT;
        outsideAll = System::sInstance->IsContext(Pulsar::PULSAR_TRANSMISSIONOUTSIDE) ? Pulsar::FORCE_TRANSMISSION_OUTSIDE : Pulsar::FORCE_TRANSMISSION_DEFAULT;
        vanilla = System::sInstance->IsContext(Pulsar::PULSAR_TRANSMISSIONVANILLA) ? Pulsar::FORCE_TRANSMISSION_VANILLA : Pulsar::FORCE_TRANSMISSION_DEFAULT;
    }
    u32 transmission = static_cast<Pulsar::Transmission>(Pulsar::Settings::Mgr::Get().GetUserSettingValue(
        static_cast<Pulsar::Settings::UserType>(Pulsar::Settings::SETTINGSTYPE_RR),
        Pulsar::SETTINGRR_RADIO_TRANSMISSION));
    if(ghostPlayerIdx >= 0) {
        u8 offset = (scenario.players[0].playerType != PLAYER_GHOST) ? 1 : 0;
        int rkgIndex = ghostPlayerIdx - offset;
        if(rkgIndex >= 0) {
            RKG& ghostRkg = Racedata::sInstance->ghosts[rkgIndex];
            u32 savedTrans = ghostRkg.header.unknown_3;
            transmission = savedTrans;
            insideAll = outsideAll = vanilla = Pulsar::FORCE_TRANSMISSION_DEFAULT;
            if(savedTrans == Pulsar::TRANSMISSION_INSIDEALL) insideAll = Pulsar::FORCE_TRANSMISSION_INSIDE;
            else if(savedTrans == Pulsar::TRANSMISSION_OUTSIDE) outsideAll = Pulsar::FORCE_TRANSMISSION_OUTSIDE;
            else if(savedTrans == Pulsar::TRANSMISSION_INSIDEBIKE) vanilla = Pulsar::FORCE_TRANSMISSION_VANILLA;
        }
    }

    if (RKNet::Controller::sInstance->roomType != RKNet::ROOMTYPE_VS_WW && 
        RKNet::Controller::sInstance->roomType != RKNet::ROOMTYPE_BT_WW && 
        (isLocalPlayer || ghostPlayerIdx >= 0)) {
        if (insideAll == Pulsar::FORCE_TRANSMISSION_INSIDE) {
            if (stats->type == INSIDE_BIKE) {
                stats->type = INSIDE_BIKE;
                stats->targetAngle = 0.0f;
            } if (stats->type == KART) {
                stats->type = INSIDE_BIKE;
                stats->mt += 20.0f;
            } if (stats->type == OUTSIDE_BIKE) {
                stats->type = INSIDE_BIKE;
                stats->targetAngle = 0.0f;
            }
        }
        else if (outsideAll == Pulsar::FORCE_TRANSMISSION_OUTSIDE) {
            if (stats->type == INSIDE_BIKE) {
                stats->type = OUTSIDE_BIKE;
                stats->targetAngle = 45.0f;
            } if (stats->type == KART) {
                stats->type = KART;
            } if (stats->type == OUTSIDE_BIKE) {
                stats->type = OUTSIDE_BIKE;
                stats->targetAngle = 45.0f;
            }
        }
        else if (vanilla == Pulsar::FORCE_TRANSMISSION_VANILLA) {
            if (stats->type == INSIDE_BIKE) {
                stats->type = INSIDE_BIKE;
            } if (stats->type == KART) {
                stats->type = KART;
            } if (stats->type == OUTSIDE_BIKE) {
                stats->type = OUTSIDE_BIKE;
            }
        }
        else if (transmission == Pulsar::TRANSMISSION_INSIDEALL)
        {
            if (stats->type == INSIDE_BIKE) {
                stats->type = INSIDE_BIKE;
                stats->targetAngle = 0.0f;
            } if (stats->type == KART) {
                stats->type = INSIDE_BIKE;
                stats->mt += 20.0f;
            } if (stats->type == OUTSIDE_BIKE) {
                stats->type = INSIDE_BIKE;
                stats->targetAngle = 0.0f;
            }
        }
        else if (transmission == Pulsar::TRANSMISSION_INSIDEBIKE)
        {
            if (stats->type == INSIDE_BIKE) {
                stats->type = INSIDE_BIKE;
                stats->targetAngle = 0.0f;
            } if (stats->type == KART) {
                stats->type = KART;
            } if (stats->type == OUTSIDE_BIKE) {
                stats->type = INSIDE_BIKE;
                stats->targetAngle = 0.0f;
            }
        }
        else if (transmission == Pulsar::TRANSMISSION_OUTSIDE)
        {
            if (stats->type == INSIDE_BIKE) {
                stats->type = OUTSIDE_BIKE;
                stats->targetAngle = 45.0f;
            } if (stats->type == KART) {
                stats->type = KART;
            } if (stats->type == OUTSIDE_BIKE) {
                stats->type = OUTSIDE_BIKE;
                stats->targetAngle = 45.0f;
            }
        }
    }

    Kart::minDriftSpeedRatio = 0.55f * (factor > 1.0f ? (1.0f / factor) : 1.0f);
    Kart::unknown_70 = 70.0f * factor;
    Kart::regularBoostAccel = 3.0f * factor;

    return stats;
}
kmCall(0x8058f670, ApplyStatChanges);

} //namespace Pulsar