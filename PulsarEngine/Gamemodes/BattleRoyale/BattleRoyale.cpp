#include <PulsarSystem.hpp>
#include <Gamemodes/BattleRoyale/BattleRoyale.hpp>
#include <Gamemodes/LapKO/LapKOMgr.hpp>
#include <MarioKartWii/Kart/KartLink.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <Network/PacketExpansion.hpp>
#include <Settings/Settings.hpp>
#include <runtimeWrite.hpp>

namespace Pulsar {
namespace BattleRoyale {

static const u8 maxPlayers = 12;
static const u8 startingBalloonCount = 1;
static const u32 balloonPlayerArrayOffset = 0x3c4;
static const u32 balloonPlayerSize = 0x18;

static bool sInitialized = false;
static u16 sLastRaceFrames = 0xffff;
static u16 sPoweredHitLossFrame[maxPlayers];
static u8 sPreviousBalloonCount[maxPlayers];
static bool sWasActive[maxPlayers];
static u8 sEliminationPlacementOrder[maxPlayers];
static u8 sEliminationPlacementCount = 0;
static u8 sLocalBalloonLossSeq = 0;
static u8 sPendingBalloonLossSeq = 0;
static u8 sPendingBalloonLossPlayerId = 0xff;
static u8 sPendingBalloonLossTimer = 0;
static u8 sLastRemoteBalloonLossSeq[maxPlayers];

bool ShouldApplyBattleRoyale();

typedef void (*BalloonAddFn)(void* mgr, int playerId, u32 teamId, u32 isInitial, int delay, u32 count, int interval);
typedef void (*BalloonRemoveFn)(void* mgr, u32 playerId, u32 visible, u32 sound, int delay, u32 count, int interval);
typedef void (*BalloonMoveFn)(void* mgr, u32 toPlayer, u32 fromPlayer, int delay, u32 count, int interval);
typedef void (*RaceModeHitFn)(void* raceMode, u32 firstPlayerId, u32 secondPlayerId);
typedef void (*CreateBalloonManagerFn)();
typedef void (*KartActionHitFn)(void* action, u32 sourcePlayerObjId);

kmRuntimeUse(0x809c4748);
kmRuntimeUse(0x80869df4);
kmRuntimeUse(0x80869fd0);
kmRuntimeUse(0x8086a0dc);
kmRuntimeUse(0x808697bc);
kmRuntimeUse(0x80568718);
kmRuntimeUse(0x80569024);
kmRuntimeUse(0x80569818);
kmRuntimeUse(0x80572814);
kmRuntimeUse(0x80570100);
kmRuntimeUse(0x8057012c);
kmRuntimeUse(0x80570494);
kmRuntimeUse(0x805704c4);
kmRuntimeUse(0x805709d4);
kmRuntimeUse(0x80570a20);
kmRuntimeUse(0x80570ba8);
kmRuntimeUse(0x80570bf4);

static void* GetBalloonManager() {
    return *reinterpret_cast<void**>(kmRuntimeAddr(0x809c4748));
}

static u8 GetBalloonCount(void* mgr, u8 playerId) {
    if (mgr == nullptr || playerId >= maxPlayers) return 0;
    const u8* base = reinterpret_cast<const u8*>(mgr);
    return base[balloonPlayerArrayOffset + playerId * balloonPlayerSize];
}

static void AddBalloons(void* mgr, u8 playerId, u8 count) {
    if (mgr == nullptr || playerId >= maxPlayers || count == 0) return;

    const RacedataScenario& scenario = Racedata::sInstance->racesScenario;
    u32 team = static_cast<u32>(scenario.players[playerId].team);
    if (team > 1) team = 0;

    BalloonAddFn add = reinterpret_cast<BalloonAddFn>(kmRuntimeAddr(0x80869df4));
    add(mgr, playerId, team, 1, 0, count, 0);
}

static u8 GetKoPerRaceSetting() {
    u8 koPerRace = 1;
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    if (controller != nullptr && controller->roomType != RKNet::ROOMTYPE_NONE) {
        koPerRace = System::sInstance->netMgr.battleRoyaleKoPerRace - 1;
    } else {
        koPerRace = Settings::Mgr::Get().GetUserSettingValue(Settings::SETTINGSTYPE_KO, SCROLLER_KOPERRACE) + 1;
    }

    if (koPerRace < 1) return 1;
    if (koPerRace > 4) return 4;
    return koPerRace;
}

static u8 GetStartingBalloonAddCount() {
    u8 count = GetKoPerRaceSetting();
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    return count;
}

static void RemoveBalloon(void* mgr, u8 playerId) {
    if (mgr == nullptr || playerId >= maxPlayers || GetBalloonCount(mgr, playerId) == 0) return;

    BalloonRemoveFn remove = reinterpret_cast<BalloonRemoveFn>(kmRuntimeAddr(0x80869fd0));
    remove(mgr, playerId, 1, 1, 0, 1, 0);
}

static bool IsOnline() {
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    return controller != nullptr && controller->roomType != RKNet::ROOMTYPE_NONE;
}

static bool IsLocalAid(u8 aid) {
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    if (controller == nullptr) return true;
    const RKNet::ControllerSub& sub = controller->subs[controller->currentSub];
    return aid == sub.localAid;
}

static bool IsLocalPlayer(u8 playerId) {
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    if (controller == nullptr || controller->roomType == RKNet::ROOMTYPE_NONE) return true;
    if (playerId >= maxPlayers) return false;
    return IsLocalAid(controller->aidsBelongingToPlayerIds[playerId]);
}

static void QueueLocalBalloonLoss(u8 playerId) {
    if (!IsOnline() || !IsLocalPlayer(playerId)) return;

    ++sLocalBalloonLossSeq;
    if (sLocalBalloonLossSeq == 0) ++sLocalBalloonLossSeq;
    sPendingBalloonLossSeq = sLocalBalloonLossSeq;
    sPendingBalloonLossPlayerId = playerId;
    sPendingBalloonLossTimer = 90;
}

void WriteRH1Packet(Network::PulRH1& packet) {
    if (!ShouldApplyBattleRoyale() || sPendingBalloonLossTimer == 0 || sPendingBalloonLossPlayerId >= maxPlayers) {
        packet.battleRoyaleLossSeq = 0;
        packet.battleRoyaleLossPlayerId = 0xFF;
        return;
    }

    packet.battleRoyaleLossSeq = sPendingBalloonLossSeq;
    packet.battleRoyaleLossPlayerId = sPendingBalloonLossPlayerId;
}

static u16 GetCurrentRaceFrames() {
    const Raceinfo* raceinfo = Raceinfo::sInstance;
    return raceinfo == nullptr ? 0xffff : raceinfo->raceFrames;
}

static bool HasPoweredHitLossThisFrame(u8 playerId) {
    if (playerId >= maxPlayers) return false;
    return sPoweredHitLossFrame[playerId] == GetCurrentRaceFrames();
}

static void RemovePoweredHitBalloon(u8 playerId) {
    if (!ShouldApplyBattleRoyale() || playerId >= maxPlayers) return;
    if (HasPoweredHitLossThisFrame(playerId)) return;

    RemoveBalloon(GetBalloonManager(), playerId);
    QueueLocalBalloonLoss(playerId);
    sPoweredHitLossFrame[playerId] = GetCurrentRaceFrames();
}

static void MoveBalloon(void* mgr, u8 toPlayer, u8 fromPlayer) {
    if (mgr == nullptr || toPlayer >= maxPlayers || fromPlayer >= maxPlayers) return;
    if (GetBalloonCount(mgr, fromPlayer) == 0) return;

    BalloonMoveFn move = reinterpret_cast<BalloonMoveFn>(kmRuntimeAddr(0x8086a0dc));
    move(mgr, toPlayer, fromPlayer, 0, 1, 0);
}

static void AddStartingBalloons(void* mgr, int playerId, u32 teamId, u32 isInitial, int delay, u32 count, int interval) {
    BalloonAddFn add = reinterpret_cast<BalloonAddFn>(kmRuntimeAddr(0x80869df4));
    if (ShouldApplyBattleRoyale()) count = GetStartingBalloonAddCount();
    add(mgr, playerId, teamId, isInitial, delay, count, interval);
}

kmCall(0x80869ba8, AddStartingBalloons);

bool ShouldApplyBattleRoyale() {
    const System* system = System::sInstance;
    if (system == nullptr || !system->IsContext(PULSAR_MODE_BATTLEROYALE)) return false;

    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    if (controller == nullptr) return false;
    return controller->roomType == RKNet::ROOMTYPE_NONE ||
           controller->roomType == RKNet::ROOMTYPE_FROOM_HOST ||
           controller->roomType == RKNet::ROOMTYPE_FROOM_NONHOST;
}

static void CallRaceModeHit(void* raceMode, u32 vtableOffset, u32 firstPlayerId, u32 secondPlayerId) {
    if (raceMode == nullptr) return;
    void** vtable = *reinterpret_cast<void***>(raceMode);
    RaceModeHitFn fn = reinterpret_cast<RaceModeHitFn>(vtable[vtableOffset / sizeof(void*)]);
    fn(raceMode, firstPlayerId, secondPlayerId);
}

static void OnRemoveHit(void* raceMode, u32 hitterPlayerId, u32 hittedPlayerId) {
    if (!ShouldApplyBattleRoyale()) {
        CallRaceModeHit(raceMode, 0x2c, hitterPlayerId, hittedPlayerId);
        return;
    }

    if (hitterPlayerId == hittedPlayerId) return;
    if (HasPoweredHitLossThisFrame(static_cast<u8>(hittedPlayerId))) return;
    RemoveBalloon(GetBalloonManager(), static_cast<u8>(hittedPlayerId));
    QueueLocalBalloonLoss(static_cast<u8>(hittedPlayerId));
}

static void OnMoveHit(void* raceMode, u32 losingPlayerId, u32 gainingPlayerId) {
    if (!ShouldApplyBattleRoyale()) {
        CallRaceModeHit(raceMode, 0x30, losingPlayerId, gainingPlayerId);
        return;
    }

    if (losingPlayerId == gainingPlayerId) return;
    if (HasPoweredHitLossThisFrame(static_cast<u8>(losingPlayerId))) return;
    MoveBalloon(GetBalloonManager(), static_cast<u8>(gainingPlayerId), static_cast<u8>(losingPlayerId));
    QueueLocalBalloonLoss(static_cast<u8>(losingPlayerId));
}

static void FinishPoweredHitAction(void* action) {
    const u8 playerId = reinterpret_cast<Kart::Link*>(action)->GetPlayerIdx();
    RemovePoweredHitBalloon(playerId);
}

static void OnStarHitAction(void* action, u32 sourcePlayerObjId) {
    KartActionHitFn original = reinterpret_cast<KartActionHitFn>(kmRuntimeAddr(0x80568718));
    original(action, sourcePlayerObjId);
    FinishPoweredHitAction(action);
}

static void OnBulletHitAction(void* action, u32 sourcePlayerObjId) {
    KartActionHitFn original = reinterpret_cast<KartActionHitFn>(kmRuntimeAddr(0x80569024));
    original(action, sourcePlayerObjId);
    FinishPoweredHitAction(action);
}

static void OnMegaHitAction(void* action, u32 sourcePlayerObjId) {
    KartActionHitFn original = reinterpret_cast<KartActionHitFn>(kmRuntimeAddr(0x80569818));
    original(action, sourcePlayerObjId);
    FinishPoweredHitAction(action);
}

kmWritePointer(0x808b4d6c, OnStarHitAction);
kmWritePointer(0x808b4d90, OnBulletHitAction);
kmWritePointer(0x808b4de4, OnMegaHitAction);

kmCall(0x8057293c, OnRemoveHit);
kmCall(0x80572960, OnRemoveHit);
kmCall(0x80572998, OnRemoveHit);
kmCall(0x805729c0, OnRemoveHit);
kmCall(0x805729f8, OnRemoveHit);
kmCall(0x80572a20, OnRemoveHit);
kmCall(0x80572a5c, OnRemoveHit);
kmCall(0x80572a84, OnRemoveHit);

kmCall(0x8057015c, OnMoveHit);
kmCall(0x80570184, OnMoveHit);
kmCall(0x805701c0, OnMoveHit);
kmCall(0x805701e8, OnMoveHit);
kmCall(0x805704f4, OnMoveHit);
kmCall(0x8057051c, OnMoveHit);
kmCall(0x80570558, OnMoveHit);
kmCall(0x80570580, OnMoveHit);
kmCall(0x80570a50, OnMoveHit);
kmCall(0x80570a78, OnMoveHit);
kmCall(0x80570ab4, OnMoveHit);
kmCall(0x80570adc, OnMoveHit);
kmCall(0x80570c24, OnMoveHit);
kmCall(0x80570c4c, OnMoveHit);
kmCall(0x80570c88, OnMoveHit);
kmCall(0x80570cb0, OnMoveHit);

static void ApplyCollisionPatches(bool active) {
    if (active) {
        kmRuntimeWrite32A(0x80572814, 0x38800003);  // li r4, MODE_BATTLE for item balloon branch only
        kmRuntimeWrite32A(0x80570100, 0x2c1d0001);  // let VS enter kart balloon branches
        kmRuntimeWrite32A(0x8057012c, 0x38a00003);
        kmRuntimeWrite32A(0x80570494, 0x2c1d0001);
        kmRuntimeWrite32A(0x805704c4, 0x38a00003);
        kmRuntimeWrite32A(0x805709d4, 0x2c1d0001);
        kmRuntimeWrite32A(0x80570a20, 0x38a00003);
        kmRuntimeWrite32A(0x80570ba8, 0x2c1d0001);
        kmRuntimeWrite32A(0x80570bf4, 0x38a00003);
        return;
    }

    kmRuntimeWrite32A(0x80572814, 0x80850b70);
    kmRuntimeWrite32A(0x80570100, 0x2c1d0000);
    kmRuntimeWrite32A(0x8057012c, 0x80a40b70);
    kmRuntimeWrite32A(0x80570494, 0x2c1d0000);
    kmRuntimeWrite32A(0x805704c4, 0x80a40b70);
    kmRuntimeWrite32A(0x805709d4, 0x2c1d0000);
    kmRuntimeWrite32A(0x80570a20, 0x80a40b70);
    kmRuntimeWrite32A(0x80570ba8, 0x2c1d0000);
    kmRuntimeWrite32A(0x80570bf4, 0x80a40b70);
}

static void CreateBalloonManager() {
    CreateBalloonManagerFn create = reinterpret_cast<CreateBalloonManagerFn>(kmRuntimeAddr(0x808697bc));

    if (!ShouldApplyBattleRoyale()) {
        create();
        return;
    }

    RacedataSettings& settings = Racedata::sInstance->racesScenario.settings;
    const GameMode prevMode = settings.gamemode;
    const BattleType prevBattleType = settings.battleType;
    settings.gamemode = MODE_BATTLE;
    settings.battleType = BATTLE_BALLOON;
    create();
    settings.gamemode = prevMode;
    settings.battleType = prevBattleType;
}

kmCall(0x8082a7c4, CreateBalloonManager);

static bool IsAuthoritative() {
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    if (controller == nullptr || controller->roomType == RKNet::ROOMTYPE_NONE) return true;
    const RKNet::ControllerSub& sub = controller->subs[controller->currentSub];
    return sub.localAid == sub.hostAid;
}

static void ResetState() {
    sInitialized = false;
    sLastRaceFrames = 0xffff;
    for (u8 playerId = 0; playerId < maxPlayers; ++playerId) {
        sPoweredHitLossFrame[playerId] = 0xffff;
        sPreviousBalloonCount[playerId] = 0;
        sWasActive[playerId] = false;
        sEliminationPlacementOrder[playerId] = 0xff;
        sLastRemoteBalloonLossSeq[playerId] = 0;
    }
    sEliminationPlacementCount = 0;
    sPendingBalloonLossSeq = 0;
    sPendingBalloonLossPlayerId = 0xff;
    sPendingBalloonLossTimer = 0;
}

static void InitForRace(LapKO::Mgr& lapKoMgr, void* balloonMgr) {
    lapKoMgr.InitForRace();
    sEliminationPlacementCount = 0;

    for (u8 playerId = 0; playerId < maxPlayers; ++playerId) {
        sPoweredHitLossFrame[playerId] = 0xffff;
        sEliminationPlacementOrder[playerId] = 0xff;
        sLastRemoteBalloonLossSeq[playerId] = 0;
    }

    const u8 playerCount = System::sInstance->nonTTGhostPlayersCount;
    for (u8 playerId = 0; playerId < playerCount && playerId < maxPlayers; ++playerId) {
        const u8 current = GetBalloonCount(balloonMgr, playerId);
        if (current < startingBalloonCount) {
            AddBalloons(balloonMgr, playerId, static_cast<u8>(startingBalloonCount - current));
        }
        sPreviousBalloonCount[playerId] = GetBalloonCount(balloonMgr, playerId);
        sWasActive[playerId] = lapKoMgr.IsActive(playerId);
    }

    sInitialized = true;
}

static bool HasPlacementRecord(u8 playerId) {
    for (u8 i = 0; i < sEliminationPlacementCount; ++i) {
        if (sEliminationPlacementOrder[i] == playerId) return true;
    }
    return false;
}

static void RecordPlacementElimination(u8 playerId) {
    if (playerId >= maxPlayers || HasPlacementRecord(playerId) || sEliminationPlacementCount >= maxPlayers) return;
    sEliminationPlacementOrder[sEliminationPlacementCount++] = playerId;
}

static bool IsActiveForPlacement(const LapKO::Mgr& lapKoMgr, u8 playerId) {
    return lapKoMgr.IsActive(playerId) && !HasPlacementRecord(playerId);
}

static void RecordEliminatedPlacements(const LapKO::Mgr& lapKoMgr) {
    const u8 playerCount = System::sInstance->nonTTGhostPlayersCount;
    for (u8 playerId = 0; playerId < playerCount && playerId < maxPlayers; ++playerId) {
        const bool active = lapKoMgr.IsActive(playerId);
        if (sWasActive[playerId] && !active) {
            RecordPlacementElimination(playerId);
        }
        sWasActive[playerId] = active;
    }
}

static void UpdateRemainingPlayerPlacements(LapKO::Mgr& lapKoMgr, Raceinfo& raceinfo, bool updateRaceOrder) {
    if (raceinfo.playerIdInEachPosition == nullptr) return;

    RecordEliminatedPlacements(lapKoMgr);

    const u8 playerCount = System::sInstance->nonTTGhostPlayersCount;
    u8 order[maxPlayers];
    u8 count = 0;

    for (u8 pos = 0; pos < playerCount && pos < maxPlayers; ++pos) {
        const u8 playerId = raceinfo.playerIdInEachPosition[pos];
        if (playerId >= maxPlayers || !IsActiveForPlacement(lapKoMgr, playerId)) continue;
        order[count++] = playerId;
    }

    for (s8 idx = static_cast<s8>(sEliminationPlacementCount) - 1; idx >= 0 && count < maxPlayers; --idx) {
        const u8 playerId = sEliminationPlacementOrder[idx];
        if (playerId < playerCount) order[count++] = playerId;
    }

    for (u8 playerId = 0; playerId < playerCount && playerId < maxPlayers && count < playerCount; ++playerId) {
        bool alreadyAdded = false;
        for (u8 i = 0; i < count; ++i) {
            if (order[i] == playerId) {
                alreadyAdded = true;
                break;
            }
        }
        if (!alreadyAdded) order[count++] = playerId;
    }

    for (u8 pos = 0; pos < count && pos < playerCount; ++pos) {
        const u8 playerId = order[pos];
        if (updateRaceOrder) raceinfo.playerIdInEachPosition[pos] = playerId;
        RaceinfoPlayer* player = raceinfo.players[playerId];
        if (player != nullptr) player->position = static_cast<u8>(pos + 1);
    }
}

static void TickLapKoPieces(LapKO::Mgr& lapKoMgr, Raceinfo& raceinfo) {
    lapKoMgr.TickEliminationDisplay();

    RKNet::Controller* controller = RKNet::Controller::sInstance;
    const RKNet::ControllerSub& sub = controller->subs[controller->currentSub];

    if (lapKoMgr.isHost && controller->roomType != RKNet::ROOMTYPE_NONE) {
        lapKoMgr.HostMonitorDisconnects(*controller, sub);
    }

    if (lapKoMgr.isSpectating) {
        lapKoMgr.UpdateSpectatorInputs(raceinfo);
        lapKoMgr.MaintainSpectatorView(raceinfo);
    }

    lapKoMgr.ProcessPendingItemReweight();

    if (lapKoMgr.isHost) {
        lapKoMgr.HostDistributeEvents(*controller, sub);
    } else {
        lapKoMgr.ClientConsumeHostEvents(*controller, sub);
    }
}

static void ProcessBalloonEliminations(LapKO::Mgr& lapKoMgr, void* balloonMgr) {
    const u8 playerCount = System::sInstance->nonTTGhostPlayersCount;
    for (u8 playerId = 0; playerId < playerCount && playerId < maxPlayers; ++playerId) {
        const u8 current = GetBalloonCount(balloonMgr, playerId);
        if (lapKoMgr.IsActive(playerId) && sPreviousBalloonCount[playerId] != 0 && current == 0) {
            Raceinfo* raceinfo = Raceinfo::sInstance;
            if (raceinfo != nullptr) {
                RecordPlacementElimination(playerId);
                const bool finalElimination = lapKoMgr.GetActiveCount() <= 2;
                UpdateRemainingPlayerPlacements(lapKoMgr, *raceinfo, finalElimination);
            }
            lapKoMgr.ProcessElimination(playerId, LapKO::Mgr::ELIMINATION_CAUSE_ROUND, false, true);
        }
        sPreviousBalloonCount[playerId] = current;
    }
}

static void ConsumeRemoteBalloonLosses(RKNet::Controller& controller, const RKNet::ControllerSub& sub, void* balloonMgr) {
    if (!IsOnline()) return;

    for (u8 aid = 0; aid < maxPlayers; ++aid) {
        if (aid == sub.localAid) continue;
        if ((sub.availableAids & (1 << aid)) == 0) continue;

        const u32 bufferIdx = controller.lastReceivedBufferUsed[aid][RKNet::PACKET_RACEHEADER1];
        RKNet::SplitRACEPointers* split = controller.splitReceivedRACEPackets[bufferIdx][aid];
        if (split == nullptr) continue;

        const RKNet::PacketHolder<Network::PulRH1>* holder = split->GetPacketHolder<Network::PulRH1>();
        if (holder == nullptr || holder->packetSize < Network::PulRH1SizeFull) continue;

        const Network::PulRH1* packet = holder->packet;
        const u8 seq = packet->battleRoyaleLossSeq;
        const u8 playerId = packet->battleRoyaleLossPlayerId;
        if (seq == 0 || seq == sLastRemoteBalloonLossSeq[aid] || playerId >= maxPlayers) continue;
        if (controller.aidsBelongingToPlayerIds[playerId] != aid) continue;

        sLastRemoteBalloonLossSeq[aid] = seq;
        RemoveBalloon(balloonMgr, playerId);
    }
}

static void FrameUpdate() {
    if (!ShouldApplyBattleRoyale()) {
        ApplyCollisionPatches(false);
        ResetState();
        return;
    }
    ApplyCollisionPatches(true);

    System* system = System::sInstance;
    LapKO::Mgr* lapKoMgr = system->lapKoMgr;
    Raceinfo* raceinfo = Raceinfo::sInstance;
    if (lapKoMgr == nullptr || raceinfo == nullptr) return;

    void* balloonMgr = GetBalloonManager();
    if (balloonMgr == nullptr) return;

    if (sLastRaceFrames != 0xffff && raceinfo->raceFrames < sLastRaceFrames) {
        sInitialized = false;
    }
    sLastRaceFrames = raceinfo->raceFrames;

    if (!sInitialized && raceinfo->IsAtLeastStage(RACESTAGE_COUNTDOWN)) {
        InitForRace(*lapKoMgr, balloonMgr);
    }

    if (!sInitialized) return;

    TickLapKoPieces(*lapKoMgr, *raceinfo);

    RKNet::Controller* controller = RKNet::Controller::sInstance;
    const RKNet::ControllerSub& sub = controller->subs[controller->currentSub];
    ConsumeRemoteBalloonLosses(*controller, sub, balloonMgr);

    if (sPendingBalloonLossTimer > 0) --sPendingBalloonLossTimer;

    if (!lapKoMgr->raceFinished && IsAuthoritative() && raceinfo->IsAtLeastStage(RACESTAGE_RACE)) {
        ProcessBalloonEliminations(*lapKoMgr, balloonMgr);
    }

    UpdateRemainingPlayerPlacements(*lapKoMgr, *raceinfo, false);
}

static RaceFrameHook battleRoyaleFrameHook(FrameUpdate);

}  // namespace BattleRoyale
}  // namespace Pulsar
