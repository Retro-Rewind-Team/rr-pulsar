#include <Network/PacketExpansion.hpp>

namespace Pulsar {
namespace Network {

// todo: is this really the best place to put this?
static bool IsValidRACESectionSizes(const RKNet::RACEPacketHeader &packet, u32 receivedSize) {
    using namespace RKNet;

    if (receivedSize < sizeof(RACEPacketHeader) || receivedSize > totalRACESize) return false;
    if (packet.sizes[PACKET_RACEHEADER] != sizeof(RACEPacketHeader)) return false;

    const u8 rh1Size = packet.sizes[PACKET_RACEHEADER1];
    if (rh1Size != 0 && rh1Size != sizeof(RACEHEADER1Packet) &&
        rh1Size != PulRH1SizeBase && rh1Size != PulRH1SizeLapKo && rh1Size != PulRH1SizeFull)
        return false;

    const u8 rh2Size = packet.sizes[PACKET_RACEHEADER2];
    if (rh2Size != 0 && rh2Size != sizeof(PulRH2)) return false;

    const u8 selectRoomSize = packet.sizes[PACKET_SELECTROOM];
    if (selectRoomSize != 0 && selectRoomSize != sizeof(ROOMPacket) && selectRoomSize != sizeof(SELECTPacket) &&
        selectRoomSize != sizeof(PulROOM) && selectRoomSize != sizeof(PulSELECT))
        return false;

    const u8 raceDataSize = packet.sizes[PACKET_RACEDATA];
    if (raceDataSize != 0 && raceDataSize != sizeof(PulRACEDATA) && raceDataSize != 2 * sizeof(PulRACEDATA)) return false;

    const u8 userSize = packet.sizes[PACKET_USER];
    if (userSize != 0 && userSize != sizeof(PulUSER)) return false;

    const u8 itemSize = packet.sizes[PACKET_ITEM];
    if (itemSize != 0 && itemSize != sizeof(PulITEM) && itemSize != 2 * sizeof(PulITEM)) return false;

    const u8 eventSize = packet.sizes[PACKET_EVENT];
    const u32 minEventSize = offsetof(EVENTPacket, entryData);
    if (eventSize != 0 && (eventSize < minEventSize || eventSize > sizeof(PulEVENT))) return false;

    u32 declaredSize = 0;
    for (u32 i = 0; i < 8; ++i) declaredSize += packet.sizes[i];
    if (declaredSize != receivedSize) return false;

    if (userSize != 0) {
        u32 userOffset = 0;
        for (u32 i = 0; i < PACKET_USER; ++i) userOffset += packet.sizes[i];
        if (userOffset > receivedSize || sizeof(PulUSER) > receivedSize - userOffset) return false;

        const PulUSER *user = reinterpret_cast<const PulUSER *>(reinterpret_cast<const u8 *>(&packet) + userOffset);
        // RFLiSetRecvPacket uses this network value as both a loop bound and a
        // memcpy multiplier into a fixed buffer. MKWii allocates exactly two Miis.
        if (user->rflPacket.maxMiiCount != 2) return false;
    }

    return true;
}

void *CreateSendAndRecvBuffers() {
    register RKNet::PacketHolder<void> *holder;
    register CustomRKNetController *controller;
    register u8 aid;
    asm(mr holder, r24;);
    asm(mr controller, r27;);
    asm(mr aid, r20;);

    controller->fullPulRecvPackets[aid] = new u8[totalRACESize];
    holder->bufferSize = totalRACESize;
    return new u8[totalRACESize];
}
kmCall(0x806570b4, CreateSendAndRecvBuffers);

// Buffer size must be the FULL size to accommodate any packet (including LapKO in friend rooms)
// The actual transmitted packet size is controlled dynamically in BeforeRH1Send based on context
kmWrite8(0x8089a19b, PulRH1SizeFull);
kmWrite8(0x8089a19f, sizeof(PulRH2));
kmWrite8(0x8089a1a3, sizeof(PulSELECT));
kmWrite8(0x8089a1a7, 2 * sizeof(PulRACEDATA));
kmWrite8(0x8089a1ab, sizeof(PulUSER));
kmWrite8(0x8089a1af, 2 * sizeof(PulITEM));
kmWrite8(0x8089a1b3, sizeof(PulEVENT));

void SetProperRecvBuffers(u8 aid, void *usualBuffer, u32 usualSize) {
    register CustomRKNetController *controller;
    asm(mr controller, r31;);
    memset(controller->fullPulRecvPackets[aid], 0, totalRACESize);
    DWC::SetRecvBuffer(aid, controller->fullPulRecvPackets[aid], totalRACESize);
}
kmWrite32(0x80658c78, 0x60000000);  // prevent usual memset
kmCall(0x80658c90, SetProperRecvBuffers);

void ProperRecvBuffersClear() {
    const CustomRKNetController *controller = reinterpret_cast<CustomRKNetController *>(RKNet::Controller::sInstance);
    for (int aid = 0; aid < 12; ++aid) memset(controller->fullPulRecvPackets[aid], 0, totalRACESize);
}
kmCall(0x8065607c, ProperRecvBuffersClear);

void CheckPacket(CustomRKNetController *controller, RKNet::RACEPacketHeader &packet, u32 size, u32 sizeUnused, u32 aid) {
    using namespace RKNet;

    (void)sizeUnused;
    if (controller == nullptr || aid >= 12) return;

    bool disconnect = !IsValidRACESectionSizes(packet, size);
    if (!disconnect) {
        const u32 recvCRC = packet.crc32;
        packet.crc32 = 0;
        const u32 calcCRC = OS::CalcCRC32(&packet, size);
        packet.crc32 = recvCRC;
        disconnect = recvCRC != calcCRC;
    }

    if (!disconnect) {
        u32 *lastUsedBufferAid = &controller->lastReceivedBufferUsed[aid][0];
        if (lastUsedBufferAid[0] >= 2) {
            disconnect = true;
        } else {
            SplitRACEPointers *recv = controller->splitReceivedRACEPackets[lastUsedBufferAid[0]][aid];
            if (recv == nullptr) {
                disconnect = true;
            } else {
                PacketHolder<void> **holderRecv = &recv->packetHolders[0];
                for (int i = 0; i < 8; ++i) {
                    const PacketHolder<void> *curHolder = holderRecv[i];  // starts at header etc...
                    const u8 curSize = packet.sizes[i];  // transmitted in packet
                    if (curHolder == nullptr || curHolder->bufferSize < curSize) disconnect = true;
                }
            }
        }
    }
    // if a non-host sends a invalid packet, each console will close the connection to that player,
    // this also means the host will close the connection to the player and it will cause them to be disconnected from the room

    // however if the person sending the invalid packet is the host,
    // every console will independently close the connection to the host

    if (disconnect)
        controller->toDisconnectAids |= 1 << aid;
    else
        reinterpret_cast<RKNet::Controller *>(controller)->ProcessRACEPacket(aid, packet, size);
}
kmBranch(0x80658608, CheckPacket);

bool DisconnectBadAids() {
    register CustomRKNetController *controller;
    asm(mr controller, r15;);

    int old = OS::DisableInterrupts();
    for (int aid = 0; aid < 12; ++aid) {
        if ((controller->toDisconnectAids >> aid) & 1) DWC::CloseConnectionHard(aid);
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

ExpSELECTHandler *CreateRecvArr(ExpSELECTHandler *handler) {  // wiimmfi hook prevents a more natural hook...
    register RKNet::OnlineMode mode;
    asm(mr mode, r31;);
    handler->mode = mode;
    handler->receivedPackets = new PulSELECT[12];
    return handler;
}
kmCall(0x8065fec0, CreateRecvArr);

void DeleteSELECT(ExpSELECTHandler *handler) {
    delete[] handler->receivedPackets;
    delete reinterpret_cast<RKNet::SELECTHandler *>(handler);
}
kmCall(0x8065ff84, DeleteSELECT);

u8 GetLastRecvSECTIONSize(u8 aid, u8 sectionIdx) {
    const CustomRKNetController *controller = reinterpret_cast<CustomRKNetController *>(RKNet::Controller::sInstance);
    if (controller == nullptr || aid >= 12 || sectionIdx >= 8 || controller->fullPulRecvPackets[aid] == nullptr) return 0;
    RKNet::RACEPacketHeader *header = reinterpret_cast<RKNet::RACEPacketHeader *>(controller->fullPulRecvPackets[aid]);
    return header->sizes[sectionIdx];
}

}  // namespace Network
}  // namespace Pulsar
