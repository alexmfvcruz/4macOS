#include "../database_time.hpp"
#include <cassert>
#include <cstdlib>
#include <iostream>
using namespace fourstory::server;
int main() {
    assert(setenv("TZ","UTC0",1)==0);tzset();
    assert(database_to_time({1999,12,31,23,59,59,0})==0);
    const auto sentinel=time_to_database(0);assert(sentinel.year==1900 && sentinel.month==1 && sentinel.day==1 && !sentinel.fraction);
    assert(database_to_time({2000,1,1,0,0,0,999})==946684800);
    const auto leap=database_to_time({2024,2,29,12,30,40,123});
    auto roundtrip=time_to_database(leap);assert(roundtrip.year==2024 && roundtrip.month==2 && roundtrip.day==29);
    assert(roundtrip.hour==12 && roundtrip.minute==30 && roundtrip.second==40 && !roundtrip.fraction);
    bool invalid=false;
    try{database_to_time({2023,2,29,0,0,0,0});}catch(const std::invalid_argument&){invalid=true;}
    assert(invalid);
    assert(setenv("TZ","EST5",1)==0);tzset();
    assert(database_to_time({2000,1,1,0,0,0,0})==946684800+5*3600);
    assert(time_to_database(946684800+5*3600).hour==0);
    std::cout<<"PASS portable database time, null sentinel, leap date, 64-bit epoch and explicit local timezone\n";
}
