#ifndef _PULSAR_LOOSEARCHIVEOVERRIDES_
#define _PULSAR_LOOSEARCHIVEOVERRIDES_

#include <kamek.hpp>
#include <core/egg/mem/Heap.hpp>

namespace Pulsar {
namespace IOOverrides {

enum { OVERRIDE_MAX_PATH = 512 };

struct OverrideEntry {
    char fullPath[OVERRIDE_MAX_PATH];
    char relativePath[OVERRIDE_MAX_PATH];
    u32 size;
};

struct OverrideList {
    OverrideEntry* entries;
    u32 count;
};

const char* GetOverrideRoot();
bool IsModsPath(const char* path);
bool IsSZSPath(const char* path);

bool BuildOverrideList(OverrideList& list, EGG::Heap* heap);
void FreeOverrideList(OverrideList& list, EGG::Heap* heap);

bool TryLoadWholeArchiveOverride(const char* archivePath, EGG::Heap* heap, void*& outData, u32& outSize);
u32 CalculateOverrideExpansion(const OverrideList& list, const char* archivePath, u32 baseSize, u32& outMatchCount);
u32 ApplyArchiveOverrides(void* archiveBase, u32 baseSize, const char* archivePath, const OverrideList& list,
                          u32& outOverrideFilesApplied, u32& outNodesPatched);

}  // namespace IOOverrides
}  // namespace Pulsar

#endif
