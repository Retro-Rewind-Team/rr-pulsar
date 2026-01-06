#include <kamek.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/RKNet/RH1.hpp>
#include <MarioKartWii/GlobalFunctions.hpp>
#include <SlotExpansion/CupsConfig.hpp>
#include <PulsarSystem.hpp>
#include <Network/Network.hpp>
#include <Network/PacketExpansion.hpp>
#include <Gamemodes/ItemRain/ItemRain.hpp>

namespace Pulsar {
namespace Network {

// Fixes for spectating
void BeforeRH1Send(RKNet::PacketHolder<PulRH1>& packetHolder, PulRH1* packet, u32 len) {
    packetHolder.Copy(packet, len);

    if (System::sInstance->IsContext(PULSAR_CT)) {  // CHECK if this only exists in races
        packetHolder.packetSize += sizeof(Network::PulRH1) - sizeof(RKNet::RACEHEADER1Packet);  // this has been changed by copy so it's safe to do this
        packetHolder.packet->pulsarTrackId = static_cast<u16>(CupsConfig::sInstance->GetWinning());
        packetHolder.packet->variantIdx = CupsConfig::sInstance->GetCurVariantIdx();
    }

    // Pack ItemRain sync data if host
    if (ItemRain::IsHost() && (System::sInstance->IsContext(PULSAR_ITEMMODERAIN) || System::sInstance->IsContext(PULSAR_ITEMMODESTORM))) {
        packetHolder.packetSize = sizeof(Network::PulRH1);
        
        // Clear ItemRain fields first to avoid garbage data
        memset(packetHolder.packet->itemRainItems, 0, sizeof(packetHolder.packet->itemRainItems));
        
        ItemRain::ItemRainSyncData syncData;
        ItemRain::PackItemData(&syncData);
        packetHolder.packet->itemRainItemCount = syncData.itemCount;
        packetHolder.packet->itemRainSyncFrame = syncData.syncFrame;
        for (u32 i = 0; i < syncData.itemCount && i < ItemRain::MAX_RAIN_ITEMS_PER_PACKET; i++) {
            u8* dest = &packetHolder.packet->itemRainItems[i * 6];
            dest[0] = syncData.items[i].itemObjId;
            dest[1] = syncData.items[i].targetPlayer;
            dest[2] = static_cast<u8>(syncData.items[i].forwardOffset >> 8);
            dest[3] = static_cast<u8>(syncData.items[i].forwardOffset & 0xFF);
            dest[4] = static_cast<u8>(syncData.items[i].rightOffset >> 8);
            dest[5] = static_cast<u8>(syncData.items[i].rightOffset & 0xFF);
        }
    } else {
        // Not host or ItemRain disabled - ensure ItemRain fields are cleared
        packetHolder.packet->itemRainItemCount = 0;
        packetHolder.packet->itemRainSyncFrame = 0;
    }
}
kmCall(0x80655458, BeforeRH1Send);
kmCall(0x806550e4, BeforeRH1Send);

static void AfterRH1Reception(register u8* aidArrDest, const RKNet::PacketHolder<PulRH1>& holder, u32 len) {
    register RKNet::RH1Data* data;
    register u8 senderAid;
    asm(subi data, aidArrDest, 0x20;);  // offset of the array in data
    asm(mr senderAid, r29;);  // r29 contains the current AID being processed in the loop

    const PulRH1* packet = holder.packet;
    const u32 packetSize = holder.packetSize;
    CourseId track;
    u8 variantIdx = 0;
    if (packetSize == sizeof(PulRH1))
        track = static_cast<CourseId>(packet->pulsarTrackId);
    else
        track = static_cast<CourseId>(packet->trackId);
    data->trackId = track;
    memcpy(aidArrDest, &packet->aidsBelongingToPlayerIds[0], len);

    // Process ItemRain sync data from host
    if (packetSize == sizeof(PulRH1) && !ItemRain::IsHost() &&
        (System::sInstance->IsContext(PULSAR_ITEMMODERAIN) || System::sInstance->IsContext(PULSAR_ITEMMODESTORM))) {
        
        RKNet::Controller* controller = RKNet::Controller::sInstance;
        if (controller) {
            const RKNet::ControllerSub& sub = controller->subs[controller->currentSub];
            
            // Only process ItemRain data if this packet is from the host
            if (senderAid == sub.hostAid && packet->itemRainItemCount > 0) {
                ItemRain::ItemRainSyncData syncData;
                syncData.itemCount = packet->itemRainItemCount;
                syncData.syncFrame = packet->itemRainSyncFrame;
                for (u32 i = 0; i < syncData.itemCount && i < ItemRain::MAX_RAIN_ITEMS_PER_PACKET; i++) {
                    const u8* src = &packet->itemRainItems[i * 6];
                    syncData.items[i].itemObjId = src[0];
                    syncData.items[i].targetPlayer = src[1];
                    syncData.items[i].forwardOffset = static_cast<s16>((src[2] << 8) | src[3]);
                    syncData.items[i].rightOffset = static_cast<s16>((src[4] << 8) | src[5]);
                }
                ItemRain::UnpackAndSpawn(&syncData, senderAid);
            }
        }
    }
}
kmCall(0x806652d0, AfterRH1Reception);

CourseId ReturnCorrectId(const RKNet::RH1Handler& rh1Handler) {
    CupsConfig* cupsConfig = CupsConfig::sInstance;
    const System* system = System::sInstance;
    for (int aid = 0; aid < 12; ++aid) {
        const RKNet::RH1Data& cur = rh1Handler.rh1Data[aid];
        const CourseId curTrack = cur.trackId;
        if (curTrack != 0xFFFFFFFF && (curTrack <= 0x42 || curTrack > 0xff) && cur.timer != 0) {
            PulsarId id;
            u8 variantIdx = 0;
            const RKNet::Controller* controller = RKNet::Controller::sInstance;
            const RKNet::RoomType roomType = controller->roomType;  // only ever called when joining (this is used to correct liveview), therefore simply checkings roomtype is enough

            if (roomType != RKNet::ROOMTYPE_VS_REGIONAL && roomType != RKNet::ROOMTYPE_JOINING_REGIONAL && roomType != RKNet::ROOMTYPE_BT_REGIONAL)
                id = CupsConfig::ConvertTrack_RealIdToPulsarId(curTrack);
            else {
                id = static_cast<PulsarId>(curTrack);
                const u32 lastBufferUsed = controller->lastReceivedBufferUsed[aid][RKNet::PACKET_RACEHEADER1];
                const RKNet::PacketHolder<Network::PulRH1>* holder = controller->splitReceivedRACEPackets[lastBufferUsed][aid]->GetPacketHolder<Network::PulRH1>();
                variantIdx = holder->packet->variantIdx;
            }
            cupsConfig->SetWinning(id, variantIdx);
            return cupsConfig->GetCorrectTrackSlot();
        }
    }
    return COURSEID_NONE;
}
kmBranch(0x80664560, ReturnCorrectId);

const u8* GetRH1aidArray(const RKNet::RH1Handler& rh1) {
    for (int i = 0; i < 12; ++i) {
        const RKNet::RH1Data& cur = rh1.rh1Data[i];
        const CourseId curTrack = cur.trackId;
        if (curTrack != 0xFFFFFFFF && (curTrack <= 0x42 || curTrack > 0xff)) return &cur.aidsBelongingToPlayer[0];
    }
    return nullptr;
}
kmBranch(0x80664b34, GetRH1aidArray);

static bool IsThereAValidId() {
    const RKNet::RH1Handler* rh1 = RKNet::RH1Handler::sInstance;
    bool isValid = false;
    for (int i = 0; i < 12; ++i) {
        const RKNet::RH1Data& cur = rh1->rh1Data[i];
        const CourseId curTrack = cur.trackId;
        if (curTrack != 0xFFFFFFFF && (curTrack <= 0x42 || curTrack > 0xff)) {
            isValid = true;
            break;
        }
    }
    return isValid;
}
kmCall(0x806643a4, IsThereAValidId);
kmCall(0x80664080, IsThereAValidId);
kmWrite32(0x80664084, 0x2C030000);  // change comparison from r0 to r3

kmWrite32(0x8066529c, 0x60000000);  // preserve packetholder in r4
// kmWrite32(0x806652b4, 0x60000000); //nop track store

}  // namespace Network
}  // namespace Pulsar