#pragma once
#include "role_workers.hpp"
#include <atomic>
#include <chrono>

namespace fourstory::server {
// One pending execution per timer. Measure the next wait after completion,
// matching a timed wait inside a serialized worker loop, without catch-up bursts.
class Periodic {
public:
    using Clock=std::chrono::steady_clock;
    using Now=std::function<Clock::time_point()>;
    using Submit=std::function<bool(Lane,std::function<void()>)>;
private:
    struct State {
        std::atomic<bool> pending{false};
        std::atomic<std::int64_t> completed_ticks{0};
        std::atomic<std::uint64_t> generation{0};
    };
    struct Entry {
        Lane lane;
        Clock::duration interval;
        Clock::time_point due;
        std::function<void()> job;
        std::shared_ptr<State> state=std::make_shared<State>();
        std::uint64_t observed=0;
    };
    Now now_;
    std::vector<Entry> entries_;
    std::shared_ptr<std::atomic<bool>> cancelled_=std::make_shared<std::atomic<bool>>(false);
public:
    explicit Periodic(Now now=[] {return Clock::now();}):now_(std::move(now)) {
        if(!now_) throw std::invalid_argument("Empty periodic clock");
    }
    void add(Lane lane,std::chrono::milliseconds interval,std::function<void()> job) {
        if(cancelled_->load() || interval.count()<=0 || !job)
            throw std::invalid_argument("Invalid periodic job");
        entries_.push_back({lane,interval,now_()+interval,std::move(job)});
    }
    void cancel() noexcept {cancelled_->store(true);}
    void pump(const Submit& submit) {
        if(cancelled_->load()) return;
        const auto current=now_();
        for(auto& entry:entries_) {
            auto state=entry.state;
            if(state->pending.load()) continue;
            auto generation=state->generation.load();
            if(generation!=entry.observed) {
                entry.observed=generation;
                entry.due=Clock::time_point(Clock::duration(state->completed_ticks.load()))+entry.interval;
            }
            if(current<entry.due) continue;
            state->pending=true;
            auto job=entry.job;auto now=now_;auto cancelled=cancelled_;
            bool accepted;
            try {
                accepted=submit(entry.lane,[state,job=std::move(job),now=std::move(now),cancelled] {
                    std::exception_ptr failure;
                    try {if(!cancelled->load()) job();}catch(...) {failure=std::current_exception();}
                    state->completed_ticks=now().time_since_epoch().count();
                    ++state->generation;state->pending=false;
                    if(failure) std::rethrow_exception(failure);
                });
            } catch(...) {state->pending=false;throw;}
            if(!accepted) state->pending=false; // Keep due work for a later bounded-queue retry.
        }
    }
};
}
