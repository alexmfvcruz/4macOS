#include "../role_timers.hpp"
#include <cassert>
#include <iostream>
using namespace fourstory::server;
int main() {
    static_assert(CTRL_TIMER==1000 && WORLD_TIMER==1000 && MAP_TIMER==100);
    Config config;config.listen={"127.0.0.1",0};config.service=Service::map;
    Runtime inactive(config);
    DWORD tick=0xfffffff0;std::vector<DWORD> deltas;
    RoleClock clock;clock.tick=[&]{return tick;};clock.epoch=[] {return std::int64_t(5000000000);};
    auto map=map_timer(inactive,{[](DWORD,std::int64_t time){assert(time==5000000000);},
                               [&](DWORD delta){deltas.push_back(delta);}},clock);
    map.before_iteration();tick=0x10;map.before_iteration();map.before_iteration();
    assert(deltas==std::vector<DWORD>({32}));
    inactive.request_stop();tick=0x20;map.before_iteration();assert(deltas.size()==1);
    bool refused=false;try{world_timer(inactive,{});}catch(const std::invalid_argument&){refused=true;}assert(refused);
    unsigned controls=0;
    config.service=Service::control;Runtime::Hooks control_hooks;
    control_hooks.prepare=[&](Runtime& owner){owner.configure_loop(Lane::timer,
        control_timer(owner,[&](CPacket& packet){assert(packet.GetID()==CT_TIMER_REQ);++controls;owner.request_stop();}));};
    Runtime control(config,control_hooks);control.run();assert(controls==1);
    config.service=Service::world;Runtime::Hooks world_hooks;std::vector<char> sequence;
    world_hooks.prepare=[&](Runtime& owner){owner.configure_loop(Lane::timer,world_timer(owner,{
        [&](std::int64_t time){assert(time==5000000000);sequence.push_back('C');},
        [&](DWORD time){assert(time==DWORD(5000000000ULL));sequence.push_back('G');},
        [&]{sequence.push_back('E');},[&]{sequence.push_back('T');owner.request_stop();}},clock));};
    Runtime world(config,world_hooks);world.run();assert(sequence==std::vector<char>({'C','G','E','T'}));
    config.service=Service::map;Runtime::Hooks map_hooks;unsigned updates=0,timers=0;
    DWORD map_tick=0;RoleClock live_clock;live_clock.tick=[&]{return map_tick;};
    map_hooks.prepare=[&](Runtime& owner){owner.configure_loop(Lane::ai,map_timer(owner,{
        [&](DWORD,std::int64_t){++updates;map_tick+=100;},[&](DWORD elapsed){assert(elapsed==100);++timers;owner.request_stop();}},live_clock));};
    Runtime live_map(config,map_hooks);live_map.run();assert(updates>=2 && timers==1);
    std::cout<<"PASS original timer intervals, Control batch packet, World callback order and Map tick rollover\n";
}
