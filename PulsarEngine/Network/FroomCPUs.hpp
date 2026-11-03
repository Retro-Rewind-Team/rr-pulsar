#ifndef _PUL_FROOMCPUS_
#define _PUL_FROOMCPUS_

#include <kamek.hpp>

class RacedataScenario;
namespace RKNet {
struct RACEPacketHeader;
}

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
#pragma pack(pop)

extern Roster roster;
extern u32 lastSelectId;
extern u8 playerSlots[12];

bool IsEnabled();
bool IsCPU(u32 playerId);
void WriteSELECT(Roster &dest, u32 selectId);
void ReadSELECT(const Roster &src, u32 aid);
void PrepareRace(RacedataScenario &scenario);
void SaveResults();
bool ReceivePacket(const RKNet::RACEPacketHeader &packet, u32 size, u32 aid);

}  // namespace FroomCPUs
}  // namespace Pulsar

#endif
