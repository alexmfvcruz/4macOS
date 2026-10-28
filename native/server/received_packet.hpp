#pragma once
#ifndef FOURSTORY_NATIVE_PACKET
#error "Original packet bridge requires FOURSTORY_NATIVE_PACKET"
#endif
#include "packet_platform.hpp"
#include "../../source/4Story 5.0 Source Files/Server/TNetLib/Packet.h"
#include "session.hpp"
#include <memory>

namespace fourstory::server {
// Converts a completed native session frame to the original handler packet
// representation. No pointer to the event loop's temporary bytes escapes.
inline std::unique_ptr<CPacket> received_packet(const Session::Packet& frame) {
    if(frame.size()<16 || frame.size()>=MAX_PACKET_SIZE ||
       (std::size_t(frame[0]) | (std::size_t(frame[1])<<8))!=frame.size())
        throw FrameError("Invalid original-handler packet size");
    auto packet=std::make_unique<CPacket>();packet->m_bType=PACKET_RECV;
    packet->Clear();packet->ExpandIoBuffer(static_cast<DWORD>(frame.size()));
    std::memcpy(packet->GetBuffer(),frame.data(),frame.size());
    if(!packet->ReadBytes(static_cast<DWORD>(frame.size())))
        throw FrameError("Original-handler receive buffer full");
    packet->Rewind(FALSE);return packet;
}
}
