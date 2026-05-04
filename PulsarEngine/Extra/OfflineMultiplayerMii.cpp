#include <kamek.hpp>
#include <runtimeWrite.hpp>
#include <MarioKartWii/RKSYS/RKSYSMgr.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>

namespace Pulsar {

kmRuntimeUse(0x805fa6e0);
static void LoadStoreMii(MiiGroup& miiGroup, u8 idx, RFL::CreateID* createId) {
    typedef void (*LoadStoreMiiFn)(MiiGroup*, u8, RFL::CreateID*);
    const LoadStoreMiiFn loadStoreMii = reinterpret_cast<LoadStoreMiiFn>(kmRuntimeAddr(0x805fa6e0));
    loadStoreMii(&miiGroup, idx, createId);
}

static void LoadLicenseMiiForOfflineMultiplayer() {
    SectionMgr* const sectionMgr = SectionMgr::sInstance;
    RKSYS::Mgr* const rksysMgr = RKSYS::Mgr::sInstance;
    if (sectionMgr == nullptr || sectionMgr->sectionParams == nullptr || rksysMgr == nullptr) return;

    const u32 licenseId = rksysMgr->curLicenseId;
    if (licenseId >= 4 || !rksysMgr->CheckLicenseMagic(licenseId)) return;

    SectionParams* const params = sectionMgr->sectionParams;
    const u32 localPlayerCount = params->localPlayerCount > 4 ? 4 : params->localPlayerCount;
    RFL::CreateID* const createId = &rksysMgr->licenses[licenseId].createID;

    for (u32 hudSlotId = 0; hudSlotId < localPlayerCount; ++hudSlotId) {
        LoadStoreMii(params->playerMiis, static_cast<u8>(hudSlotId), createId);
    }
}

kmRuntimeUse(0x80860484);
static bool ShouldUseDirectMiiSelect(u32 sectionId) {
    typedef bool (*IsDirectMiiSelectSectionFn)(u32);
    const IsDirectMiiSelectSectionFn original = reinterpret_cast<IsDirectMiiSelectSectionFn>(kmRuntimeAddr(0x80860484));
    if (original(sectionId)) return true;

    if (sectionId != SECTION_LOCAL_MULTIPLAYER) return false;

    LoadLicenseMiiForOfflineMultiplayer();
    return true;
}

kmCall(0x807e393c, ShouldUseDirectMiiSelect);

}  // namespace Pulsar
