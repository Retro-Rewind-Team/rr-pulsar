#include <Network/ServerDateTime.hpp>
#include <kamek.hpp>
#include <core/System/SystemManager.hpp>

namespace Pulsar {

ServerDateTime serverDateTimeInstance;
ServerDateTime* ServerDateTime::sInstance = &serverDateTimeInstance;

static void SyncGameDate(const ServerDateTime& sdt) {
    SystemManager* systemManager = SystemManager::sInstance;
    if (systemManager == nullptr || !sdt.isValid) return;

    systemManager->year = (u8)(sdt.year - 2000);
    systemManager->month = sdt.month;
    systemManager->day = sdt.day;
    systemManager->isValidDate = true;
}

static void StoreDateTime(const OS::CalendarTime& calendarTime, u64 serverTicks) {
    ServerDateTime* sdt = ServerDateTime::sInstance;
    if (sdt != nullptr) {
        sdt->SetDateTime(calendarTime, serverTicks, OS::GetTime());
        SyncGameDate(*sdt);
        OS::Report("ServerDateTime: %d/%d/%d %d:%d:%d\n", sdt->year, sdt->month, sdt->day, sdt->hour, sdt->minute,
                   sdt->second);
    }
}

extern "C" s64 HookOSCalendarTimeToTicks(OS::CalendarTime* calendarTime) {
    const s64 ticks = OS::CalendarTimeToTicks(calendarTime);
    StoreDateTime(*calendarTime, ticks);
    return ticks;
}
kmCall(0x800ee5a4, HookOSCalendarTimeToTicks);

static void UpdateServerDateTime() {
    ServerDateTime* sdt = ServerDateTime::sInstance;
    if (sdt == nullptr || !sdt->Update()) return;
    SyncGameDate(*sdt);
}
static FrameLoadHook serverDateTimeHook(UpdateServerDateTime);

}  // namespace Pulsar
