#pragma once
#include "odbc_platform.hpp"
#include <arpa/inet.h>
#include "client_reference_file.hpp"
#include "../../source/4Story 5.0 Source Files/Server/TLoginSvr/VeteranData.h"
struct LoginStartupData {
    sockaddr_in control_address{};
    BYTE nation=0;
    std::map<DWORD,DWORD> current_users;
    std::vector<tagTVETERAN> veterans;
    fourstory::server::ClientReferenceFile client_reference;
    std::uint64_t client_checksum=0;
};
extern DWORD LoadOriginalLoginStartup(CSqlDatabase*,LoginStartupData*,const char*);
