#pragma once
#ifndef FOURSTORY_NATIVE_ODBC
#error "Original ODBC boundary requires FOURSTORY_NATIVE_ODBC"
#endif
#include <sql.h>
#include <sqlext.h>
#include <sqlucode.h>
#include "packet_platform.hpp"
#include "../../source/4Story 5.0 Source Files/Server/TNetLib/CriticalSectionLock.h"
#include <map>
#include <cstdio>
using TCHAR=char;
using INT=int;
#define _T(value) value
inline void ZeroMemory(void* pointer,std::size_t bytes) {std::memset(pointer,0,bytes);}
inline void native_odbc_diagnostics(SQLHSTMT statement) {
    // Report state/code only; diagnostic text and query strings may contain data.
    for(SQLSMALLINT record=1;record<=256;++record) {
        SQLCHAR state[6]{},text[512]{};SQLINTEGER code=0;SQLSMALLINT length=0;
        const auto result=SQLGetDiagRec(SQL_HANDLE_STMT,statement,record,state,&code,text,sizeof(text),&length);
        if(result==SQL_NO_DATA || !SQL_SUCCEEDED(result)) break;
        std::fprintf(stderr,"ODBC state=%.5s code=%ld\n",reinterpret_cast<const char*>(state),static_cast<long>(code));
    }
}
using std::map;
using std::string;
using std::vector;
#include "../../source/4Story 5.0 Source Files/Server/TNetLib/SqlDatabase.h"
#include "../../source/4Story 5.0 Source Files/Server/TNetLib/SqlBase.h"
#include "../../source/4Story 5.0 Source Files/Server/TNetLib/BindDesc.h"
#include "../../source/4Story 5.0 Source Files/Server/TNetLib/SqlQuery.h"
#include "../../source/4Story 5.0 Source Files/Server/TNetLib/SqlDirect.h"

// Startup loaders must distinguish a complete empty result from a failed read.
inline bool native_query_open(CSqlBase& query) {
    if (!query.Open()) throw std::runtime_error("Startup database query failed to open");
    return true;
}
inline bool native_query_fetch(CSqlBase& query) {
    if (query.Fetch()) return true;
    if (query.NativeFetchStatus() != SQL_NO_DATA)
        throw std::runtime_error("Startup database query failed while reading");
    return false;
}
