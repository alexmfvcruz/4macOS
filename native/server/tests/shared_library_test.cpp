#include "../odbc_platform.hpp"
#include "../../../source/4Story 5.0 Source Files/Server/TNetLib/Packet.h"
#include <queue>
using std::queue;
#include "../../../source/4Story 5.0 Source Files/Server/TNetLib/TNetDef.h"
#include <cassert>
#include <iostream>
int main() {
    CPacket packet; packet << DWORD(0x12345678);
    packet.Rewind(FALSE); DWORD value=0; packet >> value;
    assert(value==0x12345678);
    CSqlDatabase database; database.Close();
    TIMESTAMP_STRUCT stamp{}; __TIMETODB(0,stamp);
    assert(stamp.year==1900 && stamp.month==1 && __DBTOTIME(stamp)==0);
    __TIMETODB(946684800,stamp);
    assert(__DBTOTIME(stamp)==946684800);
    std::cout<<"PASS linked original shared library, packet/ODBC coexistence and timestamp bridge\n";
}
