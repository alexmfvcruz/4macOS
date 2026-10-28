#include "../runtime.hpp"
#include "../../packet/wire.hpp"
#include <cassert>
#include <iostream>
using namespace fourstory::server;
int main() {
    Config startup_config;startup_config.service=Service::control;
    startup_config.listen={"127.0.0.1",0};
    std::atomic<unsigned> startup_jobs=0;
    bool loaded=false,activated=false;
    Runtime::Hooks startup_hooks;
    startup_hooks.prepare=[&](Runtime& owner){
        assert(startup_jobs==0 && owner.port()==0);loaded=true;
    };
    startup_hooks.activated=[&](Runtime& owner){
        assert(loaded && owner.port()!=0);activated=true;owner.request_stop();
    };
    Runtime startup(startup_config,startup_hooks);
    assert(startup.work([&](const Runtime& owner){assert(loaded && owner.port()!=0);++startup_jobs;}));
    startup.run();assert(activated && startup_jobs==1);
    Runtime::Hooks failed_hooks;
    failed_hooks.prepare=[](Runtime&){throw std::runtime_error("LoadData failed");};
    failed_hooks.activated=[](Runtime&){assert(false);};
    Runtime failed_startup(startup_config,failed_hooks);
    assert(failed_startup.work([&](const Runtime&){++startup_jobs;}));
    bool startup_failed=false;
    try{failed_startup.run();}catch(const std::runtime_error&){startup_failed=true;}
    assert(startup_failed && failed_startup.finished() && startup_jobs==1 && failed_startup.port()==0);
    Runtime::Hooks cancelled_hooks;
    cancelled_hooks.prepare=[](Runtime& owner){owner.request_stop();};
    cancelled_hooks.activated=[](Runtime&){assert(false);};
    Runtime cancelled_startup(startup_config,cancelled_hooks);
    assert(cancelled_startup.work([&](const Runtime&){++startup_jobs;}));
    cancelled_startup.run();assert(startup_jobs==1 && cancelled_startup.finished());

    unsigned opens=0,packets=0;Network::Id peer=0;
    Network world({[&](auto id,bool incoming){assert(incoming);peer=id;++opens;},
        [&](auto,Session::Packet packet){story::Reader reader(packet);assert(reader.u32()==123);++packets;}, {}});
    auto world_port=world.listen({"127.0.0.1",0});
    Config config;config.service=Service::map;config.listen={"127.0.0.1",0};
    config.world={"127.0.0.1",world_port};config.reconnect_initial_ms=5;config.reconnect_max_ms=20;
    config.dependency_loss=DependencyLoss::retry;
    config.shutdown_ms=500;
    std::atomic<bool> prepared=false,saved=false,resources_closed=false;
    std::atomic<unsigned> timer_ticks=0;
    Runtime runtime(config,{{},[&](Runtime& owner,auto id,Session::Packet packet){
        assert(owner.work([&,id,packet=std::move(packet)](const Runtime& token) {
            if(!token.cancelled()) assert(runtime.post([id,packet](Network& network){assert(network.send(id,packet));}));
        }));
    },{},[&](Runtime& owner){
        assert(owner.port()==0);prepared=true;
        owner.every(Lane::ai,std::chrono::milliseconds(5),[&](const Runtime& token){if(!token.cancelled()) ++timer_ticks;});
    },
    [&](Runtime& owner){
        assert(prepared && owner.cancelled());
        assert(owner.cleanup_work(Lane::database,[&](const Runtime&){saved=true;}));
    },[&](Runtime&){assert(saved);resources_closed=true;}});
    std::exception_ptr failure;
    std::thread service([&]{try{runtime.run();}catch(...){failure=std::current_exception();}});
    auto until=[&](auto predicate) {
        auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
        while(!predicate() && std::chrono::steady_clock::now()<deadline) world.step(5);
        assert(predicate());
    };
    until([&]{return opens==1 && runtime.port()!=0;});
    story::Writer writer(1);writer.u32(123);auto packet=writer.finish();
    assert(world.send(peer,packet));until([&]{return packets==1 && timer_ticks.load()>0;});
    // A real peer restart closes the old listener and connection, then opens
    // the same endpoint; Runtime must reconnect without manual connect calls.
    world.close_all();world.listen({"127.0.0.1",world_port});
    until([&]{return opens==2;});assert(world.send(peer,packet));until([&]{return packets==2;});
    std::atomic<bool> job_started=false,job_cancelled=false;
    assert(runtime.work([&](const Runtime& token) {
        job_started=true;
        while(!token.cancelled()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        job_cancelled=true;
    }));
    until([&]{return job_started.load();});runtime.request_stop();service.join();
    if(failure) std::rethrow_exception(failure);
    assert(runtime.finished() && job_cancelled && resources_closed && !runtime.work([](const Runtime&){}));
    assert(!runtime.post([](Network&){}));world.close_all();
    // Default Map behavior is service shutdown after losing World, matching
    // OnCloseSession(m_world), rather than silently restoring TCP alone.
    config.dependency_loss=DependencyLoss::stop;
    world.listen({"127.0.0.1",world_port});opens=0;
    Runtime faithful(config);failure=nullptr;
    std::thread faithful_service([&]{try{faithful.run();}catch(...){failure=std::current_exception();}});
    until([&]{return opens==1;});world.close_all();
    auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
    while(!faithful.finished() && std::chrono::steady_clock::now()<deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    if(!faithful.finished()) faithful.request_stop();
    faithful_service.join();assert(failure && faithful.finished());
    std::cout<<"PASS integrated runtime, worker commands, peer restart/retry, cancellation and cleanup-before-teardown\n";
}
