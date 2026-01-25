#ifndef _PULSAR_LOOSE_ARCHIVE_OVERRIDES_
#define _PULSAR_LOOSE_ARCHIVE_OVERRIDES_

#include <kamek.hpp>

#ifdef IO
#undef IO
#endif

namespace Pulsar {
namespace IOOverrides {

enum { OVERRIDE_MAX_PATH = 256, OVERRIDE_MAX_NAME = 64 };

bool IsModsPath(const char* path);

const char* ResolveWholeFileOverride(const char* path, char* resolvedPath, u32 resolvedSize, bool* outRedirected);

bool ShouldApplyLooseOverrides(const char* path, char* archiveBaseLower, u32 archiveBaseLowerSize);

u32 GetLooseOverridesTotalAligned(const char* archiveBaseLower, u32* outApplicableCount);

void ApplyLooseOverrides(const char* archiveBaseLower, u8* archiveBase, u32 baseSize, u32* outAppliedOverrides,
                         u32* outPatchedNodes, u32* outMissingOverrides);

}  // namespace IOOverrides
}  // namespace Pulsar

#endif  // _PULSAR_LOOSE_ARCHIVE_OVERRIDES_
