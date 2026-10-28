#include "../work_queue.hpp"
#include <atomic>
#include <cassert>
#include <future>
#include <iostream>
using namespace fourstory::server;
int main() {
    WorkQueue queue(1,4,false);
    std::vector<char> sequence;
    std::atomic<unsigned> wakes=0;
    std::promise<void> entered,release;
    auto released=release.get_future().share();
    queue.configure_loop({std::chrono::milliseconds(10),
        [&]{sequence.push_back('B');},
        [&]{
            sequence.push_back('I');++wakes;
            assert(queue.submit([&]{sequence.push_back('J');entered.set_value();released.wait();}));
        }});
    queue.activate();entered.get_future().wait();
    // Holding actual work must not launch another idle-wait callback.
    std::this_thread::sleep_for(std::chrono::milliseconds(30));assert(wakes==1);
    queue.request_stop();release.set_value();queue.stop();
    assert(sequence==std::vector<char>({'B','I','B','J'}));
    bool refused=false;
    try{queue.configure_loop({});}catch(const std::logic_error&){refused=true;}
    assert(refused);
    WorkQueue failures(1,2,false);
    std::promise<void> called;
    failures.configure_loop({std::chrono::milliseconds(10),{},[&]{called.set_value();throw std::runtime_error("timer failure");}});
    failures.activate();called.get_future().wait();failures.stop();
    refused=false;try{failures.rethrow_failure();}catch(const std::runtime_error&){refused=true;}assert(refused);
    std::cout<<"PASS worker loop ordering, idle wake, busy suppression, startup-only binding and timer failure\n";
}
