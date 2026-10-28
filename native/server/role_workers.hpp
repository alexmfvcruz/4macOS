#pragma once
#include "config.hpp"
#include "work_queue.hpp"
#include <map>
#include <memory>

namespace fourstory::server {
enum class Lane { network, batch, database, ai, log, management, timer };
// Original CreateThreads separates serialized role work from network workers.
// A slow DB/AI job must not hold up the batch lane. Periodic scheduling remains
// an owner responsibility; the timer lane only executes already scheduled jobs.
class RoleWorkers {
    std::map<Lane,std::unique_ptr<WorkQueue>> queues_;
public:
    explicit RoleWorkers(const Config& config,bool active=true) {
        queues_.emplace(Lane::network,std::make_unique<WorkQueue>(config.workers,config.queue_limit,active));
        auto add=[&](Lane lane){queues_.emplace(lane,std::make_unique<WorkQueue>(1,config.queue_limit,active));};
        if(config.service!=Service::login) add(Lane::batch);
        if(config.service==Service::control) {add(Lane::management);add(Lane::timer);}
        if(config.service==Service::world) {add(Lane::database);add(Lane::timer);}
        if(config.service==Service::map) {add(Lane::database);add(Lane::ai);add(Lane::log);}
    }
    bool has(Lane lane) const{return queues_.count(lane)!=0;}
    ~RoleWorkers() {stop();}
    bool submit(Lane lane,std::function<void()> job) {
        auto found=queues_.find(lane);if(found==queues_.end()) return false;
        return found->second->submit(std::move(job));
    }
    bool submit(std::function<void()> job) {return submit(Lane::network,std::move(job));}
    void configure_loop(Lane lane,WorkQueue::LoopHooks hooks) {
        auto found=queues_.find(lane);
        if(found==queues_.end()) throw std::invalid_argument("Missing role worker lane");
        found->second->configure_loop(std::move(hooks));
    }
    void activate() {for(auto& entry:queues_) entry.second->activate();}
    void request_stop() noexcept {for(auto& entry:queues_) entry.second->request_stop();}
    void stop() noexcept {request_stop();for(auto& entry:queues_) entry.second->stop();}
    bool idle() {for(auto& entry:queues_) if(!entry.second->idle()) return false;return true;}
    void rethrow_failure() {for(auto& entry:queues_) entry.second->rethrow_failure();}
};
}
