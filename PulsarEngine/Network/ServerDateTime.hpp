#ifndef _SERVER_DATE_TIME_HPP_
#define _SERVER_DATE_TIME_HPP_
#include <types.hpp>

namespace Pulsar {

struct ServerDateTime {
    static ServerDateTime* sInstance;

    u16 year;  // Full year (e.g., 2025)
    u8 month;  // 1-12
    u8 day;  // 1-31
    u8 hour;  // 0-23
    u8 minute;  // 0-59
    u8 second;  // 0-59
    bool isValid;  // True if we successfully parsed server datetime

    ServerDateTime() : year(0), month(0), day(0), hour(0), minute(0), second(0), isValid(false) {}

    void SetDateTime(u16 y, u8 mo, u8 d, u8 h, u8 mi, u8 s) {
        year = y;
        month = mo;
        day = d;
        hour = h;
        minute = mi;
        second = s;
        isValid = true;
    }

    void Reset() {
        year = 0;
        month = 0;
        day = 0;
        hour = 0;
        minute = 0;
        second = 0;
        isValid = false;
    }

    static u8 GetDayOfWeek(u16 y, u8 m, u8 d) {
        if (m < 3) {
            m += 12;
            y -= 1;
        }
        int k = y % 100;
        int j = y / 100;
        int h = (d + (13 * (m + 1)) / 5 + k + k / 4 + j / 4 - 2 * j) % 7;
        return (u8)((h + 6) % 7);  // Convert to 0=Sunday format
    }

    u8 GetDayOfWeek() const {
        if (!isValid) return 0;
        return GetDayOfWeek(year, month, day);
    }

    bool IsWeekend() const {
        if (!isValid) return false;
        u8 dow = GetDayOfWeek();
        return dow == 0 || dow == 6;  // Sunday or Saturday
    }

    u32 GetWeekNumber() const {
        if (!isValid) return 0;
        s32 days = 0;
        for (u16 yr = 2024; yr < year; ++yr) {
            bool leap = (yr % 4 == 0 && yr % 100 != 0) || (yr % 400 == 0);
            days += leap ? 366 : 365;
        }
        static const u8 daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        for (u8 mo = 1; mo < month; ++mo) {
            days += daysInMonth[mo - 1];
            if (mo == 2) {
                bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
                if (leap) days += 1;
            }
        }
        days += day - 1;
        return days / 7;
    }

    bool IsVRMultiplierWeekend() const {
        if (!IsWeekend()) return false;
        u32 weekNum = GetWeekNumber();
        return (weekNum % 2) == 1;  // Odd weeks get the multiplier
    }

    static u8 GetVRMultiplierRegion(u32 weekNum) {
        static const u8 regions[] = {0x0C, 0x0B, 0x0D};  // vs_12, vs_11, vs_13
        u32 cycleIndex = (weekNum / 2) % 3;  // Every 2 weeks, cycle to next region
        return regions[cycleIndex];
    }

    u8 GetCurrentVRMultiplierRegion() const {
        return GetVRMultiplierRegion(GetWeekNumber());
    }
};

}  // namespace Pulsar

#endif