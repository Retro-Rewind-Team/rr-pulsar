#include <RetroRewind.hpp>
#include <runtimeWrite.hpp>
#include <Network/ServerDateTime.hpp>
#include <MarioKartWii/Race/RaceData.hpp>

namespace Pulsar {
namespace Events {

kmRuntimeUse(0x8083DFD8);
kmRuntimeUse(0x80846C38);
void OperationDoglord() {
    ServerDateTime* sdt = ServerDateTime::sInstance;
    kmRuntimeWrite32A(0x8083DFD8, 0x7cbd2b78);
    kmRuntimeWrite32A(0x80846C38, 0x83e40240);
    if (sdt->month == 4 && sdt->day == 1) {
        if ((sdt->hour >= 0 && sdt->hour < 2) || (sdt->hour >= 6 && sdt->hour < 8) || (sdt->hour >= 9 && sdt->hour < 11) || (sdt->hour >= 16 && sdt->hour < 20) || (sdt->hour >= 21 && sdt->hour < 24)) {
            if (Racedata::sInstance->menusScenario.settings.gamemode == MODE_PUBLIC_VS) {
                kmRuntimeWrite32A(0x8083DFD8, 0x3BA0000B);  // li r29, 0x0B -> Wario
                kmRuntimeWrite32A(0x80846C38, 0x3BE00008);  // li r31, 0x08 -> Flame Flyer
            }
        }
    }
}
static FrameLoadHook comboHook(OperationDoglord);

}  // namespace Events
}  // namespace Pulsar