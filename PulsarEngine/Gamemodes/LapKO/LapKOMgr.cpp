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
#include <runtimeWrite.hpp>

namespace Pulsar {
namespace LapKO {

static const u16 pendingBroadcastFrames = 120;
static const u16 eliminationDisplayDuration = 180;

Mgr::Mgr()
    : koPerRaceSetting(1),
      orderCursor(0),
      activeCount(0),
      playerCount(0),
      roundIndex(1),
      totalRounds(0),
      eventSequence(0),
      appliedSequence(0),
      pendingSequence(0),
      pendingElimination(0xFF),
      pendingRound(0),
      pendingActiveCount(0),
      hasPendingEvent(false),
      pendingBatchCount(0),
      isSpectating(false),
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
    this->totalRounds = 0;
    this->eventSequence = 0;
    this->appliedSequence = 0;
    this->pendingSequence = 0;
    this->pendingElimination = 0xFF;
    this->pendingRound = 0;
    this->pendingActiveCount = 0;
    this->pendingTimer = 0;
    this->hasPendingEvent = false;
    this->pendingBatchCount = 0;
    this->isSpectating = false;
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
        this->ProcessElimination(playerId, "disconnect", false);
    }
}

void Mgr::TryResolveRound() {
    if (this->raceFinished) return;
    if (this->activeCount <= 1) return;

    // Special case for 1-lap tracks: when first place finishes, eliminate everyone else
    const u8 usualLaps = this->GetUsualTrackLapCount();

    // Determine number of eliminations this round
    u8 planIdx = (this->roundIndex - 1);
    if (planIdx >= this->totalRounds) return;
    u8 toEliminate = (planIdx < MaxRounds) ? this->eliminationPlan[planIdx] : 0;
    if (toEliminate == 0) return;  // no eliminations scheduled this round
    if (toEliminate >= this->activeCount) toEliminate = static_cast<u8>(this->activeCount - 1);

    u8 requiredCrossings = static_cast<u8>(this->activeCount - toEliminate);
    if (usualLaps <= 1) {
        // Require just the first to cross to trigger elimination of the rest
        requiredCrossings = 1;
        // Eliminate everyone except the first crosser
        toEliminate = static_cast<u8>(this->activeCount - 1);
    }
    if (this->orderCursor < requiredCrossings) return;  // wait until all safe players crossed

    // Collect elimination targets = bottom ranked active players at this moment
    // Prefer Raceinfo's current placement ordering, BUT only eliminate players who have NOT crossed into the new lap yet.
    // This avoids eliminating a safe player (e.g., host) if placements are momentarily incomplete online at the round boundary.
    // Provenance: observed early-round host elimination due to transient standings; fixed by gating on !crossed first and
    // breaking ties using placement order and crossOrder tail.
    u8 eliminatedList[12];
    u8 elimCount = 0;

    Raceinfo* raceinfoLocal = Raceinfo::sInstance;
    if (raceinfoLocal != nullptr && raceinfoLocal->playerIdInEachPosition != nullptr) {
        // Pass 1: Prefer eliminating those who have not crossed into the next lap yet
        for (int pos = 11; pos >= 0 && elimCount < toEliminate; --pos) {
            const u8 pid = raceinfoLocal->playerIdInEachPosition[pos];
            if (pid >= 12) continue;
            if (!this->active[pid]) continue;
            if (this->crossed[pid]) continue;  // safe in this round
            bool already = false;
            for (u8 c = 0; c < elimCount; ++c) {
                if (eliminatedList[c] == pid) {
                    already = true;
                    break;
                }
            }
            if (!already) eliminatedList[elimCount++] = pid;
        }
        // Pass 2: If still short (e.g., everyone crossed on 1-lap tracks), fill from the tail of cross order
        for (int idx = this->orderCursor - 1; elimCount < toEliminate && idx >= 0; --idx) {
            const u8 pid = this->crossOrder[idx];
            if (pid >= 12) continue;
            if (!this->active[pid]) continue;
            bool already = false;
            for (u8 c = 0; c < elimCount; ++c) {
                if (eliminatedList[c] == pid) {
                    already = true;
                    break;
                }
            }
            if (!already) eliminatedList[elimCount++] = pid;
        }
    } else {
        // Fallback: use previous approach based on not-crossed and tail of cross order
        for (u8 i = 0; i < 12 && elimCount < toEliminate; ++i) {
            if (!this->active[i]) continue;
            if (this->crossed[i]) continue;
            eliminatedList[elimCount++] = i;
        }
        for (int idx = this->orderCursor - 1; elimCount < toEliminate && idx >= 0; --idx) {
            u8 pid = this->crossOrder[idx];
            bool already = false;
            for (u8 c = 0; c < elimCount; ++c)
                if (eliminatedList[c] == pid) already = true;
            if (!already) eliminatedList[elimCount++] = pid;
        }
    }

    if (elimCount == 0) return;

    // Apply eliminations sequentially but avoid advancing round until last in batch
    const u8 concludedRound = this->roundIndex;
    for (u8 i = 0; i < elimCount; ++i) {
        const bool lastOne = (i == elimCount - 1);
        this->ProcessEliminationInternal(eliminatedList[i], "round", false, !lastOne);
    }
    // Host in friend room broadcasts the batch to all clients
    if (this->IsFriendRoomOnline() && this->isHost) {
        this->BroadcastBatch(eliminatedList, elimCount, concludedRound);
    }
}

