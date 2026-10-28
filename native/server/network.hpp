#pragma once
#include "config.hpp"
#include "session.hpp"
#include <poll.h>
#include <map>
#include <memory>
#include <chrono>

namespace fourstory::server {
// poll replaces IOCP behind an owner-thread API. IDs never reuse socket fd
// values: delayed service work cannot accidentally address a new connection.
class Network {
public:
    using Id=std::uint64_t;
    struct Hooks {
        std::function<void(Id,bool)> opened; // true for accepted connections
        std::function<void(Id,Session::Packet)> packet;
        std::function<void(Id)> closed;
    };
private:
    struct Connection {
        Session session;
        bool connecting;
        std::chrono::steady_clock::time_point deadline;
        Connection(Socket socket,std::size_t limit,bool pending,
                   std::chrono::steady_clock::time_point timeout)
            :session(std::move(socket),limit),connecting(pending),deadline(timeout) {}
    };
    Socket listener_;
    std::map<Id,std::shared_ptr<Connection>> sessions_;
    Id next_=1;
    std::size_t byte_limit_,connection_limit_;
    Hooks hooks_;
    static sockaddr_in address(const Endpoint& endpoint) {
        sockaddr_in result{};result.sin_family=AF_INET;result.sin_port=htons(endpoint.port);
        if(inet_pton(AF_INET,endpoint.address.c_str(),&result.sin_addr)!=1)
            throw std::invalid_argument("Invalid network IPv4 endpoint");
        return result;
    }
    Id insert(Socket socket,bool connecting,std::chrono::steady_clock::time_point deadline={}) {
        if(next_==0) throw std::overflow_error("Connection ID exhausted");
        auto id=next_++;
        sessions_.emplace(id,std::make_shared<Connection>(std::move(socket),byte_limit_,connecting,deadline));
        return id;
    }
    void notify_open(Id id,bool accepted) {if(hooks_.opened) hooks_.opened(id,accepted);}
public:
    Network(Hooks hooks,std::size_t byte_limit=2*1024*1024,std::size_t connection_limit=1024)
        :byte_limit_(byte_limit),connection_limit_(connection_limit),hooks_(std::move(hooks)) {
        if(!byte_limit || !connection_limit) throw std::invalid_argument("Invalid network limits");
    }
    Network(const Network&)=delete;
    Network& operator=(const Network&)=delete;
    std::uint16_t listen(const Endpoint& endpoint) {
        if(listener_.get()>=0) throw std::logic_error("Listener already open");
        auto socket=Socket::tcp();const int yes=1;
        if(setsockopt(socket.get(),SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes))<0)
            throw std::system_error(errno,std::generic_category(),"Listener reuse address");
        auto target=address(endpoint);
        if(::bind(socket.get(),reinterpret_cast<sockaddr*>(&target),sizeof(target))<0 ||
           ::listen(socket.get(),128)<0)
            throw std::system_error(errno,std::generic_category(),"TCP listener");
        socklen_t size=sizeof(target);
        if(getsockname(socket.get(),reinterpret_cast<sockaddr*>(&target),&size)<0)
            throw std::system_error(errno,std::generic_category(),"Listener address");
        listener_=std::move(socket);return ntohs(target.sin_port);
    }
    Id connect(const Endpoint& endpoint,std::chrono::milliseconds timeout=std::chrono::seconds(10)) {
        if(sessions_.size()>=connection_limit_) throw std::length_error("Connection limit reached");
        auto socket=Socket::tcp();auto target=address(endpoint);
        int result=::connect(socket.get(),reinterpret_cast<sockaddr*>(&target),sizeof(target));
        if(result<0 && errno!=EINPROGRESS)
            throw std::system_error(errno,std::generic_category(),"TCP connect");
        auto id=insert(std::move(socket),result<0,std::chrono::steady_clock::now()+timeout);
        if(result==0) notify_open(id,false);
        return id;
    }
    bool send(Id id,Session::Packet packet) {
        auto found=sessions_.find(id);
        return found!=sessions_.end() && found->second->session.send(std::move(packet));
    }
    void close(Id id) {
        auto found=sessions_.find(id);if(found==sessions_.end()) return;
        found->second->session.close();sessions_.erase(found);if(hooks_.closed) hooks_.closed(id);
    }
    void stop_accepting() noexcept {listener_.close();}
    void drain() {
        stop_accepting();
        std::vector<Id> closed;
        for(auto& entry:sessions_) {
            if(entry.second->connecting) {closed.push_back(entry.first);continue;}
            entry.second->session.drain();
            if(entry.second->session.descriptor()<0) closed.push_back(entry.first);
        }
        for(auto id:closed) close(id);
    }
    void close_all() {
        stop_accepting();std::vector<Id> ids;
        for(const auto& entry:sessions_) ids.push_back(entry.first);
        for(auto id:ids) close(id);
    }
    std::size_t connections() const{return sessions_.size();}
    void step(int timeout_ms) {
        if(timeout_ms<0 || timeout_ms>1000) throw std::invalid_argument("Poll timeout outside 0..1000 ms");
        std::vector<pollfd> descriptors;
        std::vector<Id> ids;
        if(listener_.get()>=0) {descriptors.push_back({listener_.get(),POLLIN,0});ids.push_back(0);}
        auto now=std::chrono::steady_clock::now();
        for(const auto& entry:sessions_) {
            const auto& connection=*entry.second;
            short events=POLLIN;
            if(connection.connecting || connection.session.pending()) events|=POLLOUT;
            descriptors.push_back({connection.session.descriptor(),events,0});ids.push_back(entry.first);
            if(connection.connecting) {
                auto remaining=std::chrono::duration_cast<std::chrono::milliseconds>(connection.deadline-now).count();
                timeout_ms=static_cast<int>(std::min<std::int64_t>(timeout_ms,std::max<std::int64_t>(0,remaining)));
            }
        }
        int result=::poll(descriptors.data(),static_cast<nfds_t>(descriptors.size()),timeout_ms);
        if(result<0) {
            if(errno==EINTR) return;
            throw std::system_error(errno,std::generic_category(),"Network poll");
        }
        now=std::chrono::steady_clock::now();
        for(std::size_t index=0;index<ids.size();++index) {
            const auto id=ids[index];auto events=descriptors[index].revents;
            if(!id) {
                if(!(events&POLLIN) || listener_.get()<0) continue;
                // Cap accept work per iteration too.
                for(unsigned accepted=0;accepted<64;++accepted) {
                    Socket socket(::accept(listener_.get(),nullptr,nullptr));
                    if(socket.get()<0) {
                        if(errno==EINTR) continue;
                        if(errno==EAGAIN || errno==EWOULDBLOCK) break;
                        throw std::system_error(errno,std::generic_category(),"TCP accept");
                    }
                    if(sessions_.size()>=connection_limit_) continue;
                    auto accepted_id=insert(std::move(socket),false);notify_open(accepted_id,true);
                    if(listener_.get()<0) break;
                }
                continue;
            }
            auto found=sessions_.find(id);if(found==sessions_.end()) continue;
            auto owner=found->second;auto& connection=*owner;
            if(connection.connecting) {
                if(now>=connection.deadline) {close(id);continue;}
                if(!events) continue;
                int error=0;socklen_t size=sizeof(error);
                if(getsockopt(connection.session.descriptor(),SOL_SOCKET,SO_ERROR,&error,&size)<0 || error) {
                    close(id);continue;
                }
                connection.connecting=false;notify_open(id,false);
                found=sessions_.find(id);if(found==sessions_.end()) continue;
            }
            if(events&POLLNVAL) {close(id);continue;}
            if(events&(POLLIN|POLLHUP|POLLERR)) {
                bool alive;
                try {
                    alive=owner->session.readable([&](Session::Packet packet){
                        if(sessions_.count(id) && hooks_.packet) hooks_.packet(id,std::move(packet));
                    });
                } catch(const FrameError&) {close(id);continue;}
                catch(...) {close(id);throw;}
                if(!alive) {close(id);continue;}
            }
            if(sessions_.count(id) && (events&POLLOUT) && !owner->session.writable()) close(id);
        }
    }
};
}
