#include "../odbc_platform.hpp"
#include <cassert>
#include <iostream>
#include <dlfcn.h>
class StartupQuery : public CSqlQuery {
public:
    DEF_QUERY(StartupQuery, "SELECT 1")
};
BEGIN_INIT_QUERY(NativeFailure)
    PREDEFINE_QUERY(StartupQuery)
END_INIT_QUERY()
static SQLRETURN fetch_result=SQL_NO_DATA;
extern "C" SQLRETURN SQL_API SQLFetch(SQLHSTMT) { return fetch_result; }
extern "C" SQLRETURN SQL_API SQLCloseCursor(SQLHSTMT) { return SQL_SUCCESS; }
extern "C" SQLRETURN SQL_API SQLFreeHandle(SQLSMALLINT kind,SQLHANDLE handle) {
    if(kind==SQL_HANDLE_STMT && handle==reinterpret_cast<SQLHANDLE>(1)) return SQL_SUCCESS;
    const auto original=reinterpret_cast<decltype(&SQLFreeHandle)>(dlsym(RTLD_NEXT,"SQLFreeHandle"));
    assert(original);return original(kind,handle);
}
extern "C" SQLRETURN SQL_API SQLGetDiagRec(SQLSMALLINT,SQLHANDLE,SQLSMALLINT,
    SQLCHAR*,SQLINTEGER*,SQLCHAR*,SQLSMALLINT,SQLSMALLINT*) { return SQL_NO_DATA; }
class FetchQuery : public CSqlBase {
public:
    explicit FetchQuery(CSqlDatabase* db):CSqlBase(db,"SELECT 1") {}
    void arm() {m_hstmt=reinterpret_cast<SQLHSTMT>(1);}
};
int main() {
    CSqlDatabase database;assert(!database.IsOpen() && database.HDBC()==SQL_NULL_HANDLE);
    FetchQuery fetch(&database);
    for(auto result : {SQL_SUCCESS,SQL_SUCCESS_WITH_INFO,SQL_NO_DATA,SQL_ERROR,SQL_INVALID_HANDLE}) {
        fetch.arm();fetch_result=result;
        bool threw=false, row=false;
        try {row=native_query_fetch(fetch);} catch(const std::runtime_error&) {threw=true;}
        assert(threw==(result==SQL_ERROR || result==SQL_INVALID_HANDLE));
        assert(row==(result==SQL_SUCCESS || result==SQL_SUCCESS_WITH_INFO));
        assert(fetch.NativeFetchStatus()==result);
        fetch.FreeStatement();
    }
    bool missing_handle=false;
    try {native_query_fetch(fetch);} catch(const std::runtime_error&) {missing_handle=true;}
    assert(missing_handle && fetch.NativeFetchStatus()==SQL_ERROR);
    for(unsigned i=0;i<3;++i) {
        assert(!database.Open("4Story_Nonexistent_Test_DSN","",""));
        assert(!database.IsOpen() && database.HDBC()==SQL_NULL_HANDLE);database.Close();
    }
    assert(!InitQueryNativeFailure(&database));
    assert(!database.GetQuery("StartupQuery"));
    auto* query=new CSqlQuery(&database,"SELECT 1");
    assert(database.AddQuery(query,"owned"));assert(database.GetQuery("owned")==query);
    {CSqlQuery failed_duplicate(&database,"owned","SELECT 2");}
    assert(database.GetQuery("owned")==query); // failed registration must not remove existing ownership
    assert(database.DelQuery("owned")==query);delete query;
    auto* cleanup=new CSqlQuery(&database,"SELECT 3");assert(database.AddQuery(cleanup,"cleanup"));
    database.Close();assert(!database.GetQuery("cleanup"));database.Close();
    bool refused=false;try{CSqlQuery too_long(&database,std::string(1024,'x').c_str());}
    catch(const std::length_error&){refused=true;}assert(refused);
    CSqlQuery unbound(&database,"SELECT 1");long wide=0;
    refused=false;try{unbound.BindCol(1,&wide);}catch(const std::logic_error&){refused=true;}
    assert(refused);
    std::cout<<"PASS original native ODBC failure cleanup, query ownership, repeated close and unsafe-width rejection\n";
}
