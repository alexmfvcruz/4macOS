// Native replacement for <wincrypt.h> (FOURSTORY_CLIENT_NATIVE): the CryptoAPI
// subset that CryptographyExt.cpp calls. Supported: provider PROV_RSA_FULL with
// CRYPT_VERIFYCONTEXT, an MD5 hash, and an RC4 key derived from the hash. The
// default provider of PROV_RSA_FULL on current Windows is the Microsoft Strong
// Cryptographic Provider, whose CryptDeriveKey(CALG_RC4) key is the whole
// 16-byte MD5 digest without salt. Checked against vectors from Windows
// CryptoAPI (tests/fixtures/rc4-cryptoapi.txt). Every other algorithm, flag or
// use fails with FALSE and a Win32 error code, as an unsupported request does.
#pragma once
#include "windows.h"
#include <array>

typedef unsigned int ALG_ID;
typedef ULONG_PTR HCRYPTPROV;
typedef ULONG_PTR HCRYPTHASH;
typedef ULONG_PTR HCRYPTKEY;
#define PROV_RSA_FULL 1
#define CRYPT_VERIFYCONTEXT 0xF0000000u
#define CRYPT_EXPORTABLE 0x00000001u
#define CALG_MD5 0x00008003u
#define CALG_SHA1 0x00008004u
#define CALG_RC4 0x00006801u
#define CALG_3DES 0x00006603u
#define HP_ALGID 0x0001
#define HP_HASHVAL 0x0002
#define HP_HASHSIZE 0x0004
#define NTE_BAD_HASH ((DWORD)0x80090002L)
#define NTE_BAD_KEY ((DWORD)0x80090003L)
#define NTE_BAD_LEN ((DWORD)0x80090004L)
#define NTE_BAD_ALGID ((DWORD)0x80090008L)
#define NTE_BAD_FLAGS ((DWORD)0x80090009L)
#define NTE_BAD_TYPE ((DWORD)0x8009000AL)
#define NTE_BAD_HASH_STATE ((DWORD)0x8009000CL)
#define NTE_BAD_PROV_TYPE ((DWORD)0x80090014L)
#define NTE_BAD_KEYSET_PARAM ((DWORD)0x8009001FL)

namespace fourstory { namespace compat { namespace crypt {
class md5 {
public:
    md5() : length_(0), used_(0) { state_ = {0x67452301u, 0xefcdab89u, 0x98badcfeu, 0x10325476u}; }
    void update(const BYTE* data, std::size_t count) {
        length_ += count;
        while (count) {
            const std::size_t take = count < 64 - used_ ? count : 64 - used_;
            std::memcpy(block_ + used_, data, take);
            used_ += take; data += take; count -= take;
            if (used_ == 64) { transform(block_); used_ = 0; }
        }
    }
    std::array<BYTE, 16> finish() {
        const unsigned long long bits = static_cast<unsigned long long>(length_) * 8u;
        static const BYTE pad[64] = {0x80};
        update(pad, used_ < 56 ? 56 - used_ : 120 - used_);
        BYTE size[8];
        for (int i = 0; i < 8; ++i) size[i] = static_cast<BYTE>(bits >> (8 * i));
        update(size, 8);
        std::array<BYTE, 16> digest{};
        for (int i = 0; i < 16; ++i) digest[static_cast<std::size_t>(i)] = static_cast<BYTE>(state_[static_cast<std::size_t>(i / 4)] >> (8 * (i % 4)));
        return digest;
    }
private:
    void transform(const BYTE* block) {
        static const std::uint32_t k[64] = {
            0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,
            0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,0x6b901122,0xfd987193,0xa679438e,0x49b40821,
            0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
            0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,
            0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,
            0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
            0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,
            0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391};
        static const int r[64] = {7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22,5,9,14,20,5,9,14,20,5,9,14,20,5,9,14,20,
                                  4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23,6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21};
        std::uint32_t w[16];
        for (int i = 0; i < 16; ++i)
            w[i] = std::uint32_t(block[4 * i]) | std::uint32_t(block[4 * i + 1]) << 8 | std::uint32_t(block[4 * i + 2]) << 16 | std::uint32_t(block[4 * i + 3]) << 24;
        std::uint32_t a = state_[0], b = state_[1], c = state_[2], d = state_[3];
        for (int i = 0; i < 64; ++i) {
            std::uint32_t f; int g;
            if (i < 16) { f = (b & c) | (~b & d); g = i; }
            else if (i < 32) { f = (d & b) | (~d & c); g = (5 * i + 1) % 16; }
            else if (i < 48) { f = b ^ c ^ d; g = (3 * i + 5) % 16; }
            else { f = c ^ (b | ~d); g = (7 * i) % 16; }
            const std::uint32_t t = d; d = c; c = b;
            const std::uint32_t x = a + f + k[i] + w[g];
            b = b + ((x << r[i]) | (x >> (32 - r[i]))); a = t;
        }
        state_[0] += a; state_[1] += b; state_[2] += c; state_[3] += d;
    }
    std::array<std::uint32_t, 4> state_;
    std::size_t length_, used_;
    BYTE block_[64];
};
class rc4 {
public:
    explicit rc4(const BYTE* key, std::size_t count) { std::memcpy(key_, key, count); count_ = count; reset(); }
    void reset() {
        for (int n = 0; n < 256; ++n) s_[n] = static_cast<BYTE>(n);
        for (int n = 0, m = 0; n < 256; ++n) { m = (m + s_[n] + key_[static_cast<std::size_t>(n) % count_]) & 255; const BYTE t = s_[n]; s_[n] = s_[m]; s_[m] = t; }
        i_ = j_ = 0;
    }
    void apply(BYTE* data, std::size_t count) {
        for (std::size_t n = 0; n < count; ++n) {
            i_ = (i_ + 1) & 255; j_ = (j_ + s_[i_]) & 255;
            const BYTE t = s_[i_]; s_[i_] = s_[j_]; s_[j_] = t;
            data[n] ^= s_[(s_[i_] + s_[j_]) & 255];
        }
    }
private:
    BYTE key_[16], s_[256];
    std::size_t count_;
    int i_, j_;
};
const unsigned int provider_magic = 0x50525631u, hash_magic = 0x48415331u, key_magic = 0x4B455931u;
struct crypt_provider { unsigned int magic; };
struct crypt_hash { unsigned int magic; md5 state; bool finished; std::array<BYTE, 16> digest; };
struct crypt_key { unsigned int magic; rc4 cipher; };
template<class T> inline T* object(ULONG_PTR handle, unsigned int magic) {
    T* item = reinterpret_cast<T*>(handle);
    return item && item->magic == magic ? item : nullptr;
}
inline BOOL fail(DWORD code) { SetLastError(code); return FALSE; }
}}}

