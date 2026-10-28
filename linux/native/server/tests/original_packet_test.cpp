#include "../packet_platform.hpp"
#include "../received_packet.hpp"
#include "../../../source/4Story 5.0 Source Files/Server/TNetLib/Packet.h"
#include "../../packet/wire.hpp"
#include <cassert>
#include <iostream>
int main() {
    static_assert(sizeof(PACKETHEADER)==16 && sizeof(DWORD)==4 && sizeof(INT64)==8);
    CPacket packet;packet.SetID(0x1234);
    packet<<BYTE(0xab)<<WORD(0xcdef)<<DWORD(0x12345678)<<long(-2)<<float(1.5)
          <<static_cast<__int64>(0x0102030405060708)<<"4Story";
    story::Writer reference(0x1234);reference.u8(0xab);reference.u16(0xcdef);
    reference.u32(0x12345678);reference.windows_long(-2);reference.f32(1.5);
    reference.i64(0x0102030405060708);reference.string_bytes("4Story");
    auto expected=reference.finish();assert(packet.GetSize()==expected.size());
    assert(std::memcmp(packet.GetBuffer(),expected.data(),expected.size())==0);
    packet.Rewind(FALSE);BYTE byte=0;WORD word=0;DWORD dword=0;long value=0;float floating=0;
    __int64 wide=0;CString text;
    packet>>byte>>word>>dword>>value>>floating>>wide>>text;
    assert(byte==0xab && word==0xcdef && dword==0x12345678 && value==-2 && floating==1.5);
    assert(wide==0x0102030405060708 && text.bytes()=="4Story" && packet.IsEOF());
    bool overflow=false;try{packet<<std::numeric_limits<long>::max();}catch(const std::out_of_range&){overflow=true;}
    assert(overflow);
    auto incoming=fourstory::server::received_packet(expected);
    incoming->m_ptrOffset=incoming->GetBuffer()+expected.size()-10;
    CString decoded;*incoming>>decoded;assert(decoded.bytes()=="4Story");
    auto bad=expected;bad[bad.size()-10]=255;bad[bad.size()-9]=255;
    bad[bad.size()-8]=255;bad[bad.size()-7]=255;
    auto corrupt=fourstory::server::received_packet(bad);
    corrupt->m_ptrOffset=corrupt->GetBuffer()+bad.size()-10;
    bool rejected=false;try{*corrupt>>decoded;}catch(const std::out_of_range&){rejected=true;}
    assert(rejected);
    rejected=false;try{incoming->Read(&byte,1);}catch(const std::out_of_range&){rejected=true;}
    assert(rejected);
    CPacket full;std::vector<BYTE> too_large(65535);
    rejected=false;try{full.Write(too_large.data(),static_cast<int>(too_large.size()));}
    catch(const std::length_error&){rejected=true;}
    assert(rejected && full.GetSize()==16);
    std::cout<<"PASS original CPacket compiled natively, 16-byte header, Windows-width fixture and CString decode\n";
}
