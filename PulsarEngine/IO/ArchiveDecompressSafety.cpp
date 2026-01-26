#include <kamek.hpp>
#include <MarioKartWii/Archive/ArchiveFile.hpp>
#include <core/egg/Decomp.hpp>
#include <core/rvl/os/OS.hpp>
#include <core/rvl/os/OSCache.hpp>
#include <core/nw4r/ut/Misc.hpp>
#include <IO/LooseArchiveOverrides.hpp>

namespace Pulsar {
namespace IOOverrides {

static void SafeDecompress(ArchiveFile* file, const char* path, EGG::Heap* heap, EGG::Archive::FileInfo* info) {
    u8* compressedData = static_cast<u8*>(file->compressedArchive);
    if (compressedData == nullptr) {
        return;
    }

    u32 expandSize = EGG::Decomp::getExpandSize(compressedData);
    if (expandSize == 0) {
        return;
    }

    char archiveBaseLower[OVERRIDE_MAX_NAME];
    archiveBaseLower[0] = '\0';
    u32 applicableOverrides = 0;
    u32 overridesTotalAligned = 0;
    const bool canApplyOverrides = ShouldApplyLooseOverrides(path, archiveBaseLower, sizeof(archiveBaseLower));
    if (canApplyOverrides) {
        overridesTotalAligned = GetLooseOverridesTotalAligned(archiveBaseLower, &applicableOverrides);
    }

    const u32 allocSize = nw4r::ut::RoundUp(expandSize + overridesTotalAligned, 0x20);
    void* buffer = EGG::Heap::alloc(allocSize, 0x20, heap);
    u8* decompressedBuffer = static_cast<u8*>(buffer);

    if (decompressedBuffer == nullptr) {
        OS::Report("[Pulsar] ArchiveFile::Decompress allocation failed! Size: 0x%X\n", allocSize);
        return;
    }

    EGG::Decomp::decodeSZS(compressedData, decompressedBuffer);

    if (file->compressedArchive != nullptr && file->dumpHeap != nullptr) {
        EGG::Heap::free(file->compressedArchive, file->dumpHeap);
        file->compressedArchive = nullptr;
        file->compressedArchiveSize = 0;
        file->dumpHeap = nullptr;
    }

    u32 appliedOverrides = 0;
    u32 patchedNodes = 0;
    u32 missingOverrides = 0;
    u32 finalSize = allocSize;

    if (canApplyOverrides && overridesTotalAligned > 0) {
        ApplyLooseOverrides(archiveBaseLower, decompressedBuffer, expandSize, &appliedOverrides, &patchedNodes, &missingOverrides);
    } else if (canApplyOverrides && overridesTotalAligned == 0 && IsDVDOverrideSource()) {
        u8* newBuffer = decompressedBuffer;
        if (ApplyLooseOverridesDVD(archiveBaseLower, &newBuffer, expandSize, heap, &finalSize, &appliedOverrides, &patchedNodes, &missingOverrides)) {
            decompressedBuffer = newBuffer;
        }
    }

    file->archiveSize = finalSize;
    file->rawArchive = decompressedBuffer;
    file->archiveHeap = heap;

    OS::DCStoreRange(decompressedBuffer, finalSize);
    file->status = ARCHIVE_STATUS_DECOMPRESSED;
}
kmBranch(0x80519508, SafeDecompress);

}  // namespace IOOverrides
}  // namespace Pulsar
