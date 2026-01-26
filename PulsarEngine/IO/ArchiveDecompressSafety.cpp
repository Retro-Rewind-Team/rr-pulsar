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
    const bool canApplyOverrides = ShouldApplyLooseOverrides(path, archiveBaseLower, sizeof(archiveBaseLower));
    const u32 allocSize = nw4r::ut::RoundUp(expandSize, 0x20);
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

    u8* archiveBase = decompressedBuffer;
    EGG::Heap* archiveHeap = heap;
    if (canApplyOverrides) {
        ApplyLooseOverrides(archiveBaseLower, archiveBase, finalSize, heap, archiveHeap, &appliedOverrides, &patchedNodes,
                            &missingOverrides);
    }

    file->archiveSize = finalSize;
    file->rawArchive = archiveBase;
    file->archiveHeap = archiveHeap;

    OS::DCStoreRange(archiveBase, finalSize);
    file->status = ARCHIVE_STATUS_DECOMPRESSED;
}
kmBranch(0x80519508, SafeDecompress);

}  // namespace IOOverrides
}  // namespace Pulsar
