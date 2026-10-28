#pragma once
#include "runtime.hpp"
#include "received_packet.hpp"
#include "../../source/4Story 5.0 Source Files/Server/TNetLib/RoleTimerIntervals.h"
#include "../../source/4Story 5.0 Source Files/Lib/Own/TProtocol/include/CTProtocol.h"

namespace fourstory::server {
struct RoleClock {
    using Tick=std::function<DWORD()>;
    using Epoch=std::function<std::int64_t()>;
    Tick tick=[] {
        return static_cast<DWORD>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    };
    Epoch epoch=[] {
        return static_cast<std::int64_t>(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
    };
};
inline WorkQueue::LoopHooks control_timer(Runtime& runtime,std::function<void(CPacket&)> handler) {
    if(!handler) throw std::invalid_argument("Missing Control timer handler");
    return {std::chrono::milliseconds(CTRL_TIMER),{},[&runtime,handler=std::move(handler)] {
        if(runtime.cancelled()) return;
        auto packet=std::make_shared<CPacket>();packet->SetID(CT_TIMER_REQ);packet->Rewind(FALSE);
        if(!runtime.work(Lane::batch,[packet,handler](const Runtime&){handler(*packet);}) && !runtime.cancelled())
            throw std::runtime_error("Control timer batch queue full");
    }};
}
struct WorldTimerCallbacks {
    std::function<void(std::int64_t)> current_time;
    std::function<void(DWORD)> guild_extinction;
    std::function<void()> event_expired,on_timer;
};
inline WorkQueue::LoopHooks world_timer(Runtime& runtime,WorldTimerCallbacks callbacks,RoleClock clock={}) {
    if(!callbacks.current_time || !callbacks.guild_extinction || !callbacks.event_expired ||
       !callbacks.on_timer || !clock.epoch) throw std::invalid_argument("Incomplete World timer callbacks");
    auto current=std::make_shared<std::int64_t>(0);
    return {std::chrono::milliseconds(WORLD_TIMER),
        [&runtime,callbacks,clock,current] {
            if(runtime.cancelled()) return;
            *current=clock.epoch();callbacks.current_time(*current);
        },
        [&runtime,callbacks,current] {
            if(runtime.cancelled()) return;
            callbacks.guild_extinction(static_cast<DWORD>(*current));
            callbacks.event_expired();callbacks.on_timer();
        }};
}
struct MapTimerCallbacks {
    std::function<void(DWORD,std::int64_t)> current_time;
    std::function<void(DWORD)> on_timer;
};
inline WorkQueue::LoopHooks map_timer(Runtime& runtime,MapTimerCallbacks callbacks,RoleClock clock={}) {
    if(!callbacks.current_time || !callbacks.on_timer || !clock.tick || !clock.epoch)
        throw std::invalid_argument("Incomplete Map timer callbacks");
    struct State {DWORD previous=0;bool initialized=false;};
    auto state=std::make_shared<State>();
    return {std::chrono::milliseconds(MAP_TIMER),
        [&runtime,callbacks,clock,state] {
            if(runtime.cancelled()) return;
            if(!state->initialized) {state->previous=clock.tick();state->initialized=true;}
            const auto tick=clock.tick();callbacks.current_time(tick,clock.epoch());
            if(state->initialized && state->previous!=tick) callbacks.on_timer(tick-state->previous);
            state->previous=tick;state->initialized=true;
        },{}};
}
}
