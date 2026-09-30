#ifndef _LOOSEBRSAROVERRIDES_
#define _LOOSEBRSAROVERRIDES_

#include <core/nw4r/snd/SoundArchiveLoader.hpp>

namespace Pulsar {
namespace Sound {

void SetLooseBRSARGroupItemBuffer(u32 fileId, bool waveData, void *buffer);
bool FindLooseSoundEffectPath(u32 fileId, const char *extension, char *path, u32 pathSize, u32 *outFileSize = nullptr);

}  // namespace Sound
}  // namespace Pulsar

#endif
