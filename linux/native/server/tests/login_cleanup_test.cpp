#include "../original_database_init.hpp"
#include "../login_startup_data.hpp"
#include "../../../source/4Story 5.0 Source Files/Lib/Own/TProtocol/include/NetCode.h"
namespace fourstory::server::queries::TLoginSvr {
#include "../../../source/4Story 5.0 Source Files/Server/TLoginSvr/DBAccess.h"
}
#include <cassert>
#include <iostream>
#include <thread>
CSqlDatabase* current_database=nullptr;
unsigned attempts=0;
extern "C" SQLRETURN SQL_API SQLAllocHandle(SQLSMALLINT type,SQLHANDLE,SQLHANDLE* output) {
    assert(type==SQL_HANDLE_STMT && current_database);++attempts;
    bool acquired=false;
    std::thread probe([&]{
        acquired=current_database->m_csExecution.try_lock();
        if(acquired) current_database->m_csExecution.unlock();
    });probe.join();assert(!acquired);
    *output=SQL_NULL_HANDLE;return SQL_ERROR;
}
int main() {
    CSqlDatabase database;current_database=&database;
    ClearOriginalLoginUsers(&database);assert(attempts==0);
    auto* query=new fourstory::server::queries::TLoginSvr::CSPClearLoginUser(&database);
    assert(database.AddQuery(query,"CSPClearLoginUser"));
    ClearOriginalLoginUsers(&database);assert(attempts==1);
    using namespace fourstory::server::queries::TLoginSvr;
    assert(database.AddQuery(new CSPLoadService(&database),"CSPLoadService"));
    assert(database.AddQuery(new CSPGetNation(&database),"CSPGetNation"));
    assert(database.AddQuery(new CTBLUserCount(&database),"CTBLUserCount"));
    assert(database.AddQuery(new CTBLVeteranChart(&database),"CTBLVeteranChart"));
    LoginStartupData startup;
    assert(LoadOriginalLoginStartup(&database,&startup,nullptr)==EC_INITSERVICE_DBOPENFAILED);
    assert(attempts==2 && startup.nation==0 && startup.current_users.empty());
    // The original void callback ignores Call failure; preserve that behavior.
    database.Close();assert(database.GetQuery("CSPClearLoginUser")==nullptr);
    std::cout<<"PASS original Login cleanup query lookup, Call attempt, critical-section lock and owned cleanup\n";
}
