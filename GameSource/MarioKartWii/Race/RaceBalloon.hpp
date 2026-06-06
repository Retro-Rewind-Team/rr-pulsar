#ifndef _RACEBALLOON_
#define _RACEBALLOON_

#include <kamek.hpp>

namespace GeoObj {
class ObjBalloon {
   public:
    ObjBalloon(u8 poolIdx, u8 teamId);  // 8086e224
    void OnAdd(u32 time, u8 playerId, u8 balloonIndex, u8 isInitial);  // 8086ec5c
};
}  // namespace GeoObj

class RaceBalloonManager {
   public:
    static RaceBalloonManager* sInstance;  // 809c4748
    static void CreateInstance();  // 808697bc

    void Add(int playerId, u32 teamId, u32 isInitial, int delay, u32 count, int interval);  // 80869df4
    void Remove(u32 playerId, u32 visible, u32 sound, int delay, u32 count, int interval);  // 80869fd0
    void Move(u32 toPlayer, u32 fromPlayer, int delay, u32 count, int interval);  // 8086a0dc
};

#endif
