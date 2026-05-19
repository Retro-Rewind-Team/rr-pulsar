/*
 * Loose Archive Overrides
 *
 * Developed by patchzy as part of the Retro Rewind project.
 *
 * Copyright (C) Retro Rewind.
 * SPDX-License-Identifier: MIT
 * 
 * This code is licensed under the MIT License.
 *
 * Credit is not legally required, but if you use or adapt this system,
 * please consider crediting patchzy and/or Retro Rewind team.
 */

#include <kamek.hpp>
#include <CustomCharacters.hpp>
#include <PulsarSystem.hpp>
#include <IO/LooseArchiveOverrides.hpp>
#include <core/RK/RKSystem.hpp>
#include <core/nw4r/snd.hpp>
#include <core/rvl/OS/OSCache.hpp>
#include <core/rvl/dvd/dvd.hpp>
#include <core/rvl/os/OS.hpp>
#include <include/c_stdio.h>
#include <runtimeWrite.hpp>

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

enum ExternalBufferSource {
    EXTERNALBUFFER_NONE = 0,
    EXTERNALBUFFER_AUDIO_HEAP,
    EXTERNALBUFFER_PERSISTENT_HEAP,
    EXTERNALBUFFER_GROUP_ALLOCATER
};

struct ResolvedBRSARTarget {
    const void* address;
    u32 capacity;
    u32 groupId;
    u32 groupIndex;
    u8 kind;
    u8 padding[3];
};

kmRuntimeUse(0x800a0180);
kmRuntimeUse(0x800a0420);
kmRuntimeUse(0x8009fa10);
kmRuntimeUse(0x8009dff0);
kmRuntimeUse(0x8009e000);
kmRuntimeUse(0x8009dfc0);
kmRuntimeUse(0x8009dfd0);
static LoadFileFn sOriginalLoadFile = reinterpret_cast<LoadFileFn>(kmRuntimeAddr(0x800a0180));
static LoadWaveDataFileFn sOriginalLoadWaveDataFile = reinterpret_cast<LoadWaveDataFileFn>(kmRuntimeAddr(0x800a0420));
static LoadGroupFn sOriginalLoadGroup = reinterpret_cast<LoadGroupFn>(kmRuntimeAddr(0x8009fa10));
static ReadFileInfoFn sReadFileInfo = reinterpret_cast<ReadFileInfoFn>(kmRuntimeAddr(0x8009dff0));
static ReadFilePosFn sReadFilePos = reinterpret_cast<ReadFilePosFn>(kmRuntimeAddr(0x8009e000));
static ReadGroupInfoFn sReadGroupInfo = reinterpret_cast<ReadGroupInfoFn>(kmRuntimeAddr(0x8009dfc0));
static ReadGroupItemInfoFn sReadGroupItemInfo = reinterpret_cast<ReadGroupItemInfoFn>(kmRuntimeAddr(0x8009dfd0));
static const void* sPatchedFileAddresses[1024] = {};
static const void* sPatchedWaveAddresses[1024] = {};
static void* sExternalFileBuffers[1024] = {};
static void* sExternalWaveBuffers[1024] = {};
static EGG::Heap* sExternalFileBufferHeaps[1024] = {};
static EGG::Heap* sExternalWaveBufferHeaps[1024] = {};
static u8 sExternalFileBufferSources[1024] = {};
static u8 sExternalWaveBufferSources[1024] = {};
static u8 sExternalFileAttempts[1024] = {};
static u8 sExternalWaveAttempts[1024] = {};

struct LooseVoiceLayout {
    u32 fileSize;
    u32 waveOffset;
    u32 waveSize;
};

static u32 ReadBE32(const void* data) {
    const u8* bytes = reinterpret_cast<const u8*>(data);
    return (static_cast<u32>(bytes[0]) << 24) | (static_cast<u32>(bytes[1]) << 16) |
           (static_cast<u32>(bytes[2]) << 8) | static_cast<u32>(bytes[3]);
}

