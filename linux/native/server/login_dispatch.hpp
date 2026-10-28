#pragma once
#include "packet_dispatch.hpp"
#include "../../source/4Story 5.0 Source Files/Lib/Own/TProtocol/include/CTProtocol.h"
#include "../../source/4Story 5.0 Source Files/Lib/Own/TProtocol/include/CSProtocol.h"
namespace fourstory::server {
inline bool login_message(WORD id) {
    switch(id) {
#define ON_RECEIVE(message) case message: return true;
#include "../../source/4Story 5.0 Source Files/Server/TLoginSvr/LoginServerMessages.inc"
#include "../../source/4Story 5.0 Source Files/Server/TLoginSvr/LoginClientMessages.inc"
#undef ON_RECEIVE
    default: return false;
    }
}
class LoginDispatch : public PacketDispatch {
    struct Connection {
        std::deque<std::shared_ptr<CPacket>> pending;
        bool running=false,closed=false;
    };
    struct State {
        std::mutex mutex;
        std::map<Network::Id,std::shared_ptr<Connection>> connections;
        std::size_t pending=0;
        void discard(Connection& connection) {
            pending-=connection.pending.size();connection.pending.clear();connection.closed=true;
        }
    };
    static void close(Runtime& runtime,Network::Id id) {
        if(!runtime.post([id](Network& network){network.close(id);}))
            throw std::runtime_error("Login close command queue full");
    }
public:
    LoginDispatch():PacketDispatch(login_message) {}
    // Install both hooks: closed cancels queued work and releases per-session state.
    // Each session has one drain job; separate sessions can use separate workers.
    Runtime::Hooks hooks() const {
        auto state=std::make_shared<State>();
        auto handlers=std::make_shared<const std::map<WORD,Handler>>(handlers_);
        Runtime::Hooks result;
        result.closed=[state](Runtime&,Network::Id id) {
            std::lock_guard<std::mutex> lock(state->mutex);
            auto found=state->connections.find(id);
            if(found!=state->connections.end()) {state->discard(*found->second);state->connections.erase(found);}
        };
        result.packet=[state,handlers](Runtime& runtime,Network::Id id,Session::Packet frame) {
            auto packet=std::shared_ptr<CPacket>(received_packet(frame));
            std::shared_ptr<Connection> connection;bool start=false,rejected=false;
            {
                std::lock_guard<std::mutex> lock(state->mutex);
                auto& entry=state->connections[id];if(!entry) entry=std::make_shared<Connection>();
                connection=entry;
                if(connection->closed) return;
                if(state->pending>=runtime.queue_limit()) {state->discard(*connection);rejected=true;}
                else {
                    connection->pending.push_back(std::move(packet));++state->pending;
                    if(!connection->running) {connection->running=true;start=true;}
                }
            }
            if(rejected) {close(runtime,id);return;}
            if(!start) return;
            const auto drain=[state,handlers,connection,id,&runtime](const Runtime&) {
                try {
                    for(;;) {
                        std::shared_ptr<CPacket> next;
                        {
                            std::lock_guard<std::mutex> lock(state->mutex);
                            if(connection->closed || connection->pending.empty()) {connection->running=false;return;}
                            next=std::move(connection->pending.front());connection->pending.pop_front();--state->pending;
                        }
                        auto found=handlers->find(next->GetID());
                        if(found==handlers->end() || found->second(runtime,id,*next)!=0) {
                            {std::lock_guard<std::mutex> lock(state->mutex);state->discard(*connection);}
                            close(runtime,id);return;
                        }
                    }
                } catch(...) {
                    {std::lock_guard<std::mutex> lock(state->mutex);state->discard(*connection);}
                    throw;
                }
            };
            if(!runtime.work(Lane::network,drain)) {
                {std::lock_guard<std::mutex> lock(state->mutex);state->discard(*connection);}
                close(runtime,id);
            }
        };
        return result;
    }
};
}
