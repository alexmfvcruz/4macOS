#include "runtime.hpp"
#include "debug_channel.hpp"
#include "control_dispatch.hpp"
#include "world_dispatch.hpp"
#include "login_dispatch.hpp"
#include "map_dispatch.hpp"
#include <csignal>
#include <fstream>
#include <iostream>

using namespace fourstory::server;
namespace { volatile std::sig_atomic_t stop_signal=0; }
extern "C" void service_signal(int signal) {stop_signal=signal;}
int main(int argc,char** argv) {
    if(argc!=2) {std::cerr<<"Usage: fourstory-service <config-file>\n";return 2;}
    try {
        std::ifstream input(argv[1]);
        if(!input) throw std::runtime_error("Cannot open service configuration");
        auto config=read_config(input);
        struct sigaction action{};action.sa_handler=service_signal;sigemptyset(&action.sa_mask);
        if(sigaction(SIGTERM,&action,nullptr)<0 || sigaction(SIGINT,&action,nullptr)<0)
            throw std::runtime_error("Cannot install service signal handlers");
        struct sigaction pipe_action{};pipe_action.sa_handler=SIG_IGN;sigemptyset(&pipe_action.sa_mask);
        if(sigaction(SIGPIPE,&pipe_action,nullptr)<0)
            throw std::runtime_error("Cannot install broken-pipe signal policy");
        DebugChannel debug;
        Runtime::Hooks hooks;
        // Bind the real role packet boundary. Business handlers register during
        // the next readiness milestone; absent handlers must never succeed.
        switch(config.service) {
        case Service::control: hooks.packet=ControlDispatch{}.bind();break;
        case Service::world: hooks.packet=WorldDispatch{}.bind();break;
        case Service::login: hooks=LoginDispatch{}.hooks();break;
        case Service::map: hooks.packet=MapDispatch{}.bind();break;
        }
        hooks.prepare=[&](Runtime&) {
            if(config.service==Service::map) debug.initialize(config.log,"TMapSvr");
        };
        hooks.tick=[&](Runtime&){debug.step();};
        hooks.stopped=[&](Runtime&){debug.close();};
        Runtime runtime(config,std::move(hooks));
        std::atomic<bool> monitor_done=false;
        std::thread monitor([&] {
            bool announced=false;
            while(!monitor_done.load()) {
                if(stop_signal) runtime.request_stop();
                if(!announced && runtime.port()) {
                    // This is an infrastructure listener, not game readiness.
                    std::cout<<"INFRASTRUCTURE_LISTENING port="<<runtime.port()<<std::endl;announced=true;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });
        std::exception_ptr failure;
        try {runtime.run();}catch(...) {failure=std::current_exception();}
        monitor_done=true;monitor.join();
        if(failure) std::rethrow_exception(failure);
        std::cout<<"STOPPED\n";return 0;
    } catch(const std::exception& error) {
        std::cerr<<"Service failed: "<<error.what()<<'\n';return 1;
    }
}
