#include <IO/SDIO_RKSYS.hpp>
#include <PulsarSystem.hpp>
#include <IO/SDIO.hpp>
#include <RetroRewindChannel.hpp>

namespace Pulsar {
char GetRegion() {
    return *(char*)0x80000003;
}

/* Must be preallocated */
void SDIO_RKSYS_path(char* path, u32 pathlen) {
    if (IO::sInstance->type == IOType_DOLPHIN) {
        // this exists on NAND, usually in shared2 (not SD)
        snprintf(path, pathlen, "/RetroRewind6/rksys-%c.dat", GetRegion());
    } else {
        snprintf(path, pathlen, "/riivolution/save/RetroWFC/RMC%c/rksys.dat", GetRegion());
    }
}

bool useRedirectedRKSYS() {
    return IsNewChannel() && NewChannel_UseSeparateSavegame();
}

NandUtils::Result SDIO_ReadRKSYS(NandMgr* nm, void* buffer, u32 size, u32 offset, bool r7)  // 8052c0b0
{
    OS::Report("* SDIO_RKSYS: ReadRKSYS base\n");

    if (useRedirectedRKSYS()) {
        OS::Report("* SDIO_RKSYS: ReadRKSYS (size: %i offset: %i bool: %i)\n", size, offset, r7);
        bool res;
        char path[64];
        SDIO_RKSYS_path(path, sizeof(path));
        int mode = IO::sInstance->type == IOType_DOLPHIN ? FILE_MODE_READ : O_RDONLY;
        res = IO::sInstance->OpenFile(path, mode);
        if (!res) {
            OS::Report("* SDIO_RKSYS: ReadRKSYS: Failed to open RKSYS\n");
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
    OS::Report("* SDIO_RKSYS: CheckRKSYSLength base\n");

    if (useRedirectedRKSYS()) {
        OS::Report("* SDIO_RKSYS: CheckRKSYSLength (length: %i)\n", length);
        bool res;
        char path[64];
        SDIO_RKSYS_path(path, sizeof(path));
        int mode = IO::sInstance->type == IOType_DOLPHIN ? FILE_MODE_READ : O_RDONLY;
        res = IO::sInstance->OpenFile(path, mode);
        if (!res) {
            OS::Report("* SDIO_RKSYS: CheckRKSYSLength: Failed to open RKSYS\n");
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
            return NandUtils::NAND_RESULT_OK;
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
    OS::Report("* SDIO_RKSYS: WriteToRKSYS base\n");

    if (useRedirectedRKSYS()) {
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
    OS::Report("* SDIO_RKSYS: CreateRKSYS base (length = %i)\n", length);

    if (useRedirectedRKSYS()) {
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
        IO::sInstance->Close();

        OS::Report("* SDIO_RKSYS: CreateRKSYS done\n");
        return NandUtils::NAND_RESULT_OK;
    } else {
        asmVolatile(stwu sp, -0x00B0(sp););
        NandUtils::Result res = nm->CreateRKSYS2ndInst(length);
        OS::Report("* SDIO_RKSYS: CreateRKSYS trampoline: returned %i\n", res);
        return res;
    }
}
kmBranch(0x8052c68c, SDIO_CreateRKSYS);

NandUtils::Result SDIO_DeleteRKSYS(NandMgr* nm, u32 length, bool r5)  // 8052c7e4
{
    OS::Report("* SDIO_RKSYS: DeleteRKSYS base\n");

    if (useRedirectedRKSYS()) {
        OS::Report("* SDIO_RKSYS: DeleteRKSYS (length: %p/%i)\n", length, length);
        return NandUtils::NAND_RESULT_OK;
    } else {
        asmVolatile(stwu sp, -0x0030(sp););
        return nm->DeleteRKSYS2ndInst(length, r5);
    }
}
kmBranch(0x8052c7e4, SDIO_DeleteRKSYS);
}  // namespace Pulsar