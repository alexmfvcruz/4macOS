#include "../debug_channel.hpp"
#include <cassert>
#include <iostream>
#include <thread>
using namespace fourstory::server;
int main() {
    auto listener=Socket::tcp();sockaddr_in address{};
    address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    assert(bind(listener.get(),reinterpret_cast<sockaddr*>(&address),sizeof(address))==0);
    assert(listen(listener.get(),1)==0);socklen_t size=sizeof(address);
    assert(getsockname(listener.get(),reinterpret_cast<sockaddr*>(&address),&size)==0);
    DebugChannel channel(64);channel.initialize({"127.0.0.1",ntohs(address.sin_port)},"TMapSvr");
    assert(channel.send(" debug text"));assert(!channel.send(std::string(64,'x')));
    Socket peer;
    for(unsigned turn=0;peer.get()<0 && turn<1000;++turn) {
        peer=Socket(accept(listener.get(),nullptr,nullptr));channel.step();
        if(peer.get()<0) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    assert(peer.get()>=0);peer.nonblocking();std::string received;
    const std::string expected="TMapSvr is online! debug text";
    for(unsigned turn=0;received.size()<expected.size() && turn<1000;++turn) {
        channel.step();char bytes[64];auto count=peer.read(bytes,sizeof(bytes));
        if(count>0) received.append(bytes,static_cast<std::size_t>(count));
        else std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    assert(received==expected);channel.close();assert(!channel.send("closed"));
    listener.close();DebugChannel absent;
    absent.initialize({"127.0.0.1",ntohs(address.sin_port)},"TMapSvr");
    for(unsigned turn=0;turn<100;++turn) {absent.step();std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    assert(!absent.send("receiver unavailable"));
    std::cout<<"PASS raw debug TCP text, bounded queue, close and nonfatal missing receiver\n";
}
