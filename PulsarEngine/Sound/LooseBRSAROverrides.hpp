#ifndef _LOOSEBRSAROVERRIDES_
#define _LOOSEBRSAROVERRIDES_

#include <core/nw4r/snd/SoundArchiveLoader.hpp>

namespace Pulsar {
namespace Sound {

void SetLooseBRSARGroupItemBuffer(u32 fileId, bool waveData, void *buffer);

}  // namespace Sound
}  // namespace Pulsar

#endif
