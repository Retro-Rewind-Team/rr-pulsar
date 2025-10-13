#ifndef _PUL_POSITIONCOUNTER_
#define _PUL_POSITIONCOUNTER_

#include <kamek.hpp>
#include <MarioKartWii/UI/Ctrl/CtrlRace/CtrlRaceRankNum.hpp>

namespace Pulsar {

class PositionCounter {
   public:
    static void UpdatePositionDisplay(CtrlRaceRankNum& posTracker);
    static void UpdateAnimationFrame(u8 hudSlotId, bool isInDanger);
    static void ResetAnimationFrames();

   private:
    static u8 posTrackerAnmFrames[2];
};

}  // namespace Pulsar

#endif
