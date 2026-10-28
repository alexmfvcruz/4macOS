#pragma once
#include <chrono>
#include <cerrno>
#include <mutex>
#include <optional>
#include <poll.h>
#include <string>
#include <unistd.h>

namespace fourstory::server {
// Borrowed dedicated supervisor pipe endpoints. Serialize request/response pairs;
// after a timeout the stream is unusable, so a late response cannot match a new request.
class SupervisorCommands {
    int input_,output_;
    std::mutex mutex_;
    bool failed_=false;
public:
    SupervisorCommands(int input,int output):input_(input),output_(output) {}
    struct Result {bool ok=false;unsigned state=0;};
private:
    Result request(const char* action,std::size_t service) {
        std::lock_guard<std::mutex> lock(mutex_);
        if(failed_ || (std::string(action)!="start" && std::string(action)!="stop" && std::string(action)!="status")) return {};
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
        auto ready=[&](int fd,short events) -> bool {
            while(std::chrono::steady_clock::now()<deadline) {
                auto left=std::chrono::duration_cast<std::chrono::milliseconds>(deadline-std::chrono::steady_clock::now()).count();
                if(left<=0) return false;
                pollfd descriptor{fd,events,0};
                const auto result=::poll(&descriptor,1,static_cast<int>(left));
                if(result<0 && errno==EINTR) continue;
                return result>0 && (descriptor.revents&events);
            }
            return {};
        };
        const auto request=std::string("{\"action\":\"")+action+"\",\"service\":"+std::to_string(service)+"}\n";
        std::size_t offset=0;
        while(offset<request.size()) {
            if(!ready(input_,POLLOUT)) {failed_=true;return {};}
            const auto count=::write(input_,request.data()+offset,request.size()-offset);
            if(count<0 && errno==EINTR) continue;
            if(count<=0) {failed_=true;return {};}
            offset+=static_cast<std::size_t>(count);
        }
        std::string line;
        for(std::size_t bytes=0;bytes<65536;++bytes) {
            if(!ready(output_,POLLIN)) {failed_=true;return {};}
            char value;
            const auto count=::read(output_,&value,1);
            if(count<0 && errno==EINTR) continue;
            if(count!=1) {failed_=true;return {};}
            if(value!='\n') {line+=value;continue;}
            if(line.rfind("CONTROL ",0)==0) {
                const auto target="\"service\":"+std::to_string(service)+",";
                if(line.find("\"ok\":true")==std::string::npos) return {};
                const auto state_pos=line.find("\"state\":");
                if(line.find(target)==std::string::npos || state_pos==std::string::npos) {
                    failed_=true;return {};
                }
                const auto value_pos=state_pos+8;
                if(value_pos+1>=line.size() || line[value_pos]<'1' || line[value_pos]>'4' ||
                   (line[value_pos+1]!=',' && line[value_pos+1]!='}')) {failed_=true;return {};}
                return {true,static_cast<unsigned>(line[value_pos]-'0')};
            }
            line.clear();
        }
        failed_=true;return {};
    }
public:
    bool operator()(const char* action,std::size_t service) {return request(action,service).ok;}
    std::optional<unsigned> status(std::size_t service) {
        const auto result=request("status",service);
        return result.ok?std::optional<unsigned>(result.state):std::nullopt;
    }
};
}
