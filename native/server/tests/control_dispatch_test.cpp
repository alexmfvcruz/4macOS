#include "../control_dispatch.hpp"
#include "../../packet/wire.hpp"
#include <cassert>
#include <iostream>
using namespace fourstory::server;
int main() {
    ControlDispatch dispatch;
    bool invalid=false;
    try{dispatch.add(0xffff,[](Runtime&,auto,CPacket&){return DWORD(0);});}
    catch(const std::invalid_argument&){invalid=true;}assert(invalid);
    std::vector<DWORD> received;
    dispatch.add(CT_OPLOGIN_REQ,[&](Runtime&,auto,CPacket& packet){
        DWORD value=0;packet>>value;received.push_back(value);return value==2 ? DWORD(1):DWORD(0);
    });
    auto callback=dispatch.bind(); // independent of the builder's lifetime/mutations
    Config config;config.service=Service::control;config.listen={"127.0.0.1",0};
    Runtime::Hooks hooks;hooks.packet=callback;
    Runtime runtime(config,hooks);
    std::exception_ptr failure;
    std::thread service([&]{try{runtime.run();}catch(...){failure=std::current_exception();}});
    auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
    while(!runtime.port() && std::chrono::steady_clock::now()<deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    assert(runtime.port());
    Network::Id connection=0;bool closed=false;
    Network peer({[&](auto id,bool){connection=id;},{},[&](auto){closed=true;}});
    peer.connect({"127.0.0.1",runtime.port()});
    while(!connection && std::chrono::steady_clock::now()<deadline) peer.step(5);
    assert(connection);
    for(DWORD value: {DWORD(1),DWORD(2)}) {
        story::Writer writer(CT_OPLOGIN_REQ);writer.u32(value);assert(peer.send(connection,writer.finish()));
    }
    while(!closed && std::chrono::steady_clock::now()<deadline) peer.step(5);
    runtime.request_stop();service.join();if(failure) std::rethrow_exception(failure);
    assert(closed && received==std::vector<DWORD>({1,2}));
    std::cout<<"PASS original Control routing list, owned CPacket batch dispatch, FIFO and handler-error close\n";
}
