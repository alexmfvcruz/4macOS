#include "../transport.hpp"
#include "../../packet/wire.hpp"
#include <cassert>
#include <iostream>
using namespace fourstory::server;
int main() {
    story::Writer packet(0x1234);packet.string_bytes("4Story");auto bytes=packet.finish();
    FrameDecoder decoder;unsigned delivered=0;
    auto consume=[&](std::vector<std::uint8_t> frame){assert(frame==bytes);++delivered;};
    for(auto byte:bytes) decoder.feed(&byte,1,consume);
    assert(delivered==1 && decoder.empty());
    auto combined=bytes;combined.insert(combined.end(),bytes.begin(),bytes.end());
    decoder.feed(combined.data(),combined.size(),consume);assert(delivered==3);
    std::uint8_t bad[]={15,0};bool rejected=false;
    try{decoder.feed(bad,2,consume);}catch(const std::runtime_error&){rejected=true;}
    assert(rejected);
    FrameDecoder large_decoder;
    std::vector<std::uint8_t> largest(maximum_frame_size,0xab);largest[0]=254;largest[1]=255;
    unsigned large_delivered=0;
    auto large_consume=[&](std::vector<std::uint8_t> frame){
        assert(frame==(large_delivered==0?largest:bytes));++large_delivered;
    };
    large_decoder.feed(largest.data(),largest.size()-1,large_consume);
    std::vector<std::uint8_t> tail{largest.back()};
    tail.insert(tail.end(),bytes.begin(),bytes.end());
    large_decoder.feed(tail.data(),tail.size(),large_consume);
    assert(large_delivered==2 && large_decoder.empty());
    FrameDecoder reserved_decoder;std::uint8_t reserved[]={255,255};rejected=false;
    try{reserved_decoder.feed(reserved,2,consume);}catch(const FrameError&){rejected=true;}
    assert(rejected);
    int descriptors[2];assert(socketpair(AF_UNIX,SOCK_STREAM,0,descriptors)==0);
    Socket writer(descriptors[0]),reader(descriptors[1]);writer.nonblocking();reader.nonblocking();
    std::uint8_t value=0;
    assert(reader.read(&value,1)<0 && (errno==EAGAIN || errno==EWOULDBLOCK));
    assert(writer.write(bytes.data(),bytes.size())==static_cast<ssize_t>(bytes.size()));
    std::vector<std::uint8_t> received(bytes.size());
    assert(reader.read(received.data(),received.size())==static_cast<ssize_t>(bytes.size()));
    assert(received==bytes);writer.close();assert(reader.read(&value,1)==0);
    auto moved=std::move(reader);assert(reader.get()==-1 && moved.get()>=0);
    std::cout<<"PASS fragmented/coalesced frames, invalid length, nonblocking I/O, EOF and socket ownership\n";
}
