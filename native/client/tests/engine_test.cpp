#include <fourstory_engine_data.h>
#include <cassert>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <limits>
int main(int argc,char** argv){
    assert(argc==2);static_assert(sizeof(PACKETHEADER)==16,"client packet disk ABI");
    CPacket packet;packet.SetID(0x1234);
    packet<<BYTE(0xab)<<WORD(0xcdef)<<DWORD(0x12345678)<<long(-2)<<float(1.5f)<<__int64(0x0102030405060708)<<LPCTSTR("4Story");
    std::ostringstream bytes;for(WORD i=0;i<packet.GetSize();++i)bytes<<std::hex<<std::setw(2)<<std::setfill('0')<<unsigned(packet.GetBuffer()[i]);
    std::ifstream fixture(argv[1]);std::string expected;fixture>>expected;assert(fixture&&!expected.empty());assert(bytes.str()==expected);
    packet.Rewind(FALSE);
    BYTE byte;WORD word;DWORD dword;long integer;float real;__int64 wide;CString text;
    packet>>byte>>word>>dword>>integer>>real>>wide>>text;
    assert(byte==0xab&&word==0xcdef&&dword==0x12345678&&integer==-2&&real==1.5f&&wide==0x0102030405060708&&text=="4Story");
    packet.Rewind(FALSE);packet.Encrypt(std::numeric_limits<INT64>::max());assert(packet.Decrypt(std::numeric_limits<INT64>::max()));
    std::ostringstream restored;for(WORD i=0;i<packet.GetSize();++i)restored<<std::hex<<std::setw(2)<<std::setfill('0')<<unsigned(packet.GetBuffer()[i]);assert(restored.str().substr(32)==expected.substr(32));
    for(int length=1;length<=7;++length){CPacket tail;for(int i=0;i<length;++i)tail<<BYTE(0);tail.Encrypt(std::numeric_limits<INT64>::max());assert(tail.Decrypt(std::numeric_limits<INT64>::max()));}
    CPacket header;header.SetID(0x1234);header.EncryptHeader(std::numeric_limits<INT64>::max());header.DecryptHeader(std::numeric_limits<INT64>::max());assert(header.GetID()==0x1234);
    BYTE bits[]={0xab,0xcd,0xef,0x12,0x34,0x56,0x78,0x90};CTBitStream bitstream(bits,sizeof bits);assert(bitstream.ReadBITs(4)==0xa&&bitstream.GetPOS()==0);assert(bitstream.PopBITs(8)==0xab&&bitstream.PopBITs(8)==0xcd);
    CTachyonCompressor compressor;const char payload[]="compressed original engine bytes";compressor.Write(payload,sizeof payload);compressor.Compress();
    CTachyonUncompressor uncompressor;uncompressor.Uncompress(compressor.GetCompressedData(),compressor.GetCompressedLength(),compressor.GetLength());char decoded[sizeof payload];assert(uncompressor.Read(decoded,sizeof decoded)==sizeof decoded);assert(!std::memcmp(decoded,payload,sizeof payload));
    CTBSPPortal::InitCommon();{CTBSPNode node;assert(node.SetID(7));assert(CTBSPNode::GetNode(7)==&node);CTBSPPortal portal;assert(portal.SetID(19));assert(CTBSPPortal::GetPortal(19)==&portal);}CTBSPPortal::ReleaseCommon();CTBSPNode::ReleaseCommon();
    std::cout<<"PASS original client scalar fixture, encrypt/decrypt, bitstream, zlib and BSP identities\n";
}
