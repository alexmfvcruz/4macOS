#pragma once
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#ifdef _WIN32
#include <wincrypt.h>
#else
#include <CommonCrypto/CommonDigest.h>
#endif

namespace c2dump {
inline std::string hexBytes(const void* bytes, size_t size) {
    static const char digits[]="0123456789ABCDEF";
    const auto* p=static_cast<const unsigned char*>(bytes);
    std::string result; result.reserve(size*2);
    for(size_t i=0;i<size;++i){result+=digits[p[i]>>4];result+=digits[p[i]&15];}
    return result;
}
inline std::string sha256(const void* bytes, size_t size) {
    unsigned char digest[32];
#ifdef _WIN32
    HCRYPTPROV provider=0; HCRYPTHASH hash=0;
    if(!CryptAcquireContext(&provider,nullptr,nullptr,PROV_RSA_AES,CRYPT_VERIFYCONTEXT))
        throw std::runtime_error("SHA256 provider unavailable");
    bool ok=CryptCreateHash(provider,CALG_SHA_256,0,0,&hash)!=0;
    if(ok) ok=size<=UINT32_MAX && CryptHashData(hash,static_cast<const BYTE*>(bytes),static_cast<DWORD>(size),0)!=0;
    DWORD length=sizeof(digest);
    if(ok) ok=CryptGetHashParam(hash,HP_HASHVAL,digest,&length,0)!=0 && length==sizeof(digest);
    if(hash) CryptDestroyHash(hash);
    CryptReleaseContext(provider,0);
    if(!ok)throw std::runtime_error("SHA256 failed");
#else
    if(size>UINT32_MAX)throw std::runtime_error("SHA256 input too large");
    CC_SHA256(bytes,static_cast<CC_LONG>(size),digest);
#endif
    return hexBytes(digest,sizeof(digest));
}
inline std::string escape(const CString& s) {
    std::string out;
    for(int i=0;i<s.GetLength();++i){unsigned char c=static_cast<unsigned char>(s[i]);
        if(c>=32&&c<=126&&c!='\\')out+=char(c);
        else out+="\\x"+hexBytes(&c,1);}
    return out;
}
struct Row {
    std::ostream& output; std::string dataset,key,prefix;
    Row child(const std::string& name) const {return {output,dataset,key,prefix+name+"."};}
    void text(const std::string& name,const std::string& v) const {
        output<<dataset<<'\t'<<key<<'\t'<<prefix<<name<<'\t'<<v<<'\n';
        if(!output)throw std::runtime_error("dump output write failed");
    }
    template<class T> typename std::enable_if<std::is_integral<T>::value||std::is_enum<T>::value,void>::type
    field(const std::string& name,T v) const {text(name,std::is_signed<T>::value?std::to_string(static_cast<long long>(v)):std::to_string(static_cast<unsigned long long>(v)));}
    void field(const std::string& name,float v) const {
        uint32_t bits;std::memcpy(&bits,&v,4);std::ostringstream s;s<<std::uppercase<<std::hex<<std::setw(8)<<std::setfill('0')<<bits;text(name,s.str());
    }
    void field(const std::string& name,const CString& v) const {text(name,escape(v));}
    void field(const std::string& name,const D3DXVECTOR3& v)const {field(name+".x",v.x);field(name+".y",v.y);field(name+".z",v.z);}
    void field(const std::string& name,const D3DXQUATERNION& v)const {field(name+".x",v.x);field(name+".y",v.y);field(name+".z",v.z);field(name+".w",v.w);}
    void bytes(const std::string& name,const void* p,size_t n)const {field(name+".size",n);text(name+".sha256",sha256(p,n));}
};
struct Ids {
    std::map<const void*,DWORD> entries;
    template<class M> void add(const M& map){for(const auto& v:map)entries.emplace(v.second,v.first);}
    DWORD operator()(const void* p)const {if(!p)return 0;auto i=entries.find(p);if(i==entries.end())throw std::runtime_error("resource pointer has no canonical ID");return i->second;}
};
}
