#include <kamek.hpp>
#include <MarioKartWii/Kart/KartValues.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <PulsarSystem.hpp>
#include <UI/TransmissionSelect/TransmissionSelect.hpp>

namespace Pulsar {
namespace Race {

static Kart::Stats playerStats[12];

Kart::Stats* ApplyStatChanges(KartId kartId, CharacterId characterId, KartType kartType);

static void ApplyInsideAll(Kart::Stats& stats) {
    if (stats.type == INSIDE_BIKE) {
        stats.targetAngle = 0.0f;
    } else if (stats.type == KART) {
        stats.type = INSIDE_BIKE;
        stats.mt += 20.0f;
    } else if (stats.type == OUTSIDE_BIKE) {
        stats.type = INSIDE_BIKE;
        stats.targetAngle = 0.0f;
    }
}

static void ApplyInsideBike(Kart::Stats& stats) {
    if (stats.type == INSIDE_BIKE) {
        stats.targetAngle = 0.0f;
    } else if (stats.type == OUTSIDE_BIKE) {
        stats.type = INSIDE_BIKE;
        stats.targetAngle = 0.0f;
    }
}

static void ApplyOutside(Kart::Stats& stats) {
    if (stats.type == INSIDE_BIKE) {
        stats.type = OUTSIDE_BIKE;
        stats.targetAngle = 45.0f;
    } else if (stats.type == OUTSIDE_BIKE) {
        stats.targetAngle = 45.0f;
    }
}

static int GetGhostRkgIndex(u32 playerId) {
    const RacedataScenario& scenario = Racedata::sInstance->racesScenario;
    const u8 offset = scenario.players[0].playerType != PLAYER_GHOST ? 1 : 0;
    const int rkgIndex = static_cast<int>(playerId) - offset;
    return rkgIndex >= 0 ? rkgIndex : -1;
}

static Transmission GetPlayerTransmission(u32 playerId) {
    const RacedataScenario& scenario = Racedata::sInstance->racesScenario;
    const RacedataPlayer& player = scenario.players[playerId];
    if (player.playerType == PLAYER_GHOST) {
        const int rkgIndex = GetGhostRkgIndex(playerId);
        if (rkgIndex >= 0) return static_cast<Transmission>(Racedata::sInstance->ghosts[rkgIndex].header.unknown_3);
        return TRANSMISSION_DEFAULT;
    }

    const u32 hudSlotId = player.hudSlotId >= 0 ? static_cast<u32>(player.hudSlotId) : playerId;
    return UI::GetSelectedTransmission(hudSlotId);
}

static bool CanApplyTransmission(u32 playerId) {
    const RKNet::RoomType roomType = RKNet::Controller::sInstance->roomType;
    if (roomType == RKNet::ROOMTYPE_VS_WW || roomType == RKNet::ROOMTYPE_BT_WW) return false;

    const RacedataScenario& scenario = Racedata::sInstance->racesScenario;
    if (playerId >= scenario.playerCount) return false;
    if (scenario.localPlayerCount > 1) return false;

    const PlayerType playerType = scenario.players[playerId].playerType;
    return playerType == PLAYER_REAL_LOCAL || playerType == PLAYER_GHOST;
}

static void ApplyTransmission(Kart::Stats& stats, u32 playerId) {
    if (!CanApplyTransmission(playerId)) return;

    const RKNet::RoomType roomType = RKNet::Controller::sInstance->roomType;
    const bool isFroom = roomType == RKNet::ROOMTYPE_FROOM_HOST || roomType == RKNet::ROOMTYPE_FROOM_NONHOST;
    if (isFroom && System::sInstance->IsContext(PULSAR_TRANSMISSIONINSIDE)) {
        ApplyInsideAll(stats);
        return;
    }
    if (isFroom && System::sInstance->IsContext(PULSAR_TRANSMISSIONOUTSIDE)) {
        ApplyOutside(stats);
        return;
    }
    if (isFroom && System::sInstance->IsContext(PULSAR_TRANSMISSIONVANILLA)) return;

    const Transmission transmission = GetPlayerTransmission(playerId);
    if (transmission == TRANSMISSION_INSIDEALL) {
        ApplyInsideAll(stats);
    } else if (transmission == TRANSMISSION_INSIDEBIKE) {
        ApplyInsideBike(stats);
    } else if (transmission == TRANSMISSION_OUTSIDE) {
        ApplyOutside(stats);
    }
}

static Kart::Stats* CopyAndApplyPlayerTransmission(Kart::Stats* stats, u32 playerId) {
    if (playerId >= 12 || stats == nullptr) return stats;

    playerStats[playerId] = *stats;
    ApplyTransmission(playerStats[playerId], playerId);
    return &playerStats[playerId];
}

static Kart::Stats* ApplyPlayerStatChanges(KartId kartId, CharacterId characterId, KartType kartType) {
    register u32 playerId;
    asm(mr playerId, r28;);
    return CopyAndApplyPlayerTransmission(ApplyStatChanges(kartId, characterId, kartType), playerId);
}
kmCall(0x8058f670, ApplyPlayerStatChanges);

}  // namespace Race
}  // namespace Pulsar
