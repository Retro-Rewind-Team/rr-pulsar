#include <kamek.hpp>
#include <MarioKartWii/Archive/ArchiveMgr.hpp>
#include <MarioKartWii/KMP/ENPH.hpp>
#include <MarioKartWii/KMP/ITPH.hpp>
#include <MarioKartWii/KMP/KMPManager.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>

namespace Pulsar {
namespace Race {

/*
Route group condition encoding (ENPH/ITPH unknown_0xE):
- bits 1-8: lap mask (bit1 = lap 1 ... bit8 = lap 8)
- bit 9: invert mask (0 = disable on set bits, 1 = disable on clear bits)
*/

static const u8 MAX_CONDITIONAL_LAP_INDEX_COUNT = 8;
static const u16 INVALID_GROUP_ID = 0xFFFF;
static const char* CONDITIONAL_ROUTE_GROUPS_ENABLE_FILE = "enable.rgrp";

enum ConditionalRouteTrackFileState {
    CONDITIONAL_ROUTE_TRACK_FILE_UNKNOWN = -1,
    CONDITIONAL_ROUTE_TRACK_FILE_MISSING = 0,
    CONDITIONAL_ROUTE_TRACK_FILE_PRESENT = 1
};

static const void* sCachedCourseArchive = nullptr;
static s8 sConditionalRouteTrackFileState = CONDITIONAL_ROUTE_TRACK_FILE_UNKNOWN;
static const KMP::Manager* sCachedKmpMgr = nullptr;
static u16 sEnemyGroupByPoint[256];
static u16 sItemGroupByPoint[256];
static u16 sEnemyGroupRules[256];
static u16 sItemGroupRules[256];
static u32 sLapCacheRaceFrame = 0xFFFFFFFF;
static bool sLapCacheValid = false;
static u8 sLapCacheIdx = 0;

static void ResetRouteGroupCache() {
    for (u16 i = 0; i < 256; ++i) {
        sEnemyGroupByPoint[i] = INVALID_GROUP_ID;
        sItemGroupByPoint[i] = INVALID_GROUP_ID;
        sEnemyGroupRules[i] = 0;
        sItemGroupRules[i] = 0;
    }
    sCachedKmpMgr = nullptr;
}

static bool IsTrackConditionalRouteGroupsEnabled() {
    const ArchiveMgr* archiveMgr = ArchiveMgr::sInstance;
    if (archiveMgr == nullptr) {
        sCachedCourseArchive = nullptr;
        sConditionalRouteTrackFileState = CONDITIONAL_ROUTE_TRACK_FILE_UNKNOWN;
        ResetRouteGroupCache();
        return false;
    }

    const void* courseArchive = archiveMgr->GetArchive(ARCHIVE_HOLDER_COURSE, 0);
    if (courseArchive != sCachedCourseArchive) {
        sCachedCourseArchive = courseArchive;
        sConditionalRouteTrackFileState = CONDITIONAL_ROUTE_TRACK_FILE_UNKNOWN;
        ResetRouteGroupCache();
    }

    if (sConditionalRouteTrackFileState == CONDITIONAL_ROUTE_TRACK_FILE_UNKNOWN) {
        if (courseArchive == nullptr) return false;

        const void* condFile = archiveMgr->GetFile(ARCHIVE_HOLDER_COURSE, CONDITIONAL_ROUTE_GROUPS_ENABLE_FILE, nullptr);
        sConditionalRouteTrackFileState = (condFile != nullptr) ? CONDITIONAL_ROUTE_TRACK_FILE_PRESENT : CONDITIONAL_ROUTE_TRACK_FILE_MISSING;
    }

    return sConditionalRouteTrackFileState == CONDITIONAL_ROUTE_TRACK_FILE_PRESENT;
}

static bool TryGetTrackDefinedLapCount(u8& lapCount) {
    const KMP::Manager* kmp = KMP::Manager::sInstance;
    if (kmp == nullptr || kmp->stgiSection == nullptr || kmp->stgiSection->holdersArray[0] == nullptr ||
        kmp->stgiSection->holdersArray[0]->raw == nullptr) {
        return false;
    }

    lapCount = kmp->stgiSection->holdersArray[0]->raw->lapCount;
    if (lapCount == 0) return false;
    if (lapCount > MAX_CONDITIONAL_LAP_INDEX_COUNT) lapCount = MAX_CONDITIONAL_LAP_INDEX_COUNT;
    return true;
}

static bool TryGetCurrentLapIdxFromLeader(u8& lapIdx) {
    const Raceinfo* raceInfo = Raceinfo::sInstance;
    if (raceInfo == nullptr || raceInfo->players == nullptr) return false;

    const RaceinfoPlayer* raceInfoPlayer = nullptr;
    if (raceInfo->playerIdInEachPosition != nullptr) {
        const u8 leaderId = raceInfo->playerIdInEachPosition[0];
        if (leaderId < 12) raceInfoPlayer = raceInfo->players[leaderId];
    }

    if (raceInfoPlayer == nullptr) raceInfoPlayer = raceInfo->players[0];
    if (raceInfoPlayer == nullptr) return false;

    u16 currentLap = raceInfoPlayer->currentLap;
    if (currentLap == 0) currentLap = 1;

    u8 trackLapCount;
    if (TryGetTrackDefinedLapCount(trackLapCount)) {
        currentLap = static_cast<u16>(((currentLap - 1) % trackLapCount) + 1);
    } else if (currentLap > MAX_CONDITIONAL_LAP_INDEX_COUNT) {
        currentLap = MAX_CONDITIONAL_LAP_INDEX_COUNT;
    }

    if (currentLap > MAX_CONDITIONAL_LAP_INDEX_COUNT) currentLap = MAX_CONDITIONAL_LAP_INDEX_COUNT;
    lapIdx = static_cast<u8>(currentLap - 1);
    return true;
}

static bool TryGetCurrentLapIdx(u8& lapIdx) {
    const Raceinfo* raceInfo = Raceinfo::sInstance;
    if (raceInfo == nullptr) {
        sLapCacheRaceFrame = 0xFFFFFFFF;
        sLapCacheValid = false;
        return false;
    }

    if (raceInfo->raceFrames != sLapCacheRaceFrame) {
        sLapCacheRaceFrame = raceInfo->raceFrames;
        sLapCacheValid = TryGetCurrentLapIdxFromLeader(sLapCacheIdx);
    }

    if (!sLapCacheValid) return false;
    lapIdx = sLapCacheIdx;
    return true;
}

static void BuildRouteGroupCache(const KMP::Manager& kmpMgr) {
    ResetRouteGroupCache();
    sCachedKmpMgr = &kmpMgr;

    const KMP::Section<ENPH>* enphSection = kmpMgr.enphSection;
    if (enphSection != nullptr) {
        const u16 enphCount = enphSection->pointCount;
        for (u16 groupId = 0; groupId < enphCount && groupId < 256; ++groupId) {
            const KMP::Holder<ENPH>* groupHolder = kmpMgr.GetHolder<ENPH>(groupId);
            if (groupHolder == nullptr || groupHolder->raw == nullptr) continue;

            const ENPH& group = *groupHolder->raw;
            sEnemyGroupRules[groupId] = group.unknown_0xE;

            const u16 start = group.start;
            const u16 end = static_cast<u16>(start + group.length);
            for (u16 pointId = start; pointId < end && pointId < 256; ++pointId) {
                sEnemyGroupByPoint[pointId] = groupId;
            }
        }
    }

    const KMP::Section<ITPH>* itphSection = kmpMgr.itphSection;
    if (itphSection != nullptr) {
        const u16 itphCount = itphSection->pointCount;
        for (u16 groupId = 0; groupId < itphCount && groupId < 256; ++groupId) {
            const KMP::Holder<ITPH>* groupHolder = kmpMgr.GetHolder<ITPH>(groupId);
            if (groupHolder == nullptr || groupHolder->raw == nullptr) continue;

            const ITPH& group = *groupHolder->raw;
            sItemGroupRules[groupId] = group.unknown_0xE;

            const u16 start = group.start;
            const u16 end = static_cast<u16>(start + group.length);
            for (u16 pointId = start; pointId < end && pointId < 256; ++pointId) {
                sItemGroupByPoint[pointId] = groupId;
            }
        }
    }
}

static void EnsureRouteGroupCache(const KMP::Manager* kmpMgr) {
    if (kmpMgr == nullptr) return;
    if (sCachedKmpMgr == kmpMgr) return;
    BuildRouteGroupCache(*kmpMgr);
}

static bool IsLapDisabledByRule(u16 rule, u8 lapIdx) {
    const u16 lapMask = static_cast<u16>(rule & 0x1FE);
    if (lapMask == 0 || lapIdx >= MAX_CONDITIONAL_LAP_INDEX_COUNT) return false;

    const bool invert = (rule & 0x200) != 0;
    const bool lapBitSet = (lapMask & (1 << (lapIdx + 1))) != 0;
    return invert ? !lapBitSet : lapBitSet;
}

static bool IsEnemyPointDisabled(u8 enptId, u8 lapIdx) {
    const u16 groupId = sEnemyGroupByPoint[enptId];
    if (groupId == INVALID_GROUP_ID) return false;
    return IsLapDisabledByRule(sEnemyGroupRules[groupId], lapIdx);
}

static bool IsItemPointDisabled(u8 itptId, u8 lapIdx) {
    const u16 groupId = sItemGroupByPoint[itptId];
    if (groupId == INVALID_GROUP_ID) return false;
    return IsLapDisabledByRule(sItemGroupRules[groupId], lapIdx);
}

static bool ShouldFilterRouteGroups(const KMP::Manager* kmpMgr, u8& lapIdx) {
    if (!IsTrackConditionalRouteGroupsEnabled()) return false;
    if (kmpMgr == nullptr) return false;
    if (!TryGetCurrentLapIdx(lapIdx)) return false;
    EnsureRouteGroupCache(kmpMgr);
    return true;
}

static s8 GetENPTCount(const KMP::Manager* kmpMgr, const u8& curENPT, bool useNextLinks) {
    if (kmpMgr == nullptr) return 0;

    const KMP::Holder<ENPT>* enptHolder = kmpMgr->GetHolder<ENPT>(curENPT);
    if (enptHolder == nullptr) return 0;

    const u8 rawCount = useNextLinks ? enptHolder->nextCount : enptHolder->prevCount;
    const u8* rawLinks = useNextLinks ? enptHolder->nextLinks : enptHolder->prevLinks;
    if (rawLinks == nullptr) return 0;

    u8 lapIdx = 0;
    if (!ShouldFilterRouteGroups(kmpMgr, lapIdx)) return rawCount;

    s8 filteredCount = 0;
    for (u8 i = 0; i < rawCount; ++i) {
        const u8 link = rawLinks[i];
        if (IsEnemyPointDisabled(link, lapIdx)) continue;
        ++filteredCount;
    }

    // Keep vanilla behavior stable: never let filtering remove every branch if raw links exist.
    if (filteredCount == 0 && rawCount > 0) return rawCount;
    return filteredCount;
}

static s8 GetENPTLink(const KMP::Manager* kmpMgr, const u8& curENPT, u8 linkIdx, bool useNextLinks) {
    if (kmpMgr == nullptr) return -1;

    const KMP::Holder<ENPT>* enptHolder = kmpMgr->GetHolder<ENPT>(curENPT);
    if (enptHolder == nullptr) return -1;

    const u8 rawCount = useNextLinks ? enptHolder->nextCount : enptHolder->prevCount;
    const u8* rawLinks = useNextLinks ? enptHolder->nextLinks : enptHolder->prevLinks;
    if (rawLinks == nullptr) return -1;

    u8 lapIdx = 0;
    if (!ShouldFilterRouteGroups(kmpMgr, lapIdx)) {
        if (linkIdx >= rawCount) return -1;
        return static_cast<s8>(rawLinks[linkIdx]);
    }

    u8 filteredIdx = 0;
    for (u8 i = 0; i < rawCount; ++i) {
        const u8 link = rawLinks[i];
        if (IsEnemyPointDisabled(link, lapIdx)) continue;
        if (filteredIdx == linkIdx) return static_cast<s8>(link);
        ++filteredIdx;
    }

    // Keep at least one fallback route if filtering removed all candidates.
    if (rawCount > 0) {
        const u8 fallbackIdx = (linkIdx < rawCount) ? linkIdx : 0;
        return static_cast<s8>(rawLinks[fallbackIdx]);
    }
    return -1;
}

static u8 GetITPTCount(const KMP::Manager* kmpMgr, const u8& itpt, bool useNextLinks) {
    if (kmpMgr == nullptr) return 0;

    const KMP::Holder<ITPT>* itptHolder = kmpMgr->GetHolder<ITPT>(itpt);
    if (itptHolder == nullptr) return 0;

    u8 rawCount = useNextLinks ? itptHolder->nextCount : itptHolder->prevCount;
    if (rawCount > 6) rawCount = 6;

    u8 lapIdx = 0;
    if (!ShouldFilterRouteGroups(kmpMgr, lapIdx)) return rawCount;

    u8 filteredCount = 0;
    for (u8 i = 0; i < rawCount; ++i) {
        const u8 link = useNextLinks ? itptHolder->nextLinks[i] : itptHolder->prevLinks[i];
        if (IsItemPointDisabled(link, lapIdx)) continue;
        ++filteredCount;
    }

    // Keep vanilla behavior stable: never let filtering remove every branch if raw links exist.
    if (filteredCount == 0 && rawCount > 0) return rawCount;
    return filteredCount;
}

static u8 GetITPTLink(const KMP::Manager* kmpMgr, const u8& curITPT, u8 linkIdx, bool useNextLinks) {
    if (kmpMgr == nullptr) return 0xFF;

    const KMP::Holder<ITPT>* itptHolder = kmpMgr->GetHolder<ITPT>(curITPT);
    if (itptHolder == nullptr) return 0xFF;

    u8 rawCount = useNextLinks ? itptHolder->nextCount : itptHolder->prevCount;
    if (rawCount > 6) rawCount = 6;

    u8 lapIdx = 0;
    if (!ShouldFilterRouteGroups(kmpMgr, lapIdx)) {
        if (linkIdx >= rawCount) return 0xFF;
        return useNextLinks ? itptHolder->nextLinks[linkIdx] : itptHolder->prevLinks[linkIdx];
    }

    u8 filteredIdx = 0;
    for (u8 i = 0; i < rawCount; ++i) {
        const u8 link = useNextLinks ? itptHolder->nextLinks[i] : itptHolder->prevLinks[i];
        if (IsItemPointDisabled(link, lapIdx)) continue;
        if (filteredIdx == linkIdx) return link;
        ++filteredIdx;
    }

    // Keep at least one fallback route if filtering removed all candidates.
    if (rawCount > 0) {
        const u8 fallbackIdx = (linkIdx < rawCount) ? linkIdx : 0;
        return useNextLinks ? itptHolder->nextLinks[fallbackIdx] : itptHolder->prevLinks[fallbackIdx];
    }
    return 0xFF;
}

static s8 ConditionalGetNextENPT(KMP::Manager* kmpMgr, const u8& curENPT, u8 linkIdx) {
    // OS::Report("ConditionalGetNextENPT: curENPT=%u, linkIdx=%u\n", curENPT, linkIdx);
    return GetENPTLink(kmpMgr, curENPT, linkIdx, true);
}
kmBranch(0x80517590, ConditionalGetNextENPT);

static s8 ConditionalGetNextENPTCount(KMP::Manager* kmpMgr, const u8& curENPT) {
    // OS::Report("ConditionalGetNextENPTCount: curENPT=%u\n", curENPT);
    return GetENPTCount(kmpMgr, curENPT, true);
}
kmBranch(0x8051760c, ConditionalGetNextENPTCount);

static s8 ConditionalGetPrevENPT(KMP::Manager* kmpMgr, const u8& curENPT, u8 linkIdx) {
    // OS::Report("ConditionalGetPrevENPT: curENPT=%u, linkIdx=%u\n", curENPT, linkIdx);
    return GetENPTLink(kmpMgr, curENPT, linkIdx, false);
}
kmBranch(0x80517670, ConditionalGetPrevENPT);

static s8 ConditionalGetPrevENPTCount(KMP::Manager* kmpMgr, const u8& curENPT) {
    // OS::Report("ConditionalGetPrevENPTCount: curENPT=%u\n", curENPT);
    return GetENPTCount(kmpMgr, curENPT, false);
}
kmBranch(0x805176ec, ConditionalGetPrevENPTCount);

static u8 ConditionalGetNextITPT(KMP::Manager* kmpMgr, const u8& curITPT, u8 linkIdx) {
    // OS::Report("ConditionalGetNextITPT: curITPT=%u, linkIdx=%u\n", curITPT, linkIdx);
    return GetITPTLink(kmpMgr, curITPT, linkIdx, true);
}
kmBranch(0x805181f0, ConditionalGetNextITPT);

static u8 ConditionalGetITPTNextCount(KMP::Manager* kmpMgr, const u8& itpt) {
    // OS::Report("ConditionalGetITPTNextCount: itpt=%u\n", itpt);
    return GetITPTCount(kmpMgr, itpt, true);
}
kmBranch(0x80518268, ConditionalGetITPTNextCount);

static u8 ConditionalGetPrevITPT(KMP::Manager* kmpMgr, const u8& curITPT, u8 linkIdx) {
    // OS::Report("ConditionalGetPrevITPT: curITPT=%u, linkIdx=%u\n", curITPT, linkIdx);
    return GetITPTLink(kmpMgr, curITPT, linkIdx, false);
}
kmBranch(0x805182cc, ConditionalGetPrevITPT);

static u8 ConditionalGetITPTPrevCount(KMP::Manager* kmpMgr, const u8& itpt) {
    // OS::Report("ConditionalGetITPTPrevCount: itpt=%u\n", itpt);
    return GetITPTCount(kmpMgr, itpt, false);
}
kmBranch(0x80518344, ConditionalGetITPTPrevCount);

}  // namespace Race
}  // namespace Pulsar
