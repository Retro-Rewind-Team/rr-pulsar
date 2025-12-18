#include <Network/ServerDateTime.hpp>
#include <runtimeWrite.hpp>
#include <kamek.hpp>

namespace Pulsar {

ServerDateTime serverDateTimeInstance;
ServerDateTime* ServerDateTime::sInstance = &serverDateTimeInstance;

typedef s64 (*OSCalendarTimeToTicks_t)(void*);

static void StoreDateTimeFromStack(s32* calendarTime) {
    ServerDateTime* sdt = ServerDateTime::sInstance;
    if (sdt != nullptr) {
        u8 second = (u8)calendarTime[0];
        u8 minute = (u8)(calendarTime[1] + 1);
        u8 hour = (u8)calendarTime[2];
        u8 day = (u8)calendarTime[3];
        u8 month = (u8)calendarTime[4] + 1;
        u16 year = (u16)calendarTime[5];

        OS::Report("ServerDateTime: %d/%d/%d %d:%d:%d\n", year, month, day, hour, minute, second);

        sdt->SetDateTime(year, month, day, hour, minute, second);
    }
}

kmRuntimeUse(0x801ab170);
extern "C" s64 HookOSCalendarTimeToTicks(void* calendarTime) {
    StoreDateTimeFromStack((s32*)calendarTime);
    OSCalendarTimeToTicks_t original = (OSCalendarTimeToTicks_t)kmRuntimeAddr(0x801ab170);
    return original(calendarTime);
}
kmCall(0x800ee5a4, HookOSCalendarTimeToTicks);

}  // namespace Pulsar
