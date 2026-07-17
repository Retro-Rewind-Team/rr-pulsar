#include <kamek.hpp>
#include <core/rvl/DWC/DWC.hpp>
#include <PulsarSystem.hpp>
#include <Settings/Settings.hpp>
#include <Network/WiiLink.hpp>
#include <Network/Network.hpp>
#include <Network/Mogi.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>

namespace Pulsar {
namespace Network {

static void PatchLoginRegion() {
    WWFC_CUSTOM_REGION = System::sInstance->netMgr.region;
    char path[0x9];
    snprintf(path, 0x9, "%08d", System::sInstance->netMgr.region + 100000);
    for (int i = 0; i < 8; ++i) {
        DWC::loginRegion[i] = path[i];
    }
}
BootHook LoginRegion(PatchLoginRegion, 2);

int PatchRegion(char* path, u32 len, const char* fmt, const char* mode) {
    return snprintf(path, len, fmt, mode, System::sInstance->netMgr.region);
}
kmCall(0x8065921c, PatchRegion);
kmCall(0x80659270, PatchRegion);
kmCall(0x80659734, PatchRegion);
kmCall(0x80659788, PatchRegion);

static int GetFriendsSearchType(int curType, u32 regionId) {
    register u8 friendRegionId;
    asm(mr friendRegionId, r0;);
    if ((System::sInstance->netMgr.region == 0x0A || System::sInstance->netMgr.region == 0x0B || System::sInstance->netMgr.region == 0x0C || System::sInstance->netMgr.region == 0x0D || System::sInstance->netMgr.region == 0x0E || System::sInstance->netMgr.region == 0x0F || System::sInstance->netMgr.region == Mogi::REGION || System::sInstance->netMgr.region == Mogi::REGION_CT || System::sInstance->netMgr.region == Mogi::REGION_REG ||
         System::sInstance->netMgr.region == 0x14 || System::sInstance->netMgr.region == 0x15) ||
        (friendRegionId == 0x0A || friendRegionId == 0x0B || friendRegionId == 0x0C || friendRegionId == 0x0D || friendRegionId == 0x0E || friendRegionId == 0x0F || friendRegionId == Mogi::REGION || friendRegionId == Mogi::REGION_CT || friendRegionId == Mogi::REGION_REG ||
         friendRegionId == 0x14 || friendRegionId == 0x15)) {
        if (curType == 7) return 6;
        return 9;
    }
    if (System::sInstance->netMgr.region != friendRegionId) return curType;
    if (curType == 7) return 6;
    return 9;
}
kmBranch(0x8065a03c, GetFriendsSearchType);
kmBranch(0x8065a088, GetFriendsSearchType);

static u32 PatchRKNetControllerRegion() {
    return System::sInstance->netMgr.region;
}
kmCall(0x80653640, PatchRKNetControllerRegion);
kmWrite32(0x80653644, 0x7c651b78);
kmCall(0x806536ac, PatchRKNetControllerRegion);  // for battle
kmWrite32(0x806536b0, 0x7c661b78);

}  // namespace Network
}  // namespace Pulsar
