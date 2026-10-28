#pragma once
#include "transport.hpp"
#include <deque>
#include <functional>

namespace fourstory::server {
// Owned by one network thread. Other threads submit commands to that owner.
// EOF, framing errors and write errors end the connection; queued bytes are
// never replayed on a new connection (protocol sequence state is per session).
class Session {
    Socket socket_;
    FrameDecoder decoder_;
    std::deque<std::vector<std::uint8_t>> outgoing_;
    std::size_t offset_=0,queued_=0,limit_;
    bool draining_=false;
public:
    using Packet=std::vector<std::uint8_t>;
    Session(Socket socket,std::size_t byte_limit):socket_(std::move(socket)),limit_(byte_limit) {
        if(!limit_) throw std::invalid_argument("Invalid session queue limit");
        socket_.nonblocking();
    }
    int descriptor() const{return socket_.get();}
    bool pending() const{return !outgoing_.empty();}
    std::size_t queued_bytes() const{return queued_;}
    bool send(Packet packet) {
        if(packet.size()<16 || packet.size()>maximum_frame_size ||
           (std::size_t(packet[0]) | (std::size_t(packet[1])<<8))!=packet.size())
            throw std::invalid_argument("Invalid outgoing packet frame");
        if(draining_ || descriptor()<0 || packet.size()>limit_-queued_) return false;
        queued_+=packet.size();outgoing_.push_back(std::move(packet));return true;
    }
    void close() noexcept {
        socket_.close();outgoing_.clear();queued_=offset_=0;draining_=true;
    }
    void drain() {draining_=true;if(!pending()) close();}
    // A bounded budget ensures a busy peer cannot monopolize the event loop.
    bool writable(std::size_t budget=65536) {
        while(pending() && budget) {
            auto& packet=outgoing_.front();
            auto count=std::min(packet.size()-offset_,budget);
            auto sent=socket_.write(packet.data()+offset_,count);
            if(sent<0 && (errno==EAGAIN || errno==EWOULDBLOCK)) return true;
            if(sent<=0) {close();return false;}
            auto size=static_cast<std::size_t>(sent);
            offset_+=size;queued_-=size;budget-=size;
            if(offset_==packet.size()) {outgoing_.pop_front();offset_=0;}
        }
        if(draining_ && !pending()) {close();return false;}
        return descriptor()>=0;
    }
    bool readable(const std::function<void(Packet)>& deliver,std::size_t budget=65536) {
        std::uint8_t buffer[8192];
        while(budget && descriptor()>=0) {
            auto received=socket_.read(buffer,std::min(sizeof(buffer),budget));
            if(received<0 && (errno==EAGAIN || errno==EWOULDBLOCK)) return true;
            if(received<=0) {close();return false;}
            auto size=static_cast<std::size_t>(received);budget-=size;
            try {decoder_.feed(buffer,size,deliver);}
            catch(...) {close();throw;}
        }
        return descriptor()>=0;
    }
};
}
