#include "../world_data_types.hpp"
#include "../../packet/wire.hpp"
#include <cassert>
#include <iostream>
#include <type_traits>
int main() {
    static_assert(std::is_same_v<__time64_t,__int64>);
    fourstory::server::world_data::EVENTINFO event{};
    event.m_dwIndex=0x11223344;event.m_bID=1;event.m_bState=2;
    event.m_bGroupID=3;event.m_bSvrType=4;event.m_bSvrID=5;
    event.m_dStartDate=0x0102030405060708LL;event.m_dEndDate=0x1112131415161718LL;
    CPacket packet;packet.SetID(0x1234);event.WrapPacketIn(&packet);
    story::Writer reference(0x1234);reference.u32(0x11223344);
    for(unsigned value=1;value<=5;++value) reference.u8(value);
    reference.i64(0x0102030405060708LL);reference.i64(0x1112131415161718LL);
    auto expected=reference.finish();
    // Ignore the header's full size: this fixture intentionally covers the prefix.
    assert(packet.GetSize()>=expected.size());
    assert(std::memcmp(packet.GetBuffer()+16,expected.data()+16,expected.size()-16)==0);
    packet.Rewind(FALSE);fourstory::server::world_data::EVENTINFO decoded{};
    decoded.WrapPacketOut(&packet);
    assert(decoded.m_dStartDate==event.m_dStartDate && decoded.m_dEndDate==event.m_dEndDate && packet.IsEOF());
    std::cout<<"PASS original World data types and event timestamp bytes preserve Windows 64-bit serialization\n";
}
