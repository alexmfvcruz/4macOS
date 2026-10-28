#pragma once
#include "packet_dispatch.hpp"
#include "../../source/4Story 5.0 Source Files/Lib/Own/TProtocol/include/SSProtocol.h"
#include "../../source/4Story 5.0 Source Files/Lib/Own/TProtocol/include/CTProtocol.h"
#include <map>

namespace fourstory::server {
inline bool control_message(WORD id) {
    switch(id) {
#define ON_RECEIVE(message) case message: return true;
#include "../../source/4Story 5.0 Source Files/Server/TControlSvr/ControlMessages.inc"
#undef ON_RECEIVE
    default: return false;
    }
}
class ControlDispatch : public PacketDispatch {
public:
    ControlDispatch():PacketDispatch(control_message) {}
};
}
