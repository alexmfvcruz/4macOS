#pragma once
#include "runtime.hpp"
#include "received_packet.hpp"
#include <map>

namespace fourstory::server {
// Register real role handlers before binding. No absent handler reports success.
// Returned callbacks own an immutable registry and original packet storage;
// socket/session pointers never cross the owner/worker boundary.
class PacketDispatch {
public:
    using Handler=std::function<DWORD(Runtime&,Network::Id,CPacket&)>;
protected:
    std::map<WORD,Handler> handlers_;
    std::function<bool(WORD)> accepts_;
public:
    explicit PacketDispatch(std::function<bool(WORD)> accepts):accepts_(std::move(accepts)) {
        if(!accepts_) throw std::invalid_argument("Missing packet roster");
    }
    void add(WORD id,Handler handler) {
        if(!accepts_(id) || !handler) throw std::invalid_argument("Invalid role handler");
        if(!handlers_.emplace(id,std::move(handler)).second)
            throw std::invalid_argument("Duplicate role handler");
    }
    std::function<void(Runtime&,Network::Id,Session::Packet)> bind(Lane lane=Lane::batch) const {
        const auto handlers=std::make_shared<const std::map<WORD,Handler>>(handlers_);
        return [handlers,lane](Runtime& runtime,Network::Id id,Session::Packet frame) {
            auto packet=std::shared_ptr<CPacket>(received_packet(frame));
            const auto job=[handlers,packet,id,&runtime](const Runtime&) {
                const auto found=handlers->find(packet->GetID());
                const bool invalid=found==handlers->end() || found->second(runtime,id,*packet)!=0;
                if(invalid && !runtime.post([id](Network& network){network.close(id);}))
                    throw std::runtime_error("Role close command queue full");
            };
            if(!runtime.work(lane,job) &&
               !runtime.post([id](Network& network){network.close(id);}))
                throw std::runtime_error("Role dispatch and close queues full");
        };
    }
};
}
