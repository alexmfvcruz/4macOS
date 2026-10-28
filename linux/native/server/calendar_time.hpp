#pragma once
#include "legacy_string.hpp"
#include <ctime>
#include <cstdint>
#include <limits>
#include <stdexcept>
namespace fourstory::server {
class CalendarSpan {
public:
    std::int64_t seconds;
    CalendarSpan(int days,int hours,int minutes,int value)
        :seconds(std::int64_t(days)*86400+std::int64_t(hours)*3600+std::int64_t(minutes)*60+value) {}
};
// The original startup calendar API, using the service's configured local TZ.
// Calendar construction validates fields; spans add elapsed seconds.
class CalendarTime {
    std::int64_t seconds_;
    std::tm calendar() const {
        static_assert(sizeof(std::time_t)>=8);
        const auto value=static_cast<std::time_t>(seconds_);std::tm result{};
        if(!localtime_r(&value,&result)) throw std::out_of_range("Server calendar timestamp");
        return result;
    }
public:
    CalendarTime(std::int64_t value):seconds_(value) {}
    CalendarTime(int year,int month,int day,int hour,int minute,int second) {
        if(year<1970 || year>3000 || month<1 || month>12 || day<1 || day>31 ||
           hour<0 || hour>23 || minute<0 || minute>59 || second<0 || second>59)
            throw std::invalid_argument("Invalid server calendar fields");
        std::tm value{};value.tm_year=year-1900;value.tm_mon=month-1;value.tm_mday=day;
        value.tm_hour=hour;value.tm_min=minute;value.tm_sec=second;value.tm_isdst=-1;
        const auto result=std::mktime(&value);
        if(result==std::time_t(-1)) throw std::out_of_range("Server calendar timestamp");
        if(value.tm_year!=year-1900 || value.tm_mon!=month-1 || value.tm_mday!=day ||
           value.tm_hour!=hour || value.tm_min!=minute || value.tm_sec!=second)
            throw std::invalid_argument("Nonexistent server calendar time");
        seconds_=result;
    }
    static CalendarTime GetCurrentTime() {return CalendarTime(std::time(nullptr));}
    std::int64_t GetTime() const noexcept {return seconds_;}
    int GetYear() const {return calendar().tm_year+1900;}
    int GetMonth() const {return calendar().tm_mon+1;}
    int GetDay() const {return calendar().tm_mday;}
    int GetDayOfWeek() const {return calendar().tm_wday+1;}
    int GetHour() const {return calendar().tm_hour;}
    int GetMinute() const {return calendar().tm_min;}
    int GetSecond() const {return calendar().tm_sec;}
    LegacyString Format(const char* pattern) const {
        if(!pattern) throw std::invalid_argument("Missing calendar format");
        if(!*pattern) return {};
        auto value=calendar();char text[256]{};
        if(!std::strftime(text,sizeof(text),pattern,&value)) throw std::out_of_range("Server calendar format");
        return LegacyString(text);
    }
    CalendarTime operator+(CalendarSpan span) const {
        if((span.seconds>0 && seconds_>std::numeric_limits<std::int64_t>::max()-span.seconds) ||
           (span.seconds<0 && seconds_<std::numeric_limits<std::int64_t>::min()-span.seconds))
            throw std::overflow_error("Server calendar span");
        return CalendarTime(seconds_+span.seconds);
    }
};
}
