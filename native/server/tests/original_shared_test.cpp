#include "../packet_platform.hpp"
#include <cstdlib>
#include <queue>
using std::queue;
#include "../../../source/4Story 5.0 Source Files/Server/TNetLib/Packet.h"
#include "../../../source/4Story 5.0 Source Files/Server/TNetLib/TNetDef.h"
#include <cassert>
#include <iostream>
#include <thread>
int main() {
    assert(setenv("TZ","UTC0",1)==0);tzset();
    TIMESTAMP_STRUCT timestamp{2000,1,1,0,0,0,0};assert(__DBTOTIME(timestamp)==946684800);
    __TIMETODB(0,timestamp);assert(timestamp.year==1900 && timestamp.month==1 && timestamp.day==1);
    CRITICAL_SECTION section;unsigned count=0;
    auto increment=[&] {
        for(unsigned i=0;i<1000;++i) {
            SMART_LOCKCS(&section);
            CCSLock nested(&section); // Windows critical sections are recursive
            ++count;
        }
    };
    std::thread first(increment),second(increment);first.join();second.join();assert(count==2000);
    try {SMART_LOCKCS(&section);throw std::runtime_error("unwind");}catch(const std::runtime_error&){}
    assert(section.try_lock());section.unlock();
    assert(TRand(0)==0 && TRand(1)==0);
    PACKETBUF buffer;assert(!buffer.m_pSESSION && buffer.m_packet.GetSize()==16);
    std::cout<<"PASS original shared header, recursive synchronization, exception unlock and database time boundary\n";
}
