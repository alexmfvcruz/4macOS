#pragma once
#include "world_data_types.hpp"
#include "calendar_time.hpp"
namespace fourstory::server::world_data {
inline __int64 next_event_time(BYTE bWeek,BYTE bHour,BYTE bMin,std::int64_t current_time) {
    using CTime=fourstory::server::CalendarTime;
    using CTimeSpan=fourstory::server::CalendarSpan;
#include "../../source/4Story 5.0 Source Files/Server/TWorldSvr/NextEventTime.inc"
}
}
