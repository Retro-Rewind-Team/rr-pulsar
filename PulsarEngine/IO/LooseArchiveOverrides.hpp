/*
 * This feature was developed by patchzy as part of the Retro Rewind project.
 *
 * Copyright (C) Retro Rewind.
 *
 * Licensed under the GNU General Public License v3.0 only.
 * You may not use, modify, or distribute this code except in compliance with
 * that license.
 *
 * Use of this system in a non-GPLv3 project requires prior permission from
 * the Retro Rewind team and/ or patchzy.
 */


#ifndef _PULSAR_LOOSE_ARCHIVE_OVERRIDES_
#define _PULSAR_LOOSE_ARCHIVE_OVERRIDES_

#include <kamek.hpp>

namespace EGG {
class Heap;
}

#ifdef IO
#undef IO
#endif

namespace Pulsar {
namespace IOOverrides {

enum { OVERRIDE_MAX_PATH = 256, OVERRIDE_MAX_NAME = 64 };

bool IsModsPath(const char* path);

const char* ResolveWholeFileOverride(const char* path, char* resolvedPath, u32 resolvedSize, bool* outRedirected);

bool ShouldApplyLooseOverrides(const char* path, char* archiveBaseLower, u32 archiveBaseLowerSize);

bool ApplyLooseOverrides(const char* archiveBaseLower, u8*& archiveBase, u32& archiveSize, EGG::Heap* sourceHeap,
                         EGG::Heap*& archiveHeap, u32* outAppliedOverrides, u32* outPatchedNodes,
                         u32* outMissingOverrides, const u8* compressedData);

}  // namespace IOOverrides
}  // namespace Pulsar

#endif  // _PULSAR_LOOSE_ARCHIVE_OVERRIDES_
