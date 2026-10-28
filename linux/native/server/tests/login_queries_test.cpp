#include "original_database_lifecycle.hpp"
#include "../../../source/4Story 5.0 Source Files/Lib/Own/TProtocol/include/NetCode.h"
#include "../../../source/4Story 5.0 Source Files/Server/TLoginSvr/DBAccess.h"
#include <cassert>
#include <iostream>
extern BOOL InitQueryTLoginSvr(CSqlDatabase*);
int main() {
    fourstory::server::tests::check_original_database_lifecycle<fourstory::server::Service::login>();
    CSqlDatabase failed_database;
    assert(fourstory::server::initialize_original_database<fourstory::server::Service::login>(
        failed_database,"4Story_Nonexistent_Test_DSN","","")==EC_INITSERVICE_DBOPENFAILED);
    assert(!failed_database.IsOpen() && failed_database.HDBC()==SQL_NULL_HANDLE);

    CSqlDatabase database; assert(!InitQueryTLoginSvr(&database));CSPLogin login(&database);
    static_assert(sizeof(login.m_dwID)==4 && sizeof(login.m_dwKEY)==4);
    assert(login.GetNumParam()==13);
    std::cout<<"PASS original Login query declarations and Windows-width login outputs compile natively\n";
}
