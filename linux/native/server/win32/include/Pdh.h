#pragma once
// Native module build: Windows performance counters are unavailable. Every
// query reports PDH_CSTATUS_NO_MACHINE, so CPlatformUsage::Open fails and the
// original Control code treats machine statistics as unavailable.
#include "../atl.hpp"
using PDH_STATUS=LONG;
using HQUERY=void*;
using HCOUNTER=void*;
#define PDH_CSTATUS_NO_MACHINE ((PDH_STATUS)0x800007D0L)
#define PDH_FMT_LONG 0x00000100u
#define PDH_FMT_DOUBLE 0x00000200u
#define PDH_FMT_LARGE 0x00000400u
struct PDH_FMT_COUNTERVALUE {DWORD CStatus;union {LONG longValue;double doubleValue;LONGLONG largeValue;const char* AnsiStringValue;};};
inline PDH_STATUS PdhOpenQuery(const char*,DWORD_PTR,HQUERY* query) {if(query) *query=nullptr;return PDH_CSTATUS_NO_MACHINE;}
inline PDH_STATUS PdhAddCounter(HQUERY,const char*,DWORD_PTR,HCOUNTER* counter) {if(counter) *counter=nullptr;return PDH_CSTATUS_NO_MACHINE;}
inline PDH_STATUS PdhRemoveCounter(HCOUNTER) {return PDH_CSTATUS_NO_MACHINE;}
inline PDH_STATUS PdhCloseQuery(HQUERY) {return PDH_CSTATUS_NO_MACHINE;}
inline PDH_STATUS PdhCollectQueryData(HQUERY) {return PDH_CSTATUS_NO_MACHINE;}
inline PDH_STATUS PdhGetFormattedCounterValue(HCOUNTER,DWORD,LPDWORD,PDH_FMT_COUNTERVALUE* value) {
    if(value) {value->CStatus=static_cast<DWORD>(PDH_CSTATUS_NO_MACHINE);value->largeValue=0;}
    return PDH_CSTATUS_NO_MACHINE;
}
