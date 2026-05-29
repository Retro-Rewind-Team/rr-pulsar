#ifndef _BINLOADER_
#define _BINLOADER_

#include <kamek.hpp>
#include <MarioKartWii/Archive/ArchiveMgr.hpp>

namespace RetroRewind {

void* GetCustomKartParam(ArchiveMgr* archive, ArchiveSource type, const char* name, u32* length);
void* GetCustomKartAIParam(ArchiveMgr* archive, ArchiveSource type, const char* name, u32* length);
void* GetCustomItemSlot(ArchiveMgr* archive, ArchiveSource type, const char* name, u32* length);

}  // namespace RetroRewind

#endif
