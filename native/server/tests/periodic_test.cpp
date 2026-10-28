#include "../periodic.hpp"
#include <cassert>
#include <iostream>
using namespace fourstory::server;
int main() {
    Periodic::Clock::time_point time{};Periodic timers([&]{return time;});
    unsigned count=0;std::vector<std::function<void()>> queued;
    timers.add(Lane::timer,std::chrono::milliseconds(100),[&]{++count;});
    auto submit=[&](Lane lane,std::function<void()> job){assert(lane==Lane::timer);queued.push_back(std::move(job));return true;};
    timers.pump(submit);assert(queued.empty());time+=std::chrono::milliseconds(100);
    timers.pump([](Lane,std::function<void()>){return false;}); // Saturation retains due work.
    timers.pump(submit);assert(queued.size()==1);
    time+=std::chrono::seconds(10);timers.pump(submit);assert(queued.size()==1); // no overlap/backlog
    queued[0]();timers.pump(submit);assert(count==1 && queued.size()==1);
    time+=std::chrono::milliseconds(99);timers.pump(submit);assert(queued.size()==1);
    time+=std::chrono::milliseconds(1);timers.pump(submit);assert(queued.size()==2);
    timers.cancel();queued[1]();assert(count==1);time+=std::chrono::seconds(10);
    timers.pump(submit);assert(queued.size()==2);
    std::cout<<"PASS periodic deadlines, saturation retry, no overlap/catch-up, completion delay and pending cancellation\n";
}
