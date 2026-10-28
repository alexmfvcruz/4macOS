#pragma once
#include "odbc_platform.hpp"
#include "control_manager_authority.hpp"
extern BOOL AuthenticateOriginalControlManager(CSqlDatabase*,const char*,const char*,BYTE*);
namespace fourstory::server {
inline bool authenticate_control_manager(CSqlDatabase& database,ControlManagerAuthority& manager,
                                        const char* id,const char* password) {
    manager.revoke();BYTE authority=0;
    return AuthenticateOriginalControlManager(&database,id,password,&authority) && manager.authenticated_login(authority);
}
}