static inline u32 Align32(u32 value) {
    return nw4r::ut::RoundUp(value, 0x20);
}

static void InvalidateRange(void* addr, u32 size) {
    if (addr == nullptr || size == 0) return;
    const u32 start = reinterpret_cast<u32>(addr) & ~0x1F;
    const u32 end = Align32(reinterpret_cast<u32>(addr) + size);
    OS::DCInvalidateRange(reinterpret_cast<void*>(start), end - start);
}

static bool ReadOpenedDVDFileRange(DVD::FileInfo& info, void* dest, u32 size, u32 offset) {
    InvalidateRange(dest, size);
    const s32 read = DVD::ReadPrio(&info, dest, static_cast<s32>(size), static_cast<s32>(offset), 2);
    return read == static_cast<s32>(size);
}

static bool FindEmbeddedRWAROffset(DVD::FileInfo& info, u32 fileSize, u32 searchStart, u32& outOffset, u32& outSize) {
    outOffset = 0;
    outSize = 0;
    if (searchStart >= fileSize) return false;

    u8 exactHeader[0x20] __attribute__((aligned(32)));
    if (searchStart + sizeof(exactHeader) <= fileSize && ReadOpenedDVDFileRange(info, exactHeader, sizeof(exactHeader), searchStart) &&
        memcmp(exactHeader, "RWAR", 4) == 0) {
        const u32 exactSize = ReadBE32(exactHeader + 8);
        if (exactSize >= 0x20 && searchStart + exactSize <= fileSize) {
            outOffset = searchStart;
            outSize = exactSize;
            return true;
        }
    }

    enum { kChunkSize = 0x800 };
    u8 chunk[kChunkSize] __attribute__((aligned(32)));
    u32 offset = Align32(searchStart + 0x20);
    while (offset + 0x20 <= fileSize) {
        u32 remaining = fileSize - offset;
        u32 readSize = remaining >= kChunkSize ? kChunkSize : remaining;
        readSize &= ~0x1F;
        if (readSize < 0x20) break;

        if (!ReadOpenedDVDFileRange(info, chunk, readSize, offset)) {
            offset += readSize;
            continue;
        }

        for (u32 chunkOffset = 0; chunkOffset + 0x20 <= readSize; chunkOffset += 0x20) {
            if (memcmp(chunk + chunkOffset, "RWAR", 4) != 0) continue;

            const u32 candidateSize = ReadBE32(chunk + chunkOffset + 8);
            const u32 candidateOffset = offset + chunkOffset;
            if (candidateSize >= 0x20 && candidateOffset + candidateSize <= fileSize) {
                outOffset = candidateOffset;
                outSize = candidateSize;
                return true;
            }
        }

        offset += readSize;
    }

    return false;
}

static void CopyUpperPostfix(char* dest, u32 destSize, const char* postfix) {
    if (dest == nullptr || destSize == 0) return;
    u32 i = 0;
    if (postfix != nullptr) {
        for (; i + 1 < destSize && postfix[i] != '\0'; ++i) {
            char c = postfix[i];
            if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
            dest[i] = c;
        }
    }
    dest[i] = '\0';
}

static bool BuildLooseVoicePath(const char* postfix, const char* suffix, const char* extension, const char* voiceName, char* path,
                                u32 pathSize) {
    char upperPostfix[32];
    CopyUpperPostfix(upperPostfix, sizeof(upperPostfix), postfix);
    if (upperPostfix[0] == '\0' || suffix == nullptr || extension == nullptr) return false;
    const int written = voiceName == nullptr ? snprintf(path, pathSize, "/sound/GRP_VO_%s_%s.%s", upperPostfix, suffix, extension)
                                             : snprintf(path, pathSize, "/sound/GRP_VO_%s_%s.%s.%s", upperPostfix, suffix,
                                                        extension, voiceName);
    return written > 0 && static_cast<u32>(written) < pathSize;
}

