#ifndef _BINLOADER_
#define _BINLOADER_

#include <kamek.hpp>
#include <MarioKartWii/Archive/ArchiveMgr.hpp>
#include <Network/SHA256.hpp>
#include <core/rvl/OS/OS.hpp>

namespace RetroRewind
{

    // Forward declarations
    void *GetCustomKartParam(ArchiveMgr *archive, ArchiveSource type, const char *name, u32 *length);
    void *GetCustomKartAIParam(ArchiveMgr *archive, ArchiveSource type, const char *name, u32 *length);
    void *GetCustomItemSlot(ArchiveMgr *archive, ArchiveSource type, const char *name, u32 *length);

    // Hash calculation functions
    void CalculateKartParamHash(const void *data, u32 length, u8 *hashOut);
    bool GetKartParamFileHash(ArchiveSource type, const char *filename, u8 *hashOut);

    // Utility function to convert hash to string
    void HashToString(const u8 *hash, char *strOut, size_t strLen);

    // Log hash to Dolphin console
    void LogKartParamHash(const char *filename);

    // Hash verification
    struct FileHash
    {
        const char *filename;
        const char *expectedHash;
    };

    bool VerifyFileHash(const char *filename, ArchiveSource type, const char *expectedHash);
    void VerifyKartParamHashes();
    void VerifyItemSlotHashes();

} // namespace RetroRewind

#endif