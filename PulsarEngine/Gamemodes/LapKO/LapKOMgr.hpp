#ifndef _PUL_LAPKOMGR_
#define _PUL_LAPKOMGR_

#include <kamek.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <Gamemodes/PositionCounter.hpp>
#include <PulsarSystem.hpp>

namespace Pulsar {
namespace LapKO {

class Mgr {
   public:
    enum { MaxRounds = 12 };
    Mgr();
    ~Mgr();

    void InitForRace();
    void ResetRound();
    void OnLapComplete(u8 playerId, RaceinfoPlayer& player);
    void OnPlayerFinished(u8 playerId);
    void OnPlayerDisconnected(u8 playerId);
    void UpdateFrame();
    void ApplyRemoteEvent(u8 seq, u8 eliminatedId, u8 roundIndex, u8 activeCount);
    void ApplyRemoteBatch(u8 seq, u8 roundIndex, u8 activeCount, const u8* elimIds, u8 elimCount);

    bool IsActive(u8 playerId) const { return this->active[playerId]; }
    u8 GetActiveCount() const { return this->activeCount; }
    u8 GetRoundIndex() const { return this->roundIndex; }
    u8 GetCurrentRoundEliminationCount() const {
        if (this->activeCount <= 1) return 0;
        const u8 idx = (this->roundIndex == 0) ? 0 : static_cast<u8>(this->roundIndex - 1);
        u8 elim = 1;
        if (idx < MaxRounds) {
            u8 planned = this->eliminationPlan[idx];
            if (planned != 0) elim = planned;
        }
        if (elim >= this->activeCount) elim = static_cast<u8>(this->activeCount - 1);
        return elim;
    }

    u8 GetPendingSequence() const { return this->pendingSequence; }
    u8 GetPendingElimination() const { return this->pendingElimination; }
    u8 GetPendingRound() const { return this->pendingRound; }
    u8 GetPendingActiveCount() const { return this->pendingActiveCount; }
    u8 GetRecentEliminationCount() const { return this->recentEliminationCount; }
    u16 GetEliminationDisplayTimer() const { return this->eliminationDisplayTimer; }
    u8 GetRecentEliminationId(u8 index) const {
        return (index < this->recentEliminationCount) ? this->recentEliminations[index] : 0xFF;
    }

    void SetKoPerRace(u8 value);
    u8 GetKoPerRace() const;
    static u8 BuildPlan(u8 playerCount, u8 koPerRace, u8 usualLapCount, u8* outPlan, u8 capacity);

    void ClearPendingEvent();

   private:
    // Schedules and performs safe, deferred item probability reweighting.
    // We avoid touching Item::Manager::playerCount mid-race; instead we only
    // re-run ItemSlot post-processing a couple frames after active player
    // count changes.
    void ReweightItemProbabilitiesNow();
    void ComputeEliminationPlan();
    u8 GetUsualTrackLapCount() const;
    void ProcessEliminationInternal(u8 playerId, const char* reason, bool fromNetwork, bool suppressRoundAdvance);
    void StartNewRound();
    void TryResolveRound();
    void ProcessElimination(u8 playerId, const char* reason, bool fromNetwork);
    bool EnterSpectateIfLocal(u8 playerId);
    void ConcludeRace(u8 winnerId);
    void FinishOfflineAtCurrentStandings();
    void BroadcastEvent(u8 playerId, u8 concludedRound);
    void BroadcastBatch(const u8* elimIds, u8 elimCount, u8 concludedRound);
    void UpdateActivePlayerCounts();
    void LogState(const char* tag, u8 value) const;
    void RecordEliminationForDisplay(u8 playerId, u8 concludedRound);
    void ResetEliminationDisplay();
    bool IsFriendRoomOnline() const;

   public:
    u8 koPerRaceSetting;
    u8 eliminationPlan[MaxRounds];
    u8 totalRounds;

    bool active[12];
    bool crossed[12];
    u8 crossOrder[12];
    u16 lastLapValue[12];
    u8 orderCursor;
    u8 activeCount;
    u8 playerCount;
    u8 roundIndex;
    u8 eventSequence;
    u8 appliedSequence;
    u8 pendingSequence;
    u8 pendingElimination;
    u8 pendingRound;
    u8 pendingActiveCount;
    bool hasPendingEvent;
    // Batch pending (friend rooms)
    u8 pendingBatchCount;
    u8 pendingBatch[12];
    bool isSpectating;
    bool isHost;
    u8 hostAid;
    u16 pendingTimer;
    u32 lastAvailableAids;
    bool raceFinished;
    bool raceInitDone;
    u16 lastRaceFrames;
    // Increase to 4 to allow elimination display of up to four names
    u8 recentEliminations[4];
    u8 recentEliminationCount;
    u8 recentEliminationRound;
    u16 eliminationDisplayTimer;
    // Frames to wait before reweighting item probabilities after an active
    // player count change. Initialized to 0.
    u8 pendingItemReweightFrames;
    u16 disconnectGraceFrames;
};

}  // namespace LapKO
}  // namespace Pulsar

#endif
