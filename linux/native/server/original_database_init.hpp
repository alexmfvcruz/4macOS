#pragma once
#include "config.hpp"
#include "odbc_platform.hpp"
#include "../../source/4Story 5.0 Source Files/Server/TNetLib/ErrorCode.h"
extern BOOL InitQueryTControlSvr(CSqlDatabase*);
extern BOOL InitQueryTLoginSvr(CSqlDatabase*);
extern BOOL InitQueryTWorldSvr(CSqlDatabase*);
extern BOOL InitQueryTMapSvr(CSqlDatabase*);
extern void ClearOriginalLoginUsers(CSqlDatabase*);
namespace fourstory::server {
// Compile the original role's InitDB body. Alias names match its existing fields;
// the database and credentials are caller-owned, with no synthetic success path.
// Link each role's actual DBAccess.cpp registration implementation.
template<Service Role>
DWORD initialize_original_database(CSqlDatabase& database,const char* dsn,
                                   const char* user,const char* password) {
    if(!dsn || !user || !password) throw std::invalid_argument("Null database configuration");
    auto& m_db=database;auto* pDB=&database;
    const auto* m_szDSN=dsn;const auto* m_szGameDSN=dsn;
    const auto* m_szDBUserID=user;const auto* m_szDBPasswd=password;const auto* m_szGamePasswd=password;
    (void)m_db;(void)pDB;(void)m_szDSN;(void)m_szGameDSN;(void)m_szDBPasswd;(void)m_szGamePasswd;
    if constexpr(Role==Service::control) {
#include "../../source/4Story 5.0 Source Files/Server/TControlSvr/InitDatabase.inc"
    } else if constexpr(Role==Service::login) {
#include "../../source/4Story 5.0 Source Files/Server/TLoginSvr/InitDatabase.inc"
    } else if constexpr(Role==Service::world) {
#include "../../source/4Story 5.0 Source Files/Server/TWorldSvr/InitDatabase.inc"
    } else if constexpr(Role==Service::map) {
#include "../../source/4Story 5.0 Source Files/Server/TMapSvr/InitDatabase.inc"
    } else {
        static_assert(Role==Service::control,"Unknown original database role");
    }
}
}
