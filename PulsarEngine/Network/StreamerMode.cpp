#include <kamek.hpp>
#include <core/rvl/RFL/RFL.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/RKNet/USER.hpp>
#include <MarioKartWii/System/random.hpp>
#include <Settings/Settings.hpp>
#include <PulsarSystem.hpp>

namespace Pulsar {
namespace Network {

// Hook after buildUserPacket to replace Mii data with random player Mii
static void ReplaceWithRandomPlayerMii(RKNet::USERHandler* handler) {
    const Settings::Mgr& settings = Settings::Mgr::Get();
    if (settings.GetUserSettingValue(Settings::SETTINGSTYPE_ONLINE, RADIO_STREAMERMODE) == STREAMERMODE_DISABLED) {
        return;
    }
    if (RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_HOST || RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_NONHOST) {
        return;
    }

    Random random;
    RFL::StoreData* miiSlot0 = &handler->toSendPacket.rflPacket.rawMiis[0];
    RFL::StoreData* miiSlot1 = &handler->toSendPacket.rflPacket.rawMiis[1];

    u32 randomIndex = random.NextLimited(6);
    RFL::GetStoreData(miiSlot0, RFL::RFLDataSource_Default, randomIndex);
    RFL::GetStoreData(miiSlot1, RFL::RFLDataSource_Default, randomIndex);
}

static void BuildUserPacketHook() {
    RKNet::USERHandler* handler = RKNet::USERHandler::sInstance;
    if (handler != nullptr) {
        ReplaceWithRandomPlayerMii(handler);
    }
}
kmBranch(0x80663190, BuildUserPacketHook);

}  // namespace Network
}  // namespace Pulsar