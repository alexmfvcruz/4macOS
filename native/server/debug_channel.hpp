#pragma once
#include "config.hpp"
#include "transport.hpp"
#include <chrono>
#include <deque>
#include <mutex>
#include <poll.h>
#include <string>

namespace fourstory::server {
// Raw TCP debug text from the original CDebugSocket, not game packets.
// Bounded queue and nonblocking connect/send replace its critical section and
// blocking Winsock calls. Missing receiver is nonfatal; dropped sends return false.
class DebugChannel {
    Socket socket_;
    std::mutex mutex_;
    std::deque<std::string> queue_;
    std::size_t bytes_=0,offset_=0,limit_;
    bool connecting_=false;
    std::chrono::steady_clock::time_point deadline_;
    void close_locked() noexcept {socket_.close();queue_.clear();bytes_=offset_=0;connecting_=false;}
public:
    explicit DebugChannel(std::size_t limit=65536):limit_(limit) {
        if(!limit) throw std::invalid_argument("Invalid debug queue limit");
    }
    void initialize(const Endpoint& endpoint,const std::string& service) {
        std::lock_guard<std::mutex> lock(mutex_);close_locked();
        auto socket=Socket::tcp();sockaddr_in target{};
        target.sin_family=AF_INET;target.sin_port=htons(endpoint.port);
        if(!endpoint.port || inet_pton(AF_INET,endpoint.address.c_str(),&target.sin_addr)!=1)
            throw std::invalid_argument("Invalid debug endpoint");
        const auto result=::connect(socket.get(),reinterpret_cast<sockaddr*>(&target),sizeof(target));
        if(result<0 && errno!=EINPROGRESS) return;
        auto online=service+" is online!";
        if(online.size()>limit_) throw std::length_error("Debug service name too large");
        socket_=std::move(socket);connecting_=result<0;
        deadline_=std::chrono::steady_clock::now()+std::chrono::seconds(2);
        bytes_=online.size();queue_.push_back(std::move(online));
    }
    bool send(std::string message) {
        std::lock_guard<std::mutex> lock(mutex_);
        if(socket_.get()<0 || message.size()>limit_-bytes_) return false;
        if(message.empty()) return true;
        bytes_+=message.size();queue_.push_back(std::move(message));return true;
    }
    void step() {
        std::lock_guard<std::mutex> lock(mutex_);if(socket_.get()<0) return;
        if(connecting_ && std::chrono::steady_clock::now()>=deadline_) {close_locked();return;}
        pollfd state{socket_.get(),POLLOUT,0};const auto result=::poll(&state,1,0);
        if(result<0 && errno==EINTR) return;
        if(result<0 || (state.revents&(POLLERR|POLLHUP|POLLNVAL))) {close_locked();return;}
        if(!(state.revents&POLLOUT)) return;
        if(connecting_) {
            int error=0;socklen_t size=sizeof(error);
            if(getsockopt(socket_.get(),SOL_SOCKET,SO_ERROR,&error,&size)<0 || error) {close_locked();return;}
            connecting_=false;
        }
        std::size_t budget=8192;
        while(!queue_.empty() && budget) {
            auto& message=queue_.front();
            const auto sent=socket_.write(message.data()+offset_,std::min(budget,message.size()-offset_));
            if(sent<0 && (errno==EAGAIN || errno==EWOULDBLOCK)) return;
            if(sent<=0) {close_locked();return;}
            auto count=static_cast<std::size_t>(sent);offset_+=count;bytes_-=count;budget-=count;
            if(offset_==message.size()) {queue_.pop_front();offset_=0;}
        }
    }
    void close() noexcept {std::lock_guard<std::mutex> lock(mutex_);close_locked();}
};
}
