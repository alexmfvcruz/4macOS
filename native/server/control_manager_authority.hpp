#pragma once
#include "packet_platform.hpp"
#include "../../source/4Story 5.0 Source Files/Server/TControlSvr/ManagerClasses.h"
#include <functional>
#include <stdexcept>
namespace fourstory::server {
class ControlManagerAuthority {
    BYTE m_bAuthority=0;
    std::function<void()> denied_;
public:
    explicit ControlManagerAuthority(std::function<void()> denied):denied_(std::move(denied)) {
        if(!denied_) throw std::invalid_argument("Missing manager authority acknowledgment");
    }
    // Call only after successful database login. Failed/invalid login revokes
    // any old authority; a fresh session's legacy zero value is never privileged.
    bool authenticated_login(BYTE authority) noexcept {
        m_bAuthority=authority>=MANAGER_ALL && authority<MANAGER_COUNT?authority:0;
        return authenticated();
    }
    void revoke() noexcept {m_bAuthority=0;}
    bool authenticated() const noexcept {return m_bAuthority!=0;}
    BYTE check(BYTE m_bClass) const {
        if(!authenticated() || m_bClass<MANAGER_ALL || m_bClass>=MANAGER_COUNT) return FALSE;
        auto SendCT_AUTHORITY_ACK=denied_;
#include "../../source/4Story 5.0 Source Files/Server/TControlSvr/CheckManagerAuthority.inc"
    }
};
}
