#pragma once
#include <cstdint>
#include <ctime>
#include <limits>
#include <stdexcept>

namespace fourstory::server {
// Same field widths as ODBC TIMESTAMP_STRUCT; no ODBC dependency in this seam.
struct DatabaseTimestamp {
    std::int16_t year;
    std::uint16_t month,day,hour,minute,second;
    std::uint32_t fraction;
};
// TNetDef.h __DBTOTIME/__TIMETODB use CTime's local calendar, not UTC.
// Set the service process TZ explicitly to match the reference environment.
inline std::int64_t database_to_time(const DatabaseTimestamp& timestamp) {
    if(timestamp.year<2000) return 0;
    if(timestamp.month<1 || timestamp.month>12 || timestamp.day<1 || timestamp.day>31 ||
       timestamp.hour>23 || timestamp.minute>59 || timestamp.second>59)
        throw std::invalid_argument("Invalid database calendar timestamp");
    std::tm calendar{};
    calendar.tm_year=timestamp.year-1900;calendar.tm_mon=timestamp.month-1;
    calendar.tm_mday=timestamp.day;calendar.tm_hour=timestamp.hour;
    calendar.tm_min=timestamp.minute;calendar.tm_sec=timestamp.second;calendar.tm_isdst=-1;
    const auto result=std::mktime(&calendar);
    if(result==static_cast<std::time_t>(-1)) throw std::out_of_range("Database timestamp outside time range");
    // Reject invalid dates instead of silently normalizing a corrupted record.
    if(calendar.tm_year!=timestamp.year-1900 || calendar.tm_mon!=timestamp.month-1 ||
       calendar.tm_mday!=timestamp.day || calendar.tm_hour!=timestamp.hour ||
       calendar.tm_min!=timestamp.minute || calendar.tm_sec!=timestamp.second)
        throw std::invalid_argument("Nonexistent database calendar timestamp");
    return static_cast<std::int64_t>(result);
}
inline DatabaseTimestamp time_to_database(std::int64_t seconds) {
    if(!seconds) return {1900,1,1,0,0,0,0};
    static_assert(sizeof(std::time_t)>=8,"Server timestamps require 64-bit time_t");
    auto value=static_cast<std::time_t>(seconds);std::tm calendar{};
    if(!localtime_r(&value,&calendar) || calendar.tm_year+1900>std::numeric_limits<std::int16_t>::max() ||
       calendar.tm_year+1900<std::numeric_limits<std::int16_t>::min())
        throw std::out_of_range("Time outside database calendar range");
    return {static_cast<std::int16_t>(calendar.tm_year+1900),
        static_cast<std::uint16_t>(calendar.tm_mon+1),static_cast<std::uint16_t>(calendar.tm_mday),
        static_cast<std::uint16_t>(calendar.tm_hour),static_cast<std::uint16_t>(calendar.tm_min),
        static_cast<std::uint16_t>(calendar.tm_sec),0};
}
}
