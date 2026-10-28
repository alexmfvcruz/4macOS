#pragma once
#include <condition_variable>
#include <chrono>
#include <cstddef>
#include <deque>
#include <exception>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace fourstory::server {
// Construct once per service lifetime. stop() is idempotent and drains accepted
// jobs once activated; stopping a gated queue discards its pending startup jobs.
// Jobs must terminate cooperatively; C++ cannot safely kill running jobs.
class WorkQueue {
public:
    struct LoopHooks {
        std::chrono::milliseconds idle_wait{0};
        std::function<void()> before_iteration;
        std::function<void()> idle_wake;
    };
private:
    LoopHooks loop_;
    std::mutex mutex_, join_mutex_;
    std::condition_variable ready_;
    std::deque<std::function<void()>> jobs_;
    std::vector<std::thread> threads_;
    std::size_t capacity_;
    bool accepting_=true,active_=true;
    std::size_t running_=0;
    std::exception_ptr failure_;
    void invoke(const std::function<void()>& job) noexcept {
        try {if(job) job();}
        catch(...) {
            std::lock_guard<std::mutex> lock(mutex_);
            if(!failure_) failure_=std::current_exception();
        }
    }
    void run() noexcept {
        for(;;) {
            {
                std::unique_lock<std::mutex> lock(mutex_);
                ready_.wait(lock,[&]{return active_ || !accepting_;});
                if(!accepting_ && jobs_.empty()) return;
            }
            invoke(loop_.before_iteration);
            std::function<void()> job;
            bool waited=false,wake=false;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                if(jobs_.empty() && accepting_) {
                    waited=true;
                    if(loop_.idle_wait.count()>0)
                        ready_.wait_for(lock,loop_.idle_wait,[&]{return !jobs_.empty() || !accepting_;});
                    else ready_.wait(lock,[&]{return !jobs_.empty() || !accepting_;});
                    wake=accepting_;
                } else if(!jobs_.empty()) {
                    job=std::move(jobs_.front());jobs_.pop_front();++running_;
                } else return;
            }
            // Source World/Control wait callbacks run before the next iteration;
            // Map's next iteration samples its clock before handling a queued job.
            if(waited) {if(wake) invoke(loop_.idle_wake);continue;}
            invoke(job);
            {std::lock_guard<std::mutex> lock(mutex_);--running_;}
        }
    }
public:
    WorkQueue(unsigned workers,std::size_t capacity,bool active=true):capacity_(capacity),active_(active) {
        if(!workers || !capacity) throw std::invalid_argument("Invalid worker queue size");
        try {
            for(unsigned i=0;i<workers;++i) threads_.emplace_back([this]{run();});
        } catch(...) { stop();throw; }
    }
    WorkQueue(const WorkQueue&)=delete;
    WorkQueue& operator=(const WorkQueue&)=delete;
    ~WorkQueue() { stop(); }
    bool submit(std::function<void()> job) {
        if(!job) throw std::invalid_argument("Empty worker job");
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if(!accepting_ || jobs_.size()>=capacity_) return false;
            jobs_.push_back(std::move(job));
        }
        ready_.notify_one();return true;
    }
    void configure_loop(LoopHooks hooks) {
        std::lock_guard<std::mutex> lock(mutex_);
        if(active_ || !accepting_ || hooks.idle_wait.count()<0)
            throw std::logic_error("Worker loop configuration requires gated startup");
        loop_=std::move(hooks);
    }
    void activate() {
        {std::lock_guard<std::mutex> lock(mutex_);
         if(!accepting_) throw std::logic_error("Activating a stopped worker queue");
         active_=true;}
        ready_.notify_all();
    }
    void stop() noexcept {
        std::lock_guard<std::mutex> joining(join_mutex_);
        request_stop();
        for(auto& thread:threads_) if(thread.joinable()) thread.join();
    }
    void request_stop() noexcept {
        std::deque<std::function<void()>> discarded;
        {std::lock_guard<std::mutex> lock(mutex_);accepting_=false;
         // Failed startup must never execute jobs queued for an uninitialized role.
         if(!active_) discarded.swap(jobs_);}
        ready_.notify_all();
    }
    bool idle() {
        std::lock_guard<std::mutex> lock(mutex_);return jobs_.empty() && running_==0;
    }
    void rethrow_failure() {
        std::exception_ptr failure;
        {std::lock_guard<std::mutex> lock(mutex_);failure=failure_;}
        if(failure) std::rethrow_exception(failure);
    }
};
}
