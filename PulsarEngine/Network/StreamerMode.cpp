#include <kamek.hpp>
#include <core/rvl/RFL/RFL.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/RKNet/USER.hpp>
#include <MarioKartWii/RKNet/SELECT.hpp>
#include <MarioKartWii/RKNet/FriendMgr.hpp>
#include <MarioKartWii/Mii/Mii.hpp>
#include <include/c_string.h>
#include <MarioKartWii/System/random.hpp>
#include <Settings/Settings.hpp>
#include <PulsarSystem.hpp>
#include <Network/PacketExpansion.hpp>
#include <GlobalFunction.hpp>

namespace Pulsar {
namespace Network {

u32 streamerModeRandomIndex = 0;
static bool trackDecidedTriggered = false;
static wchar_t fakeMiiName[11];
static RFL::StoreData s_originalMiis[2];
static bool s_originalMiisStored = false;

static bool IsStreamerModeActiveForServer() {
    const Settings::Mgr &settings = Settings::Mgr::Get();
    if (settings.GetSettingValue(Pulsar::Settings::SETTING_STREAMERMODE) == STREAMERMODE_DISABLED) {
        return false;
    }
    RKNet::Controller *controller = RKNet::Controller::sInstance;
    if (controller == nullptr)
        return false;
    if (IsFriendRoom()) {
        return false;
    }
    return true;
}

static void GetOriginalMiis(RFL::StoreData *outMii0, RFL::StoreData *outMii1) {
    if (s_originalMiisStored) {
        *outMii0 = s_originalMiis[0];
        *outMii1 = s_originalMiis[1];
    }
}

static void ReplaceUserPacketMiiForServer(RKNet::USERHandler *handler) {
    if (!IsStreamerModeActiveForServer()) {
        s_originalMiisStored = false;
        return;
    }

    s_originalMiis[0] = handler->toSendPacket.rflPacket.rawMiis[0];
    s_originalMiis[1] = handler->toSendPacket.rflPacket.rawMiis[1];
    s_originalMiisStored = true;

    u32 randomIdx = streamerModeRandomIndex;
    RFL::StoreData *miiSlot0 = &handler->toSendPacket.rflPacket.rawMiis[0];
    RFL::StoreData *miiSlot1 = &handler->toSendPacket.rflPacket.rawMiis[1];

    RFL::GetStoreData(miiSlot0, RFL::RFLDataSource_Default, randomIdx);
    RFL::GetStoreData(miiSlot1, RFL::RFLDataSource_Default, randomIdx);
}

asmFunc AsmHook_AfterMiiCopyLoop() {
    ASM(
        nofralloc;
        // The caller keeps the USER handler in r31.
        stwu    r1, -0x20(r1);
        mflr    r12;
        stw     r12, 0x24(r1);
        stw     r3, 0x8(r1);
        stw     r4, 0xc(r1);
        stw     r30, 0x10(r1);

        mr      r3, r31;
        bl      ReplaceUserPacketMiiForServer;

        lwz     r30, 0x10(r1);
        lwz     r4, 0xc(r1);
        lwz     r3, 0x8(r1);
        lwz     r12, 0x24(r1);
        addi    r1, r1, 0x20;
        mtlr    r12;

        // Execute the replaced instruction: lis r4, -0x7f64 (0x80a0)
        lis     r4, -0x7f64;

        blr;
    )
}
kmCall(0x80663088, AsmHook_AfterMiiCopyLoop);

static wchar_t *GetLoginMiiName(Mii *mii) {
    if (IsStreamerModeActiveForServer()) {
        RFL::StoreData tempMii;
        u32 randomIdx = streamerModeRandomIndex;
        RFL::GetStoreData(&tempMii, RFL::RFLDataSource_Default, randomIdx);
        memcpy(fakeMiiName, tempMii.miiName, sizeof(fakeMiiName));
        return fakeMiiName;
    }
    return mii->info.name;
}

asmFunc AsmHook_BeforeDWCLoginAsync() {
    ASM(
        nofralloc;

        stwu    r1, -0x30(r1);
        mflr    r0;
        stw     r0, 0x34(r1);
        // Save registers that DWC_LoginAsync needs
        stw     r4, 0x10(r1);
        stw     r5, 0x14(r1);
        stw     r6, 0x18(r1);

        // r3 already has the Mii pointer
        bl      GetLoginMiiName;

        // Restore registers for DWC_LoginAsync
        lwz     r4, 0x10(r1);
        lwz     r5, 0x14(r1);
        lwz     r6, 0x18(r1);
        lwz     r0, 0x34(r1);
        mtlr    r0;
        addi    r1, r1, 0x30;

        blr;
    )
}
kmCall(0x80658cd8, AsmHook_BeforeDWCLoginAsync);

static void RerandomizeBaseIndex() {
    Random random;
    streamerModeRandomIndex = random.NextLimited(6);
    trackDecidedTriggered = false;
}
static SectionLoadHook RerandomizeHook(RerandomizeBaseIndex);

static void UpdateStreamerMiis() {
    RKNet::Controller *controller = RKNet::Controller::sInstance;

    Random random;
    streamerModeRandomIndex = random.NextLimited(6);

    RKNet::USERHandler *userHandler = RKNet::USERHandler::sInstance;
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

    ExpSELECTHandler &selectHandler = ExpSELECTHandler::Get();
    selectHandler.lastSentTime = 0;
}

static CourseId OnTrackDecidedHook(RKNet::SELECTHandler *handler) {
    CourseId course = handler->GetWinningCourse();
    if (course != 0xFF && !trackDecidedTriggered) {
        UpdateStreamerMiis();
        trackDecidedTriggered = true;
    }
    return course;
}
kmCall(0x80644318, OnTrackDecidedHook);

extern "C" void *sInstance__Q23DWC12MatchControl;

static u32 GetPidForAid(u32 aid) {
    u8 *stpMatchCnt = (u8 *)sInstance__Q23DWC12MatchControl;
    if (stpMatchCnt == nullptr)
        return 0;

    u32 numHost = *(u32 *)(stpMatchCnt + 0x30);
    for (u32 i = 0; i < numHost; i++) {
        u8 *node = stpMatchCnt + 0x38 + i * 0x30;
        if (*(node + 0x16) == aid) {
            return *(u32 *)node;
        }
    }
    return 0;
}

static bool IsFriend(u32 pid) {
    if (pid == 0)
        return false;
    RKNet::FriendMgr *friendMgr = RKNet::FriendMgr::sInstance;
    if (friendMgr == nullptr)
        return false;

    for (int i = 0; i < 30; i++) {
        if (friendMgr->friendPids[i] == pid) {
            return true;
        }
    }
    return false;
}

static void ReplaceWithRandomPlayerMii(RKNet::USERHandler *handler, u32 aid, RKNet::USERPacket *userPacket) {
    const Settings::Mgr &settings = Settings::Mgr::Get();
    if (settings.GetSettingValue(Pulsar::Settings::SETTING_STREAMERMODE) == STREAMERMODE_DISABLED) {
        return;
    }
    if (IsFriendRoom()) {
        return;
    }

    u32 pid = GetPidForAid(aid);
    if (pid != 0) {
        if (IsFriend(pid)) {
            GetOriginalMiis(&userPacket->rflPacket.rawMiis[0], &userPacket->rflPacket.rawMiis[1]);
            return;
        }

        u8 *stpMatchCnt = (u8 *)sInstance__Q23DWC12MatchControl;
        if (stpMatchCnt && pid == *(u32 *)(stpMatchCnt + 0x8a8)) {
            GetOriginalMiis(&userPacket->rflPacket.rawMiis[0], &userPacket->rflPacket.rawMiis[1]);
            return;
        }
    }

    u32 playerRandomIndex = (streamerModeRandomIndex + aid) % 6;

    RFL::StoreData *miiSlot0 = &userPacket->rflPacket.rawMiis[0];
    RFL::StoreData *miiSlot1 = &userPacket->rflPacket.rawMiis[1];

    RFL::GetStoreData(miiSlot0, RFL::RFLDataSource_Default, playerRandomIndex);
    RFL::GetStoreData(miiSlot1, RFL::RFLDataSource_Default, playerRandomIndex);
}

static void CopySendToPacketHolderHook(RKNet::USERHandler *handler, u32 aid) {
    if (!handler->isInitialized)
        return;

    RKNet::USERPacket packet = handler->toSendPacket;
    ReplaceWithRandomPlayerMii(handler, aid, &packet);

    RKNet::Controller *controller = RKNet::Controller::sInstance;
    u32 bufferIdx = controller->lastSendBufferUsed[aid];
    RKNet::SplitRACEPointers *splitPointers = controller->splitToSendRACEPackets[bufferIdx][aid];
    RKNet::PacketHolder<RKNet::USERPacket> *holder = splitPointers->GetPacketHolder<RKNet::USERPacket>();

    holder->Copy(&packet, sizeof(RKNet::USERPacket));
}
kmBranch(0x80662abc, CopySendToPacketHolderHook);

}  // namespace Network
}  // namespace Pulsar
