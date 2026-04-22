#include <kamek.hpp>
#include <PulsarSystem.hpp>
#include <IO/LooseArchiveOverrides.hpp>
#include <core/RK/RKSystem.hpp>
#include <core/nw4r/snd.hpp>
#include <core/rvl/OS/OSCache.hpp>
#include <core/rvl/os/OS.hpp>

namespace Pulsar {
namespace Sound {
using namespace nw4r;

namespace {
typedef void* (*LoadFileFn)(snd::detail::SoundArchiveLoader* loader, snd::SoundArchive::FileId fileId,
                            snd::SoundMemoryAllocatable* allocater);
typedef void* (*LoadWaveDataFileFn)(snd::detail::SoundArchiveLoader* loader, snd::SoundArchive::FileId fileId,
                                    snd::SoundMemoryAllocatable* allocater);
typedef void* (*LoadGroupFn)(snd::detail::SoundArchiveLoader* loader, u32 groupId, snd::SoundMemoryAllocatable* allocater,
                             void** waveDataAddress, u32 loadBlockSize);
typedef bool (*ReadFileInfoFn)(const snd::SoundArchive* archive, snd::SoundArchive::FileId fileId,
                               snd::SoundArchive::FileInfo* info);
typedef bool (*ReadFilePosFn)(const snd::SoundArchive* archive, snd::SoundArchive::FileId fileId, u32 index,
                              snd::SoundArchive::FilePos* info);
typedef bool (*ReadGroupInfoFn)(const snd::SoundArchive* archive, snd::SoundArchive::GroupId groupId,
                                snd::SoundArchive::GroupInfo* info);
typedef bool (*ReadGroupItemInfoFn)(const snd::SoundArchive* archive, snd::SoundArchive::GroupId groupId, u32 index,
                                    snd::SoundArchive::GroupItemInfo* info);

enum ResolvedTargetKind {
    RESOLVEDTARGET_NONE = 0,
    RESOLVEDTARGET_FILE_MANAGER,
    RESOLVEDTARGET_ARCHIVE,
    RESOLVEDTARGET_GROUP
};

struct ResolvedBRSARTarget {
    const void* address;
    u32 capacity;
    u32 groupId;
    u32 groupIndex;
    u8 kind;
    u8 padding[3];
};

static LoadFileFn sOriginalLoadFile = reinterpret_cast<LoadFileFn>(0x800a0180);
static LoadWaveDataFileFn sOriginalLoadWaveDataFile = reinterpret_cast<LoadWaveDataFileFn>(0x800a0420);
static LoadGroupFn sOriginalLoadGroup = reinterpret_cast<LoadGroupFn>(0x8009fa10);
static ReadFileInfoFn sReadFileInfo = reinterpret_cast<ReadFileInfoFn>(0x8009dff0);
static ReadFilePosFn sReadFilePos = reinterpret_cast<ReadFilePosFn>(0x8009e000);
static ReadGroupInfoFn sReadGroupInfo = reinterpret_cast<ReadGroupInfoFn>(0x8009dfc0);
static ReadGroupItemInfoFn sReadGroupItemInfo = reinterpret_cast<ReadGroupItemInfoFn>(0x8009dfd0);
static const void* sPatchedFileAddresses[1024] = {};
static const void* sPatchedWaveAddresses[1024] = {};
static void* sExternalFileBuffers[1024] = {};
static void* sExternalWaveBuffers[1024] = {};
static u8 sExternalFileAttempts[1024] = {};
static u8 sExternalWaveAttempts[1024] = {};
static u8 sLoggedFileRequests[1024] = {};
static u8 sLoggedWaveRequests[1024] = {};
static u8 sLoggedFileLookups[1024] = {};
static u8 sLoggedWaveLookups[1024] = {};
static u8 sLoggedGroupLoads[512] = {};

static void LogOverrideRequest(u8* logState, snd::SoundArchive::FileId fileId, const char* stage, u32 fileSize, u32 waveDataSize) {
    if (fileId >= 1024 || logState[fileId] != 0) return;
    logState[fileId] = 1;
    OS::Report("[Pulsar] Loose BRSAR %s request: fileId=%u file=0x%X wave=0x%X\n", stage, fileId, fileSize, waveDataSize);
}

static void LogResolvedLookup(u8* logState, snd::SoundArchive::FileId fileId, bool waveData, const ResolvedBRSARTarget& target) {
    if (fileId >= 1024 || logState[fileId] != 0) return;
    logState[fileId] = 1;
    OS::Report("[Pulsar] Loose BRSAR %s lookup: fileId=%u kind=%u group=%u index=%u addr=%p cap=0x%X\n",
               waveData ? "wave" : "file", fileId, target.kind, target.groupId, target.groupIndex, target.address,
               target.capacity);
}

static EGG::Heap* GetPersistentSoundOverrideHeap(u32 requiredSize) {
    EGG::Heap* candidates[3];
    candidates[0] = RKSystem::mInstance.EGGRootMEM2;
    candidates[1] = RKSystem::mInstance.EGGRootMEM1;
    EGG::Heap* overridesHeap = nullptr;
    if (Pulsar::System::sInstance != nullptr) {
        overridesHeap = static_cast<EGG::Heap*>(Pulsar::System::sInstance->heap);
    }
    candidates[2] = overridesHeap;

    for (u32 index = 0; index < 3; ++index) {
        EGG::Heap* heap = candidates[index];
        if (heap == nullptr) continue;
        if (heap->getAllocatableSize(0x20) >= requiredSize) return heap;
    }
    return nullptr;
}

static void* AllocAudioHeapOverrideBuffer(u32 allocSize) {
    EGG::ExpAudioMgr* audioMgr = RKSystem::mInstance.audioManager;
    if (audioMgr == nullptr) return nullptr;
    return audioMgr->EGG::SoundHeapMgr::heap.Alloc(allocSize);
}

static void* AllocPersistentSoundOverrideBuffer(u32 allocSize, EGG::Heap*& outHeap) {
    outHeap = nullptr;

    EGG::Heap* candidates[3];
    candidates[0] = RKSystem::mInstance.EGGRootMEM2;
    candidates[1] = RKSystem::mInstance.EGGRootMEM1;
    EGG::Heap* overridesHeap = nullptr;
    if (Pulsar::System::sInstance != nullptr) {
        overridesHeap = static_cast<EGG::Heap*>(Pulsar::System::sInstance->heap);
    }
    candidates[2] = overridesHeap;

    for (u32 index = 0; index < 3; ++index) {
        EGG::Heap* heap = candidates[index];
        if (heap == nullptr) continue;
        if (heap->getAllocatableSize(0x20) < allocSize) continue;

        void* buffer = EGG::Heap::alloc<void>(allocSize, 0x20, heap);
        if (buffer != nullptr) {
            outHeap = heap;
            return buffer;
        }
    }

    return nullptr;
}

static const void* GetExternalLooseBRSARBuffer(snd::SoundArchive::FileId fileId, bool waveData, u32 overrideSize) {
    if (fileId >= 1024 || overrideSize == 0) return nullptr;

    void** buffers = waveData ? sExternalWaveBuffers : sExternalFileBuffers;
    u8* attempts = waveData ? sExternalWaveAttempts : sExternalFileAttempts;
    if (buffers[fileId] != nullptr) return buffers[fileId];

    const u32 allocSize = nw4r::ut::RoundUp(overrideSize, 0x20);
    EGG::Heap* heap = nullptr;
    void* buffer = nullptr;

    if (waveData) {
        buffer = AllocAudioHeapOverrideBuffer(allocSize);
        if (buffer != nullptr) {
            OS::Report("[Pulsar] Loose BRSAR external wave using audio heap: fileId=%u addr=%p size=0x%X\n", fileId,
                       buffer, allocSize);
        }
    }

    if (buffer == nullptr) {
        buffer = AllocPersistentSoundOverrideBuffer(allocSize, heap);
    }

    if (buffer == nullptr) {
        if (attempts[fileId] == 0) {
            attempts[fileId] = 1;
            OS::Report("[Pulsar] Loose BRSAR external %s skipped: fileId=%u need 0x%X, no persistent heap\n",
                       waveData ? "wave" : "file", fileId, allocSize);
        }
        return nullptr;
    }

    attempts[fileId] = 0;

    const bool readOk = waveData ? IOOverrides::ReadLooseBRSAROverrideWaveData(fileId, buffer, overrideSize)
                                 : IOOverrides::ReadLooseBRSAROverrideFile(fileId, buffer, overrideSize);
    if (!readOk) {
        if (heap != nullptr) EGG::Heap::free(buffer, heap);
        if (attempts[fileId] == 0) {
            attempts[fileId] = 1;
            OS::Report("[Pulsar] Loose BRSAR external %s skipped: fileId=%u read failed\n",
                       waveData ? "wave" : "file", fileId);
        }
        return nullptr;
    }

    if (overrideSize < allocSize) memset(reinterpret_cast<u8*>(buffer) + overrideSize, 0, allocSize - overrideSize);
    OS::DCStoreRange(buffer, allocSize);

    buffers[fileId] = buffer;
    attempts[fileId] = 0;
    OS::Report("[Pulsar] Loose BRSAR external %s ready: fileId=%u addr=%p size=0x%X\n",
               waveData ? "wave" : "file", fileId, buffer, overrideSize);
    return buffer;
}

static bool TryGetGroupItemSlotCapacity(const snd::SoundArchive& archive, snd::SoundArchive::GroupId groupId, u32 itemCount,
                                        const snd::SoundArchive::GroupItemInfo& target, bool waveData, u32 groupSize,
                                        u32& outCapacity) {
    outCapacity = 0;

    const u32 targetOffset = waveData ? target.waveDataOffset : target.offset;
    const u32 targetSize = waveData ? target.waveDataSize : target.size;
    if (targetSize == 0 || targetOffset >= groupSize) return false;

    u32 nextOffset = groupSize;
    snd::SoundArchive::GroupItemInfo other;
    for (u32 index = 0; index < itemCount; ++index) {
        if (!sReadGroupItemInfo(&archive, groupId, index, &other)) continue;

        const u32 otherOffset = waveData ? other.waveDataOffset : other.offset;
        const u32 otherSize = waveData ? other.waveDataSize : other.size;
        if (otherSize == 0 || otherOffset <= targetOffset) continue;
        if (otherOffset < nextOffset) nextOffset = otherOffset;
    }

    if (nextOffset <= targetOffset) return false;
    outCapacity = nextOffset - targetOffset;
    return true;
}

static const void* FindGroupFileAddress(const snd::SoundArchivePlayer* player, snd::SoundArchive::FileId fileId, bool waveData,
                                        ResolvedBRSARTarget* outTarget) {
    if (outTarget != nullptr) {
        outTarget->address = nullptr;
        outTarget->capacity = 0;
        outTarget->groupId = 0;
        outTarget->groupIndex = 0;
        outTarget->kind = RESOLVEDTARGET_NONE;
        outTarget->padding[0] = 0;
        outTarget->padding[1] = 0;
        outTarget->padding[2] = 0;
    }

    if (player == nullptr || player->soundArchive == nullptr) return nullptr;

    snd::SoundArchive::FileInfo fileInfo;
    if (!sReadFileInfo(player->soundArchive, fileId, &fileInfo)) return nullptr;

    for (u32 index = 0; index < fileInfo.filePosCount; ++index) {
        snd::SoundArchive::FilePos filePos;
        if (!sReadFilePos(player->soundArchive, fileId, index, &filePos)) continue;

        u32 baseAddress = 0;
        u32* groupTable = player->groupTable;
        if (groupTable != nullptr && filePos.groupId < groupTable[0]) {
            baseAddress = groupTable[filePos.groupId * 2 + (waveData ? 2 : 1)];
        }
        if (baseAddress == 0) continue;

        snd::SoundArchive::GroupInfo groupInfo;
        snd::SoundArchive::GroupItemInfo itemInfo;
        if (!sReadGroupInfo(player->soundArchive, filePos.groupId, &groupInfo) ||
            !sReadGroupItemInfo(player->soundArchive, filePos.groupId, filePos.groupIndex, &itemInfo)) {
            continue;
        }

        const u32 offset = waveData ? itemInfo.waveDataOffset : itemInfo.offset;
        const u32 groupSize = waveData ? groupInfo.waveDataSize : groupInfo.size;
        u32 capacity = 0;
        if (!TryGetGroupItemSlotCapacity(*player->soundArchive, filePos.groupId, groupInfo.itemCount, itemInfo, waveData,
                                         groupSize, capacity)) {
            capacity = waveData ? itemInfo.waveDataSize : itemInfo.size;
        }

        const void* address = reinterpret_cast<const void*>(baseAddress + offset);
        if (outTarget != nullptr) {
            outTarget->address = address;
            outTarget->capacity = capacity;
            outTarget->groupId = filePos.groupId;
            outTarget->groupIndex = filePos.groupIndex;
            outTarget->kind = RESOLVEDTARGET_GROUP;
        }
        return address;
    }

    return nullptr;
}

static const void* GetOriginalFileAddress(const snd::SoundArchivePlayer* player, snd::SoundArchive::FileId fileId,
                                          ResolvedBRSARTarget* outTarget) {
    if (outTarget != nullptr) {
        outTarget->address = nullptr;
        outTarget->capacity = 0;
        outTarget->groupId = 0;
        outTarget->groupIndex = 0;
        outTarget->kind = RESOLVEDTARGET_NONE;
        outTarget->padding[0] = 0;
        outTarget->padding[1] = 0;
        outTarget->padding[2] = 0;
    }
    if (player == nullptr || player->soundArchive == nullptr) return nullptr;

    snd::SoundArchive::FileInfo fileInfo;
    const bool hasFileInfo = sReadFileInfo(player->soundArchive, fileId, &fileInfo);

    if (player->fileManager != nullptr) {
        const void* fileAddress = player->fileManager->GetFileAddress(fileId);
        if (fileAddress != nullptr) {
            if (outTarget != nullptr) {
                outTarget->address = fileAddress;
                outTarget->capacity = hasFileInfo ? fileInfo.fileSize : 0;
                outTarget->kind = RESOLVEDTARGET_FILE_MANAGER;
            }
            return fileAddress;
        }
    }

    const void* archiveAddress = player->soundArchive->detail_GetFileAddress(fileId);
    if (archiveAddress != nullptr) {
        if (outTarget != nullptr) {
            outTarget->address = archiveAddress;
            outTarget->capacity = hasFileInfo ? fileInfo.fileSize : 0;
            outTarget->kind = RESOLVEDTARGET_ARCHIVE;
        }
        return archiveAddress;
    }

    return FindGroupFileAddress(player, fileId, false, outTarget);
}

static const void* GetOriginalWaveDataAddress(const snd::SoundArchivePlayer* player, snd::SoundArchive::FileId fileId,
                                              ResolvedBRSARTarget* outTarget) {
    if (outTarget != nullptr) {
        outTarget->address = nullptr;
        outTarget->capacity = 0;
        outTarget->groupId = 0;
        outTarget->groupIndex = 0;
        outTarget->kind = RESOLVEDTARGET_NONE;
        outTarget->padding[0] = 0;
        outTarget->padding[1] = 0;
        outTarget->padding[2] = 0;
    }
    if (player == nullptr || player->soundArchive == nullptr) return nullptr;

    snd::SoundArchive::FileInfo fileInfo;
    const bool hasFileInfo = sReadFileInfo(player->soundArchive, fileId, &fileInfo);

    const void* archiveAddress = player->soundArchive->detail_GetWaveDataFileAddress(fileId);
    if (archiveAddress != nullptr) {
        if (outTarget != nullptr) {
            outTarget->address = archiveAddress;
            outTarget->capacity = hasFileInfo ? fileInfo.waveDataFileSize : 0;
            outTarget->kind = RESOLVEDTARGET_ARCHIVE;
        }
        return archiveAddress;
    }

    if (player->fileManager != nullptr) {
        const void* fileAddress = player->fileManager->GetFileWaveDataAddress(fileId);
        if (fileAddress != nullptr) {
            if (outTarget != nullptr) {
                outTarget->address = fileAddress;
                outTarget->capacity = hasFileInfo ? fileInfo.waveDataFileSize : 0;
                outTarget->kind = RESOLVEDTARGET_FILE_MANAGER;
            }
            return fileAddress;
        }
    }

    return FindGroupFileAddress(player, fileId, true, outTarget);
}

static void PatchResolvedAddress(snd::SoundArchive::FileId fileId, bool waveData, const ResolvedBRSARTarget& target) {
    if (target.address == nullptr || target.capacity == 0) return;

    u32 fileSize = 0;
    u32 waveDataSize = 0;
    if (!IOOverrides::GetLooseBRSAROverrideSizes(fileId, fileSize, waveDataSize)) return;

    const u32 overrideSize = waveData ? waveDataSize : fileSize;
    if (overrideSize == 0) return;

    const void** patchedCache = waveData ? sPatchedWaveAddresses : sPatchedFileAddresses;
    if (fileId < 1024 && patchedCache[fileId] == target.address) return;

    if (overrideSize > target.capacity) {
        OS::Report("[Pulsar] Loose BRSAR %s patch skipped: fileId=%u override=0x%X capacity=0x%X kind=%u group=%u\n",
                   waveData ? "wave" : "file", fileId, overrideSize, target.capacity, target.kind, target.groupId);
        return;
    }

    void* dest = const_cast<void*>(target.address);
    const bool readOk = waveData ? IOOverrides::ReadLooseBRSAROverrideWaveData(fileId, dest, overrideSize)
                                 : IOOverrides::ReadLooseBRSAROverrideFile(fileId, dest, overrideSize);
    if (!readOk) {
        OS::Report("[Pulsar] Loose BRSAR %s patch read failed: fileId=%u kind=%u group=%u\n", waveData ? "wave" : "file",
                   fileId, target.kind, target.groupId);
        return;
    }

    if (overrideSize < target.capacity) {
        memset(reinterpret_cast<u8*>(dest) + overrideSize, 0, target.capacity - overrideSize);
    }
    OS::DCStoreRange(dest, target.capacity);

    if (fileId < 1024) patchedCache[fileId] = target.address;
    OS::Report("[Pulsar] Loose BRSAR %s patched in place: fileId=%u kind=%u group=%u addr=%p size=0x%X cap=0x%X\n",
               waveData ? "wave" : "file", fileId, target.kind, target.groupId, target.address, overrideSize,
               target.capacity);
}

static void* LoadLooseBRSARFile(snd::detail::SoundArchiveLoader* loader, snd::SoundArchive::FileId fileId,
                                snd::SoundMemoryAllocatable* allocater) {
    if (loader == nullptr || allocater == nullptr) return nullptr;

    u32 fileSize = 0;
    u32 waveDataSize = 0;
    if (!IOOverrides::GetLooseBRSAROverrideSizes(fileId, fileSize, waveDataSize) || fileSize == 0) {
        return nullptr;
    }
    LogOverrideRequest(sLoggedFileRequests, fileId, "file", fileSize, waveDataSize);

    void* buffer = allocater->Alloc(fileSize);
    if (buffer == nullptr) {
        OS::Report("[Pulsar] Loose BRSAR override skipped: fileId=%u alloc 0x%X failed\n", fileId, fileSize);
        return nullptr;
    }

    if (!IOOverrides::ReadLooseBRSAROverrideFile(fileId, buffer, fileSize)) {
        OS::Report("[Pulsar] Loose BRSAR override skipped: fileId=%u read failed\n", fileId);
        return nullptr;
    }

    OS::DCStoreRange(buffer, fileSize);
    if (fileId < 1024) sPatchedFileAddresses[fileId] = buffer;
    OS::Report("[Pulsar] Loose BRSAR file loaded: fileId=%u addr=%p size=0x%X\n", fileId, buffer, fileSize);
    return buffer;
}

static void* LoadFileWithLooseBRSAROverride(snd::detail::SoundArchiveLoader* loader, snd::SoundArchive::FileId fileId,
                                            snd::SoundMemoryAllocatable* allocater) {
    void* buffer = LoadLooseBRSARFile(loader, fileId, allocater);
    if (buffer != nullptr) return buffer;
    return sOriginalLoadFile(loader, fileId, allocater);
}

static void* LoadWaveDataFileWithLooseBRSAROverride(snd::detail::SoundArchiveLoader* loader,
                                                    snd::SoundArchive::FileId fileId,
                                                    snd::SoundMemoryAllocatable* allocater) {
    if (loader == nullptr || allocater == nullptr) return nullptr;

    u32 fileSize = 0;
    u32 waveDataSize = 0;
    if (IOOverrides::GetLooseBRSAROverrideSizes(fileId, fileSize, waveDataSize) && waveDataSize > 0) {
        LogOverrideRequest(sLoggedWaveRequests, fileId, "wave", fileSize, waveDataSize);
        void* buffer = allocater->Alloc(waveDataSize);
        if (buffer == nullptr) {
            OS::Report("[Pulsar] Loose BRSAR wave override skipped: fileId=%u alloc 0x%X failed\n", fileId, waveDataSize);
        } else if (IOOverrides::ReadLooseBRSAROverrideWaveData(fileId, buffer, waveDataSize)) {
            OS::DCStoreRange(buffer, waveDataSize);
            if (fileId < 1024) sPatchedWaveAddresses[fileId] = buffer;
            OS::Report("[Pulsar] Loose BRSAR wave loaded: fileId=%u addr=%p size=0x%X\n", fileId, buffer, waveDataSize);
            return buffer;
        } else {
            OS::Report("[Pulsar] Loose BRSAR wave override skipped: fileId=%u read failed\n", fileId);
        }
    }

    return sOriginalLoadWaveDataFile(loader, fileId, allocater);
}

static const void* GetFileAddressWithLooseBRSAROverride(const snd::SoundArchivePlayer* player,
                                                        snd::SoundArchive::FileId fileId) {
    ResolvedBRSARTarget target;
    const void* address = GetOriginalFileAddress(player, fileId, &target);
    if (address != nullptr) LogResolvedLookup(sLoggedFileLookups, fileId, false, target);

    u32 fileSize = 0;
    u32 waveDataSize = 0;
    if (!IOOverrides::GetLooseBRSAROverrideSizes(fileId, fileSize, waveDataSize) || fileSize == 0) return address;

    if (address == nullptr || target.capacity < fileSize) {
        const void* external = GetExternalLooseBRSARBuffer(fileId, false, fileSize);
        if (external != nullptr) return external;
    }

    if (address != nullptr) PatchResolvedAddress(fileId, false, target);
    return address;
}

static const void* GetFileWaveDataAddressWithLooseBRSAROverride(const snd::SoundArchivePlayer* player,
                                                                snd::SoundArchive::FileId fileId) {
    ResolvedBRSARTarget target;
    const void* address = GetOriginalWaveDataAddress(player, fileId, &target);
    if (address != nullptr) LogResolvedLookup(sLoggedWaveLookups, fileId, true, target);

    u32 fileSize = 0;
    u32 waveDataSize = 0;
    if (!IOOverrides::GetLooseBRSAROverrideSizes(fileId, fileSize, waveDataSize) || waveDataSize == 0) return address;

    if (address == nullptr || target.capacity < waveDataSize) {
        const void* external = GetExternalLooseBRSARBuffer(fileId, true, waveDataSize);
        if (external != nullptr) return external;
    }

    if (address != nullptr) PatchResolvedAddress(fileId, true, target);
    return address;
}

static void PatchLoadedGroupWithLooseBRSAROverrides(const snd::SoundArchive& archive, snd::SoundArchive::GroupId groupId,
                                                    void* groupData, void* waveData) {
    if (groupData == nullptr) return;

    snd::SoundArchive::GroupInfo groupInfo;
    if (!sReadGroupInfo(&archive, groupId, &groupInfo)) return;
    if (groupInfo.itemCount == 0) return;

    snd::SoundArchive::GroupItemInfo item;
    for (u32 index = 0; index < groupInfo.itemCount; ++index) {
        if (!sReadGroupItemInfo(&archive, groupId, index, &item)) continue;

        u32 fileSize = 0;
        u32 waveDataSize = 0;
        if (!IOOverrides::GetLooseBRSAROverrideSizes(item.fileId, fileSize, waveDataSize) || fileSize == 0) {
            continue;
        }

        if (groupId < 512 && sLoggedGroupLoads[groupId] == 0) {
            sLoggedGroupLoads[groupId] = 1;
            OS::Report("[Pulsar] Loose BRSAR group load: group=%u items=%u groupData=%p waveData=%p\n", groupId,
                       groupInfo.itemCount, groupData, waveData);
        }
        OS::Report("[Pulsar] Loose BRSAR group candidate: group=%u fileId=%u override=0x%X/0x%X slot=0x%X/0x%X\n",
                   groupId, item.fileId, fileSize, waveDataSize, item.size, item.waveDataSize);

        u32 fileCapacity = 0;
        if (!TryGetGroupItemSlotCapacity(archive, groupId, groupInfo.itemCount, item, false, groupInfo.size, fileCapacity) ||
            fileCapacity < fileSize) {
            OS::Report("[Pulsar] Loose BRSAR override skipped in group %u: fileId=%u needs 0x%X bytes, slot has 0x%X\n",
                       groupId, item.fileId, fileSize, fileCapacity);
            continue;
        }

        u32 waveCapacity = 0;
        if (waveDataSize > 0) {
            if (waveData == nullptr || item.waveDataSize == 0 ||
                !TryGetGroupItemSlotCapacity(archive, groupId, groupInfo.itemCount, item, true, groupInfo.waveDataSize,
                                             waveCapacity) ||
                waveCapacity < waveDataSize) {
                OS::Report("[Pulsar] Loose BRSAR wave override skipped in group %u: fileId=%u needs 0x%X bytes, slot has 0x%X\n",
                           groupId, item.fileId, waveDataSize, waveCapacity);
                continue;
            }
        }

        u8* groupDest = reinterpret_cast<u8*>(groupData) + item.offset;
        if (!IOOverrides::ReadLooseBRSAROverrideFile(item.fileId, groupDest, fileSize)) {
            OS::Report("[Pulsar] Loose BRSAR override skipped in group %u: fileId=%u read failed\n", groupId, item.fileId);
            continue;
        }
        if (fileSize < item.size) {
            memset(groupDest + fileSize, 0, item.size - fileSize);
        }
        OS::DCStoreRange(groupDest, fileSize);
        if (item.fileId < 1024) sPatchedFileAddresses[item.fileId] = groupDest;
        OS::Report("[Pulsar] Loose BRSAR file patched on group load: fileId=%u group=%u addr=%p size=0x%X\n",
                   item.fileId, groupId, groupDest, fileSize);

        if (waveDataSize > 0) {
            u8* waveDest = reinterpret_cast<u8*>(waveData) + item.waveDataOffset;
            if (!IOOverrides::ReadLooseBRSAROverrideWaveData(item.fileId, waveDest, waveDataSize)) {
                OS::Report("[Pulsar] Loose BRSAR wave override skipped in group %u: fileId=%u read failed\n", groupId,
                           item.fileId);
                continue;
            }
            if (waveDataSize < item.waveDataSize) {
                memset(waveDest + waveDataSize, 0, item.waveDataSize - waveDataSize);
            }
            OS::DCStoreRange(waveDest, waveDataSize);
            if (item.fileId < 1024) sPatchedWaveAddresses[item.fileId] = waveDest;
            OS::Report("[Pulsar] Loose BRSAR wave patched on group load: fileId=%u group=%u addr=%p size=0x%X\n",
                       item.fileId, groupId, waveDest, waveDataSize);
        }
    }
}

static void* LoadGroupWithLooseBRSAROverride(snd::detail::SoundArchiveLoader* loader, u32 groupId,
                                             snd::SoundMemoryAllocatable* allocater, void** waveDataAddress,
                                             u32 loadBlockSize) {
    void* groupData = sOriginalLoadGroup(loader, groupId, allocater, waveDataAddress, loadBlockSize);
    if (groupData == nullptr || loader == nullptr) return groupData;

    void* waveData = (waveDataAddress != nullptr) ? *waveDataAddress : nullptr;
    PatchLoadedGroupWithLooseBRSAROverrides(loader->archive, groupId, groupData, waveData);
    return groupData;
}

kmCall(0x806fec3c, LoadFileWithLooseBRSAROverride);
kmCall(0x806fed2c, LoadFileWithLooseBRSAROverride);
kmCall(0x806fedc0, LoadFileWithLooseBRSAROverride);
kmCall(0x806fef00, LoadFileWithLooseBRSAROverride);
kmCall(0x806ff040, LoadFileWithLooseBRSAROverride);
kmCall(0x806ff32c, LoadFileWithLooseBRSAROverride);
kmCall(0x806ff404, LoadFileWithLooseBRSAROverride);

kmCall(0x806fee34, LoadWaveDataFileWithLooseBRSAROverride);
kmCall(0x806fef74, LoadWaveDataFileWithLooseBRSAROverride);
kmCall(0x806ff0b4, LoadWaveDataFileWithLooseBRSAROverride);

kmCall(0x800a2994, LoadGroupWithLooseBRSAROverride);
kmBranch(0x800a1560, GetFileAddressWithLooseBRSAROverride);
kmBranch(0x800a16b0, GetFileWaveDataAddressWithLooseBRSAROverride);

}  // namespace

}  // namespace Sound
}  // namespace Pulsar