inline BOOL CryptAcquireContext(HCRYPTPROV* provider, LPCTSTR container, LPCTSTR name, DWORD type, DWORD flags) {
    using namespace fourstory::compat::crypt;
    if (!provider) return fail(ERROR_INVALID_PARAMETER);
    *provider = 0;
    if (type != PROV_RSA_FULL) return fail(NTE_BAD_PROV_TYPE);
    if (container || name || flags != CRYPT_VERIFYCONTEXT) return fail(NTE_BAD_FLAGS);
    *provider = reinterpret_cast<HCRYPTPROV>(new crypt_provider{provider_magic});
    return TRUE;
}
#define CryptAcquireContextA CryptAcquireContext
inline BOOL CryptReleaseContext(HCRYPTPROV handle, DWORD flags) {
    using namespace fourstory::compat::crypt;
    crypt_provider* item = object<crypt_provider>(handle, provider_magic);
    if (!item) return fail(ERROR_INVALID_HANDLE);
    if (flags) return fail(NTE_BAD_FLAGS);
    item->magic = 0; delete item;
    return TRUE;
}
inline BOOL CryptCreateHash(HCRYPTPROV provider_handle, ALG_ID algorithm, HCRYPTKEY key_handle, DWORD flags, HCRYPTHASH* result) {
    using namespace fourstory::compat::crypt;
    if (!result) return fail(ERROR_INVALID_PARAMETER);
    *result = 0;
    if (!object<crypt_provider>(provider_handle, provider_magic)) return fail(ERROR_INVALID_HANDLE);
    if (algorithm != CALG_MD5) return fail(NTE_BAD_ALGID);
    if (key_handle || flags) return fail(NTE_BAD_FLAGS);
    *result = reinterpret_cast<HCRYPTHASH>(new crypt_hash{hash_magic, md5(), false, {}});
    return TRUE;
}
inline BOOL CryptHashData(HCRYPTHASH handle, const BYTE* data, DWORD count, DWORD flags) {
    using namespace fourstory::compat::crypt;
    crypt_hash* item = object<crypt_hash>(handle, hash_magic);
    if (!item) return fail(ERROR_INVALID_HANDLE);
    if (flags) return fail(NTE_BAD_FLAGS);
    if (item->finished) return fail(NTE_BAD_HASH_STATE);
    if (count && !data) return fail(ERROR_INVALID_PARAMETER);
    if (count) item->state.update(data, count);
    return TRUE;
}
inline BOOL CryptGetHashParam(HCRYPTHASH handle, DWORD parameter, BYTE* data, DWORD* count, DWORD flags) {
    using namespace fourstory::compat::crypt;
    crypt_hash* item = object<crypt_hash>(handle, hash_magic);
    if (!item) return fail(ERROR_INVALID_HANDLE);
    if (flags || !count) return fail(flags ? NTE_BAD_FLAGS : ERROR_INVALID_PARAMETER);
    if (parameter == HP_HASHSIZE || parameter == HP_ALGID) {
        if (data && *count < 4) { *count = 4; return fail(ERROR_MORE_DATA); }
        if (data) { const DWORD value = parameter == HP_HASHSIZE ? 16u : CALG_MD5; std::memcpy(data, &value, 4); }
        *count = 4; return TRUE;
    }
    if (parameter != HP_HASHVAL) return fail(NTE_BAD_TYPE);
    if (data && *count < 16) { *count = 16; return fail(ERROR_MORE_DATA); }
    if (data) {
        if (!item->finished) { item->digest = item->state.finish(); item->finished = true; }
        std::memcpy(data, item->digest.data(), 16);
    }
    *count = 16; return TRUE;
}
inline BOOL CryptDestroyHash(HCRYPTHASH handle) {
    using namespace fourstory::compat::crypt;
    crypt_hash* item = object<crypt_hash>(handle, hash_magic);
    if (!item) return fail(ERROR_INVALID_HANDLE);
    item->magic = 0; delete item;
    return TRUE;
}
// The key length in the upper 16 bits of `flags` must be 0 (default) or 128.
inline BOOL CryptDeriveKey(HCRYPTPROV provider_handle, ALG_ID algorithm, HCRYPTHASH hash_handle, DWORD flags, HCRYPTKEY* result) {
    using namespace fourstory::compat::crypt;
    if (!result) return fail(ERROR_INVALID_PARAMETER);
    *result = 0;
    if (!object<crypt_provider>(provider_handle, provider_magic)) return fail(ERROR_INVALID_HANDLE);
    crypt_hash* item = object<crypt_hash>(hash_handle, hash_magic);
    if (!item) return fail(ERROR_INVALID_HANDLE);
    if (algorithm != CALG_RC4) return fail(NTE_BAD_ALGID);
    const DWORD bits = flags >> 16;
    if ((bits != 0 && bits != 128) || (flags & 0xFFFFu & ~CRYPT_EXPORTABLE)) return fail(NTE_BAD_FLAGS);
    if (!item->finished) { item->digest = item->state.finish(); item->finished = true; }
    *result = reinterpret_cast<HCRYPTKEY>(new crypt_key{key_magic, rc4(item->digest.data(), 16)});
    return TRUE;
}
inline BOOL CryptDestroyKey(HCRYPTKEY handle) {
    using namespace fourstory::compat::crypt;
    crypt_key* item = object<crypt_key>(handle, key_magic);
    if (!item) return fail(ERROR_INVALID_HANDLE);
    item->magic = 0; delete item;
    return TRUE;
}
// RC4 is a stream cipher: the output has the input length. The key stream
// continues across calls and restarts after a call with final = TRUE.
inline BOOL CryptEncrypt(HCRYPTKEY handle, HCRYPTHASH hash_handle, BOOL final, DWORD flags, BYTE* data, DWORD* count, DWORD capacity) {
    using namespace fourstory::compat::crypt;
    crypt_key* item = object<crypt_key>(handle, key_magic);
    if (!item) return fail(ERROR_INVALID_HANDLE);
    if (hash_handle || flags) return fail(NTE_BAD_FLAGS);
    if (!count) return fail(ERROR_INVALID_PARAMETER);
    if (!data) return TRUE; // size query: the output length equals the input length
    if (capacity < *count) return fail(ERROR_MORE_DATA);
    item->cipher.apply(data, *count);
    if (final) item->cipher.reset();
    return TRUE;
}
inline BOOL CryptDecrypt(HCRYPTKEY handle, HCRYPTHASH hash_handle, BOOL final, DWORD flags, BYTE* data, DWORD* count) {
    using namespace fourstory::compat::crypt;
    if (!count) return fail(ERROR_INVALID_PARAMETER);
    if (!data) return fail(ERROR_INVALID_PARAMETER);
    return CryptEncrypt(handle, hash_handle, final, flags, data, count, *count);
}
