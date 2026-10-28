#include "original_database_lifecycle.hpp"
#include "../../../source/4Story 5.0 Source Files/Lib/Own/TProtocol/include/NetCode.h"
#include "../../../source/4Story 5.0 Source Files/Server/TWorldSvr/TItemData.h"
using TITEM=tagTITEM;
#include "../../../source/4Story 5.0 Source Files/Server/TWorldSvr/DBAccess.h"
#include <cassert>
#include <iostream>
extern BOOL InitQueryTWorldSvr(CSqlDatabase*);
int main() {
    fourstory::server::tests::check_original_database_lifecycle<fourstory::server::Service::world>();
    CSqlDatabase failed_database;
    assert(fourstory::server::initialize_original_database<fourstory::server::Service::world>(
        failed_database,"4Story_Nonexistent_Test_DSN","","")==EC_INITSERVICE_DBOPENFAILED);
    assert(!failed_database.IsOpen() && failed_database.HDBC()==SQL_NULL_HANDLE);

    CSqlDatabase database; assert(!InitQueryTWorldSvr(&database)); CTBLGuildChart query(&database);
    assert(query.GetNumCol()==15);
    std::cout<<"PASS all original TWorldSvr query declarations compile natively\n";
}
