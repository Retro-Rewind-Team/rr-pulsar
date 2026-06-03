#include <PulsarSystem.hpp>
#include <Gamemodes/BattleRoyale/BattleRoyale.hpp>
#include <Gamemodes/LapKO/LapKOMgr.hpp>
#include <MarioKartWii/Item/ItemManager.hpp>
#include <MarioKartWii/Kart/KartLink.hpp>
#include <MarioKartWii/Kart/KartManager.hpp>
#include <MarioKartWii/Kart/KartMovement.hpp>
#include <MarioKartWii/Kart/KartPointers.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <Network/PacketExpansion.hpp>
#include <Settings/Settings.hpp>
#include <runtimeWrite.hpp>

namespace Pulsar {
namespace BattleRoyale {

static const u8 maxPlayers = 12;
static const u8 maxBattleRoyaleBalloons = maxPlayers * 5;
static const u8 preloadedBalloonsPerPlayer = 4;
static const u32 balloonPoolOffset = 0x4;
static const u32 balloonPoolEntrySize = 0x8;
static const u32 balloonPlayerArrayOffset = 0x3c4;
static const u32 balloonPlayerSize = 0x18;
static const u32 balloonMgrTimeOffset = 0x4e4;
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
static bool sLoadedBalloonModels[maxBattleRoyaleBalloons];
static u16 sMushroomStealVictimMask[maxPlayers];
static u8 sMushroomStealVictimTimers[maxPlayers][maxPlayers];

typedef void (*BalloonAddFn)(void* mgr, int playerId, u32 teamId, u32 isInitial, int delay, u32 count, int interval);
typedef void (*BalloonRemoveFn)(void* mgr, u32 playerId, u32 visible, u32 sound, int delay, u32 count, int interval);
typedef void (*BalloonMoveFn)(void* mgr, u32 toPlayer, u32 fromPlayer, int delay, u32 count, int interval);
typedef void (*BalloonOnAddFn)(void* balloon, u32 time, u8 playerId, u8 balloonIndex, u8 isInitial);
typedef void (*RaceModeHitFn)(void* raceMode, u32 firstPlayerId, u32 secondPlayerId);
typedef void (*CreateBalloonManagerFn)();
typedef void* (*AllocateHeapFn)(void* scene, u32 size, u32 parentHeapIdx);
typedef void (*CreateObjectsFn)(void* objectDirector, bool isInitial);
typedef void (*DestroyHeapFn)(void* scene, void* heap);
typedef void (*GeoObjectLoadFn)(void* object);
typedef void (*KartActionHitFn)(void* action, u32 sourcePlayerObjId);
typedef void (*KartMoveStartBlinkLocalFn)(Kart::Movement* movement);

void EjectItemsFromItemDamage(u8 playerId);

kmRuntimeUse(0x809c4748);
kmRuntimeUse(0x805819a8);
kmRuntimeUse(0x80869df4);
kmRuntimeUse(0x80869fd0);
kmRuntimeUse(0x8086a0dc);
kmRuntimeUse(0x8086ec5c);
kmRuntimeUse(0x808697bc);
kmRuntimeUse(0x8051b8e4);
kmRuntimeUse(0x80826e8c);
kmRuntimeUse(0x8051b988);
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

static u8 GetBalloonPoolCount(void* mgr) {
    if (mgr == nullptr) return 0;
    return reinterpret_cast<u8*>(mgr)[2];
}

static void* GetBalloonFromPool(void* mgr, u8 poolIdx) {
    u8* entry = reinterpret_cast<u8*>(mgr) + balloonPoolOffset + poolIdx * balloonPoolEntrySize;
    return *reinterpret_cast<void**>(entry);
}

static u8& GetBalloonPoolPlayerId(void* mgr, u8 poolIdx) {
    return *(reinterpret_cast<u8*>(mgr) + balloonPoolOffset + poolIdx * balloonPoolEntrySize + 5);
}

static u8& GetBalloonPoolIndex(void* mgr, u8 poolIdx) {
    return *(reinterpret_cast<u8*>(mgr) + balloonPoolOffset + poolIdx * balloonPoolEntrySize + 6);
}

static u8& GetBalloonPoolTeamId(void* mgr, u8 poolIdx) {
    return *(reinterpret_cast<u8*>(mgr) + balloonPoolOffset + poolIdx * balloonPoolEntrySize + 4);
}

static void*& GetPlayerBalloonSlot(void* mgr, u8 playerId, u8 balloonIndex) {
    u8* player = reinterpret_cast<u8*>(mgr) + balloonPlayerArrayOffset + playerId * balloonPlayerSize;
    return *reinterpret_cast<void**>(player + 4 + balloonIndex * sizeof(void*));
}

static u32 GetBalloonMgrTime(void* mgr) {
    return *reinterpret_cast<u32*>(reinterpret_cast<u8*>(mgr) + balloonMgrTimeOffset);
}

static void LoadBalloonModel(void* mgr, u8 poolIdx) {
    if (poolIdx >= maxBattleRoyaleBalloons || sLoadedBalloonModels[poolIdx]) return;

    void* balloon = GetBalloonFromPool(mgr, poolIdx);
    if (balloon == nullptr) return;

    void** vtable = *reinterpret_cast<void***>(balloon);
    GeoObjectLoadFn load = reinterpret_cast<GeoObjectLoadFn>(vtable[0x20 / sizeof(void*)]);
    load(balloon);
    sLoadedBalloonModels[poolIdx] = true;
}

static void ClearLoadedBalloonModels() {
    for (u8 i = 0; i < maxBattleRoyaleBalloons; ++i) sLoadedBalloonModels[i] = false;
}

static void LoadStartingBalloonModels(void* mgr) {
    if (mgr == nullptr) return;

    const u8 playerCount = reinterpret_cast<u8*>(mgr)[0];
    u8 modelsToLoad = static_cast<u8>(playerCount * preloadedBalloonsPerPlayer);
    const u8 poolCount = GetBalloonPoolCount(mgr);
    if (modelsToLoad > poolCount) modelsToLoad = poolCount;

    for (u8 poolIdx = 0; poolIdx < modelsToLoad; ++poolIdx) LoadBalloonModel(mgr, poolIdx);
}

static void AddBattleRoyaleBalloons(void* mgr, u8 playerId, u8 teamId, u8 isInitial, int delay, u8 count, int interval) {
    if (mgr == nullptr || playerId >= maxPlayers || count == 0) return;
    if (teamId > 1) teamId = 0;

    u8 current = GetBalloonCount(mgr, playerId);
    if (current >= 5) return;
    if (count > 5 - current) count = static_cast<u8>(5 - current);

    const u8 poolCount = GetBalloonPoolCount(mgr);
    const u8 freePlayerId = reinterpret_cast<u8*>(mgr)[0];
    BalloonOnAddFn onAdd = reinterpret_cast<BalloonOnAddFn>(kmRuntimeAddr(0x8086ec5c));
    const u32 time = GetBalloonMgrTime(mgr) + delay;

    for (u8 poolIdx = 0, added = 0; poolIdx < poolCount && added < count; ++poolIdx) {
        if (GetBalloonPoolPlayerId(mgr, poolIdx) != freePlayerId || GetBalloonPoolTeamId(mgr, poolIdx) != teamId) continue;

        if (poolIdx >= maxBattleRoyaleBalloons || !sLoadedBalloonModels[poolIdx]) continue;
        void* balloon = GetBalloonFromPool(mgr, poolIdx);
        if (balloon == nullptr) continue;

        const u8 balloonIndex = current++;
        onAdd(balloon, time + added * interval, playerId, balloonIndex, isInitial);
        GetBalloonPoolPlayerId(mgr, poolIdx) = playerId;
        GetBalloonPoolIndex(mgr, poolIdx) = balloonIndex;
        GetPlayerBalloonSlot(mgr, playerId, balloonIndex) = balloon;
        reinterpret_cast<u8*>(mgr)[balloonPlayerArrayOffset + playerId * balloonPlayerSize] = current;
        ++added;
    }
}

static void AddBalloons(void* mgr, u8 playerId, u8 count) {
    if (mgr == nullptr || playerId >= maxPlayers || count == 0) return;

    const RacedataScenario& scenario = Racedata::sInstance->racesScenario;
    u32 team = static_cast<u32>(scenario.players[playerId].team);
    if (team > 1) team = 0;

    if (ShouldApplyBattleRoyale()) {
        AddBattleRoyaleBalloons(mgr, playerId, static_cast<u8>(team), 1, 0, count, 0);
        return;
    }

    BalloonAddFn add = reinterpret_cast<BalloonAddFn>(kmRuntimeAddr(0x80869df4));
    add(mgr, playerId, team, 1, 0, count, 0);
}

static u8 GetKoPerRaceSetting() {
    const System* system = System::sInstance;
    if (system != nullptr) {
        if (system->IsContext(PULSAR_KOPERRACE_4)) return 4;
        if (system->IsContext(PULSAR_KOPERRACE_3)) return 3;
        if (system->IsContext(PULSAR_KOPERRACE_2)) return 2;
    }
    return 1;
}

static u8 GetStartingBalloonAddCount() {
    return GetKoPerRaceSetting();
}

static void StartBalloonLossBlink(u8 playerId) {
    Raceinfo* raceinfo = Raceinfo::sInstance;
    if (raceinfo == nullptr || !raceinfo->IsAtLeastStage(RACESTAGE_RACE)) return;

    Kart::Manager* kartMgr = Kart::Manager::sInstance;
    if (kartMgr == nullptr || playerId >= kartMgr->playerCount) return;

    Kart::Player* player = kartMgr->GetKartPlayer(playerId);
    if (player == nullptr) return;

    RacedataSettings& settings = Racedata::sInstance->racesScenario.settings;
    const GameMode prevMode = settings.gamemode;
    const BattleType prevBattleType = settings.battleType;
    settings.gamemode = MODE_BATTLE;
    settings.battleType = BATTLE_BALLOON;

    KartMoveStartBlinkLocalFn startBlinkLocal = reinterpret_cast<KartMoveStartBlinkLocalFn>(kmRuntimeAddr(0x805819a8));
    Kart::Movement& movement = player->GetMovement();
    startBlinkLocal(&movement);

    s16& blinkTimer = *reinterpret_cast<s16*>(reinterpret_cast<u8*>(&movement) + kartMovementBlinkTimerOffset);
    blinkTimer *= 1.5f;

    settings.gamemode = prevMode;
    settings.battleType = prevBattleType;
}

static bool RemoveBalloon(void* mgr, u8 playerId) {
    if (mgr == nullptr || playerId >= maxPlayers || GetBalloonCount(mgr, playerId) == 0) return false;

    const u8 previousBalloonCount = GetBalloonCount(mgr, playerId);
    BalloonRemoveFn remove = reinterpret_cast<BalloonRemoveFn>(kmRuntimeAddr(0x80869fd0));
    remove(mgr, playerId, 1, 1, 0, 1, 0);
    if (GetBalloonCount(mgr, playerId) >= previousBalloonCount) return false;

    StartBalloonLossBlink(playerId);
    return true;
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

static void QueueBalloonEvent(u8 eventPlayerId) {
    if (!IsOnline()) return;

    ++sLocalBalloonLossSeq;
    if (sLocalBalloonLossSeq == 0) ++sLocalBalloonLossSeq;

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
    if (!IsLocalPlayer(playerId)) return;
    QueueBalloonEvent(playerId);
}

static void QueueBalloonMoveFromLocalLoss(u8 losingPlayerId, u8 gainingPlayerId) {
    if (losingPlayerId >= maxPlayers || gainingPlayerId >= maxPlayers) return;
    QueueBalloonEvent(static_cast<u8>(balloonMoveEventBase + losingPlayerId * maxPlayers + gainingPlayerId));
}

static u8 GetPackedLocalBalloonCounts(void* balloonMgr) {
    if (balloonMgr == nullptr) return 0xFF;

    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    if (controller == nullptr || controller->roomType == RKNet::ROOMTYPE_NONE) return 0xFF;

    const RKNet::ControllerSub& sub = controller->subs[controller->currentSub];
    u8 packed = 0xFF;
    u8 localIdx = 0;
    const u8 playerCount = System::sInstance->nonTTGhostPlayersCount;

    for (u8 playerId = 0; playerId < playerCount && playerId < maxPlayers && localIdx < 2; ++playerId) {
        if (controller->aidsBelongingToPlayerIds[playerId] != sub.localAid) continue;

        u8 count = GetBalloonCount(balloonMgr, playerId);
        if (count > 5) count = 5;
        if (localIdx == 0) {
            packed = static_cast<u8>((packed & 0xF0) | count);
        } else {
            packed = static_cast<u8>((packed & 0x0F) | (count << 4));
        }
        ++localIdx;
    }

    return packed;
}

static void WriteLocalFinishTimes(Network::PulRH1& packet) {
    packet.battleRoyaleFinishMask = 0;
    packet.battleRoyaleFinishMinutes[0] = 0;
    packet.battleRoyaleFinishMinutes[1] = 0;
    packet.battleRoyaleFinishSeconds[0] = 0;
    packet.battleRoyaleFinishSeconds[1] = 0;
    packet.battleRoyaleFinishMilliseconds[0] = 0;
    packet.battleRoyaleFinishMilliseconds[1] = 0;

    const Raceinfo* raceinfo = Raceinfo::sInstance;
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    if (raceinfo == nullptr || controller == nullptr || controller->roomType == RKNet::ROOMTYPE_NONE) return;

    const RKNet::ControllerSub& sub = controller->subs[controller->currentSub];
    u8 localIdx = 0;
    const u8 playerCount = System::sInstance->nonTTGhostPlayersCount;

    for (u8 playerId = 0; playerId < playerCount && playerId < maxPlayers && localIdx < 2; ++playerId) {
        if (controller->aidsBelongingToPlayerIds[playerId] != sub.localAid) continue;

        const RaceinfoPlayer* player = raceinfo->players[playerId];
        if (player != nullptr && player->raceFinishTime != nullptr && player->raceFinishTime->isActive) {
            packet.battleRoyaleFinishMask |= 1 << localIdx;
            packet.battleRoyaleFinishMinutes[localIdx] = player->raceFinishTime->minutes;
            packet.battleRoyaleFinishSeconds[localIdx] = player->raceFinishTime->seconds;
            packet.battleRoyaleFinishMilliseconds[localIdx] = player->raceFinishTime->milliseconds;
        }
        ++localIdx;
    }
}

void WriteRH1Packet(Network::PulRH1& packet) {
    packet.battleRoyaleBalloonCounts = GetPackedLocalBalloonCounts(GetBalloonManager());
    WriteLocalFinishTimes(packet);

    if (!ShouldApplyBattleRoyale() || sPendingBalloonEventSize == 0) {
        packet.battleRoyaleLossSeq = 0;
        packet.battleRoyaleLossPlayerId = 0xFF;
        return;
    }

    packet.battleRoyaleLossSeq = sPendingBalloonEventSeq[sPendingBalloonEventReadIdx];
    packet.battleRoyaleLossPlayerId = sPendingBalloonEventPlayerId[sPendingBalloonEventReadIdx];
}

static u16 GetCurrentRaceFrames() {
    const Raceinfo* raceinfo = Raceinfo::sInstance;
    return raceinfo == nullptr ? 0xffff : raceinfo->raceFrames;
}

static bool HasPoweredHitLossThisFrame(u8 playerId) {
    if (playerId >= maxPlayers) return false;
    return sPoweredHitLossFrame[playerId] == GetCurrentRaceFrames();
}

static bool HasMushroomStolenFromVictim(u8 gainingPlayerId, u8 losingPlayerId) {
    if (gainingPlayerId >= maxPlayers || losingPlayerId >= maxPlayers) return true;
    return (sMushroomStealVictimMask[gainingPlayerId] & (1 << losingPlayerId)) != 0;
}

static void RecordMushroomStealVictim(u8 gainingPlayerId, u8 losingPlayerId) {
    if (gainingPlayerId >= maxPlayers || losingPlayerId >= maxPlayers) return;
    sMushroomStealVictimMask[gainingPlayerId] |= 1 << losingPlayerId;
    sMushroomStealVictimTimers[gainingPlayerId][losingPlayerId] = mushroomStealLockFrames;
}

static void UpdateMushroomStealVictimMasks() {
    for (u8 gainingPlayerId = 0; gainingPlayerId < maxPlayers; ++gainingPlayerId) {
        for (u8 losingPlayerId = 0; losingPlayerId < maxPlayers; ++losingPlayerId) {
            u8& timer = sMushroomStealVictimTimers[gainingPlayerId][losingPlayerId];
            if (timer == 0) continue;

            --timer;
            if (timer == 0) sMushroomStealVictimMask[gainingPlayerId] &= ~(1 << losingPlayerId);
        }
    }
}

static void RemovePoweredHitBalloon(u8 playerId) {
    if (!ShouldApplyBattleRoyale() || playerId >= maxPlayers) return;
    if (HasPoweredHitLossThisFrame(playerId)) return;

    RemoveBalloon(GetBalloonManager(), playerId);
    QueueLocalBalloonLoss(playerId);
    sPoweredHitLossFrame[playerId] = GetCurrentRaceFrames();
}

static bool MoveBalloon(void* mgr, u8 toPlayer, u8 fromPlayer) {
    if (mgr == nullptr || toPlayer >= maxPlayers || fromPlayer >= maxPlayers) return false;
    if (GetBalloonCount(mgr, fromPlayer) == 0) return false;

    const u8 previousBalloonCount = GetBalloonCount(mgr, fromPlayer);
    BalloonMoveFn move = reinterpret_cast<BalloonMoveFn>(kmRuntimeAddr(0x8086a0dc));
    move(mgr, toPlayer, fromPlayer, 0, 1, 0);
    if (GetBalloonCount(mgr, fromPlayer) >= previousBalloonCount) return false;

    StartBalloonLossBlink(fromPlayer);
    return true;
}

static void ClearActiveGoldenMushroom(u8 playerId) {
    Item::Manager* itemMgr = Item::Manager::sInstance;
    if (itemMgr == nullptr || playerId >= itemMgr->playerCount) return;

    Item::PlayerInventory& inventory = itemMgr->players[playerId].inventory;
    if (inventory.currentItemId == GOLDEN_MUSHROOM && inventory.hasGolden && inventory.goldenTimer != 0) {
        inventory.ClearAll();
    }
}

static void AddStartingBalloons(void* mgr, int playerId, u32 teamId, u32 isInitial, int delay, u32 count, int interval) {
    if (ShouldApplyBattleRoyale()) {
        const u8 current = GetBalloonCount(mgr, static_cast<u8>(playerId));
        const u8 target = GetKoPerRaceSetting();
        if (current >= target) return;

        AddBattleRoyaleBalloons(mgr, static_cast<u8>(playerId), static_cast<u8>(teamId), static_cast<u8>(isInitial), delay,
                                static_cast<u8>(target - current), interval);
        return;
    }

    BalloonAddFn add = reinterpret_cast<BalloonAddFn>(kmRuntimeAddr(0x80869df4));
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

static bool IsPlayerFinished(const Raceinfo& raceinfo, u8 playerId) {
    RaceinfoPlayer* player = raceinfo.players[playerId];
    if (player == nullptr) return false;
    return (player->stateFlags & 0x2) != 0;
}

static bool IsPlayerEliminated(u8 playerId) {
    if (!ShouldApplyBattleRoyale() || playerId >= maxPlayers) return false;
    const System* system = System::sInstance;
    if (system != nullptr && system->lapKoMgr != nullptr && !system->lapKoMgr->IsActive(playerId)) return true;
    const Raceinfo* raceinfo = Raceinfo::sInstance;
    if (raceinfo != nullptr && IsPlayerFinished(*raceinfo, playerId)) return true;
    return GetBalloonCount(GetBalloonManager(), playerId) == 0;
}

static void StartOobWipeWithoutEliminatedPlayers(Kart::Link* link, u32 state) {
    if (link == nullptr || link->pointers == nullptr || link->pointers->camera == nullptr) return;
    if (IsPlayerEliminated(link->GetPlayerIdx())) return;
    link->pointers->kartStatus->StartOobWipe(state);
}

kmBranch(0x80591784, StartOobWipeWithoutEliminatedPlayers);

static bool IsPlayerOnlineRaceComplete(const Raceinfo& raceinfo, u8 playerId) {
    if (!IsOnline()) return false;

    RaceinfoPlayer* player = raceinfo.players[playerId];
    if (player == nullptr) return false;
    return IsPlayerFinished(raceinfo, playerId) && player->currentLap >= player->maxLap;
}

static void OnRemoveHit(void* raceMode, u32 hitterPlayerId, u32 hittedPlayerId) {
    register u8* itemObj;
    asm { mr itemObj, r31 }

    if (!ShouldApplyBattleRoyale()) {
        CallRaceModeHit(raceMode, 0x2c, hitterPlayerId, hittedPlayerId);
        return;
    }

    if (hitterPlayerId == hittedPlayerId) return;
    if (HasPoweredHitLossThisFrame(static_cast<u8>(hittedPlayerId))) return;
    if (IsOnline() && !IsLocalPlayer(static_cast<u8>(hittedPlayerId))) return;
    if (IsPlayerFinished(*Raceinfo::sInstance, static_cast<u8>(hittedPlayerId))) return;
    if (*reinterpret_cast<ItemObjId*>(itemObj + 0x4) == OBJ_BLUE_SHELL &&
        GetBalloonCount(GetBalloonManager(), static_cast<u8>(hittedPlayerId)) == 1) return;
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

    void* balloonMgr = GetBalloonManager();
    const u8 losingPlayer = static_cast<u8>(losingPlayerId);
    const u8 gainingPlayer = static_cast<u8>(gainingPlayerId);
    if (HasMushroomStolenFromVictim(gainingPlayer, losingPlayer)) return;

    if (!IsOnline()) {
        const u8 previousBalloonCount = GetBalloonCount(balloonMgr, losingPlayer);
        if (previousBalloonCount <= 1) return;
        if (IsPlayerFinished(*Raceinfo::sInstance, losingPlayer)) return;
        MoveBalloon(balloonMgr, gainingPlayer, losingPlayer);
        if (GetBalloonCount(balloonMgr, losingPlayer) >= previousBalloonCount) return;
        RecordMushroomStealVictim(gainingPlayer, losingPlayer);
        ClearActiveGoldenMushroom(gainingPlayer);
        return;
    }

    if (!IsLocalPlayer(losingPlayer)) return;

    const u8 previousBalloonCount = GetBalloonCount(balloonMgr, losingPlayer);
    if (previousBalloonCount <= 1) return;
    if (IsPlayerFinished(*Raceinfo::sInstance, losingPlayer)) return;

    MoveBalloon(balloonMgr, gainingPlayer, losingPlayer);

    if (GetBalloonCount(balloonMgr, losingPlayer) >= previousBalloonCount) return;

    RecordMushroomStealVictim(gainingPlayer, losingPlayer);
    QueueBalloonMoveFromLocalLoss(losingPlayer, gainingPlayer);

    if (IsLocalPlayer(gainingPlayer)) ClearActiveGoldenMushroom(gainingPlayer);
}

static void FinishPoweredHitAction(void* action, u32 sourcePlayerObjId) {
    if (sourcePlayerObjId >= maxPlayers) return;

    const u8 playerId = reinterpret_cast<Kart::Link*>(action)->GetPlayerIdx();
    RemovePoweredHitBalloon(playerId);
}

static void OnStarHitAction(void* action, u32 sourcePlayerObjId) {
    KartActionHitFn original = reinterpret_cast<KartActionHitFn>(kmRuntimeAddr(0x80568718));
    original(action, sourcePlayerObjId);
    EjectItemsFromItemDamage(reinterpret_cast<Kart::Link*>(action)->GetPlayerIdx());
    FinishPoweredHitAction(action, sourcePlayerObjId);
}

static void OnBulletHitAction(void* action, u32 sourcePlayerObjId) {
    KartActionHitFn original = reinterpret_cast<KartActionHitFn>(kmRuntimeAddr(0x80569024));
    original(action, sourcePlayerObjId);
    EjectItemsFromItemDamage(reinterpret_cast<Kart::Link*>(action)->GetPlayerIdx());
    FinishPoweredHitAction(action, sourcePlayerObjId);
}

static void OnMegaHitAction(void* action, u32 sourcePlayerObjId) {
    KartActionHitFn original = reinterpret_cast<KartActionHitFn>(kmRuntimeAddr(0x80569818));
    original(action, sourcePlayerObjId);
    EjectItemsFromItemDamage(reinterpret_cast<Kart::Link*>(action)->GetPlayerIdx());
    FinishPoweredHitAction(action, sourcePlayerObjId);
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

kmWrite32(0x808698c8, 0x60000000);  // allocate only the team-0 balloon pool
kmWrite32(0x808698c0, 0x1c040005);  // allocate five balloons per player
kmWrite32(0x8086994c, 0x60000000);  // defer balloon model load until a slot is used
kmWrite32(0x80869950, 0x60000000);
kmWrite32(0x80869954, 0x60000000);
kmWrite32(0x80869958, 0x60000000);

kmWrite32(0x8082a554, 0x38600400);  // ObjectDirector arrays: 200 -> 256 pointers
kmWrite32(0x8082a560, 0x38600400);
kmWrite32(0x8082a56c, 0x38600400);
kmWrite32(0x8082a578, 0x38600400);
kmWrite32(0x8082a584, 0x38600400);
kmWrite32(0x8082a590, 0x38600400);
kmWrite32(0x8082a59c, 0x38600c00);  // hit depth vec3 array: 200 -> 256 entries
kmWrite32(0x8082a5b8, 0x38e00100);
kmWrite32(0x8082a5c4, 0x38600400);  // collision scenario array: 200 -> 256 entries

static void* AllocateObjectHeap(void* scene, u32 size, u32 parentHeapIdx) {
    if (ShouldApplyBattleRoyale() && size == 0x80000 && parentHeapIdx == 1) {
        size = 0x100000;
    }

    AllocateHeapFn allocateHeap = reinterpret_cast<AllocateHeapFn>(kmRuntimeAddr(0x8051b8e4));
    return allocateHeap(scene, size, parentHeapIdx);
}

kmCall(0x80554570, AllocateObjectHeap);

static void CreateBattleRoyaleBalloonManager() {
    CreateBalloonManagerFn create = reinterpret_cast<CreateBalloonManagerFn>(kmRuntimeAddr(0x808697bc));

    RacedataSettings& settings = Racedata::sInstance->racesScenario.settings;
    const GameMode prevMode = settings.gamemode;
    const BattleType prevBattleType = settings.battleType;
    settings.gamemode = MODE_BATTLE;
    settings.battleType = BATTLE_BALLOON;
    ClearLoadedBalloonModels();
    create();
    LoadStartingBalloonModels(GetBalloonManager());
    settings.gamemode = prevMode;
    settings.battleType = prevBattleType;
}

static void CreateBalloonManager() {
    if (!ShouldApplyBattleRoyale()) {
        CreateBalloonManagerFn create = reinterpret_cast<CreateBalloonManagerFn>(kmRuntimeAddr(0x808697bc));
        create();
        return;
    }

    sDeferredBalloonManagerCreation = true;
}

kmCall(0x8082a7c4, CreateBalloonManager);

static void* sDeferredDestroyScene = nullptr;
static void* sDeferredDestroyHeap = nullptr;

static void DestroyHeapAfterDeferredBalloons(void* scene, void* heap) {
    if (sDeferredBalloonManagerCreation && ShouldApplyBattleRoyale()) {
        sDeferredDestroyScene = scene;
        sDeferredDestroyHeap = heap;
        return;
    }

    DestroyHeapFn destroyHeap = reinterpret_cast<DestroyHeapFn>(kmRuntimeAddr(0x8051b988));
    destroyHeap(scene, heap);
}

kmCall(0x8082a7f4, DestroyHeapAfterDeferredBalloons);

static void CreateObjectsThenCreateDeferredBalloons(void* objectDirector, bool isInitial) {
    CreateObjectsFn createObjects = reinterpret_cast<CreateObjectsFn>(kmRuntimeAddr(0x80826e8c));
    createObjects(objectDirector, isInitial);

    if (sDeferredBalloonManagerCreation && ShouldApplyBattleRoyale()) {
        CreateBattleRoyaleBalloonManager();
        sDeferredBalloonManagerCreation = false;
    }

    if (sDeferredDestroyHeap != nullptr) {
        DestroyHeapFn destroyHeap = reinterpret_cast<DestroyHeapFn>(kmRuntimeAddr(0x8051b988));
        destroyHeap(sDeferredDestroyScene, sDeferredDestroyHeap);
        sDeferredDestroyScene = nullptr;
        sDeferredDestroyHeap = nullptr;
    }
}

kmCall(0x8082a800, CreateObjectsThenCreateDeferredBalloons);

static void ResetState() {
    sInitialized = false;
    sLastRaceFrames = 0xffff;
    sDeferredBalloonManagerCreation = false;
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
    ClearLoadedBalloonModels();
    sEliminationCount = 0;
    sPendingBalloonEventReadIdx = 0;
    sPendingBalloonEventWriteIdx = 0;
    sPendingBalloonEventSize = 0;
    for (u8 i = 0; i < pendingBalloonEventCount; ++i) {
        sPendingBalloonEventSeq[i] = 0;
        sPendingBalloonEventPlayerId[i] = 0xff;
        sPendingBalloonEventTimer[i] = 0;
    }
}

static void InitForRace(LapKO::Mgr& lapKoMgr, void* balloonMgr) {
    lapKoMgr.InitForRace();
    sEliminationCount = 0;

    for (u8 playerId = 0; playerId < maxPlayers; ++playerId) {
        sPoweredHitLossFrame[playerId] = 0xffff;
        sLastRemoteBalloonLossSeq[playerId] = 0;
    }

    const u8 playerCount = System::sInstance->nonTTGhostPlayersCount;
    const u8 targetCount = GetStartingBalloonAddCount();
    for (u8 playerId = 0; playerId < playerCount && playerId < maxPlayers; ++playerId) {
        const u8 current = GetBalloonCount(balloonMgr, playerId);
        if (current < targetCount) {
            AddBalloons(balloonMgr, playerId, static_cast<u8>(targetCount - current));
        }
        sPreviousBalloonCount[playerId] = GetBalloonCount(balloonMgr, playerId);
    }

    sInitialized = true;
}

static void EndRaceWithEliminationFinishTime(u8 playerId, u8 placement) {
    Raceinfo* raceinfo = Raceinfo::sInstance;
    if (raceinfo == nullptr || playerId >= maxPlayers || placement < 2 || placement > maxPlayers) return;

    RaceinfoPlayer* player = raceinfo->players[playerId];
    if (player == nullptr || IsPlayerFinished(*raceinfo, playerId)) return;

    Timer finishTime(false);
    finishTime.minutes = 99;
    finishTime.seconds = 99;
    finishTime.milliseconds = static_cast<u16>(900 + placement);
    finishTime.SetActive(true);
    player->EndRace(finishTime, false, 0);
    if (IsOnline()) raceinfo->CheckEndRaceOnline(playerId);
}

static void EndRaceForElimination(u8 playerId) {
    if (playerId >= maxPlayers || sEliminationCount >= maxPlayers) return;
    ++sEliminationCount;

    const System* system = System::sInstance;
    u8 playerCount = system == nullptr ? maxPlayers : system->nonTTGhostPlayersCount;
    if (playerCount > maxPlayers) playerCount = maxPlayers;

    const u8 placement = static_cast<u8>(playerCount - sEliminationCount + 1);
    EndRaceWithEliminationFinishTime(playerId, placement);
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
}

static void ProcessBalloonEliminations(LapKO::Mgr& lapKoMgr, void* balloonMgr) {
    const u8 playerCount = System::sInstance->nonTTGhostPlayersCount;
    for (u8 playerId = 0; playerId < playerCount && playerId < maxPlayers; ++playerId) {
        const u8 current = GetBalloonCount(balloonMgr, playerId);
        if (lapKoMgr.IsActive(playerId) && sPreviousBalloonCount[playerId] != 0 && current == 0) {
            Raceinfo* raceinfo = Raceinfo::sInstance;
            if (raceinfo != nullptr) {
                if (IsPlayerOnlineRaceComplete(*raceinfo, playerId)) {
                    sPreviousBalloonCount[playerId] = current;
                    continue;
                }

                EndRaceForElimination(playerId);
            }
            lapKoMgr.ProcessElimination(playerId, LapKO::Mgr::ELIMINATION_CAUSE_ROUND, IsOnline(), true);
        }
        sPreviousBalloonCount[playerId] = current;
    }
}

static u8 GetSoleActiveUnfinishedPlayer(const LapKO::Mgr& lapKoMgr, const Raceinfo& raceinfo) {
    const u8 playerCount = System::sInstance->nonTTGhostPlayersCount;
    u8 remainingPlayerId = 0xff;
    u8 remainingCount = 0;

    for (u8 playerId = 0; playerId < playerCount && playerId < maxPlayers; ++playerId) {
        if (!lapKoMgr.IsActive(playerId) || IsPlayerFinished(raceinfo, playerId)) continue;
        remainingPlayerId = playerId;
        ++remainingCount;
    }

    return remainingCount == 1 ? remainingPlayerId : 0xff;
}

static void FinishSoleActiveUnfinishedPlayer(LapKO::Mgr& lapKoMgr, Raceinfo& raceinfo) {
    const u8 playerId = GetSoleActiveUnfinishedPlayer(lapKoMgr, raceinfo);
    if (playerId >= maxPlayers) return;

    RaceinfoPlayer* player = raceinfo.players[playerId];
    if (player == nullptr) return;

    Timer now(false);
    raceinfo.CloneTimer(&now);
    now.SetActive(true);
    player->EndRace(now, false, 0);
    raceinfo.EndPlayerRace(playerId);
    if (IsOnline()) raceinfo.CheckEndRaceOnline(playerId);
    lapKoMgr.raceFinished = true;
}

static void ApplyRemoteFinishTimes(RKNet::Controller& controller, u8 aid, const Network::PulRH1& packet) {
    Raceinfo* raceinfo = Raceinfo::sInstance;
    if (raceinfo == nullptr || packet.battleRoyaleFinishMask == 0) return;

    u8 remotePlayerIdx = 0;
    const u8 playerCount = System::sInstance->nonTTGhostPlayersCount;
    for (u8 playerId = 0; playerId < playerCount && playerId < maxPlayers && remotePlayerIdx < 2; ++playerId) {
        if (controller.aidsBelongingToPlayerIds[playerId] != aid) continue;

        if ((packet.battleRoyaleFinishMask & (1 << remotePlayerIdx)) != 0) {
            RaceinfoPlayer* player = raceinfo->players[playerId];
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
        ApplyRemoteFinishTimes(controller, aid, *packet);

        const u8 seq = packet->battleRoyaleLossSeq;
        const u8 eventPlayerId = packet->battleRoyaleLossPlayerId;
        if (seq != 0 && seq != sLastRemoteBalloonLossSeq[aid]) {
            sLastRemoteBalloonLossSeq[aid] = seq;
            if (eventPlayerId >= balloonMoveEventBase) {
                const u8 move = eventPlayerId - balloonMoveEventBase;
                const u8 losingPlayerId = move / maxPlayers;
                const u8 gainingPlayerId = move % maxPlayers;
                if (losingPlayerId < maxPlayers && gainingPlayerId < maxPlayers &&
                    controller.aidsBelongingToPlayerIds[losingPlayerId] == aid &&
                    !HasMushroomStolenFromVictim(gainingPlayerId, losingPlayerId)) {
                    const u8 previousBalloonCount = GetBalloonCount(balloonMgr, losingPlayerId);
                    if (previousBalloonCount > 1) {
                        MoveBalloon(balloonMgr, gainingPlayerId, losingPlayerId);
                        if (GetBalloonCount(balloonMgr, losingPlayerId) < previousBalloonCount) {
                            RecordMushroomStealVictim(gainingPlayerId, losingPlayerId);
                            if (IsLocalPlayer(gainingPlayerId)) ClearActiveGoldenMushroom(gainingPlayerId);
                        }
                    }
                }
            } else if (eventPlayerId < maxPlayers && controller.aidsBelongingToPlayerIds[eventPlayerId] == aid) {
                RemoveBalloon(balloonMgr, eventPlayerId);
            }
        }

        const u8 packedCounts = packet->battleRoyaleBalloonCounts;
        if (packedCounts == 0xFF) continue;

        u8 remotePlayerIdx = 0;
        const u8 playerCount = System::sInstance->nonTTGhostPlayersCount;
        for (u8 playerId = 0; playerId < playerCount && playerId < maxPlayers && remotePlayerIdx < 2; ++playerId) {
            if (controller.aidsBelongingToPlayerIds[playerId] != aid) continue;

            const u8 target = (remotePlayerIdx == 0) ? static_cast<u8>(packedCounts & 0x0F) : static_cast<u8>((packedCounts >> 4) & 0x0F);
            if (target <= 5) {
                u8 current = GetBalloonCount(balloonMgr, playerId);
                while (current < target) {
                    AddBalloons(balloonMgr, playerId, 1);
                    const u8 next = GetBalloonCount(balloonMgr, playerId);
                    if (next <= current) break;
                    current = next;
                }
                while (current > target) {
                    if (!RemoveBalloon(balloonMgr, playerId)) break;
                    const u8 next = GetBalloonCount(balloonMgr, playerId);
                    if (next >= current) break;
                    current = next;
                }
            }
            ++remotePlayerIdx;
        }
    }
}

static void TickLocalBalloonEvents() {
    if (sPendingBalloonEventSize == 0) return;

    u8& timer = sPendingBalloonEventTimer[sPendingBalloonEventReadIdx];
    if (timer > 0) {
        --timer;
        return;
    }

    sPendingBalloonEventSeq[sPendingBalloonEventReadIdx] = 0;
    sPendingBalloonEventPlayerId[sPendingBalloonEventReadIdx] = 0xff;
    sPendingBalloonEventReadIdx = static_cast<u8>((sPendingBalloonEventReadIdx + 1) % pendingBalloonEventCount);
    --sPendingBalloonEventSize;
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

    TickLocalBalloonEvents();
    UpdateMushroomStealVictimMasks();

    if (!lapKoMgr->raceFinished && raceinfo->IsAtLeastStage(RACESTAGE_RACE)) {
        ProcessBalloonEliminations(*lapKoMgr, balloonMgr);
        FinishSoleActiveUnfinishedPlayer(*lapKoMgr, *raceinfo);
    }
}

static RaceFrameHook battleRoyaleFrameHook(FrameUpdate);

}  // namespace BattleRoyale
}  // namespace Pulsar
