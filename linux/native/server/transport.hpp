#pragma once
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <system_error>
#include <utility>
#include <vector>

namespace fourstory::server {
// Original CPacket::IsValid reserves 0xffff as invalid.
inline constexpr std::size_t maximum_frame_size=65534;
class Socket {
    int fd_=-1;
public:
    Socket()=default;
    explicit Socket(int fd):fd_(fd) {}
    Socket(const Socket&)=delete;
    Socket& operator=(const Socket&)=delete;
    Socket(Socket&& other) noexcept:fd_(std::exchange(other.fd_,-1)) {}
    Socket& operator=(Socket&& other) noexcept {
        if(this!=&other) {close();fd_=std::exchange(other.fd_,-1);}return *this;
    }
    ~Socket(){close();}
    int get() const{return fd_;}
    void close() noexcept {if(fd_>=0){::close(fd_);fd_=-1;}}
    void nonblocking() {
        const int flags=fcntl(fd_,F_GETFL,0);
        if(flags<0 || fcntl(fd_,F_SETFL,flags|O_NONBLOCK)<0)
            throw std::system_error(errno,std::generic_category(),"Nonblocking socket");
        const int descriptor_flags=fcntl(fd_,F_GETFD,0);
        if(descriptor_flags<0 || fcntl(fd_,F_SETFD,descriptor_flags|FD_CLOEXEC)<0)
            throw std::system_error(errno,std::generic_category(),"Socket close-on-exec");
#ifdef SO_NOSIGPIPE
        const int enabled=1;
        if(setsockopt(fd_,SOL_SOCKET,SO_NOSIGPIPE,&enabled,sizeof(enabled))<0)
            throw std::system_error(errno,std::generic_category(),"Socket SIGPIPE suppression");
#endif
    }
    static Socket tcp() {
        Socket result(::socket(AF_INET,SOCK_STREAM,0));
        if(result.get()<0) throw std::system_error(errno,std::generic_category(),"TCP socket");
        result.nonblocking();return result;
    }
    // Negative/EAGAIN and zero/EOF remain distinct for the event loop.
    ssize_t read(void* data,std::size_t size) {
        ssize_t result;do{result=::recv(fd_,data,size,0);}while(result<0 && errno==EINTR);return result;
    }
    ssize_t write(const void* data,std::size_t size) {
        int flags=0;
#ifdef MSG_NOSIGNAL
        flags=MSG_NOSIGNAL;
#endif
        ssize_t result;do{result=::send(fd_,data,size,flags);}while(result<0 && errno==EINTR);return result;
    }
};
class FrameError:public std::runtime_error {
public:
    explicit FrameError(const char* message):std::runtime_error(message) {}
};
class FrameDecoder {
    std::vector<std::uint8_t> buffered_;
public:
    template<class Callback> void feed(const std::uint8_t* bytes,std::size_t size,Callback deliver) {
        // Buffer only the current frame: a read may finish a maximum-sized
        // packet and contain more packets without exceeding the per-frame cap.
        while(size) {
            std::size_t target=2;
            if(buffered_.size()>=2) {
                target=std::size_t(buffered_[0]) | (std::size_t(buffered_[1])<<8);
                if(target<16 || target>maximum_frame_size) throw FrameError("Invalid packet frame length");
            }
            const auto count=std::min(size,target-buffered_.size());
            buffered_.insert(buffered_.end(),bytes,bytes+count);
            bytes+=count;size-=count;
            if(buffered_.size()<2) continue;
            const auto length=std::size_t(buffered_[0]) | (std::size_t(buffered_[1])<<8);
            if(length<16 || length>maximum_frame_size) throw FrameError("Invalid packet frame length");
            if(buffered_.size()==length) {
                auto frame=std::move(buffered_);buffered_.clear();
                deliver(std::move(frame));
            }
        }
    }
    bool empty() const{return buffered_.empty();}
};
}
