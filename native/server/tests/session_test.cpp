#include "../session.hpp"
#include <cassert>
#include <iostream>
using namespace fourstory::server;
int main() {
    int descriptors[2];assert(socketpair(AF_UNIX,SOCK_STREAM,0,descriptors)==0);
    int small=1024;assert(setsockopt(descriptors[0],SOL_SOCKET,SO_SNDBUF,&small,sizeof(small))==0);
    Session sender(Socket(descriptors[0]),maximum_frame_size*2),receiver(Socket(descriptors[1]),maximum_frame_size);
    Session::Packet first(maximum_frame_size,0x31),second(maximum_frame_size,0x42);
    first[0]=second[0]=254;first[1]=second[1]=255;
    assert(sender.send(first));assert(sender.send(second));assert(!sender.send(first));
    assert(sender.writable());assert(sender.pending()); // real kernel backpressure
    unsigned delivered=0;
    auto consume=[&](Session::Packet packet){assert(packet==(delivered==0?first:second));++delivered;};
    for(unsigned turns=0;sender.pending() && turns<10000;++turns) {
        assert(receiver.readable(consume));assert(sender.writable());
    }
    assert(!sender.pending() && sender.queued_bytes()==0);
    assert(receiver.readable(consume));assert(delivered==2);
    assert(sender.send(first));sender.drain();assert(!sender.send(second));
    bool alive=true;
    auto final_packet=[&](Session::Packet packet){assert(packet==first);++delivered;};
    for(unsigned turns=0;alive && turns<10000;++turns) {
        assert(receiver.readable(final_packet));alive=sender.writable();
    }
    assert(!alive && sender.descriptor()==-1);
    assert(!receiver.readable(final_packet));assert(delivered==3 && receiver.descriptor()==-1);
    std::cout<<"PASS session backpressure, partial writes, packet order, queue bound, drain and peer EOF\n";
}
