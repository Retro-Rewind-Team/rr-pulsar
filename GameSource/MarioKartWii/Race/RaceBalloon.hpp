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
    static RaceBalloonManager *sInstance;  // 809c4748
    static void CreateInstance();  // 808697bc

    void Add(int playerId, u32 teamId, u32 isInitial, int delay, u32 count, int interval);  // 80869df4
    void Remove(u32 playerId, u32 visible, u32 sound, int delay, u32 count, int interval);  // 80869fd0
    void Move(u32 toPlayer, u32 fromPlayer, int delay, u32 count, int interval);  // 8086a0dc

    struct Balloon {
        GeoObj::ObjBalloon *object;
        u8 teamId;
        u8 playerId;
        u8 index;
        u8 padding;
    };

    struct Player {
        u8 count;
        u8 padding[3];
        GeoObj::ObjBalloon *balloons[5];
    };

    u8 playerCount;
    u8 balloonsPerTeam;
    u8 balloonCount;
    u8 padding;
    Balloon balloons[120];
    Player players[12];
    u32 time;
    u8 unknown_0x4e8[0x10];
};
typedef char RaceBalloonManagerSizeCheck[sizeof(RaceBalloonManager) == 0x4f8 ? 1 : -1];

#endif
