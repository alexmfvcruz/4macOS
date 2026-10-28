#include "../login_dispatch.hpp"
#include "../../packet/wire.hpp"
#include <cassert>
#include <future>
#include <iostream>
using namespace fourstory::server;
int main() {
    LoginDispatch dispatch;
    std::promise<void> release;
    auto released=release.get_future().share();
    std::atomic<bool> first_entered=false,other_done=false;
    std::vector<DWORD> received;
    dispatch.add(CS_LOGIN_REQ,[&](Runtime&,auto,CPacket& packet){
        DWORD value=0;packet>>value;
        if(value==3) {other_done=true;return DWORD(0);}
        received.push_back(value);
        if(value==1) {first_entered=true;released.wait();}
        return value==2 ? DWORD(1):DWORD(0);
    });
    Config config;config.service=Service::login;config.listen={"127.0.0.1",0};config.workers=2;
    Runtime runtime(config,dispatch.hooks());
    std::exception_ptr failure;
    std::thread service([&]{try{runtime.run();}catch(...){failure=std::current_exception();}});
    auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
    std::vector<Network::Id> connections;bool first_closed=false;
    Network peer({[&](auto id,bool){connections.push_back(id);},{},
        [&](auto id){if(!connections.empty() && id==connections[0]) first_closed=true;}});
    auto until=[&](auto predicate){
        while(!predicate() && std::chrono::steady_clock::now()<deadline) peer.step(5);
        assert(predicate());
    };
    until([&]{return runtime.port()!=0;});
    peer.connect({"127.0.0.1",runtime.port()});until([&]{return connections.size()==1;});
    auto send=[&](Network::Id id,DWORD value){
        story::Writer packet(CS_LOGIN_REQ);packet.u32(value);assert(peer.send(id,packet.finish()));
    };
    send(connections[0],1);send(connections[0],2);send(connections[0],4);
    until([&]{return first_entered.load();});
    peer.connect({"127.0.0.1",runtime.port()});until([&]{return connections.size()==2;});
    send(connections[1],3);until([&]{return other_done.load();});
    release.set_value();until([&]{return first_closed;});
    runtime.request_stop();service.join();if(failure) std::rethrow_exception(failure);
    assert(received==std::vector<DWORD>({1,2}));peer.close_all();
    std::cout<<"PASS original Login roster, per-session FIFO, independent sessions and error cancellation\n";
}
