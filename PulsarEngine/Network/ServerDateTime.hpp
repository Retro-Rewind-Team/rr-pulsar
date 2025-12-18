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
};

}  // namespace Pulsar

#endif