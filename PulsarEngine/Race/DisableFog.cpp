#include <PulsarSystem.hpp>
#include <runtimeWrite.hpp>
#include <SlotExpansion/CupsConfig.hpp>

namespace Pulsar {
namespace Race {

kmRuntimeUse(0x805ADFF0);  // bool DisableFogInRaces
void DisableFoginRaces() {
    const CupsConfig* cupsConfig = CupsConfig::sInstance;
    PulsarId pulsarId = cupsConfig->GetWinning();
    kmRuntimeWrite32A(0x805ADFF0, 0x3CC08089);  // set DisableFogInRaces to true
    if (CupsConfig::ConvertTrack_PulsarIdToRealId(pulsarId) == 164) {  // GP Rainbow Coaster
        kmRuntimeWrite32A(0x805ADFF0, 0x4E800020);  // blr
    }
}
static PageLoadHook DisableFogHook(DisableFoginRaces);

}  // namespace Race
}  // namespace Pulsar