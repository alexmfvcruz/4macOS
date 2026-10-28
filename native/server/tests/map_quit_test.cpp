#include "../original_map_quit.hpp"
#include <cassert>
#include <iostream>
using namespace fourstory::server;
int main() {
    Config config;config.service=Service::map;config.listen={"127.0.0.1",0};
    bool logged=false,saved=false,closed=false;
    Runtime::Hooks hooks;
    hooks.activated=[&](Runtime& runtime){
        assert(original_map_quit(runtime,[&](const char* text){assert(std::string(text)=="SM_QUITSERVICE_REQ detected !!");logged=true;})==EC_NOERROR);
    };
    hooks.quiescing=[&](Runtime& runtime){assert(logged && runtime.cancelled());
        assert(runtime.cleanup_work(Lane::database,[&](const Runtime&){saved=true;}));};
    hooks.stopped=[&](Runtime&){assert(saved);closed=true;};
    Runtime runtime(config,hooks);runtime.run();assert(closed && runtime.finished());
    std::cout<<"PASS original Map quit handler, native cancellation and cleanup-before-shutdown\n";
}
