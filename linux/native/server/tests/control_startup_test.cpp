#include "../control_startup_data.hpp"
#include "../../../source/4Story 5.0 Source Files/Lib/Own/TProtocol/include/NetCode.h"
#include "../../../source/4Story 5.0 Source Files/Server/TNetLib/ErrorCode.h"
#include <cassert>
#include <iostream>
namespace fourstory::server::queries::TControlSvr {
void ParseEventValue(control_data::EVENTINFO&,CString,BYTE);
}
int main() {
    using namespace fourstory::server;
    CSqlDatabase database;control_data::StartupData startup;bool called=false;
    assert(LoadOriginalControlStartup(&database,&startup,[&]{called=true;})==EC_INITSERVICE_PREPAREQUERY);
    assert(!called && startup.services.empty());
    bool rejected=false;
    try{LoadOriginalControlStartup(&database,&startup,{});}catch(const std::invalid_argument&){rejected=true;}
    assert(rejected);
    control_data::EVENTINFO event{};
    queries::TControlSvr::ParseEventValue(event,"12-25;;13-50;",EVENT_CASHSALE);
    assert(event.m_vCashItem.size()==2 && event.m_vCashItem[0].m_wID==12 && event.m_vCashItem[1].m_bSaleValue==50);
    queries::TControlSvr::ParseEventValue(event,";;7;8;;",EVENT_MONSPAWN);
    assert(event.m_stMONEVENT.m_vSpawnID.size()==2 && event.m_stMONEVENT.m_vSpawnID[1]==8);
    queries::TControlSvr::ParseEventValue(event,"12-1000-25-1.5-2.5-3.5;",EVENT_MONREGEN);
    assert(event.m_vMONREGEN.size()==1 && event.m_vMONREGEN[0].m_dwDelay==1000 && event.m_vMONREGEN[0].m_fPosZ==3.5f);
    std::cout<<"PASS original Control startup dependency guards and populated cash/spawn/regen event parsing\n";
}
