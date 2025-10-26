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
    When separate savegame is disabled it should use the save from the RetroWFC folder,
    and if it doesnt exist, copy the save from the nand to that folder and use it from there.

    When enabled, it will use the save from the RetroWFC2 folder (like in the xml),
    and if it doesnt exist, make a blank new save in that folder and use that
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
    OS::Report("* SDIO_RKSYS: ReadRKSYS base\n");

    if (IsNewChannel() && !readingNAND) {
        OS::Report("* SDIO_RKSYS: ReadRKSYS (size: %i offset: %i bool: %i)\n", size, offset, r7);
        bool res;
        char path[64];
        SDIO_RKSYS_path(path, sizeof(path));
        int mode = IO::sInstance->type == IOType_DOLPHIN ? FILE_MODE_READ : O_RDONLY;
        res = IO::sInstance->OpenFile(path, mode);
        if (!res) {
            OS::Report("* SDIO_RKSYS: ReadRKSYS: Failed to open RKSYS\n");
            IO::sInstance->Close();
            return NandUtils::NAND_RESULT_NOEXISTS;
        }

        IO::sInstance->Seek(offset);
        OS::Report("* SDIO_RKSYS: ReadRKSYS: read %i bytes\n", IO::sInstance->Read(size, buffer));
        IO::sInstance->Close();

        return NandUtils::NAND_RESULT_OK;
    } else {
        asmVolatile(stwu sp, -0x00B0(sp););
        NandUtils::Result res = nm->ReadRKSYS2ndInst(buffer, size, offset, r7);
        OS::Report("* SDIO_RKSYS: ReadRKSYS trampoline: returned %i\n", res);
        return res;
    }
}
kmBranch(0x8052c0b0, SDIO_ReadRKSYS);

NandUtils::Result SDIO_CheckRKSYSLength(NandMgr* nm, u32 length)  // 8052c20c
{
    if (IsNewChannel()) {
        OS::Report("* SDIO_RKSYS: CheckRKSYSLength (length: %i)\n", length);
        bool res;
        char path[64];
        SDIO_RKSYS_path(path, sizeof(path));
        int mode = IO::sInstance->type == IOType_DOLPHIN ? FILE_MODE_READ : O_RDONLY;
        res = IO::sInstance->OpenFile(path, mode);
        if (!res) {
            OS::Report("* SDIO_RKSYS: CheckRKSYSLength: Failed to open RKSYS\n");
            IO::sInstance->Close();
            return NandUtils::NAND_RESULT_NOEXISTS;
        }

        s32 size = IO::sInstance->GetFileSize();
        IO::sInstance->Close();

        if (size == length) {
            return NandUtils::NAND_RESULT_OK;
        } else {
            OS::Report(
                "* SDIO_RKSYS: CheckRKSYSLength: RKSYS length not matching (queried: %i, actual: %i)\n",
                length,
                size);
            return NandUtils::NAND_RESULT_NOEXISTS;
        }
    } else {
        OS::Report("* SDIO_RKSYS: CheckRKSYSLength trampoline\n");

        asmVolatile(stwu sp, -0x00B0(sp););
        NandUtils::Result res = nm->CheckRKSYSLength2ndInst(length);
        OS::Report("* SDIO_RKSYS: CheckRKSYSLength trampoline: returned %i\n", res);
        return res;
    }
}
kmBranch(0x8052c20c, SDIO_CheckRKSYSLength);

NandUtils::Result SDIO_WriteToRKSYS(NandMgr* nm, const void* buffer, u32 size, u32 offset, bool r7)  // 8052c2d0
{
    if (IsNewChannel()) {
        /* basically - this is true if this was called into after creating rksys,
        the game wants to write some basic data. but we copied an existing rksys, so
        dont write anything and then reset this */
        if (!isNewNotSeparateSavegame) {
            OS::Report("* SDIO_RKSYS: WriteToRKSYS (size: %i offset: %i bool: %i)\n", size, offset, r7);
            bool res;
            char path[64];
            SDIO_RKSYS_path(path, sizeof(path));
            int mode = IO::sInstance->type == IOType_DOLPHIN ? FILE_MODE_READ_WRITE : O_RDWR;
            res = IO::sInstance->OpenFile(path, mode);

            if (!res) {
                OS::Report("* SDIO_RKSYS: WriteToRKSYS: Failed to open RKSYS, trying to create it\n");
                NandUtils::Result nres = SDIO_CreateRKSYS(nm, 0);
                if (nres != NandUtils::NAND_RESULT_OK) {
                    OS::Report("* SDIO_RKSYS: WriteToRKSYS: Failed to create RKSYS, aborting\n");
                    return nres;
                }
                res = IO::sInstance->OpenFile(path, O_RDWR);
                if (!res) {
                    OS::Report("* SDIO_RKSYS: WriteToRKSYS: Failed to open RKSYS, aborting\n");
                    return NandUtils::NAND_RESULT_NOEXISTS;
                }
            }

            IO::sInstance->Seek(offset);
            OS::Report("* SDIO_RKSYS: WriteToRKSYS: wrote %i bytes\n", IO::sInstance->Write(size, buffer));
            IO::sInstance->Close();
        } else {
            isNewNotSeparateSavegame = false;
        }

        return NandUtils::NAND_RESULT_OK;
    } else {
        asmVolatile(stwu sp, -0x00B0(sp););
        NandUtils::Result res = nm->WriteToRKSYS2ndInst(buffer, size, offset, r7);
        OS::Report("* SDIO_RKSYS: WriteToRKSYS trampoline: returned %i\n", res);
        return res;
    }
}
kmBranch(0x8052c2d0, SDIO_WriteToRKSYS);

