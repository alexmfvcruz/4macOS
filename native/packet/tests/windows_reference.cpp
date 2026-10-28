// Build on Windows as Release Win32/MultiByte using the original TNetLib
// include directory and TNetLib.Lib. This uses the original implementation.
#include <TNetLib.h>
#include <cstdio>
int main() {
    static_assert(sizeof(long) == 4, "Windows long wire contract");
    static_assert(sizeof(PACKETHEADER) == 16, "Confirm original header ABI");
    CPacket packet;
    packet.SetID(0x1234);
    packet << BYTE(0xab) << WORD(0xcdef) << DWORD(0x12345678)
           << long(-2) << float(1.5f) << __int64(0x0102030405060708)
           << LPCTSTR("4Story");
    for (WORD i = 0; i < packet.GetSize(); ++i)
        std::printf("%02x", unsigned(packet.GetBuffer()[i]));
    std::printf("\n");
}