static bool OpenLooseVoiceFile(const char* postfix, const char* suffix, const char* extension, const char* voiceName,
                               DVD::FileInfo& info, char* path, u32 pathSize) {
    if (!BuildLooseVoicePath(postfix, suffix, extension, nullptr, path, pathSize)) return false;
    if (DVD::Open(path, &info)) return true;
    if (voiceName == nullptr) return false;
    if (!BuildLooseVoicePath(postfix, suffix, extension, voiceName, path, pathSize)) return false;
    return DVD::Open(path, &info);
}

static bool ReadLooseVoiceLayout(DVD::FileInfo& info, const char* magic, LooseVoiceLayout& outLayout) {
    outLayout.fileSize = 0;
    outLayout.waveOffset = 0;
    outLayout.waveSize = 0;
    if (info.length < 0x20) return false;

    u8 header[0x20] __attribute__((aligned(32)));
    if (!ReadOpenedDVDFileRange(info, header, sizeof(header), 0)) return false;
    if (memcmp(header, magic, 4) != 0) return false;

    const u32 fileSize = ReadBE32(header + 8);
    if (fileSize < 0x20 || fileSize > static_cast<u32>(info.length)) return false;
    outLayout.fileSize = fileSize;

    if (memcmp(magic, "RWSD", 4) == 0 || memcmp(magic, "RBNK", 4) == 0) {
        const u32 searchStart = Align32(fileSize);
        FindEmbeddedRWAROffset(info, static_cast<u32>(info.length), searchStart, outLayout.waveOffset, outLayout.waveSize);
    }
    return true;
}

static bool PreloadLooseCustomVoiceBufferWithAllocater(snd::SoundMemoryAllocatable* allocater,
                                                       snd::SoundArchive::FileId fileId, bool waveData,
                                                       DVD::FileInfo& info, const char* path, u32 readOffset,
                                                       u32 overrideSize) {
    if (allocater == nullptr || fileId >= 1024 || overrideSize == 0) return false;

    void** buffers = waveData ? sExternalWaveBuffers : sExternalFileBuffers;
    u8* attempts = waveData ? sExternalWaveAttempts : sExternalFileAttempts;
    if (buffers[fileId] != nullptr) return true;

    const u32 allocSize = nw4r::ut::RoundUp(overrideSize, 0x20);
    void* buffer = allocater->Alloc(allocSize);
    if (buffer == nullptr) {
        if (attempts[fileId] == 0) {
            attempts[fileId] = 1;
            OS::Report("[Pulsar] Loose custom voice external %s skipped: fileId=%u path='%s' alloc 0x%X failed\n",
                       waveData ? "wave" : "file", fileId, path != nullptr ? path : "<missing>", allocSize);
        }
        return false;
    }

    if (!ReadOpenedDVDFileRange(info, buffer, overrideSize, readOffset)) {
        if (attempts[fileId] == 0) {
            attempts[fileId] = 1;
            OS::Report("[Pulsar] Loose custom voice external %s skipped: fileId=%u path='%s' read failed\n",
                       waveData ? "wave" : "file", fileId, path != nullptr ? path : "<missing>");
        }
        return false;
    }

    if (overrideSize < allocSize) memset(reinterpret_cast<u8*>(buffer) + overrideSize, 0, allocSize - overrideSize);
    OS::DCStoreRange(buffer, allocSize);

    buffers[fileId] = buffer;
    EGG::Heap** bufferHeaps = waveData ? sExternalWaveBufferHeaps : sExternalFileBufferHeaps;
    u8* bufferSources = waveData ? sExternalWaveBufferSources : sExternalFileBufferSources;
    bufferHeaps[fileId] = nullptr;
    bufferSources[fileId] = EXTERNALBUFFER_GROUP_ALLOCATER;
    attempts[fileId] = 0;
    return true;
}

