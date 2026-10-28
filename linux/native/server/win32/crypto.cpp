// Native replacement for TNetLib/CryptographyExt.cpp (Windows CryptoAPI).
// EncryptBuffer(CALG_RC4) reproduces CryptDeriveKey with an MD5 hash under the
// default PROV_RSA_FULL provider: RC4 keyed by the 16-byte MD5 digest of the
// secret. This is unverified against Windows output; milestone 6 must compare
// packet vectors from the original client before relying on client sessions.
#include "atl.hpp"
#include "../../../source/4Story 5.0 Source Files/Server/TNetLib/CryptographyExt.h"
#include <array>

namespace {
std::array<std::uint8_t,16> md5(const std::uint8_t* data,std::size_t length) {
    static const std::uint32_t k[64]={
        0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,
        0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,0x6b901122,0xfd987193,0xa679438e,0x49b40821,
        0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
        0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,
        0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,
        0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
        0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,
        0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391};
    static const int r[64]={7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22,5,9,14,20,5,9,14,20,5,9,14,20,5,9,14,20,
        4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23,6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21};
    std::uint32_t h[4]={0x67452301,0xefcdab89,0x98badcfe,0x10325476};
    std::vector<std::uint8_t> message(data,data+length);
    message.push_back(0x80);
    while(message.size()%64!=56) message.push_back(0);
    const std::uint64_t bits=static_cast<std::uint64_t>(length)*8;
    for(int i=0;i<8;++i) message.push_back(static_cast<std::uint8_t>(bits>>(8*i)));
    for(std::size_t block=0;block<message.size();block+=64) {
        std::uint32_t w[16];
        for(int i=0;i<16;++i)
            w[i]=std::uint32_t(message[block+4*i])|std::uint32_t(message[block+4*i+1])<<8|
                std::uint32_t(message[block+4*i+2])<<16|std::uint32_t(message[block+4*i+3])<<24;
        std::uint32_t a=h[0],b=h[1],c=h[2],d=h[3];
        for(int i=0;i<64;++i) {
            std::uint32_t f;int g;
            if(i<16) {f=(b&c)|(~b&d);g=i;}
            else if(i<32) {f=(d&b)|(~d&c);g=(5*i+1)%16;}
            else if(i<48) {f=b^c^d;g=(3*i+5)%16;}
            else {f=c^(b|~d);g=(7*i)%16;}
            const std::uint32_t t=d;d=c;c=b;
            const std::uint32_t x=a+f+k[i]+w[g];
            b=b+((x<<r[i])|(x>>(32-r[i])));a=t;
        }
        h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;
    }
    std::array<std::uint8_t,16> digest{};
    for(int i=0;i<16;++i) digest[static_cast<std::size_t>(i)]=static_cast<std::uint8_t>(h[i/4]>>(8*(i%4)));
    return digest;
}
void rc4(const std::uint8_t* key,std::size_t key_length,std::uint8_t* data,std::size_t length) {
    std::uint8_t s[256];
    for(int i=0;i<256;++i) s[i]=static_cast<std::uint8_t>(i);
    for(int i=0,j=0;i<256;++i) {j=(j+s[i]+key[static_cast<std::size_t>(i)%key_length])&255;std::swap(s[i],s[j]);}
    for(std::size_t n=0,i=0,j=0;n<length;++n) {
        i=(i+1)&255;j=(j+s[i])&255;std::swap(s[i],s[j]);
        data[n]^=s[(s[i]+s[j])&255];
    }
}
BOOL transform(ALG_ID algorithm,LPBYTE output,DWORD& output_length,LPBYTE input,DWORD input_length,LPBYTE secret,DWORD secret_length) {
    if(algorithm!=CALG_RC4 || !output || !input || !secret || output_length<input_length) return FALSE;
    std::memmove(output,input,input_length);
    const auto key=md5(secret,secret_length);
    rc4(key.data(),key.size(),output,input_length);
    output_length=input_length;return TRUE;
}
}

void TraceLastError(LPCTSTR,LPCTSTR,DWORD) {}
CString GetComputerID() {
    char name[256]{};
    if(::gethostname(name,sizeof(name)-1)!=0) return CString("MihaiMoga");
    return CString(name);
}
BOOL EncryptBuffer(ALG_ID algorithm,LPBYTE output,DWORD& output_length,LPBYTE input,DWORD input_length,LPBYTE secret,DWORD secret_length) {
    return transform(algorithm,output,output_length,input,input_length,secret,secret_length);
}
BOOL DecryptBuffer(ALG_ID algorithm,LPBYTE output,DWORD& output_length,LPBYTE input,DWORD input_length,LPBYTE secret,DWORD secret_length) {
    return transform(algorithm,output,output_length,input,input_length,secret,secret_length);
}
