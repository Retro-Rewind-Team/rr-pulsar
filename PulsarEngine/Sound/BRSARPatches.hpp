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

#ifndef _BRSARPATCHES_
#define _BRSARPATCHES_

#include <core/nw4r/snd/SoundArchiveLoader.hpp>

namespace Pulsar {
namespace Sound {

void SetLooseBRSARGroupItemBuffer(u32 fileId, bool waveData, void *buffer);
bool FindLooseSoundEffectPath(u32 fileId, const char *extension, char *path, u32 pathSize, u32 *outFileSize = nullptr);

}  // namespace Sound
}  // namespace Pulsar

#endif
