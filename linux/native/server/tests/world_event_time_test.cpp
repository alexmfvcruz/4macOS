#include "../world_event_time.hpp"
#include <cassert>
#include <iostream>
#include <cstdlib>
using namespace fourstory::server;
using namespace fourstory::server::world_data;
int main() {
    CalendarTime now(2024,2,29,12,30,0);assert(now.GetDayOfWeek()==5);
    assert(now.Format("%Y-%m-%d")=="2024-02-29");
    assert(next_event_time(5,12,30,now.GetTime())==now.GetTime());
    assert(next_event_time(5,12,29,now.GetTime())==CalendarTime(2024,3,7,12,29,0).GetTime());
    assert(next_event_time(6,0,0,now.GetTime())==CalendarTime(2024,3,1,0,0,0).GetTime());
    assert(next_event_time(1,0,0,now.GetTime())==CalendarTime(2024,3,3,0,0,0).GetTime());
    assert(next_event_time(0,0,0,now.GetTime())==-1 && next_event_time(8,0,0,now.GetTime())==-1);
    assert(next_event_time(5,25,0,now.GetTime())==-1);
    bool rejected=false;try{next_event_time(5,24,0,now.GetTime());}catch(const std::invalid_argument&){rejected=true;}assert(rejected);
    rejected=false;try{CalendarTime bad(2023,2,29,0,0,0);}catch(const std::invalid_argument&){rejected=true;}assert(rejected);
    const CalendarTime wide(2200,1,1,0,0,0);assert(wide.GetTime()>0xffffffffLL);
    rejected=false;try{CalendarTime(std::numeric_limits<std::int64_t>::max())+CalendarSpan(1,0,0,0);}catch(const std::overflow_error&){rejected=true;}assert(rejected);
    assert(setenv("TZ","Europe/Luxembourg",1)==0);tzset();
    CalendarTime before_dst(2024,3,25,12,0,0);
    // Original CTimeSpan adds elapsed 24-hour days, preserving its DST shift.
    assert(next_event_time(1,12,0,before_dst.GetTime())==CalendarTime(2024,3,31,13,0,0).GetTime());
    rejected=false;try{CalendarTime missing(2024,3,31,2,30,0);}catch(const std::invalid_argument&){rejected=true;}assert(rejected);
    assert(setenv("TZ","UTC",1)==0);tzset();
    std::cout<<"PASS original World event weekday/calendar rollover, exact/past boundaries, wide time and invalid dates\n";
}
