#include <kamek.hpp>
#include <core/rvl/RFL/RFL.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/RKNet/USER.hpp>
#include <MarioKartWii/RKNet/SELECT.hpp>
#include <MarioKartWii/System/random.hpp>
#include <Settings/Settings.hpp>
#include <PulsarSystem.hpp>
#include <Network/PacketExpansion.hpp>

namespace Pulsar {
namespace Network {

static u32 baseRandomIndex = 0;
static bool trackDecidedTriggered = false;

static void RerandomizeBaseIndex() {
    Random random;
    baseRandomIndex = random.NextLimited(6);
    trackDecidedTriggered = false;
}
static SectionLoadHook RerandomizeHook(RerandomizeBaseIndex);

static void UpdateStreamerMiis() {
    RKNet::Controller* controller = RKNet::Controller::sInstance;

    Random random;
    baseRandomIndex = random.NextLimited(6);

    RKNet::USERHandler* userHandler = RKNet::USERHandler::sInstance;
    if (userHandler) {
        userHandler->aidsThatHaveGivenMiis = 0;

        userHandler->CreateSendPacket();
        const u32 currentSub = controller->currentSub;
        for (u32 aid = 0; aid < 12; ++aid) {
            if (controller->subs[currentSub].availableAids & (1 << aid)) {
                userHandler->CopySendToPacketHolder(aid);
            }
        }
    }

    ExpSELECTHandler& selectHandler = ExpSELECTHandler::Get();
    selectHandler.lastSentTime = 0;
}

static CourseId OnTrackDecidedHook(RKNet::SELECTHandler* handler) {
    CourseId course = handler->GetWinningCourse();
    if (course != 0xFF && !trackDecidedTriggered) {
        UpdateStreamerMiis();
        trackDecidedTriggered = true;
    }
    return course;
}
kmCall(0x80644318, OnTrackDecidedHook);

static void ReplaceWithRandomPlayerMii(RKNet::USERHandler* handler, u32 aid) {
    const Settings::Mgr& settings = Settings::Mgr::Get();
    if (settings.GetUserSettingValue(Settings::SETTINGSTYPE_ONLINE, RADIO_STREAMERMODE) == STREAMERMODE_DISABLED) {
        return;
    }
    if (RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_HOST || RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_NONHOST) {
        return;
    }

    u32 playerRandomIndex = (baseRandomIndex + aid) % 6;

    RFL::StoreData* miiSlot0 = &handler->toSendPacket.rflPacket.rawMiis[0];
    RFL::StoreData* miiSlot1 = &handler->toSendPacket.rflPacket.rawMiis[1];

    RFL::GetStoreData(miiSlot0, RFL::RFLDataSource_Default, playerRandomIndex);
    RFL::GetStoreData(miiSlot1, RFL::RFLDataSource_Default, playerRandomIndex);
}

static void CopySendToPacketHolderHook(RKNet::USERHandler* handler, u32 aid) {
    ReplaceWithRandomPlayerMii(handler, aid);
    if (!handler->isInitialized) return;

    RKNet::Controller* controller = RKNet::Controller::sInstance;
    u32 bufferIdx = controller->lastSendBufferUsed[aid];
    RKNet::SplitRACEPointers* splitPointers = controller->splitToSendRACEPackets[bufferIdx][aid];
    RKNet::PacketHolder<RKNet::USERPacket>* holder = splitPointers->GetPacketHolder<RKNet::USERPacket>();

    holder->Copy(&handler->toSendPacket, sizeof(RKNet::USERPacket));
}
kmBranch(0x80662abc, CopySendToPacketHolderHook);

}  // namespace Network
}  // namespace Pulsar