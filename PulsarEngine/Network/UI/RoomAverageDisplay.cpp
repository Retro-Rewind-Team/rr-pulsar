#include <kamek.hpp>
#include <include/c_wchar.h>
#include <MarioKartWii/UI/Ctrl/Menu/CtrlMenuText.hpp>
#include <MarioKartWii/UI/Page/Other/SELECTStageMgr.hpp>
#include <MarioKartWii/UI/Section/Section.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>
#include <UI/UI.hpp>
#include <Network/Rating/PlayerRating.hpp>

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

static u32 GetRoomAverageVR(const Pages::SELECTStageMgr &stageMgr) {
    u32 totalVR = 0;
    u32 playerCount = 0;
    const u32 count = stageMgr.playerCount < 12 ? stageMgr.playerCount : 12;

    for (u32 i = 0; i < count; ++i) {
        const PlayerInfo &player = stageMgr.infos[i];
        u32 vr = player.vr == 0xffff ? 0 : player.vr;
        if (player.hudSlotid == 0)
            vr = Network::GetPlayerRating(player.aid, false, vr);
        if (vr == 0)
            continue;

        totalVR += vr;
        ++playerCount;
    }

    return playerCount == 0 ? 0 : (totalVR + playerCount / 2) / playerCount;
}

static u32 GetVRControlValue(const u8 *stageAtPlayerOffset) {
    const PlayerInfo &player = *reinterpret_cast<const PlayerInfo *>(stageAtPlayerOffset + offsetof(Pages::SELECTStageMgr, infos));
    const u32 fallback = player.vr == 0xffff ? 0xffffffff : player.vr;
    return player.hudSlotid == 0 ? Network::GetPlayerRating(player.aid, false, fallback) : fallback;
}
kmCall(0x8064ac58, GetVRControlValue);
kmWrite32(0x8064ac64, 0x2c00ffff);  // cmpwi r0, -1

static u32 GetBRControlValue(const u8 *stageAtPlayerOffset) {
    const PlayerInfo &player = *reinterpret_cast<const PlayerInfo *>(stageAtPlayerOffset + offsetof(Pages::SELECTStageMgr, infos));
    const u32 fallback = player.br == 0xffff ? 0xffffffff : player.br;
    return player.hudSlotid == 0 ? Network::GetPlayerRating(player.aid, true, fallback) : fallback;
}
kmCall(0x8064ac8c, GetBRControlValue);
kmWrite32(0x8064ac98, 0x2c00ffff);  // cmpwi r0, -1

static void SetRoomAverageMessage(CtrlMenuInstructionText *instructionText, u32 bmgId, const Text::Info *text) {
    if (!IsRoomAverageBmg(bmgId) || instructionText == nullptr) {
        if (instructionText != nullptr)
            instructionText->SetMessage(bmgId, text);
        return;
    }

    Pages::SELECTStageMgr *stageMgr = nullptr;
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
