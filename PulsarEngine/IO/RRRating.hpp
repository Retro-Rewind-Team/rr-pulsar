#ifndef _PULSAR_RR_RATING_
#define _PULSAR_RR_RATING_

#include <kamek.hpp>
#include <core/rvl/OS/OSMutex.hpp>

namespace Pulsar {
class IO;

namespace RRRating {

const char folder[] = "/RRRating";
const char path[] = "/RRRating/RRRating.pul";
const u32 magic = 0x52525254;  // RRRT
const u16 version = 2;
const u16 capacity = 100;
const u32 maxRating = 1000000;

extern IO *ratingIO;
extern OS::Mutex ratingMutex;
extern bool checkedPendingRace;
extern bool raceRecorded;
extern bool raceActive;

struct Entry {
    u32 pid;
    u32 vr;
    u32 br;
};

struct File {
    u32 magic;
    u16 version;
    u16 entryCapacity;
    Entry entries[capacity];
};

struct LegacyFile {
    u32 magic;
    u16 version;
    u16 entryCapacity;
    struct {
        u32 pid;
        float vr;
        float br;
        u32 flags;
    } entries[capacity];
};
static_assert(sizeof(LegacyFile) == 1608, "Legacy RRRating file size");

// Appended after the v2 entries so existing files can gain a race marker in place.
struct PendingRace {
    u32 pid;
    u32 penalizedVR;
};

void Init(IO &io);

}  // namespace RRRating
}  // namespace Pulsar

#endif
