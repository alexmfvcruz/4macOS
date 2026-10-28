#include "../odbc_platform.hpp"
#include <cassert>
#include <iostream>
SQLSMALLINT captured_type=0;SQLPOINTER captured_value=nullptr;SQLLEN* captured_indicator=nullptr;
extern "C" SQLRETURN SQL_API SQLBindCol(SQLHSTMT,SQLUSMALLINT column,SQLSMALLINT type,
    SQLPOINTER value,SQLLEN,SQLLEN* indicator) {
    assert(column==1);captured_type=type;captured_value=value;captured_indicator=indicator;return SQL_SUCCESS;
}
extern "C" SQLRETURN SQL_API SQLBindParameter(SQLHSTMT,SQLUSMALLINT parameter,SQLSMALLINT,
    SQLSMALLINT type,SQLSMALLINT sql_type,SQLULEN,SQLSMALLINT,SQLPOINTER value,SQLLEN,SQLLEN* indicator) {
    assert(parameter==1 && sql_type==SQL_INTEGER);
    captured_type=type;captured_value=value;captured_indicator=indicator;return SQL_SUCCESS;
}
class Probe:public CSqlQuery {
public:
    using CSqlQuery::CSqlQuery;
    int GetNumCol() override{return 1;}
};
class MacroQuery:public CSqlQuery {
public:
    DWORD value=0;
    DEF_QUERY(MacroQuery,_T("SELECT 1"))
    BEGIN_COLUMN(1)
    COLUMN_ENTRY(value)
    END_COL()
    BEGIN_PARAM(1)
    PARAM_ENTRY(SQL_PARAM_INPUT,value)
    END_PARAM()
};
int main() {
    CSqlDatabase database;Probe query(&database,"SELECT 1");
    struct Storage {DWORD value=0;DWORD canary=0xaabbccdd;} storage;
    assert(query.BindCol(1,&storage.value)==SQL_SUCCESS && captured_type==SQL_C_SLONG);
    assert(captured_value==&storage.value && captured_indicator);
    const SQLINTEGER minus_one=-1;std::memcpy(captured_value,&minus_one,sizeof(minus_one));
    *captured_indicator=SQL_NULL_DATA;
    assert(storage.value==0xffffffff && storage.canary==0xaabbccdd && query.IsNull(0));
    assert(query.BindParam(SQL_PARAM_INPUT,1,&storage.value)==SQL_SUCCESS && captured_type==SQL_C_SLONG);
    assert(captured_value==&storage.value);*captured_indicator=SQLLEN(0x100000000);
    assert(*captured_indicator==SQLLEN(0x100000000));
    MacroQuery macro(&database);assert(macro.GetNumCol()==1 && macro.GetNumParam()==1);
    assert(macro.BindColumns()==SQL_SUCCESS && captured_value==&macro.value);
    assert(macro.BindParameters()==SQL_SUCCESS && captured_value==&macro.value);
    std::cout<<"PASS original DWORD binding type, four-byte data writes, canary preservation and eight-byte indicators\n";
}
