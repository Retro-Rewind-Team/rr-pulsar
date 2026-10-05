#ifndef __SDIO_RKSYS__
#define __SDIO_RKSYS__

/* Override RKSYS functions to use SDIO if necessary. */

#include <IO/IO.hpp>
#include <MarioKartWii/NAND/NandUtils.hpp>
#include <MarioKartWii/NAND/NandMgr.hpp>

namespace Pulsar {
NandUtils::Result SDIO_ReadRKSYS(NandMgr *nm, void *buffer, u32 size, u32 offset, bool r7);  // 8052c0b0

NandUtils::Result SDIO_CheckRKSYSLength(NandMgr *nm, u32 length);  // 8052c20c

NandUtils::Result SDIO_WriteToRKSYS(NandMgr *nm, const void *buffer, u32 size, u32 offset, bool r7);  // 8052c2d0

NandUtils::Result SDIO_CreateRKSYS(NandMgr *nm, u32 length);  // 8052c68c
NandUtils::Result SDIO_DeleteRKSYS(NandMgr *nm, u32 length, bool r5);  // 8052c7e4
}  // namespace Pulsar

#endif