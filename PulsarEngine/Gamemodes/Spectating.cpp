#include <Gamemodes/Spectating.hpp>
#include <Gamemodes/LapKO/LapKOMgr.hpp>
#include <MarioKartWii/3D/Camera/CameraMgr.hpp>
#include <MarioKartWii/3D/Camera/RaceCamera.hpp>
#include <MarioKartWii/Driver/DriverManager.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>
#include <PulsarSystem.hpp>
#include <core/rvl/PAD.hpp>
#include <core/rvl/WPAD.hpp>

namespace Pulsar {
namespace Spectating {

static const u8 maxPlayers = 12;

static bool sIsSpectating = false;
static u8 sTargetPlayer = 0xFF;
static bool sManualTarget = false;
static u16 sLastRaceFrames = 0xFFFF;

static bool IsBattleMode(GameMode mode) {
    return mode == MODE_PUBLIC_BATTLE || mode == MODE_PRIVATE_BATTLE;
}

static bool IsBattleElimination(const System &system) {
    return system.IsContext(PULSAR_ELIMINATION) && system.IsContext(PULSAR_FFA);
}

static bool IsEligiblePlayer(const Raceinfo &raceinfo, u8 playerId) {
    if (playerId >= maxPlayers) return false;

    const System *system = System::sInstance;
    if (IsBattleElimination(*system)) {
        return raceinfo.players != nullptr && raceinfo.players[playerId] != nullptr &&
               raceinfo.players[playerId]->battleScore != 0;
    }

    const LapKO::Mgr *lapKoMgr = system->lapKoMgr;
    if (lapKoMgr == nullptr || !lapKoMgr->IsActive(playerId)) return false;

    const GameMode mode = Racedata::sInstance->menusScenario.settings.gamemode;
    if (IsBattleMode(mode) && raceinfo.players != nullptr && raceinfo.players[playerId] != nullptr &&
        raceinfo.players[playerId]->battleScore == 0) {
        return false;
    }
    return true;
}

static u8 GetLeaderPlayerId(const Raceinfo &raceinfo) {
    if (raceinfo.playerIdInEachPosition == nullptr) return 0xFF;

    u8 playerCount = System::sInstance->nonTTGhostPlayersCount;
    if (playerCount > maxPlayers) playerCount = maxPlayers;
    for (u8 pos = 0; pos < playerCount; ++pos) {
        const u8 playerId = raceinfo.playerIdInEachPosition[pos];
        if (IsEligiblePlayer(raceinfo, playerId)) return playerId;
    }
    return 0xFF;
}

static u8 BuildActivePlayerOrder(const Raceinfo &raceinfo, u8 *outOrder) {
    if (outOrder == nullptr) return 0;

    u8 playerCount = System::sInstance->nonTTGhostPlayersCount;
    if (playerCount > maxPlayers) playerCount = maxPlayers;
    u8 count = 0;

    if (raceinfo.playerIdInEachPosition != nullptr) {
        for (u8 pos = 0; pos < playerCount; ++pos) {
            const u8 playerId = raceinfo.playerIdInEachPosition[pos];
            if (!IsEligiblePlayer(raceinfo, playerId)) continue;

            bool alreadyAdded = false;
            for (u8 i = 0; i < count; ++i) {
                if (outOrder[i] == playerId) {
                    alreadyAdded = true;
                    break;
                }
            }
            if (!alreadyAdded) outOrder[count++] = playerId;
        }
    }

    for (u8 playerId = 0; playerId < playerCount && count < maxPlayers; ++playerId) {
        if (!IsEligiblePlayer(raceinfo, playerId)) continue;

        bool alreadyAdded = false;
        for (u8 i = 0; i < count; ++i) {
            if (outOrder[i] == playerId) {
                alreadyAdded = true;
                break;
            }
        }
        if (!alreadyAdded) outOrder[count++] = playerId;
    }

    return count;
}

static u8 FindNextPlayer(const Raceinfo &raceinfo, u8 current, bool forward) {
    u8 order[maxPlayers];
    const u8 count = BuildActivePlayerOrder(raceinfo, order);
    if (count == 0) return 0xFF;

    s32 index = -1;
    if (current < maxPlayers) {
        for (u8 i = 0; i < count; ++i) {
            if (order[i] == current) {
                index = static_cast<s32>(i);
                break;
            }
        }
    }

    if (index < 0) return forward ? order[0] : order[count - 1];
    if (count == 1) return order[0];

    if (forward) {
        index = (index + 1) % count;
    } else {
        index = (index + count - 1) % count;
    }
    return order[index];
}

static void FocusCameraOnPlayer(u8 playerId) {
    if (playerId >= maxPlayers) return;
    RaceCameraMgr *cameraMgr = RaceCameraMgr::sInstance;

    u8 targetCameraIndex = 0xFF;
    for (u32 i = 0; i < cameraMgr->cameraCount; ++i) {
        RaceCamera *camera = cameraMgr->cameras[i];
        if (camera != nullptr && camera->playerId == playerId) {
            targetCameraIndex = static_cast<u8>(i);
            break;
        }
    }

    if (targetCameraIndex != 0xFF) {
        DriverMgr::ChangeFocusedPlayer(targetCameraIndex);
        RaceCameraMgr::ChangeFocusedPlayer(targetCameraIndex);
        return;
    }

    const u32 currentIndex = (cameraMgr->focusedPlayerIdx < cameraMgr->cameraCount) ? cameraMgr->focusedPlayerIdx : 0;
    RaceCamera *currentCamera = cameraMgr->cameras[currentIndex];
    if (currentCamera != nullptr && currentCamera->playerId != playerId) currentCamera->playerId = playerId;
    DriverMgr::ChangeFocusedPlayer(static_cast<u8>(currentIndex));
}

static void EnsureTargetIsActive(const Raceinfo &raceinfo) {
    if (IsEligiblePlayer(raceinfo, sTargetPlayer)) return;

    sTargetPlayer = FindNextPlayer(raceinfo, sTargetPlayer, true);
    if (sTargetPlayer == 0xFF) sManualTarget = false;
}

static void UpdateInputs(const Raceinfo &raceinfo) {
    bool advanceForward = false;
    bool advanceBackward = false;

    SectionMgr *sectionMgr = SectionMgr::sInstance;
    for (u8 hudSlot = 0; hudSlot < 4; ++hudSlot) {
        Input::RealControllerHolder *holder = sectionMgr->pad.padInfos[hudSlot].controllerHolder;
        const u16 current = holder->inputStates[0].buttonRaw;
        const u16 previous = holder->inputStates[1].buttonRaw;
        const u16 newInputs = static_cast<u16>(current & static_cast<u16>(~previous));
        if (newInputs == 0) continue;

        const ControllerType type = holder->curController->GetType();
        switch (type) {
            case WHEEL:
            case NUNCHUCK:
                if ((newInputs & WPAD::WPAD_BUTTON_A) != 0) advanceForward = true;
                if ((newInputs & WPAD::WPAD_BUTTON_B) != 0) advanceBackward = true;
                break;
            case CLASSIC:
                if ((newInputs & WPAD::WPAD_CL_BUTTON_A) != 0) advanceForward = true;
                if ((newInputs & WPAD::WPAD_CL_BUTTON_B) != 0) advanceBackward = true;
                break;
            case GCN:
                if ((newInputs & PAD::PAD_BUTTON_A) != 0) advanceForward = true;
                if ((newInputs & PAD::PAD_BUTTON_B) != 0) advanceBackward = true;
                break;
            default:
                if ((newInputs & PAD::PAD_BUTTON_A) != 0) advanceForward = true;
                if ((newInputs & PAD::PAD_BUTTON_B) != 0) advanceBackward = true;
                if ((newInputs & WPAD::WPAD_BUTTON_A) != 0) advanceForward = true;
                if ((newInputs & WPAD::WPAD_BUTTON_B) != 0) advanceBackward = true;
                if ((newInputs & WPAD::WPAD_CL_BUTTON_A) != 0) advanceForward = true;
                if ((newInputs & WPAD::WPAD_CL_BUTTON_B) != 0) advanceBackward = true;
                break;
        }
    }

    if (advanceForward || advanceBackward) {
        const u8 next = FindNextPlayer(raceinfo, sTargetPlayer, advanceForward);
        if (next != 0xFF && next != sTargetPlayer) {
            sTargetPlayer = next;
            sManualTarget = true;
            FocusCameraOnPlayer(next);
        }
    }

    if (sManualTarget) EnsureTargetIsActive(raceinfo);
}

static bool HasEliminatedLocalPlayer(const Raceinfo &raceinfo) {
    const RacedataScenario &scenario = Racedata::sInstance->menusScenario;
    u8 playerCount = System::sInstance->nonTTGhostPlayersCount;
    if (playerCount > maxPlayers) playerCount = maxPlayers;

    for (u8 localIdx = 0; localIdx < scenario.localPlayerCount; ++localIdx) {
        const u32 playerId = Racedata::sInstance->GetPlayerIdOfLocalPlayer(localIdx);
        if (playerId >= playerCount || raceinfo.players == nullptr || raceinfo.players[playerId] == nullptr) continue;
        if (raceinfo.players[playerId]->battleScore == 0) return true;
    }
    return false;
}

void Reset() {
    sIsSpectating = false;
    sTargetPlayer = 0xFF;
    sManualTarget = false;
    sLastRaceFrames = 0xFFFF;
}

void Start(const Raceinfo &raceinfo) {
    sIsSpectating = true;
    sManualTarget = false;
    sTargetPlayer = GetLeaderPlayerId(raceinfo);
    if (sTargetPlayer == 0xFF) sTargetPlayer = FindNextPlayer(raceinfo, 0xFF, true);
    EnsureTargetIsActive(raceinfo);

    const GameMode mode = Racedata::sInstance->menusScenario.settings.gamemode;
    const bool useVanillaBattleCamera = IsBattleElimination(*System::sInstance) && mode == MODE_BATTLE;
    if (sTargetPlayer < maxPlayers && !useVanillaBattleCamera) FocusCameraOnPlayer(sTargetPlayer);
}

void Update(Raceinfo &raceinfo) {
    if (sLastRaceFrames != 0xFFFF && raceinfo.raceFrames < sLastRaceFrames) Reset();
    sLastRaceFrames = raceinfo.raceFrames;

    const System *system = System::sInstance;
    if (IsBattleElimination(*system) && raceinfo.IsAtLeastStage(RACESTAGE_RACE) && !sIsSpectating &&
        HasEliminatedLocalPlayer(raceinfo)) {
        Start(raceinfo);
    }
    if (!sIsSpectating) return;

    const GameMode mode = Racedata::sInstance->menusScenario.settings.gamemode;
    const bool useVanillaBattleCamera = IsBattleElimination(*system) && mode == MODE_BATTLE;
    if (!useVanillaBattleCamera) {
        UpdateInputs(raceinfo);

        if (!sManualTarget) {
            const u8 leader = GetLeaderPlayerId(raceinfo);
            if (leader != 0xFF) sTargetPlayer = leader;
        }
        EnsureTargetIsActive(raceinfo);
        if (sTargetPlayer < maxPlayers) FocusCameraOnPlayer(sTargetPlayer);
    }
}

}  // namespace Spectating
}  // namespace Pulsar
