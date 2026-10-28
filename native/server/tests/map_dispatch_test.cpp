#include "../map_dispatch.hpp"
#include "../../packet/wire.hpp"
#include <cassert>
#include <iostream>
#include <future>
using namespace fourstory::server;
int main() {
    MapDispatch dispatch;
    MapPeerMetadata client{SESSION_CLIENT,{}};
    assert(map_message(CS_CONNECT_REQ,&client));
    assert(!map_message(0xffff,&client));
    assert(map_message(DM_SKYGARDENOCCUPY_REQ));
    bool invalid=false;
    try{dispatch.add(0xffff,[](Runtime&,auto,CPacket&){return DWORD(0);});}
    catch(const std::invalid_argument&){invalid=true;}assert(invalid);
    std::vector<DWORD> received;
    dispatch.add(CS_CONNECT_REQ,[&](Runtime&,auto,CPacket& packet){
        DWORD value=0;packet>>value;received.push_back(value);return value==2 ? DWORD(1):DWORD(0);
    });
    std::atomic<bool> database_done=false;
    dispatch.add(DM_SAVECHAR_REQ,[&](Runtime&,auto,CPacket& packet){
        DWORD value=0;packet>>value;assert(value==42);database_done=true;return DWORD(0);
    });
    std::atomic<bool> ai_done=false;
    dispatch.add(SM_AICMD_REQ,[&](Runtime&,auto,CPacket& packet){
        DWORD value=0;packet>>value;assert(value==43);ai_done=true;return DWORD(0);
    });
    auto ai_callback=dispatch.bind(Lane::ai);
    auto database_callback=dispatch.bind(Lane::database);
    auto callback=dispatch.bind(); // independent of the builder's lifetime/mutations
    Config config;config.service=Service::map;config.listen={"127.0.0.1",0};
    Runtime::Hooks hooks;hooks.packet=callback;
    Runtime runtime(config,hooks);
    std::promise<void> batch_entered,release_batch;
    auto batch_release=release_batch.get_future().share();
    assert(runtime.work(Lane::batch,[&](const Runtime&){batch_entered.set_value();batch_release.wait();}));
    std::exception_ptr failure;
    std::thread service([&]{try{runtime.run();}catch(...){failure=std::current_exception();}});
    auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
    while(!runtime.port() && std::chrono::steady_clock::now()<deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    assert(runtime.port());batch_entered.get_future().wait();
    story::Writer database_packet(DM_SAVECHAR_REQ);database_packet.u32(42);
    database_callback(runtime,0,database_packet.finish());
    story::Writer ai_packet(SM_AICMD_REQ);ai_packet.u32(43);
    ai_callback(runtime,0,ai_packet.finish());
    while((!database_done || !ai_done) && std::chrono::steady_clock::now()<deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    assert(database_done && ai_done); // original DB lane stays independent of blocked batch work

    Network::Id connection=0;bool closed=false;
    Network peer({[&](auto id,bool){connection=id;},{},[&](auto){closed=true;}});
    peer.connect({"127.0.0.1",runtime.port()});
    while(!connection && std::chrono::steady_clock::now()<deadline) peer.step(5);
    assert(connection);
    for(DWORD value: {DWORD(1),DWORD(2)}) {
        story::Writer writer(CS_CONNECT_REQ);writer.u32(value);assert(peer.send(connection,writer.finish()));
    }
    release_batch.set_value();
    while(!closed && std::chrono::steady_clock::now()<deadline) peer.step(5);
    runtime.request_stop();service.join();if(failure) std::rethrow_exception(failure);
    assert(closed && received==std::vector<DWORD>({1,2}));
    std::cout<<"PASS original Map switch, independent DB dispatch, batch FIFO and handler-error close\n";
}