static void ResetLooseBRSARExternalBuffers() {
    for (u32 fileId = 0; fileId < 1024; ++fileId) {
        if (sExternalFileBuffers[fileId] != nullptr && sExternalFileBufferSources[fileId] == EXTERNALBUFFER_PERSISTENT_HEAP &&
            sExternalFileBufferHeaps[fileId] != nullptr) {
            EGG::Heap::free(sExternalFileBuffers[fileId], sExternalFileBufferHeaps[fileId]);
        }
        if (sExternalWaveBuffers[fileId] != nullptr && sExternalWaveBufferSources[fileId] == EXTERNALBUFFER_PERSISTENT_HEAP &&
            sExternalWaveBufferHeaps[fileId] != nullptr) {
            EGG::Heap::free(sExternalWaveBuffers[fileId], sExternalWaveBufferHeaps[fileId]);
        }

        sPatchedFileAddresses[fileId] = nullptr;
        sPatchedWaveAddresses[fileId] = nullptr;
        sExternalFileBuffers[fileId] = nullptr;
        sExternalWaveBuffers[fileId] = nullptr;
        sExternalFileBufferHeaps[fileId] = nullptr;
        sExternalWaveBufferHeaps[fileId] = nullptr;
        sExternalFileBufferSources[fileId] = EXTERNALBUFFER_NONE;
        sExternalWaveBufferSources[fileId] = EXTERNALBUFFER_NONE;
        sExternalFileAttempts[fileId] = 0;
        sExternalWaveAttempts[fileId] = 0;
    }
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

static const void* PreloadLooseBRSARBufferWithAllocater(snd::SoundMemoryAllocatable* allocater, snd::SoundArchive::FileId fileId,
                                                        bool waveData, u32 overrideSize) {
    if (allocater == nullptr || fileId >= 1024 || overrideSize == 0) return nullptr;

    void** buffers = waveData ? sExternalWaveBuffers : sExternalFileBuffers;
    u8* attempts = waveData ? sExternalWaveAttempts : sExternalFileAttempts;
    if (buffers[fileId] != nullptr) return buffers[fileId];

    const u32 allocSize = nw4r::ut::RoundUp(overrideSize, 0x20);
    void* buffer = allocater->Alloc(allocSize);
    if (buffer == nullptr) {
        if (attempts[fileId] == 0) {
            attempts[fileId] = 1;
            OS::Report("[Pulsar] Loose BRSAR preloaded external %s skipped: fileId=%u alloc 0x%X failed\n",
                       waveData ? "wave" : "file", fileId, allocSize);
        }
        return nullptr;
    }

    const bool readOk = waveData ? IOOverrides::ReadLooseBRSAROverrideWaveData(fileId, buffer, overrideSize)
                                 : IOOverrides::ReadLooseBRSAROverrideFile(fileId, buffer, overrideSize);
    if (!readOk) {
        if (attempts[fileId] == 0) {
            attempts[fileId] = 1;
            OS::Report("[Pulsar] Loose BRSAR preloaded external %s skipped: fileId=%u read failed\n",
                       waveData ? "wave" : "file", fileId);
        }
        return nullptr;
    }

    if (overrideSize < allocSize) memset(reinterpret_cast<u8*>(buffer) + overrideSize, 0, allocSize - overrideSize);
    OS::DCStoreRange(buffer, allocSize);

    buffers[fileId] = buffer;
    EGG::Heap** bufferHeaps = waveData ? sExternalWaveBufferHeaps : sExternalFileBufferHeaps;
    u8* bufferSources = waveData ? sExternalWaveBufferSources : sExternalFileBufferSources;
    bufferHeaps[fileId] = nullptr;
    bufferSources[fileId] = EXTERNALBUFFER_GROUP_ALLOCATER;
    attempts[fileId] = 0;
    return buffer;
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
    EGG::Heap** bufferHeaps = waveData ? sExternalWaveBufferHeaps : sExternalFileBufferHeaps;
    u8* bufferSources = waveData ? sExternalWaveBufferSources : sExternalFileBufferSources;
    bufferHeaps[fileId] = heap;
    bufferSources[fileId] = (waveData && heap == nullptr) ? EXTERNALBUFFER_AUDIO_HEAP : EXTERNALBUFFER_PERSISTENT_HEAP;
    attempts[fileId] = 0;
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
}

static const char* LooseVoiceExtensionForGroupItem(const u8* groupData, const snd::SoundArchive::GroupItemInfo& item,
                                                   const char*& magic) {
    magic = nullptr;
    if (groupData == nullptr || item.size < 4) return nullptr;
    const u8* data = groupData + item.offset;
    if (memcmp(data, "RWSD", 4) == 0) {
        magic = "RWSD";
        return "brwsd";
    }
    if (memcmp(data, "RBNK", 4) == 0) {
        magic = "RBNK";
        return "brbnk";
    }
    if (memcmp(data, "RSEQ", 4) == 0) {
        magic = "RSEQ";
        return "brseq";
    }
    return nullptr;
}

static void PatchLoadedGroupItemWithLooseCustomVoice(const snd::SoundArchive& archive, snd::SoundArchive::GroupId groupId,
                                                     snd::SoundMemoryAllocatable* allocater, u32 itemCount,
                                                     const snd::SoundArchive::GroupItemInfo& item, u32 groupSize,
                                                     u32 waveDataSize, void* groupData, void* waveData) {
    const char* groupSuffix = nullptr;
    const char* voiceName = nullptr;
    const char* postfix = CustomCharacters::GetLooseVoicePostfixForGroup(groupId, groupSuffix, voiceName);
    if (postfix == nullptr || groupSuffix == nullptr) return;

    const char* magic = nullptr;
    const char* extension = LooseVoiceExtensionForGroupItem(static_cast<const u8*>(groupData), item, magic);
    if (extension == nullptr) return;

    char path[0x80];
    DVD::FileInfo info;
    if (!OpenLooseVoiceFile(postfix, groupSuffix, extension, voiceName, info, path, sizeof(path))) return;

    LooseVoiceLayout layout;
    if (!ReadLooseVoiceLayout(info, magic, layout)) {
        DVD::Close(&info);
        OS::Report("[Pulsar] Loose custom voice skipped in group %u: invalid '%s'\n", groupId, path);
        return;
    }

    u32 fileCapacity = 0;
    const bool canPatchFileInGroup =
        TryGetGroupItemSlotCapacity(archive, groupId, itemCount, item, false, groupSize, fileCapacity) &&
        fileCapacity >= layout.fileSize;

    if (canPatchFileInGroup) {
        u8* groupDest = reinterpret_cast<u8*>(groupData) + item.offset;
        if (!ReadOpenedDVDFileRange(info, groupDest, layout.fileSize, 0)) {
            OS::Report("[Pulsar] Loose custom voice skipped in group %u: read failed '%s'\n", groupId, path);
        } else {
            if (layout.fileSize < item.size) memset(groupDest + layout.fileSize, 0, item.size - layout.fileSize);
            OS::DCStoreRange(groupDest, item.size);
            if (item.fileId < 1024) sPatchedFileAddresses[item.fileId] = groupDest;
        }
    } else {
        const bool externalReady = PreloadLooseCustomVoiceBufferWithAllocater(allocater, item.fileId, false, info, path, 0,
                                                                              layout.fileSize);
        OS::Report("[Pulsar] Loose custom voice cannot fit in group %u: '%s' needs 0x%X bytes, slot has 0x%X; %s\n",
                   groupId, path, layout.fileSize, fileCapacity,
                   externalReady ? "external fallback ready" : "override unavailable");
    }

    if (layout.waveSize > 0) {
        u32 waveCapacity = 0;
        const bool canPatchWaveInGroup =
            waveData != nullptr && item.waveDataSize != 0 &&
            TryGetGroupItemSlotCapacity(archive, groupId, itemCount, item, true, waveDataSize, waveCapacity) &&
            waveCapacity >= layout.waveSize;
        if (canPatchWaveInGroup) {
            u8* waveDest = reinterpret_cast<u8*>(waveData) + item.waveDataOffset;
            if (!ReadOpenedDVDFileRange(info, waveDest, layout.waveSize, layout.waveOffset)) {
                OS::Report("[Pulsar] Loose custom voice wave skipped in group %u: read failed '%s'\n", groupId, path);
            } else {
                if (layout.waveSize < item.waveDataSize) memset(waveDest + layout.waveSize, 0, item.waveDataSize - layout.waveSize);
                OS::DCStoreRange(waveDest, item.waveDataSize);
                if (item.fileId < 1024) sPatchedWaveAddresses[item.fileId] = waveDest;
            }
        } else {
            const bool externalReady = PreloadLooseCustomVoiceBufferWithAllocater(allocater, item.fileId, true, info, path,
                                                                                  layout.waveOffset, layout.waveSize);
            OS::Report("[Pulsar] Loose custom voice wave cannot fit in group %u: '%s' needs 0x%X bytes, slot has 0x%X; %s\n",
                       groupId, path, layout.waveSize, waveCapacity,
                       externalReady ? "external fallback ready" : "override unavailable");
        }
    }

    DVD::Close(&info);
}

static void* LoadLooseBRSARFile(snd::detail::SoundArchiveLoader* loader, snd::SoundArchive::FileId fileId,
                                snd::SoundMemoryAllocatable* allocater) {
    if (loader == nullptr || allocater == nullptr) return nullptr;

    u32 fileSize = 0;
    u32 waveDataSize = 0;
    if (!IOOverrides::GetLooseBRSAROverrideSizes(fileId, fileSize, waveDataSize) || fileSize == 0) {
        return nullptr;
    }

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
        void* buffer = allocater->Alloc(waveDataSize);
        if (buffer == nullptr) {
            OS::Report("[Pulsar] Loose BRSAR wave override skipped: fileId=%u alloc 0x%X failed\n", fileId, waveDataSize);
        } else if (IOOverrides::ReadLooseBRSAROverrideWaveData(fileId, buffer, waveDataSize)) {
            OS::DCStoreRange(buffer, waveDataSize);
            if (fileId < 1024) sPatchedWaveAddresses[fileId] = buffer;
            return buffer;
        } else {
            OS::Report("[Pulsar] Loose BRSAR wave override skipped: fileId=%u read failed\n", fileId);
        }
    }

    return sOriginalLoadWaveDataFile(loader, fileId, allocater);
}

