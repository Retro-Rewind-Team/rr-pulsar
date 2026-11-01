#include <PulsarSystem.hpp>
#include <Gamemodes/BattleRoyale/BattleRoyale.hpp>
#include <Gamemodes/EliminationDisplay.hpp>
#include <Gamemodes/LapKO/LapKOMgr.hpp>
#include <Gamemodes/Spectating.hpp>
#include <MarioKartWii/3D/Model/ModelDirector.hpp>
#include <MarioKartWii/Item/ItemManager.hpp>
#include <MarioKartWii/Item/Obj/ItemObj.hpp>
#include <MarioKartWii/Kart/KartAction.hpp>
#include <MarioKartWii/Kart/KartLink.hpp>
#include <MarioKartWii/Kart/KartManager.hpp>
#include <MarioKartWii/Kart/KartMovement.hpp>
#include <MarioKartWii/Kart/KartPointers.hpp>
#include <MarioKartWii/Objects/ObjectsMgr.hpp>
#include <MarioKartWii/Race/RaceBalloon.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/Scene/GameScene.hpp>
#include <Network/PacketExpansion.hpp>
#include <Settings/Settings.hpp>
#include <UI/ExtendedTeamSelect/ExtendedTeamManager.hpp>

namespace Pulsar {
namespace BattleRoyale {

static const u8 maxPlayers = 12;
static const u8 maxBattleRoyaleBalloons = maxPlayers * 5;
static const u8 preloadedBalloonsPerPlayer = 4;
static const u32 kartMovementBlinkTimerOffset = 0x1a8;

static bool sInitialized = false;
static u16 sLastRaceFrames = 0xffff;
static u16 sPoweredHitLossFrame[maxPlayers];
static u8 sPreviousBalloonCount[maxPlayers];
static u8 sEliminationCount = 0;
static u8 sLocalBalloonLossSeq = 0;
static const u8 pendingBalloonEventCount = 4;
static const u8 balloonMoveEventBase = 0x10;
static const u8 mushroomStealLockFrames = 120;
static u8 sPendingBalloonEventSeq[pendingBalloonEventCount];
static u8 sPendingBalloonEventPlayerId[pendingBalloonEventCount];
static u8 sPendingBalloonEventTimer[pendingBalloonEventCount];
static u8 sPendingBalloonEventReadIdx = 0;
static u8 sPendingBalloonEventWriteIdx = 0;
static u8 sPendingBalloonEventSize = 0;
static u8 sLastRemoteBalloonLossSeq[maxPlayers];
static bool sDeferredBalloonManagerCreation = false;
static bool sCollisionPatchesActive = false;
static bool sCreatingBattleRoyaleBalloonManager = false;
static bool sLoadedBalloonModels[maxBattleRoyaleBalloons];
static u16 sMushroomStealVictimMask[maxPlayers];
static u8 sMushroomStealVictimTimers[maxPlayers][maxPlayers];

typedef void (*RaceModeHitFn)(void *raceMode, u32 firstPlayerId, u32 secondPlayerId);
typedef void (*GeoObjectLoadFn)(void *object);

static u8 GetBalloonCount(RaceBalloonManager *mgr, u8 playerId) {
    if (mgr == nullptr || playerId >= maxPlayers)
        return 0;
    return mgr->players[playerId].count;
}

static void LoadBalloonModel(RaceBalloonManager *mgr, u8 poolIdx) {
    if (poolIdx >= maxBattleRoyaleBalloons || sLoadedBalloonModels[poolIdx])
        return;

    void *balloon = mgr->balloons[poolIdx].object;
    if (balloon == nullptr)
        return;

    void **vtable = *reinterpret_cast<void ***>(balloon);
    GeoObjectLoadFn load = reinterpret_cast<GeoObjectLoadFn>(vtable[0x20 / sizeof(void *)]);
    load(balloon);
    sLoadedBalloonModels[poolIdx] = true;
}

static u8 GetBattleRoyaleBalloonPoolTeam(u8 poolIdx, u8 fallbackTeam) {
    if (!ShouldApplyBattleRoyale())
        return fallbackTeam;

    const Racedata *racedata = Racedata::sInstance;
    if (racedata == nullptr)
        return fallbackTeam;

    const u8 playerId = static_cast<u8>(poolIdx / 5);
    if (playerId >= maxPlayers)
        return fallbackTeam;

    u8 team = static_cast<u8>(racedata->racesScenario.players[playerId].team);
    if (team > 1)
        team = 0;
    return team;
}

static void LoadStartingBalloonModels(RaceBalloonManager *mgr) {
    if (mgr == nullptr)
        return;

    const u8 playerCount = mgr->playerCount;
    u8 modelsToLoad = static_cast<u8>(playerCount * preloadedBalloonsPerPlayer);
    const u8 poolCount = mgr->balloonCount;
    if (modelsToLoad > poolCount)
        modelsToLoad = poolCount;

    for (u8 poolIdx = 0; poolIdx < modelsToLoad; ++poolIdx) LoadBalloonModel(mgr, poolIdx);
}

static void AddBattleRoyaleBalloons(RaceBalloonManager *mgr, u8 playerId, u8 teamId, u8 isInitial, int delay, u8 count, int interval) {
    if (mgr == nullptr || playerId >= maxPlayers || count == 0)
        return;
    if (teamId > 1)
        teamId = 0;

    u8 current = GetBalloonCount(mgr, playerId);
    if (current >= 5)
        return;
    if (count > 5 - current)
        count = static_cast<u8>(5 - current);

    const u8 poolCount = mgr->balloonCount;
    const u8 freePlayerId = mgr->playerCount;
    const u32 time = mgr->time + delay;

    for (u8 poolIdx = 0, added = 0; poolIdx < poolCount && added < count; ++poolIdx) {
        if (mgr->balloons[poolIdx].playerId != freePlayerId || GetBattleRoyaleBalloonPoolTeam(poolIdx, mgr->balloons[poolIdx].teamId) != teamId) {
            continue;
        }

        if (poolIdx >= maxBattleRoyaleBalloons)
            continue;
        LoadBalloonModel(mgr, poolIdx);
        if (!sLoadedBalloonModels[poolIdx])
            continue;
        GeoObj::ObjBalloon *balloon = mgr->balloons[poolIdx].object;
        if (balloon == nullptr)
            continue;

        const u8 balloonIndex = current++;
        balloon->OnAdd(time + added * interval, playerId, balloonIndex, isInitial);
        mgr->balloons[poolIdx].teamId = teamId;
        mgr->balloons[poolIdx].playerId = playerId;
        mgr->balloons[poolIdx].index = balloonIndex;
        mgr->players[playerId].balloons[balloonIndex] = balloon;
        mgr->players[playerId].count = current;
        ++added;
    }
}

static void AddBalloons(RaceBalloonManager *mgr, u8 playerId, u8 count) {
    if (mgr == nullptr || playerId >= maxPlayers || count == 0)
        return;

    const RacedataScenario &scenario = Racedata::sInstance->racesScenario;
    u8 team = static_cast<u8>(scenario.players[playerId].team);
    if (team > 1)
        team = 0;

    if (ShouldApplyBattleRoyale()) {
        AddBattleRoyaleBalloons(mgr, playerId, team, 1, 0, count, 0);
        return;
    }

    mgr->Add(playerId, team, 1, 0, count, 0);
}

static u8 GetStartingBalloonCountSetting() {
    const System *system = System::sInstance;
    if (system != nullptr) {
        if (system->IsContext(PULSAR_KOPERRACE_4))
            return 4;
        if (system->IsContext(PULSAR_KOPERRACE_3))
            return 3;
        if (system->IsContext(PULSAR_KOPERRACE_2))
            return 2;
    }
    return 1;
}

static void StartBalloonLossBlink(u8 playerId) {
    Raceinfo *raceinfo = Raceinfo::sInstance;
    if (raceinfo == nullptr || !raceinfo->IsAtLeastStage(RACESTAGE_RACE))
        return;

    Kart::Manager *kartMgr = Kart::Manager::sInstance;
    if (kartMgr == nullptr || playerId >= kartMgr->playerCount)
        return;

    Kart::Player *player = kartMgr->GetKartPlayer(playerId);
    if (player == nullptr)
        return;

    RacedataSettings &settings = Racedata::sInstance->racesScenario.settings;
    const GameMode prevMode = settings.gamemode;
    const BattleType prevBattleType = settings.battleType;
    settings.gamemode = MODE_BATTLE;
    settings.battleType = BATTLE_BALLOON;

    Kart::Movement &movement = player->GetMovement();
    movement.StartBlinkLocal();

    s16 &blinkTimer = *reinterpret_cast<s16 *>(reinterpret_cast<u8 *>(&movement) + kartMovementBlinkTimerOffset);
    blinkTimer *= 1.5f;

    settings.gamemode = prevMode;
    settings.battleType = prevBattleType;
}

static void EnableSecondKartModelDrawIfReady(ModelDirector *model, bool enable) {
    if (model == nullptr || model->scnMdlEx[0] == nullptr || model->scnMdlEx[1] == nullptr)
        return;
    model->EnableDraw(enable);
}
kmCall(0x80592ec8, EnableSecondKartModelDrawIfReady);

static bool RemoveBalloon(RaceBalloonManager *mgr, u8 playerId) {
    if (mgr == nullptr || playerId >= maxPlayers || GetBalloonCount(mgr, playerId) == 0)
        return false;

    const u8 previousBalloonCount = GetBalloonCount(mgr, playerId);
    mgr->Remove(playerId, 1, 1, 0, 1, 0);
    if (GetBalloonCount(mgr, playerId) >= previousBalloonCount)
        return false;

    StartBalloonLossBlink(playerId);
    return true;
}

static bool IsLocalPlayer(u8 playerId) {
    const RKNet::Controller *controller = RKNet::Controller::sInstance;
    if (controller == nullptr || controller->roomType == RKNet::ROOMTYPE_NONE)
        return true;
    if (playerId >= maxPlayers)
        return false;
    return controller->aidsBelongingToPlayerIds[playerId] == controller->subs[controller->currentSub].localAid;
}

static void QueueBalloonEvent(u8 eventPlayerId) {
    if (!(RKNet::Controller::sInstance != nullptr && RKNet::Controller::sInstance->roomType != RKNet::ROOMTYPE_NONE))
        return;

    ++sLocalBalloonLossSeq;
    if (sLocalBalloonLossSeq == 0)
        ++sLocalBalloonLossSeq;

    u8 writeIdx = sPendingBalloonEventWriteIdx;
    if (sPendingBalloonEventSize == pendingBalloonEventCount) {
        writeIdx = static_cast<u8>((sPendingBalloonEventWriteIdx + pendingBalloonEventCount - 1) % pendingBalloonEventCount);
    } else {
        ++sPendingBalloonEventSize;
        sPendingBalloonEventWriteIdx = static_cast<u8>((sPendingBalloonEventWriteIdx + 1) % pendingBalloonEventCount);
    }

    sPendingBalloonEventSeq[writeIdx] = sLocalBalloonLossSeq;
    sPendingBalloonEventPlayerId[writeIdx] = eventPlayerId;
    sPendingBalloonEventTimer[writeIdx] = 30;
}

static void QueueLocalBalloonLoss(u8 playerId) {
    if (!IsLocalPlayer(playerId))
        return;
    QueueBalloonEvent(playerId);
}

static void QueueBalloonMoveFromLocalLoss(u8 losingPlayerId, u8 gainingPlayerId) {
    if (losingPlayerId >= maxPlayers || gainingPlayerId >= maxPlayers)
        return;
    QueueBalloonEvent(static_cast<u8>(balloonMoveEventBase + losingPlayerId * maxPlayers + gainingPlayerId));
}

void WriteRH1Packet(Network::PulRH1 &packet) {
    RaceBalloonManager *balloonMgr = RaceBalloonManager::sInstance;
    const RKNet::Controller *controller = RKNet::Controller::sInstance;
    packet.battleRoyaleBalloonCounts = 0xFF;
    if (balloonMgr != nullptr && controller != nullptr && controller->roomType != RKNet::ROOMTYPE_NONE) {
        const RKNet::ControllerSub &sub = controller->subs[controller->currentSub];
        u8 localIdx = 0;
        const u8 playerCount = System::sInstance->nonTTGhostPlayersCount;
        for (u8 playerId = 0; playerId < playerCount && playerId < maxPlayers && localIdx < 2; ++playerId) {
            if (controller->aidsBelongingToPlayerIds[playerId] != sub.localAid)
                continue;

            u8 count = GetBalloonCount(balloonMgr, playerId);
            if (count > 5)
                count = 5;
            if (localIdx == 0)
                packet.battleRoyaleBalloonCounts = static_cast<u8>((packet.battleRoyaleBalloonCounts & 0xF0) | count);
            else
                packet.battleRoyaleBalloonCounts = static_cast<u8>((packet.battleRoyaleBalloonCounts & 0x0F) | (count << 4));
            ++localIdx;
        }
    }

    packet.battleRoyaleFinishMask = 0;
    for (u8 i = 0; i < 2; ++i) {
        packet.battleRoyaleFinishMinutes[i] = 0;
        packet.battleRoyaleFinishSeconds[i] = 0;
        packet.battleRoyaleFinishMilliseconds[i] = 0;
    }

    const Raceinfo *raceinfo = Raceinfo::sInstance;
    controller = RKNet::Controller::sInstance;
    if (raceinfo != nullptr && controller != nullptr && controller->roomType != RKNet::ROOMTYPE_NONE) {
        const RKNet::ControllerSub &sub = controller->subs[controller->currentSub];
        u8 localIdx = 0;
        const u8 playerCount = System::sInstance->nonTTGhostPlayersCount;
        for (u8 playerId = 0; playerId < playerCount && playerId < maxPlayers && localIdx < 2; ++playerId) {
            if (controller->aidsBelongingToPlayerIds[playerId] != sub.localAid)
                continue;

            const RaceinfoPlayer *player = raceinfo->players[playerId];
            if (player != nullptr && player->raceFinishTime != nullptr && player->raceFinishTime->isActive) {
                packet.battleRoyaleFinishMask |= 1 << localIdx;
                packet.battleRoyaleFinishMinutes[localIdx] = player->raceFinishTime->minutes;
                packet.battleRoyaleFinishSeconds[localIdx] = player->raceFinishTime->seconds;
                packet.battleRoyaleFinishMilliseconds[localIdx] = player->raceFinishTime->milliseconds;
            }
            ++localIdx;
        }
    }

    if (!ShouldApplyBattleRoyale() || sPendingBalloonEventSize == 0) {
        packet.battleRoyaleLossSeq = 0;
        packet.battleRoyaleLossPlayerId = 0xFF;
        return;
    }
    packet.battleRoyaleLossSeq = sPendingBalloonEventSeq[sPendingBalloonEventReadIdx];
    packet.battleRoyaleLossPlayerId = sPendingBalloonEventPlayerId[sPendingBalloonEventReadIdx];
}

static bool HasPoweredHitLossThisFrame(u8 playerId) {
    if (playerId >= maxPlayers)
        return false;
    const Raceinfo *raceinfo = Raceinfo::sInstance;
    return sPoweredHitLossFrame[playerId] == (raceinfo == nullptr ? 0xffff : raceinfo->raceFrames);
}

static bool HasMushroomStolenFromVictim(u8 gainingPlayerId, u8 losingPlayerId) {
    if (gainingPlayerId >= maxPlayers || losingPlayerId >= maxPlayers)
        return true;
    return (sMushroomStealVictimMask[gainingPlayerId] & (1 << losingPlayerId)) != 0;
}

static bool IsMushroomStealVictimProtected(u8 playerId) {
    Kart::Manager *kartMgr = Kart::Manager::sInstance;
    if (kartMgr == nullptr || playerId >= kartMgr->playerCount)
        return true;

    Kart::Player *player = kartMgr->GetKartPlayer(playerId);
    if (player == nullptr || player->pointers.kartStatus == nullptr)
        return true;

    const Kart::Status &status = *player->pointers.kartStatus;
    return (status.bitfield1 & 0x80000000) != 0 || (status.bitfield2 & 0x8000) != 0;
}

static void RecordMushroomStealVictim(u8 gainingPlayerId, u8 losingPlayerId) {
    if (gainingPlayerId >= maxPlayers || losingPlayerId >= maxPlayers)
        return;
    sMushroomStealVictimMask[gainingPlayerId] |= 1 << losingPlayerId;
    sMushroomStealVictimTimers[gainingPlayerId][losingPlayerId] = mushroomStealLockFrames;
}

static void UpdateMushroomStealVictimMasks() {
    for (u8 gainingPlayerId = 0; gainingPlayerId < maxPlayers; ++gainingPlayerId) {
        for (u8 losingPlayerId = 0; losingPlayerId < maxPlayers; ++losingPlayerId) {
            u8 &timer = sMushroomStealVictimTimers[gainingPlayerId][losingPlayerId];
            if (timer == 0)
                continue;

            --timer;
            if (timer == 0)
                sMushroomStealVictimMask[gainingPlayerId] &= ~(1 << losingPlayerId);
        }
    }
}

static void RemovePoweredHitBalloon(u8 playerId) {
    if (!ShouldApplyBattleRoyale() || playerId >= maxPlayers)
        return;
    if (HasPoweredHitLossThisFrame(playerId))
        return;

    if (!RemoveBalloon(RaceBalloonManager::sInstance, playerId))
        return;
    QueueLocalBalloonLoss(playerId);
    const Raceinfo *raceinfo = Raceinfo::sInstance;
    sPoweredHitLossFrame[playerId] = raceinfo == nullptr ? 0xffff : raceinfo->raceFrames;
}

static bool MoveBalloon(RaceBalloonManager *mgr, u8 toPlayer, u8 fromPlayer) {
    if (mgr == nullptr || toPlayer >= maxPlayers || fromPlayer >= maxPlayers)
        return false;
    if (GetBalloonCount(mgr, fromPlayer) == 0)
        return false;

    const u8 previousBalloonCount = GetBalloonCount(mgr, fromPlayer);
    mgr->Move(toPlayer, fromPlayer, 0, 1, 0);
    if (GetBalloonCount(mgr, fromPlayer) >= previousBalloonCount)
        return false;

    StartBalloonLossBlink(fromPlayer);
    return true;
}

static void ClearActiveGoldenMushroom(u8 playerId) {
    Item::Manager *itemMgr = Item::Manager::sInstance;
    if (itemMgr == nullptr || playerId >= itemMgr->playerCount)
        return;

    Item::PlayerInventory &inventory = itemMgr->players[playerId].inventory;
    if (inventory.currentItemId == GOLDEN_MUSHROOM && inventory.hasGolden && inventory.goldenTimer != 0) {
        inventory.ClearAll();
    }
}

static void AddStartingBalloons(RaceBalloonManager *mgr, int playerId, u32 teamId, u32 isInitial, int delay, u32 count, int interval) {
    if (ShouldApplyBattleRoyale()) {
        const u8 current = GetBalloonCount(mgr, static_cast<u8>(playerId));
        const u8 target = GetStartingBalloonCountSetting();
        if (current >= target)
            return;

        AddBattleRoyaleBalloons(mgr, static_cast<u8>(playerId), static_cast<u8>(teamId), static_cast<u8>(isInitial), delay, static_cast<u8>(target - current), interval);
        return;
    }

    mgr->Add(playerId, teamId, isInitial, delay, count, interval);
}
kmCall(0x80869ba8, AddStartingBalloons);

bool ShouldApplyBattleRoyale() {
    const System *system = System::sInstance;
    if (system == nullptr || !system->IsContext(PULSAR_MODE_BATTLEROYALE))
        return false;

    const RKNet::Controller *controller = RKNet::Controller::sInstance;
    if (controller == nullptr)
        return false;
    return controller->roomType == RKNet::ROOMTYPE_NONE || controller->roomType == RKNet::ROOMTYPE_FROOM_HOST || controller->roomType == RKNet::ROOMTYPE_FROOM_NONHOST;
}

static void CallRaceModeHit(void *raceMode, u32 vtableOffset, u32 firstPlayerId, u32 secondPlayerId) {
    if (raceMode == nullptr)
        return;
    void **vtable = *reinterpret_cast<void ***>(raceMode);
    RaceModeHitFn fn = reinterpret_cast<RaceModeHitFn>(vtable[vtableOffset / sizeof(void *)]);
    fn(raceMode, firstPlayerId, secondPlayerId);
}

static bool IsPlayerFinished(const Raceinfo &raceinfo, u8 playerId) {
    RaceinfoPlayer *player = raceinfo.players[playerId];
    if (player == nullptr)
        return false;
    return (player->stateFlags & 0x2) != 0;
}

static bool IsPlayerEliminated(u8 playerId) {
    if (!ShouldApplyBattleRoyale() || playerId >= maxPlayers)
        return false;
    const System *system = System::sInstance;
    if (system != nullptr && system->lapKoMgr != nullptr && !system->lapKoMgr->IsActive(playerId))
        return true;
    const Raceinfo *raceinfo = Raceinfo::sInstance;
    if (raceinfo != nullptr && IsPlayerFinished(*raceinfo, playerId))
        return true;
    return GetBalloonCount(RaceBalloonManager::sInstance, playerId) == 0;
}

static void StartOobWipeWithoutEliminatedPlayers(Kart::Link *link, u32 state) {
    if (link == nullptr || link->pointers == nullptr || link->pointers->camera == nullptr)
        return;
    if (IsPlayerEliminated(link->GetPlayerIdx()))
        return;
    link->pointers->kartStatus->StartOobWipe(state);
}
kmCall(0x80573f88, StartOobWipeWithoutEliminatedPlayers);
kmCall(0x80581530, StartOobWipeWithoutEliminatedPlayers);
kmCall(0x805966b0, StartOobWipeWithoutEliminatedPlayers);
kmCall(0x80598508, StartOobWipeWithoutEliminatedPlayers);
kmCall(0x805986cc, StartOobWipeWithoutEliminatedPlayers);

static bool AreOnSameBattleRoyaleTeam(u8 firstPlayerId, u8 secondPlayerId) {
    if (firstPlayerId >= maxPlayers || secondPlayerId >= maxPlayers)
        return false;

    const Racedata *racedata = Racedata::sInstance;
    if (racedata == nullptr)
        return false;

    const System *system = System::sInstance;
    UI::ExtendedTeamManager *extendedTeamMgr = UI::ExtendedTeamManager::sInstance;
    if (system != nullptr && system->IsContext(PULSAR_EXTENDEDTEAMS) && extendedTeamMgr != nullptr) {
        const UI::ExtendedTeamID firstTeam = extendedTeamMgr->GetPlayerTeam(firstPlayerId);
        return firstTeam != UI::TEAM_COUNT && firstTeam == extendedTeamMgr->GetPlayerTeam(secondPlayerId);
    }

    if ((racedata->racesScenario.settings.modeFlags & UI::ExtendedTeamManager::TEAM_MODE_FLAG) == 0)
        return false;

    const Team firstTeam = racedata->racesScenario.players[firstPlayerId].team;
    return firstTeam != TEAM_NONE && firstTeam == racedata->racesScenario.players[secondPlayerId].team;
}

static void OnRemoveHit(void *raceMode, u32 hitterPlayerId, u32 hittedPlayerId) {
    register u8 *itemObj;
    asm { mr itemObj, r31 }

    if (!ShouldApplyBattleRoyale()) {
        CallRaceModeHit(raceMode, 0x2c, hitterPlayerId, hittedPlayerId);
        return;
    }

    if (hitterPlayerId == hittedPlayerId)
        return;
    if (AreOnSameBattleRoyaleTeam(static_cast<u8>(hitterPlayerId), static_cast<u8>(hittedPlayerId)))
        return;
    if (HasPoweredHitLossThisFrame(static_cast<u8>(hittedPlayerId)))
        return;
    if ((RKNet::Controller::sInstance != nullptr && RKNet::Controller::sInstance->roomType != RKNet::ROOMTYPE_NONE) && !IsLocalPlayer(static_cast<u8>(hittedPlayerId)))
        return;
    if (IsPlayerFinished(*Raceinfo::sInstance, static_cast<u8>(hittedPlayerId)))
        return;
    if (*reinterpret_cast<ItemObjId *>(itemObj + 0x4) == OBJ_BLUE_SHELL && GetBalloonCount(RaceBalloonManager::sInstance, static_cast<u8>(hittedPlayerId)) == 1)
        return;
    if (!RemoveBalloon(RaceBalloonManager::sInstance, static_cast<u8>(hittedPlayerId)))
        return;
    QueueLocalBalloonLoss(static_cast<u8>(hittedPlayerId));
}
kmCall(0x8057293c, OnRemoveHit);
kmCall(0x80572960, OnRemoveHit);
kmCall(0x80572998, OnRemoveHit);
kmCall(0x805729c0, OnRemoveHit);
kmCall(0x805729f8, OnRemoveHit);
kmCall(0x80572a20, OnRemoveHit);
kmCall(0x80572a5c, OnRemoveHit);
kmCall(0x80572a84, OnRemoveHit);

static void OnMoveHit(void *raceMode, u32 losingPlayerId, u32 gainingPlayerId) {
    if (!ShouldApplyBattleRoyale()) {
        CallRaceModeHit(raceMode, 0x30, losingPlayerId, gainingPlayerId);
        return;
    }

    if (losingPlayerId == gainingPlayerId)
        return;
    if (HasPoweredHitLossThisFrame(static_cast<u8>(losingPlayerId)))
        return;

    RaceBalloonManager *balloonMgr = RaceBalloonManager::sInstance;
    const u8 losingPlayer = static_cast<u8>(losingPlayerId);
    const u8 gainingPlayer = static_cast<u8>(gainingPlayerId);
    if (AreOnSameBattleRoyaleTeam(losingPlayer, gainingPlayer))
        return;
    if (HasMushroomStolenFromVictim(gainingPlayer, losingPlayer))
        return;

    if ((RKNet::Controller::sInstance != nullptr && RKNet::Controller::sInstance->roomType != RKNet::ROOMTYPE_NONE) && !IsLocalPlayer(losingPlayer))
        return;

    const u8 previousBalloonCount = GetBalloonCount(balloonMgr, losingPlayer);
    if (previousBalloonCount <= 1)
        return;
    if (IsPlayerFinished(*Raceinfo::sInstance, losingPlayer))
        return;
    if (IsMushroomStealVictimProtected(losingPlayer))
        return;

    if (!MoveBalloon(balloonMgr, gainingPlayer, losingPlayer))
        return;

    RecordMushroomStealVictim(gainingPlayer, losingPlayer);
    if ((RKNet::Controller::sInstance != nullptr && RKNet::Controller::sInstance->roomType != RKNet::ROOMTYPE_NONE))
        QueueBalloonMoveFromLocalLoss(losingPlayer, gainingPlayer);

    if (IsLocalPlayer(gainingPlayer))
        ClearActiveGoldenMushroom(gainingPlayer);
}
kmCall(0x8057015c, OnMoveHit);
kmCall(0x80570184, OnMoveHit);
kmCall(0x805701c0, OnMoveHit);
kmCall(0x805701e8, OnMoveHit);
kmCall(0x80570a50, OnMoveHit);
kmCall(0x80570a78, OnMoveHit);
kmCall(0x80570ab4, OnMoveHit);
kmCall(0x80570adc, OnMoveHit);
kmCall(0x80570c24, OnMoveHit);
kmCall(0x80570c4c, OnMoveHit);
kmCall(0x80570c88, OnMoveHit);
kmCall(0x80570cb0, OnMoveHit);

static void OnMoveHitFromRemoveCall(void *raceMode, u32 firstPlayerId, u32 secondPlayerId) {
    if (!ShouldApplyBattleRoyale()) {
        CallRaceModeHit(raceMode, 0x2c, firstPlayerId, secondPlayerId);
        return;
    }

    OnMoveHit(raceMode, firstPlayerId, secondPlayerId);
}
kmCall(0x805704f4, OnMoveHitFromRemoveCall);
kmCall(0x8057051c, OnMoveHitFromRemoveCall);
kmCall(0x80570558, OnMoveHitFromRemoveCall);
kmCall(0x80570580, OnMoveHitFromRemoveCall);

static void FinishPoweredHitAction(void *action, u32 sourcePlayerObjId) {
    if (sourcePlayerObjId >= maxPlayers)
        return;

    const u8 playerId = reinterpret_cast<Kart::Link *>(action)->GetPlayerIdx();
    if (AreOnSameBattleRoyaleTeam(playerId, static_cast<u8>(sourcePlayerObjId)))
        return;
    RemovePoweredHitBalloon(playerId);
}

static void OnStarHitAction(void *action, u32 sourcePlayerObjId) {
    reinterpret_cast<Kart::Action *>(action)->StartAction3(sourcePlayerObjId);
    EjectItemsFromItemDamage(reinterpret_cast<Kart::Link *>(action)->GetPlayerIdx());
    FinishPoweredHitAction(action, sourcePlayerObjId);
}
kmWritePointer(0x808b4d6c, OnStarHitAction);

static void OnBulletHitAction(void *action, u32 sourcePlayerObjId) {
    reinterpret_cast<Kart::Action *>(action)->StartAction6(sourcePlayerObjId);
    EjectItemsFromItemDamage(reinterpret_cast<Kart::Link *>(action)->GetPlayerIdx());
    FinishPoweredHitAction(action, sourcePlayerObjId);
}
kmWritePointer(0x808b4d90, OnBulletHitAction);

static void OnMegaHitAction(void *action, u32 sourcePlayerObjId) {
    reinterpret_cast<Kart::Action *>(action)->StartAction13(sourcePlayerObjId);
    EjectItemsFromItemDamage(reinterpret_cast<Kart::Link *>(action)->GetPlayerIdx());
    FinishPoweredHitAction(action, sourcePlayerObjId);
}
kmWritePointer(0x808b4de4, OnMegaHitAction);

static asmFunc ForceItemCollisionModeWhenBattleRoyale() {
    ASM(
        nofralloc;
        lis r12, sCollisionPatchesActive @ha;
        lbz r12, sCollisionPatchesActive @l(r12);
        cmpwi r12, 0;
        beq vanilla;
        li r4, 3;
        blr;
    vanilla:
        lwz r4, 0xb70(r5);
        blr;
    )
}
kmCall(0x80572814, ForceItemCollisionModeWhenBattleRoyale);

static asmFunc CompareKartCollisionModeWhenBattleRoyale() {
    ASM(
        nofralloc;
        lis r12, sCollisionPatchesActive @ha;
        lbz r12, sCollisionPatchesActive @l(r12);
        cmpwi r12, 0;
        beq vanilla;
        cmpwi r29, 1;
        blr;
    vanilla:
        cmpwi r29, 0;
        blr;
    )
}
kmCall(0x80570100, CompareKartCollisionModeWhenBattleRoyale);
kmCall(0x80570494, CompareKartCollisionModeWhenBattleRoyale);
kmCall(0x805709d4, CompareKartCollisionModeWhenBattleRoyale);
kmCall(0x80570ba8, CompareKartCollisionModeWhenBattleRoyale);

static asmFunc ForceKartCollisionBattleTypeWhenBattleRoyale() {
    ASM(
        nofralloc;
        lis r12, sCollisionPatchesActive @ha;
        lbz r12, sCollisionPatchesActive @l(r12);
        cmpwi r12, 0;
        beq vanilla;
        li r5, 3;
        blr;
    vanilla:
        lwz r5, 0xb70(r4);
        blr;
    )
}
kmCall(0x8057012c, ForceKartCollisionBattleTypeWhenBattleRoyale);
kmCall(0x805704c4, ForceKartCollisionBattleTypeWhenBattleRoyale);
kmCall(0x80570a20, ForceKartCollisionBattleTypeWhenBattleRoyale);
kmCall(0x80570bf4, ForceKartCollisionBattleTypeWhenBattleRoyale);

kmWrite32(0x808698c0, 0x1c040005);  // allocate five balloons per player

static asmFunc SetBalloonPoolCount() {
    ASM(
        nofralloc;
        lis r12, sCreatingBattleRoyaleBalloonManager @ha;
        lbz r12, sCreatingBattleRoyaleBalloonManager @l(r12);
        cmpwi r12, 0;
        bnelr;
        rlwinm r0, r0, 1, 24, 30;
        blr;
    )
}
kmCall(0x808698c8, SetBalloonPoolCount);

static void LoadConstructedBalloonModel(GeoObj::ObjBalloon *balloon) {
    if (sCreatingBattleRoyaleBalloonManager)
        return;

    void **vtable = *reinterpret_cast<void ***>(balloon);
    GeoObjectLoadFn load = reinterpret_cast<GeoObjectLoadFn>(vtable[0x20 / sizeof(void *)]);
    load(balloon);
}
kmCall(0x8086994c, LoadConstructedBalloonModel);  // defer balloon model load until a slot is used in Battle Royale
kmWrite32(0x80869950, 0x60000000);
kmWrite32(0x80869954, 0x60000000);
kmWrite32(0x80869958, 0x60000000);

kmWrite32(0x8082a554, 0x38600500);  // ObjectDirector arrays: 200 -> 320 pointers
kmWrite32(0x8082a560, 0x38600500);
kmWrite32(0x8082a56c, 0x38600500);
kmWrite32(0x8082a578, 0x38600500);
kmWrite32(0x8082a584, 0x38600500);
kmWrite32(0x8082a590, 0x38600500);
kmWrite32(0x8082a59c, 0x38600f00);  // hit depth vec3 array: 200 -> 320 entries
kmWrite32(0x8082a5b8, 0x38e00140);
kmWrite32(0x8082a5c4, 0x38600500);  // collision scenario array: 200 -> 320 entries

static void *ConstructBalloon(void *balloon, u8 poolIdx, u8 teamId) {
    return new (balloon) GeoObj::ObjBalloon(poolIdx, GetBattleRoyaleBalloonPoolTeam(poolIdx, teamId));
}
kmCall(0x8086993c, ConstructBalloon);

static void AllocateObjectHeap(GameScene *scene, u32 size, u32 parentHeapIdx) {
    if (ShouldApplyBattleRoyale() && size == 0x80000 && parentHeapIdx == 1) {
        size = 0x100000;
    }

    scene->CreateHeap(size, parentHeapIdx);
}
kmCall(0x80554570, AllocateObjectHeap);

static void CreateBalloonManager() {
    if (!ShouldApplyBattleRoyale()) {
        RaceBalloonManager::CreateInstance();
        return;
    }

    sDeferredBalloonManagerCreation = true;
}
kmCall(0x8082a7c4, CreateBalloonManager);

static GameScene *sDeferredDestroyScene = nullptr;
static EGG::Heap *sDeferredDestroyHeap = nullptr;

static void DestroyHeapAfterDeferredBalloons(GameScene *scene, EGG::Heap *heap) {
    if (sDeferredBalloonManagerCreation && ShouldApplyBattleRoyale()) {
        sDeferredDestroyScene = scene;
        sDeferredDestroyHeap = heap;
        return;
    }

    scene->DestroyHeap(heap);
}
kmCall(0x8082a7f4, DestroyHeapAfterDeferredBalloons);

static void CreateObjectsThenCreateDeferredBalloons(ObjectsMgr *objectDirector, bool isInitial) {
    objectDirector->CreateAllObjects(isInitial);

    if (sDeferredBalloonManagerCreation && ShouldApplyBattleRoyale()) {
        RacedataSettings &settings = Racedata::sInstance->racesScenario.settings;
        const GameMode prevMode = settings.gamemode;
        const BattleType prevBattleType = settings.battleType;
        settings.gamemode = MODE_BATTLE;
        settings.battleType = BATTLE_BALLOON;
        for (u8 i = 0; i < maxBattleRoyaleBalloons; ++i) sLoadedBalloonModels[i] = false;
        sCreatingBattleRoyaleBalloonManager = true;
        RaceBalloonManager::CreateInstance();
        sCreatingBattleRoyaleBalloonManager = false;
        LoadStartingBalloonModels(RaceBalloonManager::sInstance);
        settings.gamemode = prevMode;
        settings.battleType = prevBattleType;
        sDeferredBalloonManagerCreation = false;
    }

    if (sDeferredDestroyHeap != nullptr) {
        sDeferredDestroyScene->DestroyHeap(sDeferredDestroyHeap);
        sDeferredDestroyScene = nullptr;
        sDeferredDestroyHeap = nullptr;
    }
}
kmCall(0x8082a800, CreateObjectsThenCreateDeferredBalloons);

static void DestroyBattleRoyaleItemOnFallBoundary(Item::Obj *itemObj) {
    itemObj->ProcessObjectCollisions();

    if (!ShouldApplyBattleRoyale() || itemObj->itemObjId == OBJ_BLUE_SHELL || (itemObj->kclType.bitfield & KCL_BITFIELD_FALL_BOUNDARY) == 0 || (itemObj->bitfield74 & 1) != 0)
        return;
    itemObj->DisappearDueToExcess(true);
}
kmCall(0x8079f52c, DestroyBattleRoyaleItemOnFallBoundary);
kmCall(0x8079f560, DestroyBattleRoyaleItemOnFallBoundary);

// Original code by CLF78
static void DisableItemPoof(Item::Obj *itemObj) {
    if (ShouldApplyBattleRoyale() && (itemObj->kclType.bitfield & KCL_BITFIELD_FALL_BOUNDARY) == 0)
        return;
    itemObj->TryDisappearDueToExcess();
}
kmCall(0x807965c0, DisableItemPoof);

static void ProcessBalloonEliminations(LapKO::Mgr &lapKoMgr, RaceBalloonManager *balloonMgr) {
    const u8 playerCount = System::sInstance->nonTTGhostPlayersCount;
    for (u8 playerId = 0; playerId < playerCount && playerId < maxPlayers; ++playerId) {
        const u8 current = GetBalloonCount(balloonMgr, playerId);
        if (lapKoMgr.IsActive(playerId) && sPreviousBalloonCount[playerId] != 0 && current == 0) {
            Raceinfo *raceinfo = Raceinfo::sInstance;
            if (raceinfo != nullptr) {
                if ((RKNet::Controller::sInstance != nullptr && RKNet::Controller::sInstance->roomType != RKNet::ROOMTYPE_NONE)) {
                    RaceinfoPlayer *player = raceinfo->players[playerId];
                    if (player != nullptr && IsPlayerFinished(*raceinfo, playerId) && player->currentLap >= player->maxLap) {
                        sPreviousBalloonCount[playerId] = current;
                        continue;
                    }
                }
                if (playerId < maxPlayers && sEliminationCount < maxPlayers) {
                    ++sEliminationCount;
                    const System *system = System::sInstance;
                    u8 totalPlayers = system == nullptr ? maxPlayers : system->nonTTGhostPlayersCount;
                    if (totalPlayers > maxPlayers)
                        totalPlayers = maxPlayers;
                    const u8 placement = static_cast<u8>(totalPlayers - sEliminationCount + 1);
                    LapKO::EndRaceWithEliminationFinishTime(playerId, placement);
                }
            }
            lapKoMgr.ProcessElimination(
              playerId, LapKO::Mgr::ELIMINATION_CAUSE_ROUND, (RKNet::Controller::sInstance != nullptr && RKNet::Controller::sInstance->roomType != RKNet::ROOMTYPE_NONE), true);
        }
        sPreviousBalloonCount[playerId] = current;
    }
}

static void FinishSoleActiveUnfinishedPlayer(LapKO::Mgr &lapKoMgr, Raceinfo &raceinfo) {
    const u8 playerCount = System::sInstance->nonTTGhostPlayersCount;
    u8 playerId = 0xff;
    u8 remainingCount = 0;
    for (u8 idx = 0; idx < playerCount && idx < maxPlayers; ++idx) {
        if (!lapKoMgr.IsActive(idx) || IsPlayerFinished(raceinfo, idx))
            continue;
        playerId = idx;
        ++remainingCount;
    }
    if (remainingCount != 1 || playerId >= maxPlayers)
        return;

    RaceinfoPlayer *player = raceinfo.players[playerId];
    if (player == nullptr)
        return;

    Timer now(false);
    raceinfo.CloneTimer(&now);
    now.SetActive(true);
    player->EndRace(now, false, 0);
    raceinfo.EndPlayerRace(playerId);
    if ((RKNet::Controller::sInstance != nullptr && RKNet::Controller::sInstance->roomType != RKNet::ROOMTYPE_NONE))
        raceinfo.CheckEndRaceOnline(playerId);
    lapKoMgr.raceFinished = true;
}

static void ApplyRemoteFinishTimes(RKNet::Controller &controller, u8 aid, const Network::PulRH1 &packet) {
    Raceinfo *raceinfo = Raceinfo::sInstance;
    if (raceinfo == nullptr || packet.battleRoyaleFinishMask == 0)
        return;

    u8 remotePlayerIdx = 0;
    const u8 playerCount = System::sInstance->nonTTGhostPlayersCount;
    for (u8 playerId = 0; playerId < playerCount && playerId < maxPlayers && remotePlayerIdx < 2; ++playerId) {
        if (controller.aidsBelongingToPlayerIds[playerId] != aid)
            continue;

        if ((packet.battleRoyaleFinishMask & (1 << remotePlayerIdx)) != 0) {
            RaceinfoPlayer *player = raceinfo->players[playerId];
            if (player != nullptr && player->raceFinishTime != nullptr && IsPlayerFinished(*raceinfo, playerId)) {
                player->raceFinishTime->minutes = packet.battleRoyaleFinishMinutes[remotePlayerIdx];
                player->raceFinishTime->seconds = packet.battleRoyaleFinishSeconds[remotePlayerIdx];
                player->raceFinishTime->milliseconds = packet.battleRoyaleFinishMilliseconds[remotePlayerIdx];
                player->raceFinishTime->SetActive(true);
            }
        }

        ++remotePlayerIdx;
    }
}

static void ConsumeRemoteBalloonLosses(RKNet::Controller &controller, const RKNet::ControllerSub &sub, RaceBalloonManager *balloonMgr) {
    if (!(RKNet::Controller::sInstance != nullptr && RKNet::Controller::sInstance->roomType != RKNet::ROOMTYPE_NONE))
        return;

    for (u8 aid = 0; aid < maxPlayers; ++aid) {
        if (aid == sub.localAid)
            continue;
        if ((sub.availableAids & (1 << aid)) == 0)
            continue;

        const u32 bufferIdx = controller.lastReceivedBufferUsed[aid][RKNet::PACKET_RACEHEADER1];
        RKNet::SplitRACEPointers *split = controller.splitReceivedRACEPackets[bufferIdx][aid];
        if (split == nullptr)
            continue;

        const RKNet::PacketHolder<Network::PulRH1> *holder = split->GetPacketHolder<Network::PulRH1>();
        if (holder == nullptr || holder->packetSize < Network::PulRH1SizeFull)
            continue;

        const Network::PulRH1 *packet = holder->packet;
        ApplyRemoteFinishTimes(controller, aid, *packet);

        const u8 seq = packet->battleRoyaleLossSeq;
        const u8 eventPlayerId = packet->battleRoyaleLossPlayerId;
        if (seq != 0 && seq != sLastRemoteBalloonLossSeq[aid]) {
            sLastRemoteBalloonLossSeq[aid] = seq;
            if (eventPlayerId >= balloonMoveEventBase) {
                const u8 move = eventPlayerId - balloonMoveEventBase;
                const u8 losingPlayerId = move / maxPlayers;
                const u8 gainingPlayerId = move % maxPlayers;
                if (losingPlayerId < maxPlayers && gainingPlayerId < maxPlayers && controller.aidsBelongingToPlayerIds[losingPlayerId] == aid
                  && !HasMushroomStolenFromVictim(gainingPlayerId, losingPlayerId)) {
                    const u8 previousBalloonCount = GetBalloonCount(balloonMgr, losingPlayerId);
                    if (previousBalloonCount > 1 && MoveBalloon(balloonMgr, gainingPlayerId, losingPlayerId)) {
                        RecordMushroomStealVictim(gainingPlayerId, losingPlayerId);
                        if (IsLocalPlayer(gainingPlayerId))
                            ClearActiveGoldenMushroom(gainingPlayerId);
                    }
                }
            } else if (eventPlayerId < maxPlayers && controller.aidsBelongingToPlayerIds[eventPlayerId] == aid) {
                RemoveBalloon(balloonMgr, eventPlayerId);
            }
        }

        const u8 packedCounts = packet->battleRoyaleBalloonCounts;
        if (packedCounts == 0xFF)
            continue;

        u8 remotePlayerIdx = 0;
        const u8 playerCount = System::sInstance->nonTTGhostPlayersCount;
        for (u8 playerId = 0; playerId < playerCount && playerId < maxPlayers && remotePlayerIdx < 2; ++playerId) {
            if (controller.aidsBelongingToPlayerIds[playerId] != aid)
                continue;

            const u8 target = (remotePlayerIdx == 0) ? static_cast<u8>(packedCounts & 0x0F) : static_cast<u8>((packedCounts >> 4) & 0x0F);
            if (target <= 5) {
                u8 current = GetBalloonCount(balloonMgr, playerId);
                while (current < target) {
                    AddBalloons(balloonMgr, playerId, 1);
                    const u8 next = GetBalloonCount(balloonMgr, playerId);
                    if (next <= current)
                        break;
                    current = next;
                }
                while (current > target) {
                    if (!RemoveBalloon(balloonMgr, playerId))
                        break;
                    const u8 next = GetBalloonCount(balloonMgr, playerId);
                    if (next >= current)
                        break;
                    current = next;
                }
            }
            ++remotePlayerIdx;
        }
    }
}

static void FrameUpdate() {
    if (!ShouldApplyBattleRoyale()) {
        sCollisionPatchesActive = false;
        sInitialized = false;
        sLastRaceFrames = 0xffff;
        sDeferredBalloonManagerCreation = false;
        sCreatingBattleRoyaleBalloonManager = false;
        sDeferredDestroyScene = nullptr;
        sDeferredDestroyHeap = nullptr;
        for (u8 playerId = 0; playerId < maxPlayers; ++playerId) {
            sPoweredHitLossFrame[playerId] = 0xffff;
            sPreviousBalloonCount[playerId] = 0;
            sLastRemoteBalloonLossSeq[playerId] = 0;
            sMushroomStealVictimMask[playerId] = 0;
            for (u8 otherPlayerId = 0; otherPlayerId < maxPlayers; ++otherPlayerId) {
                sMushroomStealVictimTimers[playerId][otherPlayerId] = 0;
            }
        }
        for (u8 i = 0; i < maxBattleRoyaleBalloons; ++i) sLoadedBalloonModels[i] = false;
        sEliminationCount = 0;
        sPendingBalloonEventReadIdx = 0;
        sPendingBalloonEventWriteIdx = 0;
        sPendingBalloonEventSize = 0;
        for (u8 i = 0; i < pendingBalloonEventCount; ++i) {
            sPendingBalloonEventSeq[i] = 0;
            sPendingBalloonEventPlayerId[i] = 0xff;
            sPendingBalloonEventTimer[i] = 0;
        }
        return;
    }
    sCollisionPatchesActive = true;

    System *system = System::sInstance;
    LapKO::Mgr *lapKoMgr = system->lapKoMgr;
    Raceinfo *raceinfo = Raceinfo::sInstance;
    if (lapKoMgr == nullptr || raceinfo == nullptr)
        return;

    RaceBalloonManager *balloonMgr = RaceBalloonManager::sInstance;
    if (balloonMgr == nullptr)
        return;

    if (sLastRaceFrames != 0xffff && raceinfo->raceFrames < sLastRaceFrames) {
        sInitialized = false;
    }
    sLastRaceFrames = raceinfo->raceFrames;

    if (!sInitialized && raceinfo->IsAtLeastStage(RACESTAGE_COUNTDOWN)) {
        lapKoMgr->InitForRace();
        sEliminationCount = 0;

        for (u8 playerId = 0; playerId < maxPlayers; ++playerId) {
            sPoweredHitLossFrame[playerId] = 0xffff;
            sLastRemoteBalloonLossSeq[playerId] = 0;
        }

        const u8 playerCount = System::sInstance->nonTTGhostPlayersCount;
        const u8 targetCount = GetStartingBalloonCountSetting();
        for (u8 playerId = 0; playerId < playerCount && playerId < maxPlayers; ++playerId) {
            const u8 current = GetBalloonCount(balloonMgr, playerId);
            if (current < targetCount) {
                AddBalloons(balloonMgr, playerId, static_cast<u8>(targetCount - current));
            }
            sPreviousBalloonCount[playerId] = GetBalloonCount(balloonMgr, playerId);
        }

        sInitialized = true;
    }

    if (!sInitialized)
        return;

    EliminationDisplay::Tick();
    {
        RKNet::Controller *controller = RKNet::Controller::sInstance;
        const RKNet::ControllerSub &sub = controller->subs[controller->currentSub];
        if (lapKoMgr->isHost && controller->roomType != RKNet::ROOMTYPE_NONE)
            lapKoMgr->HostMonitorDisconnects(*controller, sub);
    }
    Spectating::Update(*raceinfo);
    lapKoMgr->ProcessPendingItemReweight();

    RKNet::Controller *controller = RKNet::Controller::sInstance;
    const RKNet::ControllerSub &sub = controller->subs[controller->currentSub];
    ConsumeRemoteBalloonLosses(*controller, sub, balloonMgr);

    if (sPendingBalloonEventSize != 0) {
        u8 &timer = sPendingBalloonEventTimer[sPendingBalloonEventReadIdx];
        if (timer > 0) {
            --timer;
        } else {
            sPendingBalloonEventSeq[sPendingBalloonEventReadIdx] = 0;
            sPendingBalloonEventPlayerId[sPendingBalloonEventReadIdx] = 0xff;
            sPendingBalloonEventReadIdx = static_cast<u8>((sPendingBalloonEventReadIdx + 1) % pendingBalloonEventCount);
            --sPendingBalloonEventSize;
        }
    }
    UpdateMushroomStealVictimMasks();

    if (!lapKoMgr->raceFinished && raceinfo->IsAtLeastStage(RACESTAGE_RACE)) {
        ProcessBalloonEliminations(*lapKoMgr, balloonMgr);
        FinishSoleActiveUnfinishedPlayer(*lapKoMgr, *raceinfo);
    }
}
static RaceFrameHook battleRoyaleFrameHook(FrameUpdate);

}  // namespace BattleRoyale
}  // namespace Pulsar
