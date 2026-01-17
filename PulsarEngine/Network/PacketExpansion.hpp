#ifndef _PUL_NETWORK_EXPANSION_
#define _PUL_NETWORK_EXPANSION_

#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/RKNet/SELECT.hpp>
#include <MarioKartWii/RKNet/EVENT.hpp>
#include <MarioKartWii/RKNet/ROOM.hpp>
#include <MarioKartWii/RKNet/ITEM.hpp>
#include <MarioKartWii/RKNet/RACEDATA.hpp>
#include <MarioKartWii/RKNet/RH1.hpp>
#include <MarioKartWii/RKNet/RH2.hpp>
#include <MarioKartWii/RKNet/USER.hpp>

/*
Each section has its own way of transmitting the extra data for now:
-SELECT and ROOM use BeforeSend/AfterReception hooks, located at the Export/Import functions of the respective Handlers
-RH1 is done on the fly
*/

namespace Pulsar {
namespace Network {

#pragma pack(push, 1)
struct PulPlayerData {  // SELECT struct
    u16 pulCourseVote;  // 0x0 swapped with rank
    u16 sumPoints;  // 0x2
    u8 character;  // 0x4
    u8 kart;  // 0x5
    u8 prevRaceRank;  // 0x6 swapped with coursevote
    u8 starRank;  // 0x8 1st bit of 2nd p is also used to specify customPacket
};  // total size 0x8
// size_assert(PulPlayerData, 0x8);

struct PulRH1 : public RKNet::RACEHEADER1Packet {
    // Pulsar data (always sent)
    u16 pulsarTrackId;  // current
    u8 variantIdx;

    // HAW Vote (always sent)
    u8 chooseNextStatus;
    bool hasTrack;
    u16 nextTrack;  // PulsarId

    // These fields are only populated/read when their respective game modes are enabled
    // They are always present in the struct for memory layout, but zeroed when not in use

    // KOStats - only used when PULSAR_MODE_KO is enabled
    u16 timeInDanger;
    u8 almostKOdCounter;
    u8 finalPercentageSum;  // to be divided by racecount at the end of the GP

    // ItemRain sync - only used when PULSAR_ITEMMODERAIN or PULSAR_ITEMMODESTORM is enabled
    u8 itemRainItemCount;  // Number of valid items (0-4)
    u8 itemRainSyncFrame;  // Frame counter for ordering
    u8 itemRainItems[24];  // 4 items * 6 bytes each (itemObjId, targetPlayer, fwdOffset[2], rightOffset[2])

    // LapKO - only used when PULSAR_MODE_LAPKO is enabled AND in friend rooms
    // Must be at the END so we can conditionally expand packet size
    u8 lapKoSeq;
    u8 lapKoRoundIndex;
    u8 lapKoActiveCount;
    u8 lapKoElimCount;
    u8 lapKoElims[12];
};

// Size constants for conditional packet expansion
static const u32 PulRH1SizeBase = sizeof(PulRH1) - 16;  // Size without LapKO fields (16 bytes)
static const u32 PulRH1SizeFull = sizeof(PulRH1);  // Full size with LapKO fields

struct PulRH2 : public RKNet::RACEHEADER2Packet {};
struct PulROOM : public RKNet::ROOMPacket {
    // Generic ROOM settings
    u64 hostSystemContext;  // System's context but with just gamemodes taken from the settings
    u32 hostSystemContext2;
    u32 customItemsBitfield;
    u8 raceCount;

    // Extended Team settings
    u8 extendedTeams[6];  // 4 bits per PlayerIDX, they encode the team ID (4 * 12 = 48 bits = 6 bytes)

    // Track blocking
    u8 blockedTrackCount;  // Number of valid entries in blockedTracks (up to MAX_TRACK_BLOCKING)
    u8 curBlockingArrayIdx;  // Current write index in circular buffer
    bool lastGroupedTrackPlayed;  // Whether most recent track was a grouped track
    u8 padding;
    u16 blockedTracks[8];  // PulsarId array (up to MAX_TRACK_BLOCKING tracks)
};

enum SELECTComboStatus {
    SELECT_COMBO_DISABLED,
    SELECT_COMBO_ENABLED,
    SELECT_COMBO_SELECTING,
    SELECT_COMBO_SELECTED,
    SELECT_COMBO_WAITING_FOR_START,

    SELECT_COMBO_HOST_START,

};

struct PulSELECT : public RKNet::SELECTPacket {
    u16 pulVote;  // 0x38 no need for 2, they're guaranteed to be the same
    u16 pulWinningTrack;  // 0x3a
    u8 variantIdx;  // 0x3c

    /*Sole reason these are here and not in ROOM is for easy additions of these gamemodes to public rooms (regionals) since ROOM does not exist there*/
    // OTT Settings
    u8 allowChangeComboStatus;

    // KOSettings
    u8 koPerRace;
    u8 racesPerKO;
    bool alwaysFinal;

    u8 decimalVR[2];

    u8 voteVariantIdx[2];

    // Track blocking sync for regional rooms (late joiner support)
    u8 blockedTrackCount;  // Number of valid entries in blockedTracks
    u8 curBlockingArrayIdx;  // Current write index in circular buffer
    bool lastGroupedTrackPlayed;  // Whether most recent track was a grouped track
    u8 blockingPadding;
    u16 blockedTracks[8];  // PulsarId array (up to MAX_TRACK_BLOCKING tracks)
};

struct PulRACEDATA : public RKNet::RACEDATAPacket {};
struct PulUSER : public RKNet::USERPacket {};
struct PulITEM : public RKNet::ITEMPacket {};
struct PulEVENT : public RKNet::EVENTPacket {};  // NOT RECOMMENDED as this has variable length
#pragma pack(pop)

static const u32 totalRACESize = sizeof(RKNet::RACEPacketHeader) + sizeof(PulRH1) + sizeof(PulRH2) + sizeof(PulSELECT) + 2 * sizeof(PulRACEDATA) + sizeof(PulUSER) + 2 * sizeof(PulITEM) + sizeof(PulEVENT);

class CustomRKNetController {  // Exists to make received packets a pointer array so that the size can be variable
   public:
    // static CustomRKNetController* Get() { return reinterpret_cast<CustomRKNetController*>(RKNet::Controller::sInstance); }
    virtual ~CustomRKNetController();  // 8065741c vtable 808c097c

