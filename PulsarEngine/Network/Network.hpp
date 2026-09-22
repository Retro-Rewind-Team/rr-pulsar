#ifndef _PUL_NETWORK_
#define _PUL_NETWORK_

#include <Config.hpp>
#include <Network/Settings/SelectionRestrictions.hpp>

namespace Pulsar {
namespace Network {

static const u32 MAX_TRACK_BLOCKING = 12;  // Maximum number of blocked tracks synced via packets

static const u32 HOST_SETTINGS_PREVIEW_COUNT = 27;

enum DenyType {
    DENY_TYPE_NORMAL,
    DENY_TYPE_BAD_PACK,
    DENY_TYPE_OTT,
    DENY_TYPE_KICK,
};

class Mgr {  // Manages network related stuff within Pulsar
public:
    Mgr() : racesPerGP(3), curBlockingArrayIdx(0), lastGroupedTrackPlayed(false), region(0x0A), customItemsBitfield(0x7FFFF), customEngineClass(150), hostCustomEngineClass(0), characterRestrictionMask(Restrictions::ALL_CHARACTERS), hasHostSettingsPreview(false) {
        for (u32 i = 0; i < Restrictions::VEHICLE_WEIGHT_COUNT; ++i) vehicleRestrictionMasks[i] = Restrictions::ALL_VEHICLES;
    }
    u32 hostContext;
    u32 hostContext2;
    u32 customItemsBitfield;
    DenyType denyType;
    u8 deniesCount;
    u8 ownStatusData;
    u8 statusDatas[30];
    u8 curBlockingArrayIdx;
    u8 racesPerGP;
    bool lastGroupedTrackPlayed;  // Whether the most recent blocked track was a grouped track
    u8 padding[1];
    u32 region;
    PulsarId *lastTracks;
    u16 customEngineClass;
    u16 hostCustomEngineClass;
    u32 characterRestrictionMask;
    u16 vehicleRestrictionMasks[Restrictions::VEHICLE_WEIGHT_COUNT];
    u8 hostSettingsPreview[HOST_SETTINGS_PREVIEW_COUNT];
    bool hasHostSettingsPreview;
};

}  // namespace Network
}  // namespace Pulsar

#endif
