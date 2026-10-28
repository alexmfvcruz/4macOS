#include "original_database_lifecycle.hpp"
#include "../../../source/4Story 5.0 Source Files/Lib/Own/TProtocol/include/NetCode.h"
#include "../../../source/4Story 5.0 Source Files/Server/TControlSvr/TControlConstants.h"
#include "../../../source/4Story 5.0 Source Files/Server/TControlSvr/DBAccess.h"
#include <cassert>
#include <iostream>
extern BOOL InitQueryTControlSvr(CSqlDatabase*);
int main() {
    fourstory::server::tests::check_original_database_lifecycle<fourstory::server::Service::control>();
    CSqlDatabase failed_database;
    assert(fourstory::server::initialize_original_database<fourstory::server::Service::control>(
        failed_database,"4Story_Nonexistent_Test_DSN","","")==EC_INITSERVICE_DBOPENFAILED);
    assert(!failed_database.IsOpen() && failed_database.HDBC()==SQL_NULL_HANDLE);

    CSqlDatabase database; assert(!InitQueryTControlSvr(&database)); CTBLMachine query(&database);
    assert(query.GetNumCol()==3);
    std::cout<<"PASS all original TControlSvr query declarations compile natively\n";
}
