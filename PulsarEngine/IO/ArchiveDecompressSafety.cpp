#include <kamek.hpp>
#include <MarioKartWii/Archive/ArchiveFile.hpp>
#include <core/egg/Decomp.hpp>
#include <core/rvl/os/OS.hpp>
#include <core/rvl/os/OSCache.hpp>
#include <core/nw4r/ut/Misc.hpp>
#include <IO/LooseArchiveOverrides.hpp>

namespace Pulsar {

// Check if the path is an SZS archive (for loose override support)
static bool IsSzs(const char* path) {
    if (path == nullptr) return false;
    const char* ext = StringFindLastChar(path, '.');
    if (ext == nullptr) return false;
    return (ext[1] == 's' || ext[1] == 'S') &&
           (ext[2] == 'z' || ext[2] == 'Z') &&
           (ext[3] == 's' || ext[3] == 'S') &&
           ext[4] == '\0';
}

static void SafeDecompress(ArchiveFile* file, const char* path, EGG::Heap* heap, EGG::Archive::FileInfo* info) {
    u8* compressedData = static_cast<u8*>(file->compressedArchive);
    if (compressedData == nullptr) {
        return;
    }

    u32 expandSize = EGG::Decomp::getExpandSize(compressedData);
    if (expandSize == 0) {
        return;
    }

    // Check for loose U8 overrides if this is an SZS archive
    OverrideList overrides;
    InitOverrideList(overrides);
    bool hasOverrides = false;
    
    if (IsSzs(path)) {
        char archiveBase[64];
        ExtractArchiveBaseName(path, archiveBase, sizeof(archiveBase));
        hasOverrides = BuildOverrideList(archiveBase, overrides, heap);
    }

    // Calculate total allocation size
    u32 totalSize = expandSize;
    if (hasOverrides) {
        totalSize += overrides.totalAlignedSize;
        // Align total size
        totalSize = nw4r::ut::RoundUp(totalSize, 0x20);
    }

    void* buffer = EGG::Heap::alloc(totalSize, 0x20, heap);
    u8* decompressedBuffer = static_cast<u8*>(buffer);

    if (decompressedBuffer == nullptr) {
        OS::Report("[Pulsar] ArchiveFile::Decompress allocation failed! Size: 0x%X\n", totalSize);
        if (hasOverrides) {
            FreeOverrideList(overrides, heap);
        }
        return;
    }

    // Decompress the archive
    EGG::Decomp::decodeSZS(compressedData, decompressedBuffer);

    // Apply loose U8 overrides
    if (hasOverrides) {
        ApplyU8Overrides(decompressedBuffer, expandSize, overrides, heap);
        FreeOverrideList(overrides, heap);
    }

    file->archiveSize = totalSize;
    file->rawArchive = decompressedBuffer;
    file->archiveHeap = heap;

    OS::DCStoreRange(decompressedBuffer, totalSize);
    file->status = ARCHIVE_STATUS_DECOMPRESSED;
}
kmBranch(0x80519508, SafeDecompress);

}  // namespace Pulsar

