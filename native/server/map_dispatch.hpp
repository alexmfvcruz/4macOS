#pragma once
#include "packet_dispatch.hpp"
#include "../../source/4Story 5.0 Source Files/Server/TNetLib/SessionTypes.h"
#include "../../source/4Story 5.0 Source Files/Lib/Own/TProtocol/include/SSProtocol.h"
#include "../../source/4Story 5.0 Source Files/Lib/Own/TProtocol/include/CTProtocol.h"
#include "../../source/4Story 5.0 Source Files/Lib/Own/TProtocol/include/CSProtocol.h"
#include "../../source/4Story 5.0 Source Files/Lib/Own/TProtocol/include/MWProtocol.h"
#include "../../source/4Story 5.0 Source Files/Lib/Own/TProtocol/include/DMProtocol.h"
namespace fourstory::server {
// Metadata only, with the source field names used by its shared switch. This
// is not a Windows socket/session object and does not authorize a message.
struct MapPeerMetadata {
    BYTE m_bSessionType;
    sockaddr_in m_addr;
};
inline bool map_message(WORD wMsgID,const MapPeerMetadata* peer=nullptr,
                        WORD m_wGamePort=0,WORD m_wWorldPort=0) {
    struct PacketContext {const MapPeerMetadata* m_pSESSION;} context{peer};
    const auto* pBUF=&context;
    const auto LogSmartly=[](const char* message){throw FrameError(message);};
#define ON_RECEIVE(message) case message: return true;
#include "../../source/4Story 5.0 Source Files/Server/TMapSvr/MapMessages.inc"
#undef ON_RECEIVE
    return false;
}
class MapDispatch : public PacketDispatch {
public:
    MapDispatch():PacketDispatch([](WORD id){return map_message(id);}) {}
};
}
