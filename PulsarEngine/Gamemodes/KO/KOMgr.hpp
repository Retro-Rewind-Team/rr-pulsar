#ifndef _PUL_KOMGR_
#define _PUL_KOMGR_

#include <kamek.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/UI/Page/Leaderboard/GPVSLeaderboardTotal.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <PulsarSystem.hpp>

namespace Pulsar {
namespace KO {

enum Status {
    NORMAL,
    TIE,
    KOD,
    DISCONNECTED
};

class Mgr {
   public:
    static const u16 spectatorVote = 0x45;
    static const u32 arbitraryAlmostDied = 60;  // 60 frames in danger in the last 5s = almost out

    struct PlayerPosition {
        u8 position;
        u8 playerId;
    };

    struct Stats {
        Stats() : percentageSum(0.0f) {}

        struct Final {
            Final() : timeInDanger(0), almostKOdCounter(0), finalPercentageSum(0) {}
            u16 timeInDanger;
            u8 almostKOdCounter;
            u8 finalPercentageSum;  // Divided by race count at GP end
        };

        float percentageSum;
        bool isInDangerFrames[300];  // Updated each frame in race
        u32 boolCountArray;
        Final final;
    };

    static void Create(Page* froom, u32 director, float length);
    static void Update();  // RaceFrameHook
    static void ProcessKOs(Pages::GPVSLeaderboardUpdate::Player* playerArr,
                           size_t nitems, size_t size,
                           int (*compar)(const void*, const void*));

    static int SortPlayersByPosition(PlayerPosition* a, PlayerPosition* b) {
        return a->position - b->position;
    }

    Mgr();
    ~Mgr();

    inline void ResetRace() {
        for (int i = 0; i < 2; ++i) {
            Stats& stats = this->stats[i];
            memset(&stats.isInDangerFrames[0], 0, sizeof(u8) * 300);
            stats.boolCountArray = 0;
            this->posTrackerAnmFrames[i] = 0;
        }
        for (int i = 0; i < 12; ++i) {
            if (this->status[i][0] == TIE) this->status[i][0] = NORMAL;
            if (this->status[i][1] == TIE) this->status[i][1] = NORMAL;
        }
    }
    void AddRaceStats();
    void CalcWouldBeKnockedOut();  // Checks if player would be KO'd if race ended now

    Status GetAidStatus(u8 aid, u8 hudslotId) const {
        return static_cast<Status>(this->status[aid][hudslotId]);
    }

    Status GetPlayerStatus(u8 playerId) const {
        u32 aidSlot = this->GetAidAndSlotFromPlayerId(playerId);
        return this->GetAidStatus(aidSlot & 0xFFFF, aidSlot >> 16);
    }

    bool IsKOdAid(u8 aid, u8 hudslotId) const {
        return GetAidStatus(aid, hudslotId) == KOD;
    }

    bool IsDisconnectedAid(u8 aid, u8 hudslotId) const {
        return GetAidStatus(aid, hudslotId) == DISCONNECTED;
    }

    bool IsKOdPlayerId(u8 playerId) const {
        return GetPlayerStatus(playerId) == KOD;
    }

    bool IsDisconnectedPlayerId(u8 playerId) const {
        return GetPlayerStatus(playerId) == DISCONNECTED;
    }

    void SetKOd(u8 playerId) {
        this->SetStatus(playerId, KOD);
    }

    void SetDisconnected(u8 playerId) {
        this->SetStatus(playerId, DISCONNECTED);
    }

    void SetTie(u8 playerId, u8 playerId2) {
        this->SetStatus(playerId, TIE);
        this->SetStatus(playerId2, TIE);
    }

    bool GetWouldBeKnockedOut(u8 playerId) const {
        return this->wouldBeOut[playerId];
    }

    bool GetIsSwapped() const { return this->hasSwapped; }
    void SwapControllersAndUI();
    void PatchAids(RKNet::ControllerSub& sub) const;
    PageId KickPlayersOut(PageId defaultId);

    SectionId GetSectionAfterKO(SectionId defaultId) const;
    u32 GetAidAndSlotFromPlayerId(u8 playerId) const;
    u8 GetBaseLocalPlayerCount() const { return this->baseLocPlayerCount; }

    u8 GetRoundKoCount(u8 playerCount) const;
    bool Is1v1KoRace(u32 currentRaceNumber) const;
    bool IsOfflineVS() const;
    void ForceOfflineVSRaceCount() const;
    u32 GetCurrentRaceNumber() const;
    void AdvanceOfflineRaceNumber();
    void FinishOfflineVSIfAllLocalPlayersAreOut();
    void PrepareOfflineVSNextRace();

   private:
    void SetStatus(u8 playerId, Status status) {
        u32 aidSlot = this->GetAidAndSlotFromPlayerId(playerId);
        this->status[aidSlot & 0xFFFF][aidSlot >> 16] = status;
    }

    u8 status[12][2];  // Indexed by [aid][hudslot]
    bool wouldBeOut[12];

    u8 baseLocPlayerCount;  // Player count when GP started
    bool hasSwapped;  // Controller swap status
    bool isOfflineVS;
    u8 offlineRaceNumber;

   public:
    bool isTiebreakerRace;
    u8 racesPerKO;
    u8 koPerRace;
    bool alwaysFinal;
    bool singleRace1v1Final;
    u8 elimThresholdPlayers;
    u8 elimChangeCount;

    u8 winnerPlayerId;
    bool isSpectating;
    Stats stats[2];
    u8 posTrackerAnmFrames[2];
};

}  // namespace KO
}  // namespace Pulsar

#endif
