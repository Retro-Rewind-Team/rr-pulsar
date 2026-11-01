/*
 * Loose Archive Overrides
 *
 * Developed by patchzy as part of the Retro Rewind project.
 *
 * Copyright (C) Retro Rewind.
 * SPDX-License-Identifier: MIT
 *
 * This code is licensed under the MIT License.
 *
 * Credit is not legally required, but if you use or adapt this system,
 * please consider crediting patchzy and/or Retro Rewind team.
 */

#ifndef _PULSAR_IO_PATCHES_
#define _PULSAR_IO_PATCHES_

#include <kamek.hpp>

namespace EGG {
class Heap;
}

#ifdef IO
#undef IO
#endif

namespace Pulsar {
namespace IOOverrides {

enum {
    OVERRIDE_MAX_PATH = 256,
    OVERRIDE_MAX_NAME = 64
};

bool IsModsPath(const char *path);

s32 ConvertPathToEntryNumWithLooseOverride(const char *path);

const char *ResolveWholeFileOverride(const char *path, char *resolvedPath, u32 resolvedSize, bool *outRedirected, s32 *outSourceEntryNum = nullptr);

bool ShouldApplyLooseOverrides(const char *path, char *archiveBaseLower, u32 archiveBaseLowerSize);

bool HasStructuralLooseOverrides(const char *archiveBaseLower);

bool ApplyLooseOverrides(const char *archiveBaseLower, u8 *&archiveBase, u32 &archiveSize, EGG::Heap *sourceHeap, EGG::Heap *&archiveHeap, u32 *outAppliedOverrides, u32 *outPatchedNodes,
  u32 *outMissingOverrides, const u8 *compressedData);

bool AreLooseArchiveOverridesEnabledForDebug();
bool GetLooseBRSAROverrideSizes(u32 fileId, u32 &outFileSize, u32 &outWaveDataSize);
bool ReadLooseBRSAROverrideRange(u32 fileId, void *dest, u32 size, bool waveData);
u32 GetLooseArchiveOverrideFileCount();

}  // namespace IOOverrides
}  // namespace Pulsar

#endif  // _PULSAR_IO_PATCHES_
