#include <RetroRewind.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/UI/Ctrl/UIControl.hpp>
#include <Network/MatchCommand.hpp>
#include <MarioKartWii/UI/Page/Menu/KartSelect.hpp>

// Original code from VP, adapted to Pulsar 2.0.
namespace RetroRewind {
namespace UI {

static bool IsFriendRoom() {
    const RKNet::RoomType roomType = RKNet::Controller::sInstance->roomType;
    return roomType == RKNet::ROOMTYPE_FROOM_HOST || roomType == RKNet::ROOMTYPE_FROOM_NONHOST;
}

u8 RestrictKartSelection() {
    SectionMgr::sInstance->sectionParams->kartsDisplayType = 2;
    Pulsar::KartRestriction kartRest = Pulsar::KART_DEFAULTSELECTION;
    Pulsar::KartRestriction bikeRest = Pulsar::KART_DEFAULTSELECTION;

    if (IsFriendRoom()) {
        kartRest = System::sInstance->IsContext(Pulsar::PULSAR_KARTRESTRICT) ? Pulsar::KART_KARTONLY : Pulsar::KART_DEFAULTSELECTION;
        bikeRest = System::sInstance->IsContext(Pulsar::PULSAR_BIKERESTRICT) ? Pulsar::KART_BIKEONLY : Pulsar::KART_DEFAULTSELECTION;
    }

    if (kartRest == Pulsar::KART_KARTONLY) {
        SectionMgr::sInstance->sectionParams->kartsDisplayType = 0;
    }
    if (bikeRest == Pulsar::KART_BIKEONLY) {
        SectionMgr::sInstance->sectionParams->kartsDisplayType = 1;
    }

    return SectionMgr::sInstance->sectionParams->kartsDisplayType;
}
kmCall(0x808455a4, RestrictKartSelection);
kmWrite32(0x808455a8, 0x907f06ec);

bool IsKartAccessible(KartId kart, u32 r4) {
    bool ret = IsKartUnlocked(kart, r4);
    Pulsar::KartRestriction kartRest = Pulsar::KART_DEFAULTSELECTION;
    Pulsar::KartRestriction bikeRest = Pulsar::KART_DEFAULTSELECTION;

    if (IsFriendRoom()) {
        kartRest = System::sInstance->IsContext(Pulsar::PULSAR_KARTRESTRICT) ? Pulsar::KART_KARTONLY : Pulsar::KART_DEFAULTSELECTION;
        bikeRest = System::sInstance->IsContext(Pulsar::PULSAR_BIKERESTRICT) ? Pulsar::KART_BIKEONLY : Pulsar::KART_DEFAULTSELECTION;
    }

    if ((kart < STANDARD_BIKE_S && bikeRest == Pulsar::KART_BIKEONLY) ||
        (kart >= STANDARD_BIKE_S && kartRest == Pulsar::KART_KARTONLY)) {
        ret = false;
    }

    return ret;
}
kmCall(0x8084a45c, IsKartAccessible);

}  // namespace UI
}  // namespace RetroRewind
