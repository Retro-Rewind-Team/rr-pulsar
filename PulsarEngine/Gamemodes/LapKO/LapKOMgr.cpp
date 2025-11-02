#include <Gamemodes/LapKO/LapKOMgr.hpp>
#include <MarioKartWii/Item/ItemManager.hpp>
#include <MarioKartWii/Item/ItemSlot.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <Network/PacketExpansion.hpp>
#include <MarioKartWii/KMP/KMPManager.hpp>
#include <MarioKartWii/3D/Camera/CameraMgr.hpp>
#include <MarioKartWii/3D/Camera/RaceCamera.hpp>
#include <MarioKartWii/Driver/DriverManager.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>
#include <Settings/Settings.hpp>
#include <Settings/SettingsParam.hpp>
#include <core/rvl/PAD.hpp>
#include <core/rvl/WPAD.hpp>
#include <runtimeWrite.hpp>

namespace Pulsar {
namespace LapKO {

static const u16 pendingBroadcastFrames = 120;
static const u16 eliminationDisplayDuration = 180;
static const u8 lapKoNoRoundAdvanceFlag = 0x80;

Mgr::Mgr()
    : koPerRaceSetting(1),
    orderCursor(0),
    activeCount(0),
    playerCount(0),
    roundIndex(1),
    roundDisconnectDebits(0),
    totalRounds(0),
    eventSequence(0),
    appliedSequence(0),
    pendingSequence(0),
    pendingElimination(0xFF),
    pendingRound(0),
    pendingActiveCount(0),
    hasPendingEvent(false),
    pendingNoRoundAdvance(false),
    pendingBatchCount(0),
    isSpectating(false),
    spectateTargetPlayer(0xFF),
    spectateManualTarget(false),
    isHost(true),
    hostAid(0xFF),
    pendingTimer(0),
    raceFinished(false),
    raceInitDone(false),
    recentEliminationCount(0),
    recentEliminationRound(0),
    eliminationDisplayTimer(0),
    pendingItemReweightFrames(0),
    disconnectGraceFrames(0) {
    for (int i = 0; i < 12; ++i) {
        this->active[i] = false;
        this->crossed[i] = false;
        this->crossOrder[i] = 0xFF;
        this->lastLapValue[i] = 0;
    }
    this->lastAvailableAids = 0;
    this->lastRaceFrames = 0xFFFF;
    this->ResetEliminationDisplay();
}

Mgr::~Mgr() {}

void Mgr::SetKoPerRace(u8 value) {
    if (value == 0) value = 1;
    this->koPerRaceSetting = value;
}

u8 Mgr::GetKoPerRace() const {
    return (this->koPerRaceSetting == 0) ? 1 : this->koPerRaceSetting;
}

u8 Mgr::GetCurrentRoundEliminationCount() const {
    if (this->activeCount <= 1) return 0;
    const u8 usualLapCount = this->GetUsualTrackLapCount();
    return this->GetRemainingEliminationsForCurrentRound(usualLapCount);
}

u8 Mgr::BuildPlan(u8 playerCount, u8 koPerRace, u8 usualLapCount, u8* outPlan, u8 capacity) {
    if (outPlan != nullptr) {
        for (u8 i = 0; i < capacity; ++i) outPlan[i] = 0;
    }

    if (capacity == 0) return 0;
    if (playerCount < 2) return 0;

    if (usualLapCount <= 1) {
        if (outPlan != nullptr && capacity > 0) {
            outPlan[0] = (playerCount > 1) ? static_cast<u8>(playerCount - 1) : 0;
        }
        return 1;
    }

    if (koPerRace == 0) koPerRace = 1;

    const bool twoLapTrack = (usualLapCount == 2);
    if (twoLapTrack && playerCount >= 3) {
        u16 doubled = static_cast<u16>(koPerRace) * 2;
        if (doubled > 255) doubled = 255;
        koPerRace = static_cast<u8>(doubled);
    }

    u8 remainingPlayers = playerCount;
    u8 round = 0;
    while (remainingPlayers > 1 && round < capacity) {
        u8 planned = koPerRace;
        if (planned >= remainingPlayers) planned = static_cast<u8>(remainingPlayers - 1);
        if (planned == 0) planned = 1;
        if (outPlan != nullptr) outPlan[round] = planned;
        remainingPlayers = static_cast<u8>(remainingPlayers - planned);
        ++round;
    }
    return round;
}

void Mgr::InitForRace() {
    this->raceInitDone = true;
    OS::Report("LapKO: InitForRace start\n");
    const System* system = System::sInstance;
    const RKNet::Controller* controller = RKNet::Controller::sInstance;

    this->playerCount = system->nonTTGhostPlayersCount;
    if (this->playerCount == 0) {
        const Racedata* racedata = Racedata::sInstance;
        if (racedata != nullptr) this->playerCount = racedata->racesScenario.playerCount;
    }
    if (this->playerCount == 0) this->playerCount = 12;

    Raceinfo* raceinfo = Raceinfo::sInstance;

    for (int i = 0; i < 12; ++i) {
        this->active[i] = (i < this->playerCount);
        this->crossed[i] = false;
        this->crossOrder[i] = 0xFF;
        this->lastLapValue[i] = 0;
    }

    this->activeCount = this->playerCount;
    this->orderCursor = 0;
    this->roundIndex = 1;
    this->roundDisconnectDebits = 0;
    this->totalRounds = 0;
    this->eventSequence = 0;
    this->appliedSequence = 0;
    this->pendingSequence = 0;
    this->pendingElimination = 0xFF;
    this->pendingRound = 0;
    this->pendingActiveCount = 0;
    this->pendingTimer = 0;
    this->hasPendingEvent = false;
    this->pendingNoRoundAdvance = false;
    this->pendingBatchCount = 0;
    this->isSpectating = false;
    this->spectateTargetPlayer = 0xFF;
    this->spectateManualTarget = false;
    this->raceFinished = false;
    this->raceInitDone = true;
    this->ResetEliminationDisplay();
    // Start with a short grace window before we interpret missing AIDs as disconnects.
    // This mitigates first-round false eliminations of the host due to transient AID availability.
    this->disconnectGraceFrames = 180;  // ~3 seconds at 60fps

    if (controller != nullptr && controller->roomType != RKNet::ROOMTYPE_NONE) {
        const RKNet::ControllerSub& sub = controller->subs[controller->currentSub];
        this->hostAid = sub.hostAid;
        this->isHost = (sub.localAid == sub.hostAid);
        this->lastAvailableAids = sub.availableAids;
    } else {
        this->hostAid = 0xFF;
        this->isHost = true;
        this->lastAvailableAids = 0;

        const Settings::Mgr& settings = Settings::Mgr::Get();
        u8 offlineKo = static_cast<u8>(settings.GetUserSettingValue(Settings::SETTINGSTYPE_KO, SCROLLER_KOPERRACE) + 1);
        this->SetKoPerRace(offlineKo);
    }

    Racedata* raceDataMutable = Racedata::sInstance;
    if (raceDataMutable != nullptr) {
        raceDataMutable->menusScenario.settings.lapCount = raceDataMutable->racesScenario.settings.lapCount;
    }

    // Safe: initialize the Item::Manager playerCount at race start only.
    // Do not mutate it mid-race as it sizes internal arrays.
    if (Item::Manager::sInstance != nullptr) {
        Item::Manager::sInstance->playerCount = this->playerCount;
    }

    this->UpdateActivePlayerCounts();

    // Build elimination plan using the configured KO-per-race setting
    this->ComputeEliminationPlan();
    for (u8 i = 0; i < this->totalRounds; ++i) OS::Report("%u", this->eliminationPlan[i]);
    OS::Report("\nLapKO: InitForRace done players=%u host=%u\n", this->playerCount, this->isHost);
}

void Mgr::ResetRound() {
    this->orderCursor = 0;
    this->roundDisconnectDebits = 0;
    for (int i = 0; i < 12; ++i) {
        this->crossed[i] = false;
        this->crossOrder[i] = 0xFF;
    }
}

void Mgr::OnLapComplete(u8 playerId, RaceinfoPlayer& player) {
    if (playerId >= 12) return;
    if (!this->active[playerId]) return;
    if (this->crossed[playerId]) return;
    if (player.currentLap <= this->roundIndex) return;
    this->crossed[playerId] = true;
    if (this->orderCursor < 12) {
        this->crossOrder[this->orderCursor] = playerId;
        ++this->orderCursor;
    }
    // In friend rooms, only the host computes eliminations; non-hosts wait for network
    if (!this->IsFriendRoomOnline() || this->isHost) {
        this->TryResolveRound();
    }
}

void Mgr::OnPlayerFinished(u8 playerId) {
    if (playerId >= 12) return;
    if (!this->active[playerId]) return;
    // Treat race finish as lap completion as well
    Raceinfo* raceinfo = Raceinfo::sInstance;
    if (raceinfo != nullptr) {
        RaceinfoPlayer* infoPlayer = raceinfo->players[playerId];
        if (infoPlayer != nullptr) {
            this->OnLapComplete(playerId, *infoPlayer);
        }
    }
}

void Mgr::OnPlayerDisconnected(u8 playerId) {
    if (playerId >= 12) return;
    if (!this->active[playerId]) return;
    OS::Report("LapKO: Disconnect player=%u\n", playerId);
    // In friend rooms, only the host determines eliminations on disconnects
    if (!this->IsFriendRoomOnline() || this->isHost) {
        this->ProcessElimination(playerId, ELIMINATION_CAUSE_DISCONNECT, false, true);
    }
}

void Mgr::TryResolveRound() {
    if (this->raceFinished) return;
    if (this->activeCount <= 1) return;

    // Special case for 1-lap tracks: when first place finishes, eliminate everyone else
    const u8 usualLaps = this->GetUsualTrackLapCount();

    u8 toEliminate = this->GetRemainingEliminationsForCurrentRound(usualLaps);
    if (toEliminate == 0) return;  // no eliminations remaining this round

    u8 requiredCrossings;
    if (usualLaps <= 1) {
        requiredCrossings = 1;
        if (toEliminate >= this->activeCount) toEliminate = static_cast<u8>(this->activeCount - 1);
    } else {
        requiredCrossings = static_cast<u8>(this->activeCount - toEliminate);
    }
    if (this->orderCursor < requiredCrossings) return;

    u8 eliminatedList[12];
    const u8 elimCount = this->SelectEliminationCandidates(toEliminate, eliminatedList);
    if (elimCount == 0) return;

    const u8 concludedRound = this->roundIndex;
    for (u8 i = 0; i < elimCount; ++i) {
        const bool lastOne = (i == elimCount - 1);
        this->ProcessEliminationInternal(eliminatedList[i], ELIMINATION_CAUSE_ROUND, false, !lastOne);
    }
    if (this->IsFriendRoomOnline() && this->isHost) {
        this->BroadcastBatch(eliminatedList, elimCount, concludedRound);
    }
}

void Mgr::ProcessElimination(u8 playerId, EliminationCause cause, bool fromNetwork, bool suppressRoundAdvance) {
    this->ProcessEliminationInternal(playerId, cause, fromNetwork, suppressRoundAdvance);
}

u8 Mgr::GetBaseEliminationCountForCurrentRound(u8 usualLapCount) const {
    if (this->activeCount <= 1) return 0;

    if (usualLapCount <= 1) {
        return static_cast<u8>(this->activeCount - 1);
    }

    const u8 idx = (this->roundIndex == 0) ? 0 : static_cast<u8>(this->roundIndex - 1);
    if (idx >= this->totalRounds) return 0;

    u8 planned = (idx < MaxRounds) ? this->eliminationPlan[idx] : 0;
    if (planned == 0) return 0;
    if (planned >= this->activeCount) planned = static_cast<u8>(this->activeCount - 1);
    return planned;
}

u8 Mgr::GetRemainingEliminationsForCurrentRound(u8 usualLapCount) const {
    u8 base = this->GetBaseEliminationCountForCurrentRound(usualLapCount);
    if (base == 0) return 0;

    if (usualLapCount <= 1) {
        return base;
    }

    if (this->roundDisconnectDebits >= base) return 0;

    u8 remaining = static_cast<u8>(base - this->roundDisconnectDebits);
    if (remaining >= this->activeCount) {
        remaining = (this->activeCount > 0) ? static_cast<u8>(this->activeCount - 1) : 0;
    }
    return remaining;
}

void Mgr::ProcessEliminationInternal(u8 playerId, EliminationCause cause, bool fromNetwork, bool suppressRoundAdvance) {
    if (playerId >= 12) return;
    if (!this->active[playerId]) return;
    if (this->raceFinished) return;

    const u8 concludedRound = this->roundIndex;
    const u8 usualLapCount = this->GetUsualTrackLapCount();
    const bool supportsDisconnectAdjustments = (usualLapCount > 1);

    this->active[playerId] = false;

    if (this->activeCount > 0) --this->activeCount;
    this->UpdateActivePlayerCounts();

    if (cause == ELIMINATION_CAUSE_DISCONNECT && supportsDisconnectAdjustments) {
        if (this->roundDisconnectDebits < 12) ++this->roundDisconnectDebits;
    }

    if (cause == ELIMINATION_CAUSE_DISCONNECT) {
        if (supportsDisconnectAdjustments) {
            const u8 remaining = this->GetRemainingEliminationsForCurrentRound(usualLapCount);
            suppressRoundAdvance = (remaining > 0);
        } else {
            suppressRoundAdvance = (this->activeCount > 1);
        }
    }

    if (this->isHost && !fromNetwork) {
        this->pendingNoRoundAdvance = suppressRoundAdvance;
        if (this->IsFriendRoomOnline()) {
            u8 single[1] = {playerId};
            this->BroadcastBatch(single, 1, concludedRound);
        } else {
            this->BroadcastEvent(playerId, concludedRound);
        }
    }

    this->RecordEliminationForDisplay(playerId, concludedRound);

    Raceinfo* raceinfo = Raceinfo::sInstance;
    if (raceinfo != nullptr) {
        RaceinfoPlayer* infoPlayer = raceinfo->players[playerId];
        if (infoPlayer != nullptr) {
            infoPlayer->Vanish();
        }
    }

    // If the eliminated player is local, either enter spectate (online) or end the race offline.
    if (this->EnterSpectateIfLocal(playerId)) {
        return;  // Offline eliminations end the race immediately.
    }

    if (!suppressRoundAdvance) {
        this->ResetRound();
    }

    if (this->activeCount <= 1) {
        u8 winnerId = 0xFF;
        for (u8 i = 0; i < 12; ++i) {
            if (this->active[i]) {
                winnerId = i;
                break;
            }
        }
        OS::Report("LapKO: Winner=%u\n", winnerId);
        this->ConcludeRace(winnerId);
        return;
    }

    if (!suppressRoundAdvance) {
        ++this->roundIndex;
        OS::Report("LapKO: NextRound=%u active=%u\n", this->roundIndex, this->activeCount);
    }
}

void Mgr::ConcludeRace(u8 winnerId) {
    if (this->raceFinished) return;
    this->raceFinished = true;

    Raceinfo* raceinfo = Raceinfo::sInstance;
    if (raceinfo == nullptr) return;

    // If we're offline, immediately finish everyone at their current standings and return.
    if (RKNet::Controller::sInstance == nullptr || RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_NONE) {
        this->FinishOfflineAtCurrentStandings();
        return;
    }

    u8 finishId = winnerId;
    if (finishId >= 12) {
        finishId = 0xFF;
        if (raceinfo->players != nullptr) {
            for (u8 i = 0; i < 12; ++i) {
                if (raceinfo->players[i] != nullptr) {
                    finishId = i;
                    break;
                }
            }
        }
    }

    if (finishId < 12) {
        raceinfo->EndPlayerRace(finishId);
        raceinfo->CheckEndRaceOnline(finishId);
    }
}

void Mgr::FinishOfflineAtCurrentStandings() {
    Raceinfo* raceinfo = Raceinfo::sInstance;
    if (raceinfo == nullptr) return;
    if (raceinfo->players == nullptr || raceinfo->playerIdInEachPosition == nullptr) return;

    // Derive a reference time from Raceinfo
    Timer now(false);
    raceinfo->CloneTimer(&now);
    now.SetActive(true);

    // Walk current placements; cap to 12 and playerCount
    u8 total = 12;
    const Racedata* racedata = Racedata::sInstance;
    if (racedata != nullptr) total = racedata->racesScenario.playerCount;
    if (total > 12) total = 12;

    if (raceinfo->playerIdInEachPosition != nullptr) {
        for (u8 pos = 0; pos < total && pos < 12; ++pos) {
            const u8 pid = raceinfo->playerIdInEachPosition[pos];
            if (pid >= 12) continue;
            RaceinfoPlayer* p = raceinfo->players[pid];
            if (p == nullptr) continue;

            // Determine the finish time to commit: keep existing active finish time if any; else use 'now'.
            const Timer* commitTime = &now;
            if (p->raceFinishTime != nullptr && p->raceFinishTime->isActive) {
                commitTime = p->raceFinishTime;
            }

            // Mark player as finished and commit placement.
            p->EndRace(*commitTime, false, 0);
            raceinfo->EndPlayerRace(pid);
        }
    } else {
        // Fallback: iterate player IDs directly if standings are unavailable.
        for (u8 pid = 0; pid < total && pid < 12; ++pid) {
            RaceinfoPlayer* p = raceinfo->players[pid];
            if (p == nullptr) continue;
            const Timer* commitTime = &now;
            if (p->raceFinishTime != nullptr && p->raceFinishTime->isActive) commitTime = p->raceFinishTime;
            p->EndRace(*commitTime, false, 0);
            raceinfo->EndPlayerRace(pid);
        }
    }
}

void Mgr::BroadcastEvent(u8 playerId, u8 concludedRound) {
    this->pendingSequence = this->AdvanceSequence();
    const u8 encodedId = static_cast<u8>((playerId & 0x7F) | (this->pendingNoRoundAdvance ? lapKoNoRoundAdvanceFlag : 0));
    this->pendingElimination = encodedId;
    this->pendingBatchCount = 0;
    this->PreparePendingEvent(concludedRound, this->activeCount);
    OS::Report("LapKO: Broadcast seq=%u player=%u round=%u active=%u\n", this->pendingSequence, playerId, concludedRound, this->pendingActiveCount);
}

void Mgr::BroadcastBatch(const u8* elimIds, u8 elimCount, u8 concludedRound) {
    if (elimIds == nullptr || elimCount == 0) return;
    if (elimCount > 12) elimCount = 12;
    this->pendingSequence = this->AdvanceSequence();
    this->pendingBatchCount = elimCount;
    for (u8 i = 0; i < elimCount; ++i) this->pendingBatch[i] = elimIds[i];
    this->pendingElimination = 0xFF;
    this->PreparePendingEvent(concludedRound, this->activeCount);
    OS::Report("LapKO: BroadcastBatch seq=%u count=%u round=%u active=%u\n", this->pendingSequence, elimCount, concludedRound, this->pendingActiveCount);
}

void Mgr::UpdateActivePlayerCounts() {
    if (this->pendingItemReweightFrames < 120) this->pendingItemReweightFrames = 120;
}

void Mgr::ClearPendingEvent() {
    this->pendingSequence = 0;
    this->pendingElimination = 0xFF;
    this->pendingRound = 0;
    this->pendingActiveCount = 0;
    this->pendingTimer = 0;
    this->hasPendingEvent = false;
    this->pendingNoRoundAdvance = false;
    this->pendingBatchCount = 0;
    for (u8 i = 0; i < 12; ++i) this->pendingBatch[i] = 0xFF;
}

void Mgr::ApplyRemoteEvent(u8 seq, u8 eliminatedId, u8 roundIdx, u8 activeCnt) {
    if (seq == 0) return;
    const bool noRoundAdvance = (eliminatedId & lapKoNoRoundAdvanceFlag) != 0;
    const u8 playerId = static_cast<u8>(eliminatedId & 0x7F);
    if (playerId >= 12) return;
    if (seq == this->appliedSequence) return;

    this->appliedSequence = seq;
    this->roundIndex = roundIdx;
    const EliminationCause cause = noRoundAdvance ? ELIMINATION_CAUSE_DISCONNECT : ELIMINATION_CAUSE_ROUND;
    this->ProcessElimination(playerId, cause, true, noRoundAdvance);
    this->activeCount = activeCnt;
    this->UpdateActivePlayerCounts();
}

void Mgr::ApplyRemoteBatch(u8 seq, u8 roundIdx, u8 activeCnt, const u8* elimIds, u8 elimCount, bool noRoundAdvance) {
    if (seq == 0) return;
    if (seq == this->appliedSequence) return;
    if (elimIds == nullptr || elimCount == 0) return;
    if (elimCount > 12) elimCount = 12;
    this->appliedSequence = seq;
    this->roundIndex = roundIdx;
    const EliminationCause cause = noRoundAdvance ? ELIMINATION_CAUSE_DISCONNECT : ELIMINATION_CAUSE_ROUND;
    for (u8 i = 0; i < elimCount; ++i) {
        const bool lastOne = (i == elimCount - 1);
        const u8 elimId = static_cast<u8>(elimIds[i] & 0x7F);
        if (elimId >= 12) continue;
        const bool suppress = noRoundAdvance ? true : !lastOne;
        this->ProcessEliminationInternal(elimId, cause, true, suppress);
    }
    this->activeCount = activeCnt;
}

void Mgr::UpdateFrame() {
    this->TickEliminationDisplay();

    RKNet::Controller* controller = RKNet::Controller::sInstance;
    Raceinfo* raceinfo = Raceinfo::sInstance;
    if (controller == nullptr || raceinfo == nullptr) return;

    this->EnsureRaceInitialized(*raceinfo);

    if (this->raceFinished && !this->hasPendingEvent) return;

    const RKNet::ControllerSub& sub = controller->subs[controller->currentSub];

    if (this->isHost && controller->roomType != RKNet::ROOMTYPE_NONE) {
        this->HostMonitorDisconnects(*controller, sub);
    }

    this->UpdateLapProgress(*raceinfo);

    if (this->isSpectating) {
        this->UpdateSpectatorInputs(*raceinfo);
        this->MaintainSpectatorView(*raceinfo);
    }

    this->ProcessPendingItemReweight();

    if (this->isHost) {
        this->HostDistributeEvents(*controller, sub);
    } else {
        this->ClientConsumeHostEvents(*controller, sub);
    }
}

kmRuntimeUse(0x809c3670);
void Mgr::ReweightItemProbabilitiesNow() {
    if (!this->raceInitDone || this->raceFinished) return;
    Raceinfo* ri = Raceinfo::sInstance;
    if (ri == nullptr) return;
    if (ri->raceFrames < 90) return;
    if (Item::Manager::sInstance == nullptr) return;

    // In retail, Item::ItemSlotData::sInstance is stored at 0x809c3670 as a pointer to the instance.
    // Read the pointer value from that address.
    Item::ItemSlotData* slot = *reinterpret_cast<Item::ItemSlotData**>(kmRuntimeAddr(0x809c3670));
    if (slot == nullptr) return;

    u8 activePlayers = this->activeCount;
    if (activePlayers == 0) activePlayers = 1;

    // If nothing changed, don’t touch memory
    if (slot->playerCount == activePlayers) return;

    // Tell the setup how many effective players we have now.
    // DO NOT touch Item::Manager::playerCount mid-race.
    slot->playerCount = activePlayers;

    // Rebuild VS probabilities the same way the game does, to ensure all caches
    // and roulette-related structures are consistent.
    const bool isOnline = (RKNet::Controller::sInstance != nullptr && RKNet::Controller::sInstance->roomType != RKNet::ROOMTYPE_NONE);
    if (isOnline) {
        slot->SetupOnlineVSProbabilities();
    } else {
        slot->SetupVSProbabilities();
    }
}

// Enter spectating if the eliminated player belongs to the local AID online.
// Offline eliminations instead end the race immediately so the player skips spectating.
bool Mgr::EnterSpectateIfLocal(u8 eliminatedId) {
    if (this->raceFinished) return true;

    const Racedata* racedata = Racedata::sInstance;
    const bool isOffline = (RKNet::Controller::sInstance == nullptr || RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_NONE);
    if (isOffline && racedata != nullptr && eliminatedId < racedata->racesScenario.playerCount) {
        const RacedataPlayer& eliminatedPlayer = racedata->racesScenario.players[eliminatedId];
        if (eliminatedPlayer.playerType == PLAYER_REAL_LOCAL) {
            // Offline: instead of spectating, end the entire race now and lock in placements/times.
            this->FinishOfflineAtCurrentStandings();
            this->raceFinished = true;
            return true;
        }
    } else {
        RKNet::Controller* controller = RKNet::Controller::sInstance;
        if (controller == nullptr) return false;
        const RKNet::ControllerSub& sub = controller->subs[controller->currentSub];
        const u8 aid = controller->aidsBelongingToPlayerIds[eliminatedId];
        if (aid >= 12 || aid != sub.localAid) return false;  // not a local player

        this->isSpectating = true;
        this->spectateManualTarget = false;
        this->spectateTargetPlayer = 0xFF;

        Raceinfo* raceinfo = Raceinfo::sInstance;
        if (raceinfo != nullptr) {
            this->InitializeSpectateView(*raceinfo);
        }
    }
    return false;
}

void Mgr::ComputeEliminationPlan() {
    const u8 usualLaps = this->GetUsualTrackLapCount();
    const u8 koPerRace = this->GetKoPerRace();
    this->totalRounds = BuildPlan(this->playerCount, koPerRace, usualLaps, this->eliminationPlan, MaxRounds);
}

u8 Mgr::GetUsualTrackLapCount() const {
    // Default to 3 if KMP not available
    u8 usual = 3;
    if (KMP::Manager::sInstance != nullptr &&
        KMP::Manager::sInstance->stgiSection != nullptr &&
        KMP::Manager::sInstance->stgiSection->holdersArray[0] != nullptr &&
        KMP::Manager::sInstance->stgiSection->holdersArray[0]->raw != nullptr) {
        usual = KMP::Manager::sInstance->stgiSection->holdersArray[0]->raw->lapCount;
        if (usual == 0) usual = 3;  // safety
    }
    return usual;
}

void Mgr::RecordEliminationForDisplay(u8 playerId, u8 concludedRound) {
    if (playerId >= 12) return;
    if (this->eliminationDisplayTimer == 0 || this->recentEliminationRound != concludedRound) {
        this->ResetEliminationDisplay();
        this->recentEliminationRound = concludedRound;
    }

    if (this->recentEliminationCount < 4) {
        this->recentEliminations[this->recentEliminationCount++] = playerId;
    }

    this->eliminationDisplayTimer = eliminationDisplayDuration;
}

void Mgr::ResetEliminationDisplay() {
    this->recentEliminationCount = 0;
    this->recentEliminationRound = 0;
    this->recentEliminations[0] = 0xFF;
    this->recentEliminations[1] = 0xFF;
    this->recentEliminations[2] = 0xFF;
    this->recentEliminations[3] = 0xFF;
    this->eliminationDisplayTimer = 0;
}

bool Mgr::IsFriendRoomOnline() const {
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    if (controller == nullptr) return false;
    return (controller->roomType == RKNet::ROOMTYPE_FROOM_HOST || controller->roomType == RKNet::ROOMTYPE_FROOM_NONHOST);
}

void Mgr::TickEliminationDisplay() {
    if (this->eliminationDisplayTimer == 0) return;
    --this->eliminationDisplayTimer;
    if (this->eliminationDisplayTimer == 0) {
        this->ResetEliminationDisplay();
    }
}

void Mgr::EnsureRaceInitialized(Raceinfo& raceinfo) {
    if (raceinfo.players == nullptr) return;

    const u16 raceFrames = raceinfo.raceFrames;
    if (this->lastRaceFrames != 0xFFFF && raceFrames < this->lastRaceFrames) {
        this->raceInitDone = false;
        this->raceFinished = false;
        this->playerCount = 0;
    }
    if (!this->raceInitDone && raceFrames == 0) {
        this->InitForRace();
    }
    this->lastRaceFrames = raceFrames;
}

void Mgr::HostMonitorDisconnects(RKNet::Controller& controller, const RKNet::ControllerSub& sub) {
    const u32 availableAids = sub.availableAids;
    if (this->disconnectGraceFrames > 0) {
        --this->disconnectGraceFrames;
        this->lastAvailableAids = availableAids;
        return;
    }

    if (this->lastAvailableAids != 0) {
        const u32 lost = this->lastAvailableAids & ~availableAids;
        if (lost != 0) {
            for (u8 playerId = 0; playerId < 12; ++playerId) {
                if (!this->active[playerId]) continue;
                const u8 aid = controller.aidsBelongingToPlayerIds[playerId];
                if (aid >= 12) continue;
                if ((lost & (1 << aid)) != 0) {
                    OS::Report("LapKO: Detected disconnect AID=%u player=%u (lost=%08x)\n", aid, playerId, lost);
                    this->ProcessElimination(playerId, ELIMINATION_CAUSE_DISCONNECT, false, true);
                }
            }
        }
    }

    this->lastAvailableAids = availableAids;
}

void Mgr::UpdateLapProgress(Raceinfo& raceinfo) {
    if (raceinfo.players == nullptr) return;

    const u8 maxPlayers = (this->playerCount < 12) ? this->playerCount : 12;
    for (u8 playerId = 0; playerId < maxPlayers; ++playerId) {
        RaceinfoPlayer* infoPlayer = raceinfo.players[playerId];
        if (infoPlayer == nullptr) continue;
        const u16 lapValue = infoPlayer->currentLap;
        if (lapValue == this->lastLapValue[playerId]) continue;
        if (lapValue > this->lastLapValue[playerId]) {
            this->OnLapComplete(playerId, *infoPlayer);
        }
        this->lastLapValue[playerId] = lapValue;
    }
}

void Mgr::UpdateSpectatorInputs(const Raceinfo& raceinfo) {
    bool advanceForward = false;
    bool advanceBackward = false;

    SectionMgr* sectionMgr = SectionMgr::sInstance;
    if (sectionMgr != nullptr) {
        for (u8 hudSlot = 0; hudSlot < 4; ++hudSlot) {
            Input::RealControllerHolder* holder = sectionMgr->pad.padInfos[hudSlot].controllerHolder;
            if (holder == nullptr) continue;
            if (holder->curController == nullptr) continue;

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
    }

    if (advanceForward) {
        const u8 current = this->spectateTargetPlayer;
        const u8 next = this->FindNextActiveSpectatePlayer(raceinfo, current, true);
        if (next != 0xFF && next != current) {
            this->spectateTargetPlayer = next;
            this->spectateManualTarget = true;
            this->FocusCameraOnPlayer(next);
        }
    } else if (advanceBackward) {
        const u8 current = this->spectateTargetPlayer;
        const u8 next = this->FindNextActiveSpectatePlayer(raceinfo, current, false);
        if (next != 0xFF && next != current) {
            this->spectateTargetPlayer = next;
            this->spectateManualTarget = true;
            this->FocusCameraOnPlayer(next);
        }
    }

    if (this->spectateManualTarget) {
        this->EnsureSpectateTargetIsActive(raceinfo);
    }
}

void Mgr::MaintainSpectatorView(const Raceinfo& raceinfo) {
    if (!this->isSpectating) return;
    if (!this->spectateManualTarget) {
        const u8 leader = this->GetLeaderPlayerId(raceinfo);
        if (leader != 0xFF) {
            this->spectateTargetPlayer = leader;
        }
    }

    this->EnsureSpectateTargetIsActive(raceinfo);

    if (this->spectateTargetPlayer < 12) {
        this->FocusCameraOnPlayer(this->spectateTargetPlayer);
    }
}

void Mgr::ProcessPendingItemReweight() {
    if (this->pendingItemReweightFrames == 0) return;
    --this->pendingItemReweightFrames;
    if (this->pendingItemReweightFrames == 0) {
        this->ReweightItemProbabilitiesNow();
    }
}

void Mgr::HostDistributeEvents(RKNet::Controller& controller, const RKNet::ControllerSub& sub) {
    // Fill outgoing PulRH1 extras for all peers. If we don't have a pending event, explicitly zero seq.
    for (int aid = 0; aid < 12; ++aid) {
        if (aid == sub.localAid) continue;
        if ((sub.availableAids & (1 << aid)) == 0) continue;

        RKNet::PacketHolder<Network::PulRH1>* holder = controller.GetSendPacketHolder<Network::PulRH1>(aid);
        if (holder == nullptr) continue;
        if (holder->packetSize < sizeof(Network::PulRH1)) holder->packetSize = sizeof(Network::PulRH1);
        Network::PulRH1* packet = holder->packet;

        if (this->hasPendingEvent && this->IsFriendRoomOnline()) {
            packet->pulsarTrackId = static_cast<u16>(packet->trackId);
            packet->variantIdx = 0;
            packet->lapKoSeq = this->pendingSequence;
            packet->lapKoRoundIndex = this->pendingRound;
            packet->lapKoActiveCount = this->pendingActiveCount;
            const u8 countFlag = this->pendingNoRoundAdvance ? lapKoNoRoundAdvanceFlag : 0;
            if (this->pendingBatchCount > 0) {
                const u8 count = this->pendingBatchCount;
                packet->lapKoElimCount = static_cast<u8>((count & 0x7F) | countFlag);
                for (u8 i = 0; i < this->pendingBatchCount; ++i) packet->lapKoElims[i] = this->pendingBatch[i];
                for (u8 i = this->pendingBatchCount; i < 12; ++i) packet->lapKoElims[i] = 0xFF;
            } else {
                packet->lapKoElimCount = static_cast<u8>((1 & 0x7F) | countFlag);
                packet->lapKoElims[0] = static_cast<u8>(this->pendingElimination & 0x7F);
                for (u8 i = 1; i < 12; ++i) packet->lapKoElims[i] = 0xFF;
            }
        } else {
            packet->lapKoSeq = 0;
            packet->lapKoElimCount = 0;
            if (this->IsFriendRoomOnline()) {
                packet->pulsarTrackId = static_cast<u16>(packet->trackId);
                packet->variantIdx = 0;
            }
        }
    }

    if (this->pendingTimer > 0) {
        --this->pendingTimer;
        if (this->pendingTimer == 0) {
            this->ClearPendingEvent();
        }
    }
}

void Mgr::ClientConsumeHostEvents(RKNet::Controller& controller, const RKNet::ControllerSub&) {
    if (this->hostAid >= 12) return;

    const u32 bufferIdx = controller.lastReceivedBufferUsed[this->hostAid][RKNet::PACKET_RACEHEADER1];
    RKNet::SplitRACEPointers* split = controller.splitReceivedRACEPackets[bufferIdx][this->hostAid];
    if (split == nullptr) return;

    const RKNet::PacketHolder<Network::PulRH1>* holder = split->GetPacketHolder<Network::PulRH1>();
    if (holder == nullptr) return;
    if (holder->packetSize != sizeof(Network::PulRH1)) return;

    const Network::PulRH1* packet = holder->packet;
    if (this->IsFriendRoomOnline() && packet->lapKoSeq != 0 && packet->lapKoElimCount != 0) {
        const u8 rawCount = packet->lapKoElimCount;
        const u8 elimCount = static_cast<u8>(rawCount & 0x7F);
        if (elimCount > 0 && elimCount <= 12) {
            const bool noRoundAdvance = (rawCount & lapKoNoRoundAdvanceFlag) != 0;
            this->ApplyRemoteBatch(packet->lapKoSeq, packet->lapKoRoundIndex, packet->lapKoActiveCount, packet->lapKoElims, elimCount, noRoundAdvance);
        }
    }
}

u8 Mgr::SelectEliminationCandidates(u8 toEliminate, u8* eliminatedList) const {
    if (eliminatedList == nullptr || toEliminate == 0) return 0;

    u8 elimCount = 0;
    Raceinfo* raceinfoLocal = Raceinfo::sInstance;

    if (raceinfoLocal != nullptr && raceinfoLocal->playerIdInEachPosition != nullptr) {
        for (int pos = 11; pos >= 0 && elimCount < toEliminate; --pos) {
            const u8 pid = raceinfoLocal->playerIdInEachPosition[pos];
            if (pid >= 12) continue;
            if (!this->active[pid]) continue;
            if (this->crossed[pid]) continue;
            if (this->HasCandidate(eliminatedList, elimCount, pid)) continue;
            eliminatedList[elimCount++] = pid;
        }

        for (int idx = static_cast<int>(this->orderCursor) - 1; elimCount < toEliminate && idx >= 0; --idx) {
            const u8 pid = this->crossOrder[idx];
            if (pid >= 12) continue;
            if (!this->active[pid]) continue;
            if (this->HasCandidate(eliminatedList, elimCount, pid)) continue;
            eliminatedList[elimCount++] = pid;
        }
        return elimCount;
    }

    for (u8 i = 0; i < 12 && elimCount < toEliminate; ++i) {
        if (!this->active[i]) continue;
        if (this->crossed[i]) continue;
        eliminatedList[elimCount++] = i;
    }

    for (int idx = static_cast<int>(this->orderCursor) - 1; elimCount < toEliminate && idx >= 0; --idx) {
        const u8 pid = this->crossOrder[idx];
        if (pid >= 12) continue;
        if (this->HasCandidate(eliminatedList, elimCount, pid)) continue;
        eliminatedList[elimCount++] = pid;
    }

    return elimCount;
}

bool Mgr::HasCandidate(const u8* list, u8 count, u8 playerId) const {
    if (list == nullptr) return false;
    for (u8 idx = 0; idx < count; ++idx) {
        if (list[idx] == playerId) return true;
    }
    return false;
}

u8 Mgr::AdvanceSequence() {
    u8 next = static_cast<u8>((this->eventSequence + 1) & 0xFF);
    if (next == 0) next = 1;
    this->eventSequence = next;
    return next;
}

void Mgr::PreparePendingEvent(u8 concludedRound, u8 activeCount) {
    this->pendingRound = concludedRound;
    this->pendingActiveCount = activeCount;
    this->pendingTimer = pendingBroadcastFrames;
    this->hasPendingEvent = true;
}

void Mgr::InitializeSpectateView(const Raceinfo& raceinfo) {
    const u8 leader = this->GetLeaderPlayerId(raceinfo);
    if (leader != 0xFF) {
        this->spectateTargetPlayer = leader;
    } else {
        this->spectateTargetPlayer = this->FindNextActiveSpectatePlayer(raceinfo, 0xFF, true);
    }

    this->EnsureSpectateTargetIsActive(raceinfo);

    if (this->spectateTargetPlayer < 12) {
        this->FocusCameraOnPlayer(this->spectateTargetPlayer);
    }
}

void Mgr::EnsureSpectateTargetIsActive(const Raceinfo& raceinfo) {
    const u8 current = this->spectateTargetPlayer;
    const RacedataScenario& scenario = Racedata::sInstance->menusScenario;
    const GameMode mode = scenario.settings.gamemode;
    if (mode == MODE_PUBLIC_BATTLE || mode == MODE_PRIVATE_BATTLE) {
        if (current < 12) {
            RaceinfoPlayer* rifPlayerCur = nullptr;
            if (raceinfo.players != nullptr) rifPlayerCur = raceinfo.players[current];
            const bool isBattleEliminated = (rifPlayerCur != nullptr && rifPlayerCur->battleScore == 0);
            if (this->active[current] && !isBattleEliminated) return;
        }
    }

    const u8 fallback = this->FindNextActiveSpectatePlayer(raceinfo, current, true);
    this->spectateTargetPlayer = fallback;
    if (fallback == 0xFF) {
        this->spectateManualTarget = false;
    }
}

u8 Mgr::BuildActiveSpectateOrder(const Raceinfo& raceinfo, u8* outOrder) const {
    if (outOrder == nullptr) return 0;
    const RacedataScenario& scenario = Racedata::sInstance->menusScenario;
    const GameMode mode = scenario.settings.gamemode;
    const u8 playerCount = Pulsar::System::sInstance->nonTTGhostPlayersCount;

    u8 count = 0;
    if (raceinfo.playerIdInEachPosition != nullptr) {
        const u8 maxEntries = (this->playerCount != 0 && this->playerCount < 12) ? this->playerCount : 12;
        for (u8 pos = 0; pos < maxEntries && count < 12; ++pos) {
            const u8 pid = raceinfo.playerIdInEachPosition[pos];
            if (pid >= 12) continue;
            RaceinfoPlayer* rifPlayerPos = nullptr;
            if (raceinfo.players != nullptr) rifPlayerPos = raceinfo.players[pid];
            if (!this->active[pid]) continue;
            if (mode == MODE_PUBLIC_BATTLE || mode == MODE_PRIVATE_BATTLE) {
                if (rifPlayerPos != nullptr && rifPlayerPos->battleScore == 0) continue;
            }

            bool already = false;
            for (u8 i = 0; i < count; ++i) {
                if (outOrder[i] == pid) {
                    already = true;
                    break;
                }
            }
            if (!already) {
                outOrder[count++] = pid;
            }
        }
    }

    for (u8 pid = 0; pid < playerCount && pid < 12 && count < this->activeCount && count < 12; ++pid) {
        RaceinfoPlayer* rifPlayer = nullptr;
        if (raceinfo.players != nullptr) rifPlayer = raceinfo.players[pid];
        if (!this->active[pid]) continue;
        if (mode == MODE_PUBLIC_BATTLE || mode == MODE_PRIVATE_BATTLE) {
            if (rifPlayer != nullptr && rifPlayer->battleScore == 0) continue;
        }

        bool already = false;
        for (u8 i = 0; i < playerCount < count; ++i) {
            if (outOrder[i] == pid) {
                already = true;
                break;
            }
        }
        if (!already) {
            outOrder[count++] = pid;
        }
    }

    return count;
}

u8 Mgr::FindNextActiveSpectatePlayer(const Raceinfo& raceinfo, u8 current, bool forward) const {
    u8 order[12];
    const u8 count = this->BuildActiveSpectateOrder(raceinfo, order);
    if (count == 0) return 0xFF;

    s32 idx = -1;
    if (current < 12) {
        for (u8 i = 0; i < count; ++i) {
            if (order[i] == current) {
                idx = static_cast<s32>(i);
                break;
            }
        }
    }

    if (idx < 0) {
        return forward ? order[0] : order[count - 1];
    }

    if (count == 1) return order[0];

    if (forward) {
        idx = (idx + 1) % count;
    } else {
        idx = (idx + count - 1) % count;
    }

    return order[idx];
}

u8 Mgr::GetLeaderPlayerId(const Raceinfo& raceinfo) const {
    if (raceinfo.playerIdInEachPosition == nullptr) return 0xFF;

    const u8 maxEntries = (this->playerCount != 0 && this->playerCount < 12) ? this->playerCount : 12;
    for (u8 pos = 0; pos < maxEntries; ++pos) {
        const u8 pid = raceinfo.playerIdInEachPosition[pos];
        if (pid >= 12) continue;
        if (!this->active[pid]) continue;
        return pid;
    }

    return 0xFF;
}

bool Mgr::FocusCameraOnPlayer(u8 playerId) const {
    if (playerId >= 12) return false;

    RaceCameraMgr* camMgr = RaceCameraMgr::sInstance;
    if (camMgr == nullptr || camMgr->cameras == nullptr || camMgr->cameraCount == 0) return false;

    u8 targetCamIdx = 0xFF;
    for (u32 i = 0; i < camMgr->cameraCount; ++i) {
        RaceCamera* cam = camMgr->cameras[i];
        if (cam != nullptr && cam->playerId == playerId) {
            targetCamIdx = static_cast<u8>(i);
            break;
        }
    }

    if (targetCamIdx != 0xFF) {
        DriverMgr::ChangeFocusedPlayer(targetCamIdx);
        RaceCameraMgr::ChangeFocusedPlayer(targetCamIdx);
        return true;
    }

    const u32 currentIdx = (camMgr->focusedPlayerIdx < camMgr->cameraCount) ? camMgr->focusedPlayerIdx : 0;
    RaceCamera* currentCam = camMgr->cameras[currentIdx];
    if (currentCam != nullptr && currentCam->playerId != playerId) {
        currentCam->playerId = playerId;
    }
    DriverMgr::ChangeFocusedPlayer(static_cast<u8>(currentIdx));
    return true;
}

}  // namespace LapKO
}  // namespace Pulsar