void Mgr::ProcessElimination(u8 playerId, const char* reason, bool fromNetwork) {
    this->ProcessEliminationInternal(playerId, reason, fromNetwork, false);
}

void Mgr::ProcessEliminationInternal(u8 playerId, const char* reason, bool fromNetwork, bool suppressRoundAdvance) {
    if (playerId >= 12) return;
    if (!this->active[playerId]) return;
    if (this->raceFinished) return;
    const u8 concludedRound = this->roundIndex;

    this->active[playerId] = false;

    if (this->activeCount > 0) --this->activeCount;
    this->UpdateActivePlayerCounts();
    if (this->isHost && !fromNetwork) {
        if (this->IsFriendRoomOnline()) {
            // Friend rooms: use batched broadcast path even for single eliminations (e.g., disconnects)
            u8 single[1] = {playerId};
            this->BroadcastBatch(single, 1, concludedRound);
        } else {
            // Legacy single-elimination broadcast for non-friend online rooms
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
    this->pendingSequence = static_cast<u8>((this->eventSequence + 1) & 0xFF);
    if (this->pendingSequence == 0) this->pendingSequence = 1;
    this->eventSequence = this->pendingSequence;
    this->pendingElimination = playerId;
    this->pendingRound = concludedRound;
    this->pendingActiveCount = this->activeCount;
    this->pendingTimer = pendingBroadcastFrames;
    this->hasPendingEvent = true;

    OS::Report("LapKO: Broadcast seq=%u player=%u round=%u active=%u\n", this->pendingSequence, playerId, concludedRound, this->pendingActiveCount);
}

void Mgr::BroadcastBatch(const u8* elimIds, u8 elimCount, u8 concludedRound) {
    if (elimIds == nullptr || elimCount == 0) return;
    if (elimCount > 12) elimCount = 12;
    this->pendingSequence = static_cast<u8>((this->eventSequence + 1) & 0xFF);
    if (this->pendingSequence == 0) this->pendingSequence = 1;
    this->eventSequence = this->pendingSequence;
    this->pendingRound = concludedRound;
    this->pendingActiveCount = this->activeCount;
    this->pendingBatchCount = elimCount;
    for (u8 i = 0; i < elimCount; ++i) this->pendingBatch[i] = elimIds[i];
    this->pendingTimer = pendingBroadcastFrames;
    this->hasPendingEvent = true;
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
    this->pendingBatchCount = 0;
}

void Mgr::ApplyRemoteEvent(u8 seq, u8 eliminatedId, u8 roundIdx, u8 activeCnt) {
    if (seq == 0 || eliminatedId >= 12) return;
    if (seq == this->appliedSequence) return;

    this->appliedSequence = seq;
    this->roundIndex = roundIdx;
    this->ProcessElimination(eliminatedId, "network", true);
    this->activeCount = activeCnt;
    this->UpdateActivePlayerCounts();
}

void Mgr::ApplyRemoteBatch(u8 seq, u8 roundIdx, u8 activeCnt, const u8* elimIds, u8 elimCount) {
    if (seq == 0) return;
    if (seq == this->appliedSequence) return;
    if (elimIds == nullptr || elimCount == 0) return;
    if (elimCount > 12) elimCount = 12;
    this->appliedSequence = seq;
    this->roundIndex = roundIdx;
    for (u8 i = 0; i < elimCount; ++i) {
        const bool lastOne = (i == elimCount - 1);
        this->ProcessEliminationInternal(elimIds[i], "network", true, !lastOne);
    }
    this->activeCount = activeCnt;
}

void Mgr::UpdateFrame() {
    // Always tick the elimination display timer every frame, independent of network flow
    if (this->eliminationDisplayTimer > 0) {
        --this->eliminationDisplayTimer;
        if (this->eliminationDisplayTimer == 0) {
            this->ResetEliminationDisplay();
        }
    }

    RKNet::Controller* controller = RKNet::Controller::sInstance;
    Raceinfo* raceinfo = Raceinfo::sInstance;
    if (controller == nullptr || raceinfo == nullptr) return;

    // Observe raceFrames to detect new-race boundary and re-init at start
    if (raceinfo->players != nullptr) {
        const u16 rf = raceinfo->raceFrames;
        // If frames decreased (scene changed) mark init as not done and clear finished flag
        if (this->lastRaceFrames != 0xFFFF && rf < this->lastRaceFrames) {
            this->raceInitDone = false;
            this->raceFinished = false;
            this->playerCount = 0;
        }
        if (!this->raceInitDone && rf == 0) {
            this->InitForRace();
        }
        this->lastRaceFrames = rf;
    }

    if (this->raceFinished && !this->hasPendingEvent) return;

    const RKNet::ControllerSub& sub = controller->subs[controller->currentSub];

    if (this->isHost && controller->roomType != RKNet::ROOMTYPE_NONE) {
        const u32 availableAids = sub.availableAids;
        if (this->disconnectGraceFrames > 0) {
            --this->disconnectGraceFrames;
            // During grace period, just update lastAvailableAids but do not eliminate.
            this->lastAvailableAids = availableAids;
        } else if (this->lastAvailableAids != 0) {
            const u32 lost = this->lastAvailableAids & ~availableAids;
            if (lost != 0) {
                for (u8 playerId = 0; playerId < 12; ++playerId) {
                    if (!this->active[playerId]) continue;
                    const u8 aid = controller->aidsBelongingToPlayerIds[playerId];
                    if (aid >= 12) continue;
                    if ((lost & (1 << aid)) != 0) {
                        OS::Report("LapKO: Detected disconnect AID=%u player=%u (lost=%08x)\n", aid, playerId, lost);
                        this->ProcessElimination(playerId, "disconnect", false);
                    }
                }
            }
        }
        this->lastAvailableAids = availableAids;
    }

    for (u8 playerId = 0; playerId < this->playerCount && playerId < 12; ++playerId) {
        RaceinfoPlayer* infoPlayer = raceinfo->players[playerId];
        if (infoPlayer == nullptr) continue;
        const u16 lapValue = infoPlayer->currentLap;
        if (lapValue != this->lastLapValue[playerId]) {
            if (lapValue > this->lastLapValue[playerId]) {
                this->OnLapComplete(playerId, *infoPlayer);
            }
            this->lastLapValue[playerId] = lapValue;
        }
    }

    // If we are spectating locally, keep the camera focused on first place every frame
    // Uses RaceCameraMgr::ChangeFocusedPlayer to update the watched camera when positions change.
    // Important: ChangeFocusedPlayer expects a CAMERA INDEX, not a playerId. Map leader -> camera index safely.
    if (this->isSpectating && raceinfo->playerIdInEachPosition != nullptr) {
        RaceCameraMgr* camMgr = RaceCameraMgr::sInstance;
        const u8 leaderPid = raceinfo->playerIdInEachPosition[0];
        if (camMgr != nullptr && camMgr->cameras != nullptr && camMgr->cameraCount > 0 && leaderPid < 12) {
            // Check if current focused camera already tracks the leader
            bool alreadyOnLeader = false;
            if (camMgr->focusedPlayerIdx < camMgr->cameraCount) {
                RaceCamera* curCam = camMgr->cameras[camMgr->focusedPlayerIdx];
                if (curCam != nullptr && curCam->playerId == leaderPid) alreadyOnLeader = true;
            }

            if (!alreadyOnLeader) {
                // Find the camera index for the leader playerId
                u8 targetCamIdx = 0xFF;
                for (u32 i = 0; i < camMgr->cameraCount; ++i) {
                    RaceCamera* cam = camMgr->cameras[i];
                    if (cam != nullptr && cam->playerId == leaderPid) {
                        targetCamIdx = static_cast<u8>(i);
                        break;
                    }
                }
                if (targetCamIdx != 0xFF) {
                    // Update both driver watched index and camera focus to the camera index
                    DriverMgr::ChangeFocusedPlayer(targetCamIdx);
                    RaceCameraMgr::ChangeFocusedPlayer(targetCamIdx);
                } else {
                    // Fallback: retarget the current camera to the leader if no dedicated camera exists (single local player cases)
                    u32 idx = (camMgr->focusedPlayerIdx < camMgr->cameraCount) ? camMgr->focusedPlayerIdx : 0;
                    RaceCamera* cam = camMgr->cameras[idx];
                    if (cam != nullptr) cam->playerId = leaderPid;
                    // Keep driver watched index consistent with current camera idx
                    DriverMgr::ChangeFocusedPlayer(static_cast<u8>(idx));
                }
            }
        }
    }

    // Run the deferred item reweight outside GMData/Raceinfo loops
    if (this->pendingItemReweightFrames > 0) {
        --this->pendingItemReweightFrames;
        if (this->pendingItemReweightFrames == 0) {
            this->ReweightItemProbabilitiesNow();
        }
    }

    if (this->isHost) {
        // Fill outgoing PulRH1 extras for all peers. If we don't have a pending event, explicitly zero seq.
        for (int aid = 0; aid < 12; ++aid) {
            if (aid == sub.localAid) continue;
            if ((sub.availableAids & (1 << aid)) == 0) continue;
            RKNet::PacketHolder<Network::PulRH1>* holder = controller->GetSendPacketHolder<Network::PulRH1>(aid);
            if (holder == nullptr) continue;
            if (holder->packetSize < sizeof(Network::PulRH1)) holder->packetSize = sizeof(Network::PulRH1);
            Network::PulRH1* packet = holder->packet;
            if (this->hasPendingEvent && this->IsFriendRoomOnline()) {
                // Populate LapKO friend-room fields
                // Ensure Pulsar track id mirrors the base so AfterRH1Reception picks the correct track
                packet->pulsarTrackId = static_cast<u16>(packet->trackId);
                packet->variantIdx = 0;
                packet->lapKoSeq = this->pendingSequence;
                packet->lapKoRoundIndex = this->pendingRound;
                packet->lapKoActiveCount = this->pendingActiveCount;
                if (this->pendingBatchCount > 0) {
                    packet->lapKoElimCount = this->pendingBatchCount;
                    for (u8 i = 0; i < this->pendingBatchCount; ++i) packet->lapKoElims[i] = this->pendingBatch[i];
                    // zero any unused slots
                    for (u8 i = this->pendingBatchCount; i < 12; ++i) packet->lapKoElims[i] = 0xFF;
                } else {
                    packet->lapKoElimCount = 1;
                    packet->lapKoElims[0] = this->pendingElimination;
                    for (u8 i = 1; i < 12; ++i) packet->lapKoElims[i] = 0xFF;
                }
            } else {
                packet->lapKoSeq = 0;
                packet->lapKoElimCount = 0;
                if (this->IsFriendRoomOnline()) {
                    // Keep track id consistent even when no event
                    packet->pulsarTrackId = static_cast<u16>(packet->trackId);
                    packet->variantIdx = 0;
                }
            }
        }
        if (this->pendingTimer > 0) {
            --this->pendingTimer;
            if (this->pendingTimer == 0) this->ClearPendingEvent();
        }
    } else {
        if (this->hostAid >= 12) return;
        const u32 bufferIdx = controller->lastReceivedBufferUsed[this->hostAid][RKNet::PACKET_RACEHEADER1];
        RKNet::SplitRACEPointers* split = controller->splitReceivedRACEPackets[bufferIdx][this->hostAid];
        if (split == nullptr) return;
        const RKNet::PacketHolder<Network::PulRH1>* holder = split->GetPacketHolder<Network::PulRH1>();
        if (holder == nullptr) return;
        if (holder->packetSize != sizeof(Network::PulRH1)) return;
        const Network::PulRH1* packet = holder->packet;
        // Apply LapKO friend-room eliminations from host if present
        if (this->IsFriendRoomOnline() && packet->lapKoSeq != 0 && packet->lapKoElimCount > 0 && packet->lapKoElimCount <= 12) {
            this->ApplyRemoteBatch(packet->lapKoSeq, packet->lapKoRoundIndex, packet->lapKoActiveCount, packet->lapKoElims, packet->lapKoElimCount);
        }
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

        // Snap focus to current leader if possible (map to camera index safely). If cameras aren't ready yet, UpdateFrame will handle it later.
        RaceCameraMgr* camMgr = RaceCameraMgr::sInstance;
        Raceinfo* raceinfo = Raceinfo::sInstance;
        if (raceinfo != nullptr && raceinfo->playerIdInEachPosition != nullptr) {
            const u8 leaderPid = raceinfo->playerIdInEachPosition[0];
            if (leaderPid < 12 && camMgr != nullptr && camMgr->cameras != nullptr && camMgr->cameraCount > 0) {
                // Try to find a camera for the leader immediately
                u8 targetCamIdx = 0xFF;
                for (u32 i = 0; i < camMgr->cameraCount; ++i) {
                    RaceCamera* cam = camMgr->cameras[i];
                    if (cam != nullptr && cam->playerId == leaderPid) {
                        targetCamIdx = static_cast<u8>(i);
                        break;
                    }
                }
                if (targetCamIdx != 0xFF) {
                    DriverMgr::ChangeFocusedPlayer(targetCamIdx);
                    RaceCameraMgr::ChangeFocusedPlayer(targetCamIdx);
                } else {
                    // Fallback: retarget current camera to leader and keep indices consistent
                    u32 idx = (camMgr->focusedPlayerIdx < camMgr->cameraCount) ? camMgr->focusedPlayerIdx : 0;
                    RaceCamera* cam = camMgr->cameras[idx];
                    if (cam != nullptr && cam->playerId != leaderPid) cam->playerId = leaderPid;
                    DriverMgr::ChangeFocusedPlayer(static_cast<u8>(idx));
                }
            }
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

}  // namespace LapKO
}  // namespace Pulsar
