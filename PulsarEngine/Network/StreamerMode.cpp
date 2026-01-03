#include <kamek.hpp>
#include <core/rvl/RFL/RFL.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/RKNet/USER.hpp>
#include <MarioKartWii/RKNet/SELECT.hpp>
#include <MarioKartWii/RKNet/FriendMgr.hpp>
#include <MarioKartWii/System/random.hpp>
#include <Settings/Settings.hpp>
#include <PulsarSystem.hpp>
#include <Network/PacketExpansion.hpp>

namespace Pulsar {
namespace Network {

u32 streamerModeRandomIndex = 0;
static bool trackDecidedTriggered = false;

static void RerandomizeBaseIndex() {
    Random random;
    streamerModeRandomIndex = random.NextLimited(6);
    trackDecidedTriggered = false;
}
static SectionLoadHook RerandomizeHook(RerandomizeBaseIndex);

static void UpdateStreamerMiis() {
    RKNet::Controller* controller = RKNet::Controller::sInstance;

    Random random;
    streamerModeRandomIndex = random.NextLimited(6);

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

extern "C" void* sInstance__Q23DWC12MatchControl;

static u32 GetPidForAid(u32 aid) {
    u8* stpMatchCnt = (u8*)sInstance__Q23DWC12MatchControl;
    if (stpMatchCnt == nullptr) return 0;

    u32 numHost = *(u32*)(stpMatchCnt + 0x30);
    for (u32 i = 0; i < numHost; i++) {
        u8* node = stpMatchCnt + 0x38 + i * 0x30;
        if (*(node + 0x16) == aid) {
            return *(u32*)node;
        }
    }
    return 0;
}

static bool IsFriend(u32 pid) {
    if (pid == 0) return false;
    RKNet::FriendMgr* friendMgr = RKNet::FriendMgr::sInstance;
    if (friendMgr == nullptr) return false;

    for (int i = 0; i < 30; i++) {
        if (friendMgr->friendPids[i] == pid) {
            return true;
        }
    }
    return false;
}

static void ReplaceWithRandomPlayerMii(RKNet::USERHandler* handler, u32 aid, RKNet::USERPacket* userPacket) {
    const Settings::Mgr& settings = Settings::Mgr::Get();
    if (settings.GetUserSettingValue(Settings::SETTINGSTYPE_ONLINE, RADIO_STREAMERMODE) == STREAMERMODE_DISABLED) {
        return;
    }
    if (RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_HOST || RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_FROOM_NONHOST) {
        return;
    }

    // Check if the person we are sending to is a friend or ourselves
    u32 pid = GetPidForAid(aid);
    if (pid != 0) {
        if (IsFriend(pid)) return;

        u8* stpMatchCnt = (u8*)sInstance__Q23DWC12MatchControl;
        if (stpMatchCnt && pid == *(u32*)(stpMatchCnt + 0x8a8)) return;
    }

    u32 playerRandomIndex = (streamerModeRandomIndex + aid) % 6;

    RFL::StoreData* miiSlot0 = &userPacket->rflPacket.rawMiis[0];
    RFL::StoreData* miiSlot1 = &userPacket->rflPacket.rawMiis[1];

    RFL::GetStoreData(miiSlot0, RFL::RFLDataSource_Default, playerRandomIndex);
    RFL::GetStoreData(miiSlot1, RFL::RFLDataSource_Default, playerRandomIndex);
}

static void CopySendToPacketHolderHook(RKNet::USERHandler* handler, u32 aid) {
    if (!handler->isInitialized) return;

    RKNet::USERPacket packet = handler->toSendPacket;
    ReplaceWithRandomPlayerMii(handler, aid, &packet);

    RKNet::Controller* controller = RKNet::Controller::sInstance;
    u32 bufferIdx = controller->lastSendBufferUsed[aid];
    RKNet::SplitRACEPointers* splitPointers = controller->splitToSendRACEPackets[bufferIdx][aid];
    RKNet::PacketHolder<RKNet::USERPacket>* holder = splitPointers->GetPacketHolder<RKNet::USERPacket>();

    holder->Copy(&packet, sizeof(RKNet::USERPacket));
}
kmBranch(0x80662abc, CopySendToPacketHolderHook);

}  // namespace Network
}  // namespace Pulsar