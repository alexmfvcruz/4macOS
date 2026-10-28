#include "../world_database_actions.hpp"
#include "../../../source/4Story 5.0 Source Files/Lib/Own/TProtocol/include/DMProtocol.h"
#include "../../../source/4Story 5.0 Source Files/Server/TNetLib/ErrorCode.h"
namespace fourstory::server::queries::TWorldSvr {
using TITEM=world_data::TITEM;
#include "../../../source/4Story 5.0 Source Files/Server/TWorldSvr/DBAccess.h"
}
#include <cassert>
#include <thread>
#include <iostream>
CSqlDatabase* active_database=nullptr;unsigned attempts=0;
extern "C" SQLRETURN SQL_API SQLAllocHandle(SQLSMALLINT type,SQLHANDLE,SQLHANDLE* output) {
    assert(type==SQL_HANDLE_STMT);++attempts;
    bool unlocked=false;
    std::thread probe([&]{unlocked=active_database->m_csExecution.try_lock();if(unlocked) active_database->m_csExecution.unlock();});probe.join();assert(!unlocked);
    *output=SQL_NULL_HANDLE;return SQL_ERROR;
}
int main() {
    using namespace fourstory::server;
    using namespace queries::TWorldSvr;
    CSqlDatabase database;active_database=&database;unsigned batch=0;
    PACKETBUF refund;refund.m_packet.SetID(DM_TOURNAMENTPAYBACK_REQ)<<DWORD(123)<<DWORD(7005009);refund.m_packet.Rewind(FALSE);
    auto result=ExecuteOriginalWorldTournamentPayback(&database,&refund,[&](std::vector<BYTE>){++batch;});
    assert(!result.executed && result.handler_code==EC_INITSERVICE_PREPAREQUERY && attempts==0);
    auto* query=new CSPTournamentPayback(&database);query->m_dwPostID=999;
    assert(database.AddQuery(query,"CSPTournamentPayback"));
    result=ExecuteOriginalWorldTournamentPayback(&database,&refund,[&](std::vector<BYTE>){++batch;});
    assert(!result.executed && result.handler_code==0 && attempts==1 && !batch);
    assert(query->m_dwCharID==123 && query->m_dwGold==7 && query->m_dwSilver==5 && query->m_dwCooper==9 && query->m_dwPostID==0);
    auto* deletion=new CSPTnmtEventDel(&database);assert(database.AddQuery(deletion,"CSPTnmtEventDel"));
    PACKETBUF request;request.m_packet.SetID(DM_TNMTEVENTSCHEDULEDEL_REQ)<<WORD(6);request.m_packet.Rewind(FALSE);
    result=ExecuteOriginalWorldTournamentDelete(&database,&request);assert(!result.executed && result.handler_code==0 && attempts==2 && deletion->m_wTourID==6);
    PACKETBUF malformed;malformed.m_packet.SetID(DM_TNMTEVENTSCHEDULEDEL_REQ);malformed.m_packet.Rewind(FALSE);
    bool rejected=false;try{ExecuteOriginalWorldTournamentDelete(&database,&malformed);}catch(const std::out_of_range&){rejected=true;}
    assert(rejected && attempts==2);
    result=ExecuteOriginalWorldTournamentClear(&database);
    assert(!result.executed && result.handler_code==EC_INITSERVICE_PREPAREQUERY && attempts==2);
    auto* clear=new CSPTournamentClear(&database);assert(database.AddQuery(clear,"CSPTournamentClear"));
    result=ExecuteOriginalWorldTournamentClear(&database);
    assert(!result.executed && result.handler_code==0 && attempts==3 && clear->m_bClear==TRUE);
    database.Close();
    std::cout<<"PASS original World refund/delete/clear handlers, money widths, execution failure reporting, locking and malformed packet rejection\n";
}
