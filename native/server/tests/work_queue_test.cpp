#include "../work_queue.hpp"
#include <atomic>
#include <cassert>
#include <future>
#include <iostream>
using namespace fourstory::server;
int main() {
    std::atomic<unsigned> gated_runs=0;
    WorkQueue gated(1,2,false);
    assert(gated.submit([&]{++gated_runs;}));
    assert(gated_runs==0 && !gated.idle());
    gated.activate();gated.stop();assert(gated_runs==1);
    WorkQueue failed_start(1,2,false);
    assert(failed_start.submit([&]{++gated_runs;}));
    failed_start.stop();assert(gated_runs==1 && failed_start.idle());
    bool refused=false;
    try{failed_start.activate();}catch(const std::logic_error&){refused=true;}
    assert(refused);
    WorkQueue queue(1,2);
    std::promise<void> entered,release;
    auto released=release.get_future().share();
    std::atomic<unsigned> completed{0};
    assert(queue.submit([&]{entered.set_value();released.wait();++completed;}));
    entered.get_future().wait();
    assert(queue.submit([&]{++completed;}));
    assert(queue.submit([&]{++completed;}));
    assert(!queue.submit([&]{++completed;}));
    release.set_value();queue.stop();queue.stop();
    assert(completed==3 && !queue.submit([]{}));
    WorkQueue errors(2,8);
    assert(errors.submit([]{throw std::runtime_error("worker test failure");}));
    assert(errors.submit([&]{++completed;}));
    errors.stop();assert(completed==4);
    bool surfaced=false;
    try{errors.rethrow_failure();}catch(const std::runtime_error&){surfaced=true;}
    assert(surfaced);
    std::cout<<"PASS bounded queue, saturation, accepted-job drain, idempotent join and worker errors\n";
}
