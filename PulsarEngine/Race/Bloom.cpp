#include <RetroRewind.hpp>

namespace Pulsar {

void BloomPatch() {
    BloomHook = 0x00;
    if (Settings::Mgr::Get().GetSettingValue(Pulsar::Settings::SETTING_BLOOM) == BLOOM_ENABLED) {
        BloomHook = 0x03000000;
    }
}
static SectionLoadHook PatchBloom(BloomPatch);

}  // namespace Pulsar