NandUtils::Result SDIO_CreateRKSYS(NandMgr* nm, u32 length)  // 8052c68c
{
    /* If separate savegame - make new empty file*/
    /* If not - make new file, copy existing NAND rksys to it*/

    OS::Report("* SDIO_RKSYS: CreateRKSYS base (length = %i)\n", length);

    if (IsNewChannel()) {
        /* we have to create all the subfolders individually because yeah */
        SDIO_RKSYS_CreatePath();

        bool res;
        char path[64];
        SDIO_RKSYS_path(path, sizeof(path));

        OS::Report("* SDIO_RKSYS: CreateRKSYS (%s)\n", path);
        int mode = IO::sInstance->type == IOType_DOLPHIN ? FILE_MODE_NONE : O_CREAT;

        res = IO::sInstance->CreateAndOpen(path, mode);

        if (!res) {
            OS::Report("* SDIO_RKSYS: CreateRKSYS: Failed to create or open RKSYS\n");
            return NandUtils::NAND_RESULT_ALLOC_FAILED;
        }

        /* If not separate savegame, copy existing NAND one */
        if (!useRedirectedRKSYS()) {
            OS::Report("* SDIO_RKSYS: CreateRKSYS: Copying existing NAND RKSYS\n");

            /* this makes ReadRKSYS read from nand*/
            readingNAND = true;

            const int rksys_size = 0x2BC000;
            const int chunk_size = 128;

            char chunk[chunk_size];
            int read = 0;
            int i = 0;

            while (read < rksys_size) {
                OS::Report("* SDIO_RKSYS: CreateRKSYS: Copying chunk %i (offset %i)\n", i, read);

                /* if this check fails the file probably doesnt exist... */
                /* or if it does, wtf */
                NandUtils::Result r = SDIO_ReadRKSYS(nm, (void*)chunk, chunk_size, chunk_size * i, true);

                if (r != NandUtils::NAND_RESULT_OK) {
                    OS::Report("* SDIO_RKSYS: CreateRKSYS: Failed to read old RKSYS, error %i\n", r);
                    IO::sInstance->Close();
                    readingNAND = false;
                    return r;
                }

                if (r == NandUtils::NAND_RESULT_NOEXISTS) {
                    OS::Report("* SDIO_RKSYS: CreateRKSYS: Old RKSYS doesnt exist, skipping\n");
                    break;
                }

                IO::sInstance->Seek(chunk_size * i);
                IO::sInstance->Write(chunk_size, (void*)chunk);

                i++;
                read += chunk_size;
            }

            isNewNotSeparateSavegame = true;
            readingNAND = false;
        }

        IO::sInstance->Close();
    } else {
        asmVolatile(stwu sp, -0x00B0(sp););
        NandUtils::Result res = nm->CreateRKSYS2ndInst(length);
        OS::Report("* SDIO_RKSYS: CreateRKSYS trampoline: returned %i\n", res);
        return res;
    }

    OS::Report("* SDIO_RKSYS: CreateRKSYS done\n");

    return NandUtils::NAND_RESULT_OK;
}
kmBranch(0x8052c68c, SDIO_CreateRKSYS);

NandUtils::Result SDIO_DeleteRKSYS(NandMgr* nm, u32 length, bool r5)  // 8052c7e4
{
    OS::Report("* SDIO_RKSYS: DeleteRKSYS base\n");

    if (IsNewChannel()) {
        OS::Report("* SDIO_RKSYS: DeleteRKSYS (length: %p/%i)\n", length, length);
        return NandUtils::NAND_RESULT_OK;
    } else {
        asmVolatile(stwu sp, -0x0030(sp););
        return nm->DeleteRKSYS2ndInst(length, r5);
    }
}
kmBranch(0x8052c7e4, SDIO_DeleteRKSYS);
}  // namespace Pulsar