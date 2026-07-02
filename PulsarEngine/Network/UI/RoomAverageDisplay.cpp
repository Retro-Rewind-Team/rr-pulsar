#include <kamek.hpp>
#include <include/c_wchar.h>
#include <MarioKartWii/RKSYS/RKSYSMgr.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/UI/Ctrl/Menu/CtrlMenuText.hpp>
#include <MarioKartWii/UI/Page/Other/SELECTStageMgr.hpp>
#include <MarioKartWii/UI/Section/Section.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>
#include <Network/Rating/PlayerRating.hpp>
#include <UI/UI.hpp>

namespace Pulsar {
namespace UI {

static bool IsRoomAverageBmg(u32 bmgId) {
    switch (bmgId) {
        case 0x10e0:
        case 0x10e1:
        case 0x10e2:
        case 0x10e5:
        case 0x10e6:
        case 0x10e7:
        case 0x10e8:
        case 0x10e9:
        case 0x10ea:
            return true;
        default:
            return false;
    }
}

static u32 GetRoomAverageVR(const Pages::SELECTStageMgr& stageMgr) {
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    const RKSYS::Mgr* rksys = RKSYS::Mgr::sInstance;
    const u8 localAid = controller != nullptr ? controller->subs[controller->currentSub].localAid : 0xff;

    u32 totalVR = 0;
    u32 playerCount = 0;
    const u32 count = stageMgr.playerCount < 12 ? stageMgr.playerCount : 12;

    for (u32 i = 0; i < count; ++i) {
        const PlayerInfo& player = stageMgr.infos[i];
        if (player.vr == 0xffff) continue;

        // RR stores ratings as whole points plus two decimal digits. Convert
        // them back to the VR value shown by the UI before averaging.
        u32 vr = static_cast<u32>(player.vr) * 100;
        if (player.aid == localAid && player.hudSlotid == 0 && rksys != nullptr && rksys->curLicenseId >= 0) {
            vr = static_cast<u32>(PointRating::GetUserVR(rksys->curLicenseId) * 100.0f + 0.5f);
        } else if (player.aid < 12 && player.hudSlotid < 2) {
            vr += PointRating::remoteDecimalVR[player.aid][player.hudSlotid];
        }

        totalVR += vr;
        ++playerCount;
    }

    return playerCount == 0 ? 0 : (totalVR + playerCount / 2) / playerCount;
}

static void SetRoomAverageMessage(CtrlMenuInstructionText* instructionText, u32 bmgId, const Text::Info* text) {
    if (!IsRoomAverageBmg(bmgId) || instructionText == nullptr) {
        if (instructionText != nullptr) instructionText->SetMessage(bmgId, text);
        return;
    }

    Pages::SELECTStageMgr* stageMgr = nullptr;
    if (SectionMgr::sInstance != nullptr && SectionMgr::sInstance->curSection != nullptr) {
        stageMgr = SectionMgr::sInstance->curSection->Get<Pages::SELECTStageMgr>();
    }

    if (stageMgr == nullptr) {
        instructionText->SetMessage(bmgId, text);
        return;
    }

    static wchar_t message[48];
    swprintf(message, sizeof(message) / sizeof(message[0]), L"Average VR: %uvr", GetRoomAverageVR(*stageMgr));

    Text::Info info;
    info.strings[0] = message;
    instructionText->SetMessage(BMG_TEXT, &info);
}

// SELECTStageMgr::GetInstructionBmgId feeds these four instruction-text calls.
kmCall(0x8064aaac, SetRoomAverageMessage);  // VR page
kmCall(0x80839300, SetRoomAverageMessage);  // VS rule select
kmCall(0x808393a8, SetRoomAverageMessage);  // VS rule select (return path)
kmCall(0x8083ce64, SetRoomAverageMessage);  // Battle rule select

}  // namespace UI
}  // namespace Pulsar
