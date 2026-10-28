#pragma once
#include "odbc_platform.hpp"
#include "../../source/4Story 5.0 Source Files/Server/TNetLib/Packet.h"
#include <arpa/inet.h>
#include <functional>
namespace fourstory::server::control_data {
using DWORD_PTR=std::uintptr_t;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-declarations"
#include "../../source/4Story 5.0 Source Files/Server/TControlSvr/TControlType.h"
#pragma clang diagnostic pop
// Original pointer relationships remain stable; this state owns the records.
struct StartupData {
    MAPTMACHINE machines;
    MAPTGROUP groups;
    MAPTSVRTYPE types;
    MAPTSVRTEMP services;
    MAPEVENTINFO events;
    sockaddr_in address{};
    CString own_address;
    DWORD event_index=0;
    StartupData()=default;
    StartupData(const StartupData&)=delete;
    StartupData& operator=(const StartupData&)=delete;
    ~StartupData() {clear();}
    void clear() {
        for(auto& item:services) delete item.second;
        services.clear();
        for(auto& item:machines) delete item.second;
        machines.clear();
        for(auto& item:groups) delete item.second;
        groups.clear();
        for(auto& item:types) delete item.second;
        types.clear();events.clear();event_index=0;address={};own_address.Empty();
    }
};
}
extern DWORD LoadOriginalControlStartup(CSqlDatabase*,fourstory::server::control_data::StartupData*,
    const std::function<void()>&);
