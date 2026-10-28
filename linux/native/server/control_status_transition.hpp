#pragma once
#include "received_packet.hpp"
#include "original_control_management.hpp"
#include "../../source/4Story 5.0 Source Files/Lib/Own/TProtocol/include/CTProtocol.h"
#include <functional>
namespace fourstory::server {
// Only the two SCM states consumed by this original source fragment.
inline constexpr DWORD SERVICE_STOPPED=1,SERVICE_START_PENDING=2,SERVICE_STOP_PENDING=3,
    SERVICE_RUNNING=4,SERVICE_CONTINUE_PENDING=5,SERVICE_PAUSE_PENDING=6,SERVICE_PAUSED=7;
struct ControlServiceStatus {
    DWORD m_dwID=0,m_dwStatus=static_cast<DWORD>(-1),m_dwStopCount=0,m_dwAutoStartTick=0;
    BYTE m_bManagerControl=FALSE;
    std::int64_t m_nLatestStop=0;
    CString m_strName;
    struct Type {BYTE m_bType=0;} type;
    Type* m_pSvrType=&type;
    struct Group {BYTE m_bGroupID=0;} group;
    Group* m_pGroup=&group;
    ControlServiceStatus()=default;
    ControlServiceStatus(const ControlServiceStatus&)=delete;
    ControlServiceStatus& operator=(const ControlServiceStatus&)=delete;
};
inline void original_control_reconnect(ControlServiceStatus& service,DWORD current,
    const std::function<std::int64_t()>& wall,
    const std::function<void(const char*,const CString&)>& log,
    const std::function<void(std::unique_ptr<CPacket>)>& batch) {
    if(!wall || !log || !batch) throw std::invalid_argument("Missing Control reconnect callbacks");
    auto* pService=&service;
    struct {DWORD dwCurrentState;} ss{current};
    struct Logger {
        const std::function<void(const char*,const CString&)>& callback;
        void operator()(const char* text,const CString& name) const {callback(text,name);}
        void operator()(const char* text) const {callback(text,CString{});}
    } LogEvent{log};
    struct PACKETBUF {CPacket m_packet;};using LPPACKETBUF=PACKETBUF*;
    auto SayToBATCH=[&](LPPACKETBUF raw) {
        std::unique_ptr<PACKETBUF> owned(raw);
        Session::Packet bytes(owned->m_packet.GetBuffer(),owned->m_packet.GetBuffer()+owned->m_packet.GetSize());
        batch(received_packet(bytes));
    };
#include "../../source/4Story 5.0 Source Files/Server/TControlSvr/ServiceReconnect.inc"
}
inline void original_control_auto_start(ControlServiceStatus& service,DWORD current,BYTE enabled,
    const std::function<DWORD()>& tick,const std::function<bool(DWORD)>& start,
    const std::function<void(const char*,const CString&)>& log) {
    if(!tick || !start || !log) throw std::invalid_argument("Missing Control auto-start callbacks");
    auto* pService=&service;
    struct {DWORD dwCurrentState;} ss{current};
    const auto m_bAutoStart=enabled;
    auto GetTickCount=tick;auto LogEvent=log;
    auto StartService=[&](ControlServiceStatus* value){return start(value->m_dwID);};
#include "../../source/4Story 5.0 Source Files/Server/TControlSvr/ServiceAutoStart.inc"
}
inline void original_control_status_transition(ControlServiceStatus& service,DWORD current,
    const std::function<DWORD()>& tick,const std::function<void(const char*,const CString&)>& log,
    const std::function<void(std::unique_ptr<CPacket>)>& batch) {
    if(!tick || !log || !batch) throw std::invalid_argument("Missing Control status callbacks");
    auto* pService=&service;
    struct {DWORD dwCurrentState;} ss{current};
    auto GetTickCount=tick;auto LogEvent=log;
    struct PACKETBUF {CPacket m_packet;};using LPPACKETBUF=PACKETBUF*;
    auto SayToBATCH=[&](LPPACKETBUF raw) {
        std::unique_ptr<PACKETBUF> owned(raw);
        // CPacket deliberately cannot be copied; transfer its exact serialized bytes.
        Session::Packet bytes(owned->m_packet.GetBuffer(),owned->m_packet.GetBuffer()+owned->m_packet.GetSize());
        batch(received_packet(bytes));
    };
#include "../../source/4Story 5.0 Source Files/Server/TControlSvr/ServiceStatusTransition.inc"
}
inline void original_control_poll_status(ControlServiceStatus& service,const ControlServiceManagement& backend,
    BYTE auto_start,const std::function<DWORD()>& tick,const std::function<std::int64_t()>& wall,
    const std::function<void(const char*,const CString&)>& log,
    const std::function<void(std::unique_ptr<CPacket>)>& batch) {
    const auto current=backend.status(service.m_dwID);
    if(current!=static_cast<DWORD>(-1)) {
        original_control_reconnect(service,current,wall,log,batch);
        original_control_auto_start(service,current,auto_start,tick,
            [&](DWORD id){return backend.start(id);},log);
    }
    original_control_status_transition(service,current,tick,log,batch);
}
}
