#pragma once
#include "packet_platform.hpp"
#include <functional>
#include <map>
#include <optional>
#include <stdexcept>

#ifndef FOURSTORY_NATIVE_SERVER
#error "Original Control management requires FOURSTORY_NATIVE_SERVER"
#endif
namespace fourstory::server {
// Explicit database service-ID to supervisor configuration-index mapping.
// No address/name supplied by a network packet becomes an executable command.
class ControlServiceManagement {
public:
    using Command=std::function<bool(const char*,std::size_t)>;
    using Status=std::function<std::optional<unsigned>(std::size_t)>;
private:
    std::map<DWORD,std::size_t> targets_;
    Command command_;
    Status status_;
    bool invoke(const char* action,DWORD id) const {
        const auto target=targets_.find(id);
        if(target==targets_.end()) return false;
        return command_(action,target->second);
    }
public:
    ControlServiceManagement(std::map<DWORD,std::size_t> targets,Command command,Status status={})
        :targets_(std::move(targets)),command_(std::move(command)),status_(std::move(status)) {
        if(!command_) throw std::invalid_argument("Missing Control service backend");
    }
    bool start(DWORD id) const {return invoke("start",id);}
    bool stop(DWORD id) const {return invoke("stop",id);}
    DWORD status(DWORD id) const {
        const auto target=targets_.find(id);
        if(target==targets_.end() || !status_) return static_cast<DWORD>(-1);
        const auto value=status_(target->second);
        return value && *value>=1 && *value<=4?static_cast<DWORD>(*value):static_cast<DWORD>(-1);
    }
};
inline BYTE original_control_start(const ControlServiceManagement& m_serviceControl,DWORD service_id) {
    struct Service {DWORD m_dwID;} service{service_id};
    auto* pService=&service;
#include "../../source/4Story 5.0 Source Files/Server/TControlSvr/StartService.inc"
}
inline BYTE original_control_stop(const ControlServiceManagement& m_serviceControl,DWORD service_id) {
    struct Service {DWORD m_dwID;} service{service_id};
    auto* pService=&service;
#include "../../source/4Story 5.0 Source Files/Server/TControlSvr/StopService.inc"
}
}
