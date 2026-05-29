#include <IO/SDIO_RKSYS.hpp>
#include <PulsarSystem.hpp>
#include <IO/SDIO.hpp>
#include <RetroRewindChannel.hpp>

namespace Pulsar {

static bool readingNAND = false;
static bool isNewNotSeparateSavegame = false;

char GetRegion() {
    return *(char*)0x80000003;
}

/*
    When separate savegame is disabled, use the save from the RetroWFC folder.
    If it does not exist, copy the NAND save there and use it from then on.

    When enabled, use the RetroWFC2 folder from the XML and create a blank save
    there if needed.
*/

bool useRedirectedRKSYS() {
    return NewChannel_UseSeparateSavegame() && IsNewChannel();
}

/* Must be preallocated */
void SDIO_RKSYS_path(char* path, u32 pathlen) {
    snprintf(path, pathlen, "/riivolution/save/RetroWF%s/RMC%c/rksys.dat", useRedirectedRKSYS() ? "C2" : "C", GetRegion());
}

void SDIO_RKSYS_CreatePath() {
    char path[64];

    IO::sInstance->CreateFolder("/riivolution");
    IO::sInstance->CreateFolder("/riivolution/save");
    snprintf(path, 64, "/riivolution/save/RetroWF%s", useRedirectedRKSYS() ? "C2" : "C");
    IO::sInstance->CreateFolder(path);
    snprintf(path, 64, "/riivolution/save/RetroWF%s/RMC%c", useRedirectedRKSYS() ? "C2" : "C", GetRegion());
    IO::sInstance->CreateFolder(path);
}

NandUtils::Result SDIO_ReadRKSYS(NandMgr* nm, void* buffer, u32 size, u32 offset, bool r7)  // 8052c0b0
{
    if (IsNewChannel() && !readingNAND) {
        bool res;
        char path[64];
        SDIO_RKSYS_path(path, sizeof(path));
        int mode = IO::sInstance->type == IOType_DOLPHIN ? FILE_MODE_READ : O_RDONLY;
        res = IO::sInstance->OpenFile(path, mode);
        if (!res) {
            IO::sInstance->Close();
            return NandUtils::NAND_RESULT_NOEXISTS;
        }

        IO::sInstance->Seek(offset);
        IO::sInstance->Read(size, buffer);
        IO::sInstance->Close();

        return NandUtils::NAND_RESULT_OK;
    } else {
        asmVolatile(stwu sp, -0x00B0(sp););
        return nm->ReadRKSYS2ndInst(buffer, size, offset, r7);
    }
}
kmBranch(0x8052c0b0, SDIO_ReadRKSYS);

NandUtils::Result SDIO_CheckRKSYSLength(NandMgr* nm, u32 length)  // 8052c20c
{
    if (IsNewChannel()) {
        bool res;
        char path[64];
        SDIO_RKSYS_path(path, sizeof(path));
        int mode = IO::sInstance->type == IOType_DOLPHIN ? FILE_MODE_READ : O_RDONLY;
        res = IO::sInstance->OpenFile(path, mode);
        if (!res) {
            IO::sInstance->Close();
            // We do it this way rather than falling through "properly" because of how the game handles creation.
            // Given that when separate savegame is disabled, the game will try to read the existing save,
            // we cannot let the game normally create the save, since it assumes the created save is empty.
            // Otherwise, the game displays an empty save file (even if we copied an existing one) until it is restarted.
            // We fix this by creating one early and pretending it was there all along. This way,
            // when the game tries to read the save, it reads the correct save from the start, and if it needs to create a new one,
            // it creates it without us having to do anything special.
            NandUtils::Result cres = SDIO_CreateRKSYS(nm, length);
            return cres;
        }

        s32 size = IO::sInstance->GetFileSize();
        IO::sInstance->Close();

        if (size == length) {
            return NandUtils::NAND_RESULT_OK;
        } else {
            return NandUtils::NAND_RESULT_OK;
        }
    } else {
        asmVolatile(stwu sp, -0x00B0(sp););
        return nm->CheckRKSYSLength2ndInst(length);
    }
}
kmBranch(0x8052c20c, SDIO_CheckRKSYSLength);

NandUtils::Result SDIO_WriteToRKSYS(NandMgr* nm, const void* buffer, u32 size, u32 offset, bool r7)  // 8052c2d0
{
    if (IsNewChannel()) {
        /* After copying an existing RKSYS, skip the game's first blank-save write. */
        if (!isNewNotSeparateSavegame) {
            bool res;
            char path[64];
            SDIO_RKSYS_path(path, sizeof(path));
            int mode = IO::sInstance->type == IOType_DOLPHIN ? FILE_MODE_READ_WRITE : O_RDWR;
            res = IO::sInstance->OpenFile(path, mode);

            if (!res) {
                NandUtils::Result nres = SDIO_CreateRKSYS(nm, 0);
                if (nres != NandUtils::NAND_RESULT_OK) {
                    return nres;
                }
                res = IO::sInstance->OpenFile(path, O_RDWR);
                if (!res) {
                    return NandUtils::NAND_RESULT_NOEXISTS;
                }

                if (isNewNotSeparateSavegame) {
                    isNewNotSeparateSavegame = false;
                    IO::sInstance->Close();
                    return NandUtils::NAND_RESULT_OK;
                }
            }

            IO::sInstance->Seek(offset);
            IO::sInstance->Write(size, buffer);
            IO::sInstance->Close();
        } else {
            isNewNotSeparateSavegame = false;
        }

        return NandUtils::NAND_RESULT_OK;
    } else {
        asmVolatile(stwu sp, -0x00B0(sp););
        return nm->WriteToRKSYS2ndInst(buffer, size, offset, r7);
    }
}
kmBranch(0x8052c2d0, SDIO_WriteToRKSYS);

NandUtils::Result SDIO_CreateRKSYS(NandMgr* nm, u32 length)  // 8052c68c
{
    /* Separate savegame creates an empty file; shared savegame copies NAND RKSYS. */

    if (IsNewChannel()) {
        /* Create each folder level explicitly; SDIO does not create parent directories. */
        SDIO_RKSYS_CreatePath();

        bool res;
        char path[64];
        SDIO_RKSYS_path(path, sizeof(path));

        int mode = IO::sInstance->type == IOType_DOLPHIN ? O_RDWR : IOS::MODE_WRITE;

        res = IO::sInstance->CreateAndOpen(path, mode);

        if (!res) {
            return NandUtils::NAND_RESULT_ALLOC_FAILED;
        }

        /* If not separate savegame, copy existing NAND one */
        if (!useRedirectedRKSYS()) {
            isNewNotSeparateSavegame = true;

            /* Force SDIO_ReadRKSYS through the NAND path while copying the original save. */
            readingNAND = true;

            const int rksys_size = 0x2BC000;
            const int chunk_size = 1024*10;

            char chunk[chunk_size];
            int read = 0;
            int i = 0;

            while (read < rksys_size) {
                IO::sInstance->Close();
                NandUtils::Result r = SDIO_ReadRKSYS(nm, (void*)chunk, chunk_size, chunk_size * i, true);

                IO::sInstance->OpenFile(path, mode);

                if (r != NandUtils::NAND_RESULT_OK) {
                    IO::sInstance->Close();
                    readingNAND = false;
                    return r;
                }

                if (r == NandUtils::NAND_RESULT_NOEXISTS) {
                    break;
                }

                IO::sInstance->Seek(chunk_size * i);
                IO::sInstance->Write(chunk_size, (void*)chunk);

                i++;
                read += chunk_size;
            }

            readingNAND = false;
        }

        IO::sInstance->Close();
    } else {
        asmVolatile(stwu sp, -0x00B0(sp););
        return nm->CreateRKSYS2ndInst(length);
    }

    return NandUtils::NAND_RESULT_OK;
}
kmBranch(0x8052c68c, SDIO_CreateRKSYS);

NandUtils::Result SDIO_DeleteRKSYS(NandMgr* nm, u32 length, bool r5)  // 8052c7e4
{
    if (IsNewChannel()) {
        /* The SD backend has no delete hook here; the next write will replace the file. */
        return NandUtils::NAND_RESULT_OK;
    } else {
        asmVolatile(stwu sp, -0x0030(sp););
        return nm->DeleteRKSYS2ndInst(length, r5);
    }
}
kmBranch(0x8052c7e4, SDIO_DeleteRKSYS);
}  // namespace Pulsar
