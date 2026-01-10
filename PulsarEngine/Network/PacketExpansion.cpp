#include <Network/PacketExpansion.hpp>

namespace Pulsar {
namespace Network {

void* CreateSendAndRecvBuffers() {
    register RKNet::PacketHolder<void>* holder;
    register CustomRKNetController* controller;
    register u8 aid;
    asm(mr holder, r24;);
    asm(mr controller, r27;);
    asm(mr aid, r20;);

    controller->fullPulRecvPackets[aid] = new u8[totalRACESize];
    holder->bufferSize = totalRACESize;
    return new u8[totalRACESize];
}
kmCall(0x806570b4, CreateSendAndRecvBuffers);

/*
static const u32 sizeArray[8] = {
        sizeof(RKNet::RACEPacketHeader),
        sizeof(PulRH1),
        sizeof(PulRH2),
        sizeof(PulSELECT),
        2 * sizeof(PulRACEDATA),
        sizeof(PulUSER),
        2 * sizeof(PulITEM),
        sizeof(PulEVENT)
};
*/

// Buffer size must be the FULL size to accommodate any packet (including LapKO in friend rooms)
// The actual transmitted packet size is controlled dynamically in BeforeRH1Send based on context
kmWrite8(0x8089a19b, PulRH1SizeFull);
kmWrite8(0x8089a19f, sizeof(PulRH2));
kmWrite8(0x8089a1a3, sizeof(PulSELECT));
kmWrite8(0x8089a1a7, 2 * sizeof(PulRACEDATA));
kmWrite8(0x8089a1ab, sizeof(PulUSER));
kmWrite8(0x8089a1af, 2 * sizeof(PulITEM));
kmWrite8(0x8089a1b3, sizeof(PulEVENT));

void SetProperRecvBuffers(u8 aid, void* usualBuffer, u32 usualSize) {
    register CustomRKNetController* controller;
    asm(mr controller, r31;);
    memset(controller->fullPulRecvPackets[aid], 0, totalRACESize);
    DWC::SetRecvBuffer(aid, controller->fullPulRecvPackets[aid], totalRACESize);
}
kmWrite32(0x80658c78, 0x60000000);  // prevent usual memset
kmCall(0x80658c90, SetProperRecvBuffers);

void ProperRecvBuffersClear() {
    const CustomRKNetController* controller = reinterpret_cast<CustomRKNetController*>(RKNet::Controller::sInstance);
    for (int aid = 0; aid < 12; ++aid) memset(controller->fullPulRecvPackets[aid], 0, totalRACESize);
}
kmCall(0x8065607c, ProperRecvBuffersClear);

void CheckPacket(CustomRKNetController* controller, RKNet::RACEPacketHeader& packet, u32 size, u32 sizeUnused, u32 aid) {
    using namespace RKNet;

    const u32 recvCRC = packet.crc32;
    packet.crc32 = 0;
    const u32 calcCRC = OS::CalcCRC32(&packet, size);
    packet.crc32 = recvCRC;
    bool disconnect = false;
    if (recvCRC != calcCRC)
        disconnect = true;
    else {
        u32* lastUsedBufferAid = &controller->lastReceivedBufferUsed[aid][0];
        SplitRACEPointers* recv = controller->splitReceivedRACEPackets[lastUsedBufferAid[0]][aid];
        PacketHolder<void>** holderRecv = &recv->packetHolders[0];
        const u8* sizes = &packet.sizes[0];

        for (int i = 0; i < 8; ++i) {
            const PacketHolder<void>* curHolder = holderRecv[i];  // starts at header etc...

            const u8 curSize = sizes[i];  // transmitted in packet
            if (curSize != 0) {
                if (curHolder->bufferSize < curSize) disconnect = true;
            }
        }
    }
    if (disconnect)
        controller->toDisconnectAids |= 1 << aid;
    else
        reinterpret_cast<RKNet::Controller*>(controller)->ProcessRACEPacket(aid, packet, size);
}
kmBranch(0x80658608, CheckPacket);

bool DisconnectBadAids() {
    register CustomRKNetController* controller;
    asm(mr controller, r15;);

    int old = OS::DisableInterrupts();
    for (int aid = 0; aid < 12; ++aid) {
        if (controller->toDisconnectAids >> aid) DWC::CloseConnectionHard(aid);
    }
    controller->toDisconnectAids = 0;
    OS::RestoreInterrupts(old);

    return controller->shutdownScheduled;
}
kmCall(0x806579b4, DisconnectBadAids);
kmWrite32(0x806579b8, 0x2c030000);

// Various hardcoded size patches
kmWrite32(0x80661100, 0x418000d8);  // bne -> blt for comparison against sizeof(SELECTPacket)
kmWrite32(0x8065adc8, 0x41800014);  // bne -> blt for comparison against sizeof(ROOMPacket)
kmWrite32(0x80665244, 0x418000d0);  // bne -> blt for comparison against sizeof(RACEHEADER1Packet)

ExpSELECTHandler* CreateRecvArr(ExpSELECTHandler* handler) {  // wiimmfi hook prevents a more natural hook...
    register RKNet::OnlineMode mode;
    asm(mr mode, r31;);
    handler->mode = mode;
    handler->receivedPackets = new PulSELECT[12];
    return handler;
}
kmCall(0x8065fec0, CreateRecvArr);

void DeleteSELECT(ExpSELECTHandler* handler) {
    delete handler->receivedPackets;
    delete reinterpret_cast<RKNet::SELECTHandler*>(handler);
}
kmCall(0x8065ff84, DeleteSELECT);

u8 GetLastRecvSECTIONSize(u8 aid, u8 sectionIdx) {
    const CustomRKNetController* controller = reinterpret_cast<CustomRKNetController*>(RKNet::Controller::sInstance);
    RKNet::RACEPacketHeader* header = reinterpret_cast<RKNet::RACEPacketHeader*>(controller->fullPulRecvPackets[aid]);
    return header->sizes[sectionIdx];
}

}  // namespace Network
}  // namespace Pulsar