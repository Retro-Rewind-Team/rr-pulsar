#ifndef _PUL_NETWORK_
#define _PUL_NETWORK_

#include <Config.hpp>

namespace Pulsar {
namespace Network {

enum DenyType {
    DENY_TYPE_NORMAL,
    DENY_TYPE_BAD_PACK,
    DENY_TYPE_OTT,
    DENY_TYPE_KICK,
};

class Mgr { //Manages network related stuff within Pulsar
public:
    Mgr() : racesPerGP(3), curBlockingArrayIdx(0), region(0x0A) {}
    u32 hostContext;
    u32 hostContext2;
    DenyType denyType;
    u8 deniesCount;
    u8 ownStatusData;
    u8 statusDatas[30];
    u8 curBlockingArrayIdx;
    u8 racesPerGP;
    u8 padding[2];
    u32 region;
    PulsarId* lastTracks;
};

}//namespace Network
}//namespace Pulsar

#endif