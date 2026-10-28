#pragma once
#include "control_status_transition.hpp"
#include "control_manager_authority.hpp"
#include "../../source/4Story 5.0 Source Files/Server/TControlSvr/ManagerClasses.h"
#include "../../source/4Story 5.0 Source Files/Server/TNetLib/ErrorCode.h"
namespace fourstory::server {
inline DWORD original_control_service_request(CPacket& packet,bool authenticated,
    const std::function<bool(BYTE)>& authority,const std::function<void(BYTE)>& ack,
    std::map<DWORD,ControlServiceStatus*>& services,const ControlServiceManagement& backend,
    const std::function<DWORD()>& tick,const std::function<void(const char*,const CString&)>& log) {
    // Original CheckAuthority treats zero as privileged. The native boundary
    // therefore requires an independently authenticated manager session first.
    if(!authenticated) return EC_NOERROR;
    if(!authority || !ack || !tick || !log) throw std::invalid_argument("Missing Control request callbacks");
    for(const auto& entry:services)
        if(!entry.second || !entry.second->m_pGroup || !entry.second->m_pSvrType)
            throw std::invalid_argument("Missing Control request service state");
    struct CTManager {
        const std::function<bool(BYTE)>& authority;
        const std::function<void(BYTE)>& ack;
        bool CheckAuthority(BYTE value) {return authority(value);}
        void SendCT_SERVICECONTROL_ACK(BYTE result) {ack(result);}
    } manager{authority,ack};
    struct {CPacket& m_packet;void* m_pSESSION;} buffer{packet,&manager};
    auto* pBUF=&buffer;
    using MAPTSVRTEMP=std::map<DWORD,ControlServiceStatus*>;
    auto& m_mapTSVRTEMP=services;
    auto StartService=[&](ControlServiceStatus* service){return original_control_start(backend,service->m_dwID);};
    auto StopService=[&](ControlServiceStatus* service){return original_control_stop(backend,service->m_dwID);};
    auto GetTickCount=tick;auto LogEvent=log;
#include "../../source/4Story 5.0 Source Files/Server/TControlSvr/ServiceControlRequest.inc"
}
inline DWORD original_control_service_request(CPacket& packet,const ControlManagerAuthority& manager,
    const std::function<void(BYTE)>& ack,std::map<DWORD,ControlServiceStatus*>& services,
    const ControlServiceManagement& backend,const std::function<DWORD()>& tick,
    const std::function<void(const char*,const CString&)>& log) {
    return original_control_service_request(packet,manager.authenticated(),
        [&](BYTE value){return manager.check(value)!=FALSE;},ack,services,backend,tick,log);
}
}
