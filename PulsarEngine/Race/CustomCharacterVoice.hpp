#ifndef _CUSTOMCHARACTERVOICE_
#define _CUSTOMCHARACTERVOICE_

#include <core/nw4r/snd/SoundArchiveLoader.hpp>

namespace Pulsar {
namespace Race {

void PatchLoadedCustomVoiceGroup(nw4r::snd::detail::SoundArchiveLoader *loader, u32 groupId, nw4r::snd::SoundMemoryAllocatable *allocater, void *groupData, void *waveData);

}  // namespace Race
}  // namespace Pulsar

#endif
