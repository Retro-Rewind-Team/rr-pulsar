#ifndef _PUL_FROOMCPUS_
#define _PUL_FROOMCPUS_

#include <MarioKartWii/RKNet/ITEM.hpp>
#include <MarioKartWii/RKNet/RACEDATA.hpp>
#include <MarioKartWii/RKNet/RACEHeader.hpp>
#include <MarioKartWii/RKNet/RH1.hpp>

class RacedataScenario;

namespace Pulsar {
namespace FroomCPUs {

#pragma pack(push, 1)
struct Entry {
    u8 character;
    u8 kart;
    u8 characterSlot;
    u8 status;
    u16 score;
    u8 previousPosition;
    u8 team;
};

struct Roster {
    u8 count;
    Entry entries[11];
};

struct Progress {
    u16 checkpoint;
    float completion[5];
    u8 respawn;
    u16 battleScore;
    u16 lap;
    u8 maxLap;
    u8 kcp;
    u8 maxKCP;
    u32 flags;
    u16 minutes;
    u8 seconds;
    u16 milliseconds;
    bool finished;
    u8 balloons;
    u16 receivedFinishMask;
    u16 disconnectedMask;
};

struct CPUHeader : RKNet::RACEHEADER1Packet {
    u8 playerIds[2];
    Progress progress[2];
};

struct RacePacket {
    RKNet::RACEPacketHeader header;
    CPUHeader rh1;
    RKNet::RACEDATAPacket racedata[2];
    RKNet::ITEMPacket items[2];
};
#pragma pack(pop)

extern Roster roster;
extern u32 lastSelectId;
extern u8 playerSlots[12];
extern RKNet::RACEDATAPacket racePackets[12];
extern Progress progress[12];
extern u32 packetFrames[12];
extern u16 validPackets;

void WriteSELECT(Roster &dest, u32 selectId);
void ReadSELECT(const Roster &src, u32 aid);
void PrepareRace(RacedataScenario &scenario);
void SaveResults();

}  // namespace FroomCPUs
}  // namespace Pulsar

#endif
