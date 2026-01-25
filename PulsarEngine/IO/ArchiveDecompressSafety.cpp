#include <kamek.hpp>
#include <MarioKartWii/Archive/ArchiveFile.hpp>
#include <IO/LooseArchiveOverrides.hpp>
#include <core/egg/Decomp.hpp>
#include <core/rvl/os/OS.hpp>
#include <core/rvl/os/OSCache.hpp>

namespace Pulsar {
namespace IO {

static void SafeDecompress(ArchiveFile* file, const char* path, EGG::Heap* heap, EGG::Archive::FileInfo* info) {
    (void)info;
    u8* compressedData = static_cast<u8*>(file->compressedArchive);
    if (compressedData == nullptr) {
        OS::Report("[Pulsar][Mods] ArchiveFile::Decompress missing compressed data (%s)\n", path ? path : "<null>");
        return;
    }

    const bool isSZS = IOOverrides::IsSZSPath(path);
    const bool isModsPath = IOOverrides::IsModsPath(path);
    void* overrideCompressed = nullptr;
    u32 overrideCompressedSize = 0;
    bool usedWholeOverride = false;
    if (isSZS && !isModsPath) {
        usedWholeOverride = IOOverrides::TryLoadWholeArchiveOverride(path, heap, overrideCompressed, overrideCompressedSize);
        if (usedWholeOverride) {
            compressedData = static_cast<u8*>(overrideCompressed);
            OS::Report("[Pulsar][Mods] Using whole archive override for %s (0x%X)\n",
                       path ? path : "<null>", overrideCompressedSize);
        }
    }

    u32 expandSize = EGG::Decomp::getExpandSize(compressedData);
    if (expandSize == 0) {
        OS::Report("[Pulsar][Mods] ArchiveFile::Decompress expand size failed (%s)\n", path ? path : "<null>");
        if (overrideCompressed) {
            EGG::Heap::free(overrideCompressed, heap);
        }
        return;
    }

    IOOverrides::OverrideList overrides;
    overrides.entries = nullptr;
    overrides.count = 0;
    u32 extraSize = 0;
    u32 matchCount = 0;
    const bool allowLooseOverrides = isSZS && !isModsPath && !usedWholeOverride;
    if (allowLooseOverrides) {
        if (IOOverrides::BuildOverrideList(overrides, heap)) {
            extraSize = IOOverrides::CalculateOverrideExpansion(overrides, path, expandSize, matchCount);
            if (matchCount == 0) {
                OS::Report("[Pulsar][Mods] No matching overrides for %s\n", path ? path : "<null>");
            }
        }
    } else if (usedWholeOverride) {
        OS::Report("[Pulsar][Mods] Skipping loose overrides (whole archive override active) for %s\n", path ? path : "<null>");
    }

    const u32 totalSize = expandSize + extraSize;
    void* buffer = EGG::Heap::alloc(totalSize, 0x20, heap);
    u8* decompressedBuffer = static_cast<u8*>(buffer);

    if (decompressedBuffer == nullptr) {
        OS::Report("[Pulsar][Mods] ArchiveFile::Decompress allocation failed! Size: 0x%X\n", totalSize);
        if (overrides.entries) {
            IOOverrides::FreeOverrideList(overrides, heap);
        }
        if (overrideCompressed) {
            EGG::Heap::free(overrideCompressed, heap);
        }
        return;
    }

    EGG::Decomp::decodeSZS(compressedData, decompressedBuffer);

    file->archiveSize = totalSize;
    file->rawArchive = decompressedBuffer;
    file->archiveHeap = heap;

    OS::DCStoreRange(decompressedBuffer, expandSize);

    if (allowLooseOverrides && overrides.entries && matchCount > 0) {
        u32 appliedFiles = 0;
        u32 patchedNodes = 0;
        IOOverrides::ApplyArchiveOverrides(decompressedBuffer, expandSize, path, overrides, appliedFiles, patchedNodes);
        OS::Report("[Pulsar][Mods] Overrides applied to %s: files=%u nodes=%u extra=0x%X\n",
                   path ? path : "<null>", appliedFiles, patchedNodes, extraSize);
    }

    if (overrides.entries) {
        IOOverrides::FreeOverrideList(overrides, heap);
    }
    if (overrideCompressed) {
        EGG::Heap::free(overrideCompressed, heap);
    }

    file->status = ARCHIVE_STATUS_DECOMPRESSED;
}
kmBranch(0x80519508, SafeDecompress);

}  // namespace IO
}  // namespace Pulsar