    u32 unkVtable;  // unknown class vtable 808c0988
    OS::Mutex mutex;  // 0x8

    EGG::Heap* Heap;  // 0x20
    EGG::TaskThread* taskThread;  // 0x24
    RKNet::ConnectionState connectionState;  // 0x28

    RKNet::ErrorParams errorParams;  // 0x2c

    u32 unknown_0x34;

    RKNet::ControllerSub subs[2];  // 0x38
    RKNet::RoomType roomType;  // 0xe8
    u8 unknown_0xec[4];

    RKNet::SplitRACEPointers* splitToSendRACEPackets[2][12];  // 0xf0 split pointers for the outgoing packets, double buffered, indexed by aid
    RKNet::SplitRACEPointers* splitReceivedRACEPackets[2][12];  // 0x150 split pointers for the incoming packets, double buffered, indexed by aid
    RKNet::PacketHolder<void>* fullSendPackets[12];  // 0x1b0 combined outgoing packets, indexed by aid
    u64 lastRACEToSendTimes[12];  // 0x1e0 time when last sent to that aid
    u64 lastRACERecivedTimes[12];  // 0x240 time when last received from that aid
    u64 RACEToSendTimesTaken[12];  // 0x2a0 last send time minus the time of the send before it
    u64 RACEReceivedTimesTaken[12];  // 0x300 last receive time minus the time of the receive before it
    u8 lastRACESendAid;  // 0x360 aid a packet was last sent to

    // MODIFICATIONS
    u8 defaultBuffer[0x25b0 - 0x361];  // 0x361 1 per aid
    u32 toDisconnectAids;  // 0x25b0 for wiimmfi only?
    void* fullPulRecvPackets[12];  // 0x25b4
    // END

    RKNet::StatusData localStatusData;  // 0x25e4 8 bytes, see http://wiki.tockdom.com/wiki/MKWii_Network_Protocol/Server/gpcm.gs.nintendowifi.net#locstring
    RKNet::Friend friends[30];  // 0x25ec
    bool friendsListIsChanged;  // 0x2754 true if unprocessed changes have happeend
    bool shutdownScheduled;  // 0x2755 will cause shutdown of wifi connection on next run of the main loop if true
    bool friendStatusUpdateScheduled;  // 0x2756 if true, the main loop will update all friend statuses on the next iteration
    bool nwc24Related;  // 0x2757
    bool hasprofanity;  // 0x2758 1 if name is bad
    u8 padding2[3];
    s32 badWordsNum;  // 0x275c number of bad strings in the profanity check
    u8 unknown_0x2760[4];
    s32 vr;  // 0x2764
    s32 br;  // 0x2768
    u32 lastSendBufferUsed[12];  // 0x276c last full send buffer used for each aid, 0 or 1
    u32 lastReceivedBufferUsed[12][8];  // 0x279c last Received buffer used for each packet per aid, 1 or 0
    s32 currentSub;  // 0x291c index of the current sub to use, 0 or 1
    u8 aidsBelongingToPlayerIds[12];  // 0x2920 index is player id, value is aid
    u32 disconnectedAids;  // 0x292c 1 if disconnected, index 1 << aid
    u32 disconnectedPlayerIds;  // 0x2930 index 1 << playerId
    u8 unknown_0x2934[0x29c8 - 0x2934];
};

static_assert(sizeof(PulROOM) < sizeof(PulSELECT), "ROOM SELECT");

class ExpSELECTHandler {
   public:
    static ExpSELECTHandler& Get() { return *reinterpret_cast<ExpSELECTHandler*>(RKNet::SELECTHandler::sInstance); };
    static void DecideTrack(ExpSELECTHandler& self);

    // Get the vote variant index for a specific player
    u8 GetVoteVariantIdx(u8 aid, u8 hudSlotId) const;

    RKNet::OnlineMode mode;  // from page 0x90 OnInit SectionId Switch
    u32 unknown_0x4;
    PulSELECT toSendPacket;  // 0x8
    PulSELECT* receivedPackets;  // sizeof(PulSELECT) + 0x8

    u8 padding[0x2e0 - (sizeof(PulSELECT) + 0x8 + 4)];

    u8 lastSentToAid;  // 0x2e0
    u8 unknown_0x2e4[7];
    u64 lastSentTime;  // 0x2e8
    u64 lastReceivedTimes[12];  // 0x2f0
    u64 unknown_0x350[12];  // 0x350
    u32 unknown_0x3b0[12];  // 0x3b0
    u32 hasNewSELECT;  // 0x3e0 bitflag
    u32 hasNewRACEHEADER_1;  // 0x3e4 bitflag
    u32 aidsWithAccurateRaceSettings;  // 0x3e8
    u32 aidsWithAccurateAidPidMap;  // 0x3ec
    u32 aidsThatHaveVoted;  // 0x3f0
    u8 unknown_0x3F4[4];
};

u8 GetLastRecvSECTIONSize(u8 aid, u8 sectionIdx);

}  // namespace Network
}  // namespace Pulsar

#endif