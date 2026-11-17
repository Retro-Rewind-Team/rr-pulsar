#include <core/rvl/DWC/DWCAccount.hpp>
#include <core/System/SystemManager.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/UI/Page/Other/FriendList.hpp>
#include <MarioKartWii/UI/Page/Other/Globe.hpp>
#include <MarioKartWii/UI/Page/Other/WFCMenu.hpp>
#include <PulsarSystem.hpp>
#include <Settings/Settings.hpp>
#include <UI/UI.hpp>
#include <Network/FriendStatusColors.hpp>

namespace Pulsar {
namespace Network {

// wiimmfi hook forces the u64 trick
u64 AddModeToStatusData(const RKNet::StatusData *own) {
    static char customData[9];
    u32 length = 8;
    if (own->status == RKNet::FRIEND_STATUS_PUBLIC_VS) {
        if (own->regionId != 0xff) {
            length = 9;
            for (int i = 0; i < 8; ++i) {
                customData[i] = reinterpret_cast<const u8 *>(own)[i];
            }
            customData[8] = System::sInstance->netMgr.ownStatusData;
            own = reinterpret_cast<RKNet::StatusData *>(&customData);
        }
    }
    u64 ret = ((reinterpret_cast<const u64>(own)) << 32) | (length & 0xffffffffL);
    return ret;
}
kmCall(0x8065a0e8, AddModeToStatusData);

u8 ReceiveMode(const DWC::AccFriendData *data, char *dest, int *size) {
    register u32 idx;
    asm(mr idx, r26;);
    char temp[32];
    const u8 ret = DWC::GetFriendStatusData(data, temp, size);
    u8 val = 0;
    if (*size >= 9) {
        val = temp[8];
    }

    int copySize = *size;
    if (copySize > 16)
        copySize = 16;
    for (int i = 0; i < copySize; ++i) {
        dest[i] = temp[i];
    }

    if (*size == 9) {
        *size = 8;
    }
    if (idx < 30) {
        System::sInstance->netMgr.statusDatas[idx] = val;
    }
    return ret;
}
kmCall(0x8065a1d4, ReceiveMode);

RKNet::SearchType SetModeOnJoin(const RKNet::Controller &controller, u32 friendIdx) {
    RKNet::SearchType type = controller.GetFriendSearchType(friendIdx);
    if (type == RKNet::SEARCH_TYPE_VS_REGIONAL) {
        Mgr &netMgr = System::sInstance->netMgr;
        netMgr.ownStatusData = netMgr.statusDatas[friendIdx];
    }
    return type;
}
kmCall(0x80656534, SetModeOnJoin);

void GlobeMsgColor(Pages::Globe::MessageWindow &msg, u32 bmgId, Text::Info *info) {
    register Pages::Globe *globe;
    asm(mr globe, r31;);

    // Get the friend's region
    const RKNet::Friend *friendData = &RKNet::Controller::sInstance->friends[globe->selFriendIdx];
    const u8 friendRegion = friendData->statusData.regionId;

    // Set appropriate BMG based on region
    if (friendRegion == 0xA) {
        bmgId = UI::BMG_RETRO_PLAYING;
    } else if (friendRegion == 0xB) {
        bmgId = UI::BMG_OTT_PLAYING;
    } else if (friendRegion == 0xC) {
        bmgId = UI::BMG_200_PLAYING;
    } else if (friendRegion == 0xD) {
        bmgId = UI::BMG_ITEM_RAIN_PLAYING;
    } else if (friendRegion == 0x14) {
        bmgId = UI::BMG_CUSTOM_PLAYING;
    } else if (friendRegion == 0x15) {
        bmgId = UI::BMG_REGS_PLAYING;
    }

    msg.SetMessage(bmgId, info);
    ut::Color color;
    if (globe->curSelFriendStatus >= 0x15 && globe->curSelFriendStatus <= 0x18)
        color = GetFriendColor(globe->selFriendIdx);
    else
        color = -1;
    SetGlobeMsgColor(globe->message, color);
}
kmCall(0x805e504c, GlobeMsgColor);

void GlobeSearchTopMsg(CtrlMenuPageTitleText &title, u32 bmgId, Text::Info *info) {
    if (System::sInstance->netMgr.region == 0x0A)
        bmgId = UI::BMG_TITLE_TEXT_RT;
    else if (System::sInstance->netMgr.region == 0x0B)
        bmgId = UI::BMG_OTT_TITLE_TEXT;
    else if (System::sInstance->netMgr.region == 0x0C)
        bmgId = UI::BMG_200_TITLE_TEXT;
    else if (System::sInstance->netMgr.region == 0x0D)
        bmgId = UI::BMG_ITEM_RAIN_TITLE_TEXT;
    else if (System::sInstance->netMgr.region == 0x14)
        bmgId = UI::BMG_TITLE_TEXT_CT;
    else if (System::sInstance->netMgr.region == 0x15)
        bmgId = UI::BMG_TITLE_TEXT_REGS;
    title.SetMessage(bmgId, info);
}
kmCall(0x80608658, GlobeSearchTopMsg);

}  // namespace Network
}  // namespace Pulsar
