#include <kamek.hpp>
#include <MarioKartWii/Archive/ArchiveFile.hpp>
#include <core/RK/RKSystem.hpp>
#include <core/egg/Decomp.hpp>
#include <core/egg/mem/Heap.hpp>
#include <core/rvl/os/OS.hpp>
#include <core/rvl/os/OSCache.hpp>
#include <core/nw4r/ut/Misc.hpp>
#include <IO/LooseArchiveOverrides.hpp>

namespace Pulsar {
namespace IOOverrides {

static void ClearCompressedArchive(ArchiveFile* file) {
    if (file->compressedArchive != nullptr && file->dumpHeap != nullptr) {
        EGG::Heap::free(file->compressedArchive, file->dumpHeap);
    }
    file->compressedArchive = nullptr;
    file->compressedArchiveSize = 0;
    file->dumpHeap = nullptr;
}

static void FailDecompress(ArchiveFile* file) {
    ClearCompressedArchive(file);
    file->rawArchive = nullptr;
    file->archiveSize = 0;
    file->archiveHeap = nullptr;
    file->status = ARCHIVE_STATUS_NONE;
}

static u8* AllocDecompressedArchive(u32 allocSize, EGG::Heap* primaryHeap, EGG::Heap* fallbackHeap,
                                    EGG::Heap*& outHeap) {
    outHeap = primaryHeap;
    u8* buffer = static_cast<u8*>(EGG::Heap::alloc(allocSize, 0x20, primaryHeap));
    if (buffer == nullptr && fallbackHeap != nullptr && fallbackHeap != primaryHeap) {
        outHeap = fallbackHeap;
        buffer = static_cast<u8*>(EGG::Heap::alloc(allocSize, 0x20, fallbackHeap));
    }
    return buffer;
}

static EGG::Heap* SelectStructuralDecodeScratchHeap(const char* archiveBaseLower, u32 allocSize,
                                                    EGG::Heap* archiveHeap, EGG::Heap* dumpHeap) {
    if (!HasStructuralLooseOverrides(archiveBaseLower)) return nullptr;

    EGG::Heap* candidates[3];
    candidates[0] = dumpHeap;
    candidates[1] = RKSystem::mInstance.EGGRootMEM2;
    candidates[2] = RKSystem::mInstance.EGGRootMEM1;

    for (u32 i = 0; i < 3; ++i) {
        EGG::Heap* candidate = candidates[i];
        if (candidate == nullptr || candidate == archiveHeap) continue;
        bool alreadyChecked = false;
        for (u32 j = 0; j < i; ++j) {
            if (candidates[j] == candidate) alreadyChecked = true;
        }
        if (alreadyChecked) continue;

        const u32 available = candidate->getAllocatableSize(0x20);
        if (available >= allocSize) return candidate;
    }

    return nullptr;
}

static void SafeDecompress(ArchiveFile* file, const char* path, EGG::Heap* heap, EGG::Archive::FileInfo* info) {
    u8* compressedData = static_cast<u8*>(file->compressedArchive);
    if (compressedData == nullptr) {
        FailDecompress(file);
        return;
    }

    u32 expandSize = EGG::Decomp::getExpandSize(compressedData);
    if (expandSize == 0) {
        FailDecompress(file);
        return;
    }

    char archiveBaseLower[OVERRIDE_MAX_NAME];
    archiveBaseLower[0] = '\0';
    // This gate is cheap on purpose: skip all index work for non-SZS loads before allocating extra scratch logic.
    const bool canApplyOverrides = ShouldApplyLooseOverrides(path, archiveBaseLower, sizeof(archiveBaseLower));
    const u32 allocSize = nw4r::ut::RoundUp(expandSize, 0x20);
    EGG::Heap* archiveHeap = nullptr;
    EGG::Heap* sourceArchiveHeap = nullptr;
    u8* decompressedBuffer = nullptr;
    if (canApplyOverrides) {
        sourceArchiveHeap = SelectStructuralDecodeScratchHeap(archiveBaseLower, allocSize, heap, file->dumpHeap);
    }
    if (sourceArchiveHeap != nullptr) {
        archiveHeap = heap;
        decompressedBuffer = static_cast<u8*>(EGG::Heap::alloc(allocSize, 0x20, sourceArchiveHeap));
    }
    if (decompressedBuffer == nullptr) {
        decompressedBuffer = AllocDecompressedArchive(allocSize, heap, file->dumpHeap, sourceArchiveHeap);
        archiveHeap = sourceArchiveHeap;
    }

    if (decompressedBuffer == nullptr) {
        OS::Report("[Pulsar] ArchiveFile::Decompress allocation failed! Size: 0x%X\n", allocSize);
        FailDecompress(file);
        return;
    }

    EGG::Decomp::decodeSZS(compressedData, decompressedBuffer);

    u32 appliedOverrides = 0;
    u32 patchedNodes = 0;
    u32 missingOverrides = 0;
    u32 finalSize = expandSize;

    u8* archiveBase = decompressedBuffer;
    if (canApplyOverrides) {
        // `ApplyLooseOverrides()` may swap `archiveBase` to a repacked buffer on another heap.
        ApplyLooseOverrides(archiveBaseLower, archiveBase, finalSize, sourceArchiveHeap, archiveHeap, &appliedOverrides,
                            &patchedNodes, &missingOverrides, compressedData);
    }
    if (archiveBase == decompressedBuffer) {
        archiveHeap = sourceArchiveHeap;
    }

    ClearCompressedArchive(file);

    file->archiveSize = finalSize;
    file->rawArchive = archiveBase;
    file->archiveHeap = archiveHeap;

    OS::DCStoreRange(archiveBase, finalSize);
    file->status = ARCHIVE_STATUS_DECOMPRESSED;
}
kmBranch(0x80519508, SafeDecompress);

}  // namespace IOOverrides
}  // namespace Pulsar
