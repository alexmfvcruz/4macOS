#pragma once
// Included only by the explicitly selected native packet compilation. These
// aliases are not a general Win32 emulation layer and provide no socket handles.
#include "legacy_string.hpp"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <mutex>
#include "database_time.hpp"
using BYTE=std::uint8_t;
using WORD=std::uint16_t;
using DWORD=std::uint32_t;
using FLOAT=float;
#ifndef BOOL
using BOOL=int;
#endif
using INT64=long long;
using PINT64=INT64*;
using __int64=long long;
using LPVOID=void*;
using LPBYTE=BYTE*;
using LPCTSTR=const char*;
using LPTSTR=char*;
using CString=fourstory::server::LegacyString;
constexpr BOOL TRUE=1,FALSE=0;
template<class A,class B> constexpr std::common_type_t<A,B> max(A a,B b) {
    using T=std::common_type_t<A,B>;return std::max(static_cast<T>(a),static_cast<T>(b));
}
// Windows CRT time64 is long long. On macOS int64_t is long, whose original
// packet overload deliberately writes only four bytes for Windows compatibility.
using __time64_t=long long;
#ifndef FOURSTORY_NATIVE_ODBC
using TIMESTAMP_STRUCT=fourstory::server::DatabaseTimestamp;
#endif
using CRITICAL_SECTION=std::recursive_mutex;
inline void EnterCriticalSection(CRITICAL_SECTION* section) {section->lock();}
inline void LeaveCriticalSection(CRITICAL_SECTION* section) {section->unlock();}
constexpr WORD HIWORD(DWORD value) {return static_cast<WORD>(value>>16);}
constexpr WORD LOWORD(DWORD value) {return static_cast<WORD>(value);}
constexpr BYTE HIBYTE(WORD value) {return static_cast<BYTE>(value>>8);}
constexpr BYTE LOBYTE(WORD value) {return static_cast<BYTE>(value);}
class CSession;
inline int lstrlen(const char* text) {
#ifdef FOURSTORY_NATIVE_MODULE
    // Win32 lstrlen(NULL) is 0; original module code relies on it.
    if(!text) return 0;
#endif
    if(!text) throw std::invalid_argument("Null packet string");
    const auto length=std::strlen(text);
    if(length>static_cast<std::size_t>(std::numeric_limits<int>::max()))
        throw std::length_error("Packet string too large");
    return static_cast<int>(length);
}
