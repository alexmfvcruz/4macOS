#pragma once
#include "network.hpp"
#include "role_workers.hpp"
#include "periodic.hpp"
#include <atomic>
#include <mutex>

namespace fourstory::server {
// run() owns all network state. Workers post bounded owner commands, never
// touch sockets directly. Application jobs must observe the cancellation token.
class Runtime {
public:
    using Command=std::function<void(Network&)>;
    struct Hooks {
        std::function<void(Runtime&,Network::Id,bool)> opened={};
        std::function<void(Runtime&,Network::Id,Session::Packet)> packet={};
        std::function<void(Runtime&,Network::Id)> closed={};
        std::function<void(Runtime&)> prepare={};
        std::function<void(Runtime&)> quiescing={};
        std::function<void(Runtime&)> stopped={};
        std::function<void(Runtime&)> tick={};
        std::function<void(Runtime&)> activated={};
        std::function<void(Runtime&)> network_ready={};
    };
private:
    Config config_;
    Hooks hooks_;
    RoleWorkers workers_;
    Periodic periodic_;
    std::atomic<bool> stop_{false},started_{false},finished_{false};
    std::atomic<std::uint16_t> port_{0};
    std::mutex commands_mutex_;
    std::deque<Command> commands_;
    bool commands_open_=true;
    bool cleanup_phase_=false;
    std::thread::id owner_thread_;
    Network::Id world_=0;
    unsigned retry_ms_;
    bool dependency_failed_=false;
    std::chrono::steady_clock::time_point retry_at_{};
    void commands(Network& network) {
        std::deque<Command> batch;
        {std::lock_guard<std::mutex> lock(commands_mutex_);batch.swap(commands_);}
        for(auto& command:batch) command(network);
    }
    void retry_later() {
        retry_at_=std::chrono::steady_clock::now()+std::chrono::milliseconds(retry_ms_);
        retry_ms_=std::min(config_.reconnect_max_ms,retry_ms_*2);
    }
public:
    explicit Runtime(Config config):Runtime(std::move(config),Hooks{}) {}
    Runtime(Config config,Hooks hooks)
        :config_(std::move(config)),hooks_(std::move(hooks)),
         workers_(config_,false),retry_ms_(config_.reconnect_initial_ms) {}
    Runtime(const Runtime&)=delete;
    Runtime& operator=(const Runtime&)=delete;
    // The caller must finish/join run() first. Join worker callbacks while all
    // referenced runtime fields still exist, including an unstarted runtime.
    ~Runtime() {request_stop();periodic_.cancel();workers_.stop();}
    void request_stop() noexcept {stop_.store(true);}
    std::size_t queue_limit() const noexcept {return config_.queue_limit;}
    Service service() const noexcept {return config_.service;}
    bool cancelled() const noexcept {return stop_.load();}
    bool finished() const noexcept {return finished_.load();}
    std::uint16_t port() const noexcept {return port_.load();}
    bool post(Command command) {
        if(!command) throw std::invalid_argument("Empty runtime command");
        std::lock_guard<std::mutex> lock(commands_mutex_);
        if(!commands_open_ || commands_.size()>=config_.queue_limit) return false;
        commands_.push_back(std::move(command));return true;
    }
    bool work(std::function<void(const Runtime&)> job) {
        return work(Lane::network,std::move(job));
    }
    bool work(Lane lane,std::function<void(const Runtime&)> job) {
        if(!job) throw std::invalid_argument("Empty runtime worker job");
        if(cancelled()) return false;
        return workers_.submit(lane,[this,job=std::move(job)]{job(*this);});
    }
    void configure_loop(Lane lane,WorkQueue::LoopHooks hooks) {
        if(std::this_thread::get_id()!=owner_thread_)
            throw std::logic_error("Worker loop configuration outside owner startup");
        workers_.configure_loop(lane,std::move(hooks));
    }
    // Register from prepare or another owner callback, before scheduling starts.
    void every(Lane lane,std::chrono::milliseconds interval,std::function<void(const Runtime&)> job) {
        if(std::this_thread::get_id()!=owner_thread_ || cancelled() || !workers_.has(lane))
            throw std::logic_error("Periodic registration outside an active owner lane");
        if(!job) throw std::invalid_argument("Empty periodic runtime job");
        periodic_.add(lane,interval,[this,job=std::move(job)]{job(*this);});
    }
    // Role cleanup (Map SaveAllCharData, for example) queues persistence work
    // after listener closure but before worker shutdown. Only the control hook
    // may submit here; regular producers are already cancelled.
    bool cleanup_work(std::function<void(const Runtime&)> job) {
        return cleanup_work(Lane::network,std::move(job));
    }
    bool cleanup_work(Lane lane,std::function<void(const Runtime&)> job) {
        if(std::this_thread::get_id()!=owner_thread_ || !cleanup_phase_)
            throw std::logic_error("Cleanup work outside quiescing hook");
        if(!job) throw std::invalid_argument("Empty cleanup worker job");
        return workers_.submit(lane,[this,job=std::move(job)]{job(*this);});
    }
    // Called once, on the service control thread. Hook/worker failures surface
    // after cancellation and join; they must not silently restart corrupt state.
    void run() {
        if(started_.exchange(true)) throw std::logic_error("Runtime already started");
        owner_thread_=std::this_thread::get_id();
        Network network({
            [&](auto id,bool accepted) {
                if(!accepted) {world_=id;retry_ms_=config_.reconnect_initial_ms;}
                if(hooks_.opened) hooks_.opened(*this,id,accepted);
            },
            [&](auto id,Session::Packet packet) {
                if(hooks_.packet) hooks_.packet(*this,id,std::move(packet));
            },
            [&](auto id) {
                if(id==world_) {
                    world_=0;
                    if(!cancelled()) {
                            if(config_.dependency_loss==DependencyLoss::stop) {
                                dependency_failed_=true;request_stop();
                            } else retry_later();
                        }
                    }
                    if(hooks_.closed) hooks_.closed(*this,id);
                }
            });
            std::exception_ptr failure;
            try {
                if(hooks_.prepare) hooks_.prepare(*this);
                if(!cancelled()) {
                port_.store(network.listen(config_.listen));
                if(hooks_.network_ready) hooks_.network_ready(*this);
                if(!cancelled()) workers_.activate();
                // Original Login ClearLoginUser and World TournamentUpdate run after
                // ResumeThreads. Map InitEnvironment belongs in network_ready.
                if(!cancelled() && hooks_.activated) hooks_.activated(*this);
            }
            while(!cancelled()) {
                commands(network);workers_.rethrow_failure();
                periodic_.pump([&](Lane lane,std::function<void()> job){return workers_.submit(lane,std::move(job));});
                if(hooks_.tick) hooks_.tick(*this);
                if(config_.world.port && !world_ && std::chrono::steady_clock::now()>=retry_at_) {
                    try {world_=network.connect(config_.world);}
                    catch(const std::system_error&) {
                        if(config_.dependency_loss==DependencyLoss::stop) throw;
                        retry_later();
                    }
                }
                network.step(10);
            }
        } catch(...) {failure=std::current_exception();request_stop();}
        network.stop_accepting();
        periodic_.cancel();
        cleanup_phase_=true;
        try {if(hooks_.quiescing) hooks_.quiescing(*this);}
        catch(...) {if(!failure) failure=std::current_exception();}
        cleanup_phase_=false;workers_.request_stop();
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(config_.shutdown_ms);
        try {
            while(!workers_.idle()) {
                commands(network);network.step(10);
                if(std::chrono::steady_clock::now()>=deadline) break;
            }
            // No detached thread can outlive Runtime. Process supervision is
            // responsible for a hard deadline if an application ignores cancellation.
            workers_.stop();workers_.rethrow_failure();commands(network);
            {std::lock_guard<std::mutex> lock(commands_mutex_);commands_open_=false;}
            network.drain();
            while(network.connections() && std::chrono::steady_clock::now()<deadline) network.step(10);
        } catch(...) {if(!failure) failure=std::current_exception();}
        request_stop();workers_.stop();
        {std::lock_guard<std::mutex> lock(commands_mutex_);commands_open_=false;commands_.clear();}
        try {network.close_all();} catch(...) {if(!failure) failure=std::current_exception();}
        try {if(hooks_.stopped) hooks_.stopped(*this);} catch(...) {if(!failure) failure=std::current_exception();}
        port_.store(0);finished_.store(true);
        if(failure) std::rethrow_exception(failure);
        if(dependency_failed_) throw std::runtime_error("Required service connection lost");
    }
};
}