static const void* GetFileAddressWithLooseBRSAROverride(const snd::SoundArchivePlayer* player,
                                                        snd::SoundArchive::FileId fileId) {
    if (fileId < 1024 && sExternalFileBuffers[fileId] != nullptr) return sExternalFileBuffers[fileId];

    ResolvedBRSARTarget target;
    const void* address = GetOriginalFileAddress(player, fileId, &target);

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
    if (fileId < 1024 && sExternalWaveBuffers[fileId] != nullptr) return sExternalWaveBuffers[fileId];

    ResolvedBRSARTarget target;
    const void* address = GetOriginalWaveDataAddress(player, fileId, &target);

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
                                                    snd::SoundMemoryAllocatable* allocater, void* groupData, void* waveData) {
    if (groupData == nullptr) return;

    snd::SoundArchive::GroupInfo groupInfo;
    if (!sReadGroupInfo(&archive, groupId, &groupInfo)) return;
    if (groupInfo.itemCount == 0) return;

    snd::SoundArchive::GroupItemInfo item;
    for (u32 index = 0; index < groupInfo.itemCount; ++index) {
        if (!sReadGroupItemInfo(&archive, groupId, index, &item)) continue;

        u32 fileSize = 0;
        u32 waveDataSize = 0;
        if (IOOverrides::GetLooseBRSAROverrideSizes(item.fileId, fileSize, waveDataSize) && fileSize != 0) {
            u32 fileCapacity = 0;
            const bool canPatchFileInGroup =
                TryGetGroupItemSlotCapacity(archive, groupId, groupInfo.itemCount, item, false, groupInfo.size, fileCapacity) &&
                fileCapacity >= fileSize;

            u32 waveCapacity = 0;
            bool canPatchWaveInGroup = false;
            if (waveDataSize > 0) {
                canPatchWaveInGroup =
                    waveData != nullptr && item.waveDataSize != 0 &&
                    TryGetGroupItemSlotCapacity(archive, groupId, groupInfo.itemCount, item, true, groupInfo.waveDataSize,
                                                waveCapacity) &&
                    waveCapacity >= waveDataSize;
            }

            if (canPatchFileInGroup) {
                u8* groupDest = reinterpret_cast<u8*>(groupData) + item.offset;
                if (!IOOverrides::ReadLooseBRSAROverrideFile(item.fileId, groupDest, fileSize)) {
                    OS::Report("[Pulsar] Loose BRSAR override skipped in group %u: fileId=%u read failed\n", groupId,
                               item.fileId);
                } else {
                    if (fileSize < item.size) {
                        memset(groupDest + fileSize, 0, item.size - fileSize);
                    }
                    OS::DCStoreRange(groupDest, fileSize);
                    if (item.fileId < 1024) sPatchedFileAddresses[item.fileId] = groupDest;
                }
            } else {
                const void* external = PreloadLooseBRSARBufferWithAllocater(allocater, item.fileId, false, fileSize);
                OS::Report("[Pulsar] Loose BRSAR file override cannot fit in group %u: fileId=%u needs 0x%X bytes, slot has 0x%X; %s\n",
                           groupId, item.fileId, fileSize, fileCapacity,
                           (external != nullptr) ? "external fallback ready" : "override unavailable");
            }

            if (waveDataSize > 0 && canPatchWaveInGroup) {
                u8* waveDest = reinterpret_cast<u8*>(waveData) + item.waveDataOffset;
                if (!IOOverrides::ReadLooseBRSAROverrideWaveData(item.fileId, waveDest, waveDataSize)) {
                    OS::Report("[Pulsar] Loose BRSAR wave override skipped in group %u: fileId=%u read failed\n", groupId,
                               item.fileId);
                } else {
                    if (waveDataSize < item.waveDataSize) {
                        memset(waveDest + waveDataSize, 0, item.waveDataSize - waveDataSize);
                    }
                    OS::DCStoreRange(waveDest, waveDataSize);
                    if (item.fileId < 1024) sPatchedWaveAddresses[item.fileId] = waveDest;
                }
            } else if (waveDataSize > 0) {
                const void* external = PreloadLooseBRSARBufferWithAllocater(allocater, item.fileId, true, waveDataSize);
                OS::Report("[Pulsar] Loose BRSAR wave override cannot fit in group %u: fileId=%u needs 0x%X bytes, slot has 0x%X; %s\n",
                           groupId, item.fileId, waveDataSize, waveCapacity,
                           (external != nullptr) ? "external fallback ready" : "override unavailable");
            }
        }

        PatchLoadedGroupItemWithLooseCustomVoice(archive, groupId, allocater, groupInfo.itemCount, item, groupInfo.size,
                                                 groupInfo.waveDataSize, groupData, waveData);
    }
}

static void* LoadGroupWithLooseBRSAROverride(snd::detail::SoundArchiveLoader* loader, u32 groupId,
                                             snd::SoundMemoryAllocatable* allocater, void** waveDataAddress,
                                             u32 loadBlockSize) {
    void* groupData = sOriginalLoadGroup(loader, groupId, allocater, waveDataAddress, loadBlockSize);
    if (groupData == nullptr || loader == nullptr) return groupData;

    void* waveData = (waveDataAddress != nullptr) ? *waveDataAddress : nullptr;
    PatchLoadedGroupWithLooseBRSAROverrides(loader->archive, groupId, allocater, groupData, waveData);
    return groupData;
}

static RaceLoadHook ResetLooseBRSARExternalBuffersOnRaceLoad(ResetLooseBRSARExternalBuffers);
static SectionLoadHook ResetLooseBRSARExternalBuffersOnSectionLoad(ResetLooseBRSARExternalBuffers);

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
