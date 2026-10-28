#pragma once
#include "packet_dispatch.hpp"
#include "../../source/4Story 5.0 Source Files/Lib/Own/TProtocol/include/SSProtocol.h"
#include "../../source/4Story 5.0 Source Files/Lib/Own/TProtocol/include/CTProtocol.h"
#include "../../source/4Story 5.0 Source Files/Lib/Own/TProtocol/include/MWProtocol.h"
#include "../../source/4Story 5.0 Source Files/Lib/Own/TProtocol/include/DMProtocol.h"
namespace fourstory::server {
inline bool world_message(WORD id) {
    switch(id) {
#define ON_RECEIVE(message) case message: return true;
#include "../../source/4Story 5.0 Source Files/Server/TWorldSvr/WorldMessages.inc"
#undef ON_RECEIVE
    default: return false;
    }
}
class WorldDispatch : public PacketDispatch {
public:
    WorldDispatch():PacketDispatch(world_message) {}
};
}
