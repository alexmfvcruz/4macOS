#pragma once
#include "control_status_transition.hpp"
#include "role_timers.hpp"
namespace fourstory::server {
// State is confined to the management lane; completed original packets cross
// to the serialized batch lane. Both state and backend outlive queued work.
inline WorkQueue::LoopHooks control_management_worker(Runtime& runtime,
    std::shared_ptr<ControlServiceManagement> backend,
    std::vector<std::shared_ptr<ControlServiceStatus>> services,BYTE auto_start,
    std::function<void(const char*,const CString&)> log,
    std::function<void(CPacket&)> handler,RoleClock clock={}) {
    if(runtime.service()!=Service::control || !backend || !log || !handler || !clock.tick || !clock.epoch)
        throw std::invalid_argument("Incomplete Control management worker");
    for(const auto& service:services) if(!service) throw std::invalid_argument("Missing Control service state");
    return {std::chrono::milliseconds(CTRL_TIMER),{},
        [&runtime,backend,services=std::move(services),auto_start,log=std::move(log),handler=std::move(handler),clock] {
            for(const auto& service:services) {
                if(runtime.cancelled()) return;
                original_control_poll_status(*service,*backend,auto_start,clock.tick,clock.epoch,log,
                    [&runtime,handler](std::unique_ptr<CPacket> value) {
                        auto packet=std::shared_ptr<CPacket>(std::move(value));
                        if(!runtime.work(Lane::batch,[packet,handler](const Runtime&){handler(*packet);}) && !runtime.cancelled())
                            throw std::runtime_error("Control management batch queue full");
                    });
            }
        }};
}
}
