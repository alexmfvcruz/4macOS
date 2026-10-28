#include "original_database_lifecycle.hpp"
#include "../../../source/4Story 5.0 Source Files/Lib/Own/TProtocol/include/NetCode.h"
#include "../../../source/4Story 5.0 Source Files/Server/TMapSvr/TMapQueryConstants.h"
#include "../../../source/4Story 5.0 Source Files/Server/TMapSvr/TItemQueryData.h"
using TITEMQUERY=tagTITEMQUERY;
#include "../../../source/4Story 5.0 Source Files/Server/TMapSvr/DBAccess.h"
#include <cassert>
#include <iostream>
extern BOOL InitQueryTMapSvr(CSqlDatabase*);
int main() {
    fourstory::server::tests::check_original_database_lifecycle<fourstory::server::Service::map>();
    CSqlDatabase failed_database;
    assert(fourstory::server::initialize_original_database<fourstory::server::Service::map>(
        failed_database,"4Story_Nonexistent_Test_DSN","","")==EC_INITSERVICE_DBOPENFAILED);
    assert(!failed_database.IsOpen() && failed_database.HDBC()==SQL_NULL_HANDLE);

    CSqlDatabase database; assert(!InitQueryTMapSvr(&database)); CTBLChannelList query(&database);
    assert(query.GetNumCol()==1);
    std::cout<<"PASS all original TMapSvr query declarations compile natively\n";
}
