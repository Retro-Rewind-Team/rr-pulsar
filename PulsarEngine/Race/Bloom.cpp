#include <RetroRewind.hpp>

namespace Pulsar {

void BloomPatch() {
    BloomHook = 0x00;
    if (Settings::Mgr::Get().GetUserSettingValue(Settings::SETTINGSTYPE_RACE2, RADIO_BLOOM) == BLOOM_ENABLED) {
        BloomHook = 0x03000000;
    }
}
static SectionLoadHook PatchBloom(BloomPatch);

}  // namespace Pulsar