#include <kamek.hpp>
#include <MarioKartWii/Kart/KartLink.hpp>
#include <MarioKartWii/RKNet/RH1.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/RKNet/SELECT.hpp>
#include <MarioKartWii/RKNet/User.hpp>
#include <MarioKartWii/UI/Page/Other/SELECTStageMgr.hpp>
#include <core/rvl/DWC/DWCCore.hpp>
#include <core/rvl/OS/OS.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>
#include <runtimeWrite.hpp>

extern "C" u32 DWC_GetAidBitmap();
extern "C" BOOL DWC_IsValidAid(u8 aid);

namespace Pulsar {
namespace Network {

static const u32 PHANTOM_AID_TIMEOUT_SECONDS = 2;
static u32 s_phantomAids = 0;
static u32 s_phantomConnectionUserDatas = 0;
static RKNet::ConnectionUserData s_connectionUserDatas[12];
static u32 s_phantomUSERPackets = 0;
static RKNet::USERPacket s_userPackets[12];

static void ClearPhantomAidData(u32 aid) {
    const u32 aidBit = 1 << aid;
    s_phantomAids &= ~aidBit;
    s_phantomConnectionUserDatas &= ~aidBit;
    s_phantomUSERPackets &= ~aidBit;
}

static void ClearAllPhantomAids() {
    s_phantomAids = 0;
    s_phantomConnectionUserDatas = 0;
    s_phantomUSERPackets = 0;
}

void MarkPhantomAid(u32 aid) {
    if (aid >= 12) return;
    const u32 aidBit = 1 << aid;
    s_phantomAids |= aidBit;

    RKNet::Controller* controller = RKNet::Controller::sInstance;
    if (controller != nullptr) {
        const RKNet::ControllerSub& curSub = controller->subs[controller->currentSub];
        const RKNet::ControllerSub& prevSub = controller->subs[controller->currentSub ^ 1];
        const RKNet::ConnectionUserData& connectionUserData =
            curSub.connectionUserDatas[aid].playersAtConsole != 0 ? curSub.connectionUserDatas[aid] : prevSub.connectionUserDatas[aid];
        if (connectionUserData.playersAtConsole != 0) {
            s_connectionUserDatas[aid] = connectionUserData;
            s_phantomConnectionUserDatas |= aidBit;
        }
    }

    RKNet::USERHandler* userHandler = RKNet::USERHandler::sInstance;
    if (userHandler != nullptr && userHandler->isInitialized) {
        s_userPackets[aid] = userHandler->receivedPackets[aid];
        s_phantomUSERPackets |= aidBit;
    }
}

void ClearPhantomAid(u32 aid) {
    if (aid >= 12) return;
    ClearPhantomAidData(aid);
}

bool ShouldPreservePhantomAid(u32 aid) {
    if (aid >= 12) return false;

    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    if (controller == nullptr) return false;

    const RKNet::ControllerSub& sub = controller->subs[controller->currentSub];
    if (sub.localAid == sub.hostAid || aid == sub.hostAid) return false;

    return (s_phantomAids & (1 << aid)) != 0;
}

static void ExpireStalePhantomAids(RKNet::Controller* controller) {
    const u32 phantomAids = s_phantomAids;
    if (phantomAids == 0) return;

    const u64 now = OS::GetTime();
    const u64 timeout = static_cast<u64>(OS::GetTimerClock()) * PHANTOM_AID_TIMEOUT_SECONDS;
    for (u32 aid = 0; aid < 12; ++aid) {
        const u32 aidBit = 1 << aid;
        if ((phantomAids & aidBit) == 0) continue;

        const u64 lastReceived = controller->lastRACERecivedTimes[aid];
        if (lastReceived != 0 && now - lastReceived <= timeout) continue;

        ClearPhantomAidData(aid);
        controller->ProcessPlayerDisconnect(aid);
    }
}

static u32 GetAidBitmapWithPhantomAids() {
    u32 aidBitmap = DWC_GetAidBitmap();

    RKNet::Controller* controller = RKNet::Controller::sInstance;
    if (controller == nullptr) {
        ClearAllPhantomAids();
        return aidBitmap;
    }

    const RKNet::ControllerSub& sub = controller->subs[controller->currentSub];
    const u32 localAidBit = 1 << sub.localAid;
    const u32 hostAidBit = 1 << sub.hostAid;
    if ((aidBitmap & localAidBit) == 0 || (aidBitmap & hostAidBit) == 0) {
        ClearAllPhantomAids();
        return aidBitmap;
    }

    ClearPhantomAidData(sub.localAid);
    ClearPhantomAidData(sub.hostAid);
    ExpireStalePhantomAids(controller);
    return aidBitmap | s_phantomAids;
}
kmCall(0x80658e10, GetAidBitmapWithPhantomAids);

static bool SelectInfoHasAid(const Pages::SELECTStageMgr& stageMgr, u32 aid, u32 slot) {
    for (u32 i = 0; i < stageMgr.playerCount; ++i) {
        const PlayerInfo& info = stageMgr.infos[i];
        if (info.aid == aid && info.hudSlotid == slot) return true;
    }
    return false;
}

static Team GetPhantomSelectTeam(u32 aid, u32 slot) {
    const SectionId sectionId = SectionMgr::sInstance->curSection->sectionId;
    switch (sectionId) {
        case SECTION_P1_WIFI_VS_VOTING:
        case SECTION_P2_WIFI_VS_VOTING:
        case SECTION_P1_WIFI_FROOM_VS_VOTING:
        case SECTION_P2_WIFI_FROOM_VS_VOTING:
            return TEAM_NONE;
        default:
            break;
    }

    RKNet::SELECTHandler* selectHandler = RKNet::SELECTHandler::sInstance;
    if (selectHandler == nullptr) return TEAM_NONE;
    return static_cast<Team>(selectHandler->GetTeam(aid, slot));
}

void AppendPhantomSelectInfos(Pages::SELECTStageMgr& stageMgr) {
    if (s_phantomAids == 0) return;

    for (u32 aid = 0; aid < 12 && stageMgr.playerCount < 12; ++aid) {
        const u32 aidBit = 1 << aid;
        if ((s_phantomAids & aidBit) == 0 || (s_phantomConnectionUserDatas & aidBit) == 0) continue;
        if (!ShouldPreservePhantomAid(aid)) continue;

        const u32 playerCount = s_connectionUserDatas[aid].playersAtConsole;
        for (u32 slot = 0; slot < playerCount && slot < 2 && stageMgr.playerCount < 12; ++slot) {
            if (SelectInfoHasAid(stageMgr, aid, slot)) continue;

            const u32 playerId = stageMgr.playerCount;
            PlayerInfo& info = stageMgr.infos[playerId];
            info.aid = aid;
            info.hudSlotid = slot;
            info.team = GetPhantomSelectTeam(aid, slot);
            if (slot == 0 && (s_phantomUSERPackets & aidBit) != 0) {
                info.vr = s_userPackets[aid].vr;
                info.br = s_userPackets[aid].br;
            } else {
                info.vr = 0xffff;
                info.br = 0xffff;
            }

            stageMgr.miiGroup.ReplaceMiiByPlayerMii(playerId, aid, slot);
            stageMgr.playerCount = playerId + 1;
        }
    }
}

static bool IsValidAidOrPhantom(u32 aid) {
    return DWC_IsValidAid(aid) || ShouldPreservePhantomAid(aid);
}
kmCall(0x800e8690, IsValidAidOrPhantom);

// Reduce PING retry time from 700 to 80 [Wiimmfi]
kmWrite16(0x8011B47A, 80);

// Do not wait the retry time in case of successful NATNEG [Wiimmfi]
kmWrite32(0x8011B4B0, 0x60000000);

// Change the SYN-ACK timeout to 7 seconds instead of 5 seconds per node [Wiimmfi]
kmWrite32(0x800E1A58, 0x38C00000 | 7000);

// Fix the "suspend bug" where DWC stalls suspending due to ongoing NATNEG [WiiLink24, MrBean35000vr]
kmWrite32(0x800E77F8, 0x60000000);
kmWrite32(0x800E77FC, 0x60000000);

// Slower High Data Rate [MrBean35000vr, Chadderz]
kmWrite32(0x80657EA8, 0x2804000C);

}  // namespace Network
}  // namespace Pulsar
