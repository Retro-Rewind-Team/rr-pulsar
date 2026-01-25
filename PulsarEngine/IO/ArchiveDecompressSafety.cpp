#include <kamek.hpp>
#include <MarioKartWii/Archive/ArchiveFile.hpp>
#include <core/egg/Decomp.hpp>
#include <core/rvl/os/OS.hpp>
#include <core/rvl/os/OSCache.hpp>

namespace Pulsar {
namespace IO {

static void SafeDecompress(ArchiveFile* file, const char* path, EGG::Heap* heap, EGG::Archive::FileInfo* info) {
    u8* compressedData = static_cast<u8*>(file->compressedArchive);
    if (compressedData == nullptr) {
        return;
    }

    u32 expandSize = EGG::Decomp::getExpandSize(compressedData);
    if (expandSize == 0) {
        return;
    }

    void* buffer = EGG::Heap::alloc(expandSize, 0x20, heap);
    u8* decompressedBuffer = static_cast<u8*>(buffer);

    if (decompressedBuffer == nullptr) {
        OS::Report("[Pulsar] ArchiveFile::Decompress allocation failed! Size: 0x%X\n", expandSize);
        return;
    }

    EGG::Decomp::decodeSZS(compressedData, decompressedBuffer);

    file->archiveSize = expandSize;
    file->rawArchive = decompressedBuffer;
    file->archiveHeap = heap;

    OS::DCStoreRange(decompressedBuffer, expandSize);
    file->status = ARCHIVE_STATUS_DECOMPRESSED;
}
kmBranch(0x80519508, SafeDecompress);

}  // namespace IO
}  // namespace Pulsar
