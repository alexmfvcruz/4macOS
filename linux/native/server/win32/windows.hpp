#pragma once
// Native Win32 subset for compiling complete original server modules
// (FOURSTORY_NATIVE_MODULE). Only APIs used by the supplied server sources are
// provided. Semantics follow the Win32 contracts those sources depend on; see
// research/milestone-4.md for the documented differences.
#ifndef FOURSTORY_NATIVE_MODULE
#error "windows.hpp is only for FOURSTORY_NATIVE_MODULE builds"
#endif
#include "../odbc_platform.hpp"
#include <atomic>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <list>
#include <map>
#include <queue>
#include <set>
#include <string>
#include <vector>
#include <strings.h>
#include <unistd.h>

// CHAR, UCHAR, USHORT, UINT, ULONG, HWND, HINSTANCE, LPSTR, LPCSTR, LPDWORD,
// FAR and CALLBACK come from unixODBC sqltypes.h. Its ULONG is unsigned long
// (64-bit); the server sources use ULONG only in excluded minidump code.
using SHORT=std::int16_t;
using LONG=std::int32_t;
using LONGLONG=long long;
using ULONGLONG=unsigned long long;
using DWORD64=unsigned long long;
using UINT64=unsigned long long;
using INT32=std::int32_t;
using UINT32=std::uint32_t;
using INT16=std::int16_t;
using UINT16=std::uint16_t;
using INT8=std::int8_t;
using UINT8=std::uint8_t;
using LONG64=long long;
using DWORD_PTR=std::uintptr_t;
using ULONG_PTR=std::uintptr_t;
using UINT_PTR=std::uintptr_t;
using LONG_PTR=std::intptr_t;
using INT_PTR=std::intptr_t;
using PULONG_PTR=ULONG_PTR*;
using SIZE_T=std::size_t;
using HANDLE=void*;
using HMODULE=void*;
using HKEY=void*;
using PHKEY=HKEY*;
using HRESULT=std::int32_t;
using LRESULT=std::intptr_t;
using WPARAM=std::uintptr_t;
using LPARAM=std::intptr_t;
using PCHAR=char*;
using PDWORD=DWORD*;
using LPWORD=WORD*;
using LPINT=int*;
using LPLONG=LONG*;
using PBYTE=BYTE*;
using LPCVOID=const void*;
using PVOID=void*;
using FLOAT=float;
using LPFLOAT=float*;
using REGSAM=DWORD;
using ALG_ID=unsigned int;
using u_short=unsigned short;
#define VOID void
#define WINAPI
#define APIENTRY
#define __stdcall
#define __cdecl
#define __forceinline inline
#define NEAR
#define IN
#define OUT
#define OPTIONAL
#define UNREFERENCED_PARAMETER(P) ((void)(P))
#define _countof(array) (sizeof(array)/sizeof((array)[0]))
#define MAX_PATH 260
#define INFINITE 0xFFFFFFFFu
#define WAIT_OBJECT_0 0u
#define WAIT_ABANDONED 0x80u
#define WAIT_TIMEOUT 258u
#define WAIT_FAILED 0xFFFFFFFFu
#define S_OK ((HRESULT)0)
#define S_FALSE ((HRESULT)1)
#define E_FAIL ((HRESULT)0x80004005)
#define SUCCEEDED(hr) (((HRESULT)(hr))>=0)
#define FAILED(hr) (((HRESULT)(hr))<0)
#define ERROR_SUCCESS 0L
#define NO_ERROR 0L
#define ERROR_INVALID_HANDLE 6L
#define ERROR_FILE_NOT_FOUND 2L
#define ERROR_MORE_DATA 234L
#define ERROR_OPERATION_ABORTED 995L
#define ERROR_IO_PENDING 997L
#define ERROR_NETNAME_DELETED 64L
#define ERROR_CONNECTION_ABORTED 1236L
#define INVALID_HANDLE_VALUE (reinterpret_cast<HANDLE>(~std::uintptr_t(0)))
#define CREATE_SUSPENDED 0x4u
#define STILL_ACTIVE 259u
#define THREAD_PRIORITY_NORMAL 0
#define THREAD_PRIORITY_ABOVE_NORMAL 1
#define THREAD_PRIORITY_HIGHEST 2
#define THREAD_PRIORITY_BELOW_NORMAL (-1)
#define THREAD_PRIORITY_LOWEST (-2)
#define WM_QUIT 0x0012u
#define MAKEWORD(a,b) ((WORD)(((BYTE)(((DWORD_PTR)(a))&0xff))|((WORD)((BYTE)(((DWORD_PTR)(b))&0xff)))<<8))
#define MAKELONG(a,b) ((LONG)(((WORD)(((DWORD_PTR)(a))&0xffff))|((DWORD)((WORD)(((DWORD_PTR)(b))&0xffff)))<<16))
#define MAKELPARAM(l,h) ((LPARAM)(DWORD)MAKELONG(l,h))
#define RGB(r,g,b) ((DWORD)(((BYTE)(r)|((WORD)((BYTE)(g))<<8))|(((DWORD)(BYTE)(b))<<16)))
#define _MAX_PATH 260
#define _TRUNCATE ((std::size_t)-1)

template<class A,class B> constexpr std::common_type_t<A,B> min(A a,B b) {
    using T=std::common_type_t<A,B>;return std::min(static_cast<T>(a),static_cast<T>(b));
}

// ----------------------------------------------------------------------------
// Errors
namespace fourstory::win32 {
DWORD& last_error();
}
inline DWORD GetLastError() {return fourstory::win32::last_error();}
inline void SetLastError(DWORD value) {fourstory::win32::last_error()=value;}

// ----------------------------------------------------------------------------
// Memory and C runtime names
inline void CopyMemory(void* to,const void* from,std::size_t bytes) {std::memmove(to,from,bytes);}
inline void MoveMemory(void* to,const void* from,std::size_t bytes) {std::memmove(to,from,bytes);}
inline void FillMemory(void* to,std::size_t bytes,int value) {std::memset(to,value,bytes);}
inline int lstrcmp(const char* a,const char* b) {return std::strcmp(a?a:"",b?b:"");}
inline int lstrcmpi(const char* a,const char* b) {return strcasecmp(a?a:"",b?b:"");}
inline char* lstrcpy(char* to,const char* from) {return std::strcpy(to,from?from:"");}
inline char* lstrcat(char* to,const char* from) {return std::strcat(to,from?from:"");}
inline char* lstrcpyn(char* to,const char* from,int count) {
    if(count<=0) return to;
    std::size_t limit=static_cast<std::size_t>(count-1),length=0;
    if(from) while(length<limit && from[length]) ++length;
    if(length) std::memcpy(to,from,length);
    to[length]=0;return to;
}
inline int _stricmp(const char* a,const char* b) {return strcasecmp(a,b);}
inline int stricmp(const char* a,const char* b) {return strcasecmp(a,b);}
inline int _strnicmp(const char* a,const char* b,std::size_t n) {return strncasecmp(a,b,n);}
inline int strnicmp(const char* a,const char* b,std::size_t n) {return strncasecmp(a,b,n);}
inline int _mbsicmp(const unsigned char* a,const unsigned char* b) {
    return strcasecmp(reinterpret_cast<const char*>(a),reinterpret_cast<const char*>(b));
}
inline char* _strupr(char* text) {for(char* at=text;*at;++at) *at=static_cast<char>(std::toupper(static_cast<unsigned char>(*at)));return text;}
inline char* _strlwr(char* text) {for(char* at=text;*at;++at) *at=static_cast<char>(std::tolower(static_cast<unsigned char>(*at)));return text;}
inline long long _atoi64(const char* text) {return std::strtoll(text,nullptr,10);}
inline long long _strtoi64(const char* text,char** end,int base) {return std::strtoll(text,end,base);}
inline unsigned long long _strtoui64(const char* text,char** end,int base) {return std::strtoull(text,end,base);}
inline char* _itoa(int value,char* buffer,int radix) {
    if(radix==10) {std::snprintf(buffer,16,"%d",value);return buffer;}
    unsigned v=static_cast<unsigned>(value);char digits[40];int n=0;
    do {const unsigned d=v%static_cast<unsigned>(radix);digits[n++]=static_cast<char>(d<10?'0'+d:'a'+d-10);v/=static_cast<unsigned>(radix);} while(v);
    for(int i=0;i<n;++i) buffer[i]=digits[n-1-i];
    buffer[n]=0;return buffer;
}
inline char* itoa(int value,char* buffer,int radix) {return _itoa(value,buffer,radix);}
inline char* _ltoa(long value,char* buffer,int radix) {return _itoa(static_cast<int>(value),buffer,radix);}
inline char* _i64toa(long long value,char* buffer,int radix) {
    if(radix!=10) throw std::invalid_argument("Unsupported _i64toa radix");
    std::snprintf(buffer,24,"%lld",value);return buffer;
}
#define _vsnprintf vsnprintf
#define _tcslen strlen
#define _tcscpy strcpy
#define _tcsncpy strncpy
#define _tcscmp strcmp
#define _tcsicmp strcasecmp
#define _tcschr strchr
#define _tcsrchr strrchr
#define _tcsstr strstr
#define _ttoi atoi
#define _tstoi atoi
#define _tcstok strtok
#define _tWinMain WinMain
#define _vsntprintf vsnprintf

#define wsprintfA wsprintf
inline int strcpy_s(char* to,std::size_t size,const char* from) {
    if(!to || !size) return EINVAL;
    const auto length=std::strlen(from?from:"");
    if(length>=size) {to[0]=0;return ERANGE;}
    std::memcpy(to,from,length+1);return 0;
}
template<std::size_t N> int strcpy_s(char (&to)[N],const char* from) {return strcpy_s(to,N,from);}
inline int strncpy_s(char* to,std::size_t size,const char* from,std::size_t count) {
    if(!to || !size) return EINVAL;
    std::size_t length=0;const char* source=from?from:"";
    const auto limit=count==_TRUNCATE?size-1:count;
    while(length<limit && source[length]) ++length;
    if(length>=size) {to[0]=0;return ERANGE;}
    std::memcpy(to,source,length);to[length]=0;return 0;
}
template<std::size_t N> int strncpy_s(char (&to)[N],const char* from,std::size_t count) {return strncpy_s(to,N,from,count);}
inline int strcat_s(char* to,std::size_t size,const char* from) {
    const auto used=strnlen(to,size);
    if(used==size) return EINVAL;
    return strcpy_s(to+used,size-used,from);
}
template<std::size_t N> int strcat_s(char (&to)[N],const char* from) {return strcat_s(to,N,from);}
inline int _vsnprintf_s(char* buffer,std::size_t size,std::size_t,const char* format,va_list args) {
    return std::vsnprintf(buffer,size,format,args);
}
template<class T> void* SecureZeroMemory(T* pointer,std::size_t bytes) {
    volatile unsigned char* at=reinterpret_cast<volatile unsigned char*>(pointer);
    while(bytes--) *at++=0;
    return pointer;
}

// ----------------------------------------------------------------------------
// printf-family calls. ATL CString is pointer-sized and the original sources
// pass it directly as a %s argument; these templates pass its bytes instead.
namespace fourstory::win32 {
template<class T> decltype(auto) vararg(const T& value) {
    if constexpr(std::is_same_v<T,CString>) return static_cast<const char*>(value);
    else return value;
}
template<class... Args> int printf_into(char* buffer,std::size_t size,const char* format,const Args&... args) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
#pragma clang diagnostic ignored "-Wformat-security"
    return std::snprintf(buffer,size,format,vararg(args)...);
#pragma clang diagnostic pop
}
template<class... Args> std::string printf_string(const char* format,const Args&... args) {
    const int size=printf_into(nullptr,0,format,args...);
    if(size<0) return {};
    std::string text(static_cast<std::size_t>(size)+1,'\0');
    printf_into(text.data(),text.size(),format,args...);
    text.resize(static_cast<std::size_t>(size));return text;
}
}
// Unbounded legacy sprintf: callers supply Win32-sized buffers.
template<class... Args> int fourstory_sprintf(char* buffer,const char* format,const Args&... args) {
    const auto text=fourstory::win32::printf_string(format,args...);
    std::memcpy(buffer,text.c_str(),text.size()+1);return static_cast<int>(text.size());
}
template<class... Args> int fourstory_snprintf(char* buffer,std::size_t size,const char* format,const Args&... args) {
    return fourstory::win32::printf_into(buffer,size,format,args...);
}

// wsprintf output is limited to 1024 bytes by Win32.
template<class... Args> int wsprintf(char* buffer,const char* format,const Args&... args) {
    return fourstory::win32::printf_into(buffer,1024,format,args...);
}
#define _snprintf fourstory_snprintf
#define sprintf_s fourstory_snprintf
#define _stprintf fourstory_sprintf

// std::random_shuffle (removed in C++17) as MSVC implemented it, using rand().
template<class Iterator> void random_shuffle(Iterator first,Iterator last) {
    const auto count=last-first;
    for(decltype(last-first) i=1;i<count;++i) std::iter_swap(first+i,first+static_cast<decltype(i)>(std::rand()%(i+1)));
}

// ----------------------------------------------------------------------------
// Synchronization. CRITICAL_SECTION is a recursive mutex (packet_platform).
inline void InitializeCriticalSection(CRITICAL_SECTION*) {}
inline BOOL InitializeCriticalSectionAndSpinCount(CRITICAL_SECTION*,DWORD) {return TRUE;}
inline void DeleteCriticalSection(CRITICAL_SECTION*) {}
inline BOOL TryEnterCriticalSection(CRITICAL_SECTION* section) {return section->try_lock()?TRUE:FALSE;}

inline LONG InterlockedIncrement(volatile LONG* value) {return __atomic_add_fetch(value,1,__ATOMIC_SEQ_CST);}
inline LONG InterlockedDecrement(volatile LONG* value) {return __atomic_sub_fetch(value,1,__ATOMIC_SEQ_CST);}
inline DWORD InterlockedIncrement(volatile DWORD* value) {return __atomic_add_fetch(value,1u,__ATOMIC_SEQ_CST);}
inline DWORD InterlockedDecrement(volatile DWORD* value) {return __atomic_sub_fetch(value,1u,__ATOMIC_SEQ_CST);}
inline LONG InterlockedExchange(volatile LONG* target,LONG value) {return __atomic_exchange_n(target,value,__ATOMIC_SEQ_CST);}
inline DWORD InterlockedExchange(volatile DWORD* target,DWORD value) {return __atomic_exchange_n(target,value,__ATOMIC_SEQ_CST);}
inline LONG InterlockedExchangeAdd(volatile LONG* target,LONG value) {return __atomic_fetch_add(target,value,__ATOMIC_SEQ_CST);}
inline LONG InterlockedCompareExchange(volatile LONG* target,LONG exchange,LONG comparand) {
    __atomic_compare_exchange_n(target,&comparand,exchange,false,__ATOMIC_SEQ_CST,__ATOMIC_SEQ_CST);return comparand;
}

// ----------------------------------------------------------------------------
// Time
struct SYSTEMTIME {WORD wYear,wMonth,wDayOfWeek,wDay,wHour,wMinute,wSecond,wMilliseconds;};
using LPSYSTEMTIME=SYSTEMTIME*;
struct FILETIME {DWORD dwLowDateTime,dwHighDateTime;};
DWORD GetTickCount();
ULONGLONG GetTickCount64();
inline DWORD timeGetTime() {return GetTickCount();}
inline DWORD timeBeginPeriod(UINT) {return 0;}
inline DWORD timeEndPeriod(UINT) {return 0;}
void Sleep(DWORD milliseconds);
void GetLocalTime(LPSYSTEMTIME time);
void GetSystemTime(LPSYSTEMTIME time);
inline __time64_t _time64(__time64_t* out) {const __time64_t now=std::time(nullptr);if(out) *out=now;return now;}
inline std::tm* _localtime64(const __time64_t* value) {
    static thread_local std::tm result;const std::time_t t=static_cast<std::time_t>(*value);
    return localtime_r(&t,&result);
}
inline __time64_t _mktime64(std::tm* value) {return std::mktime(value);}

// ----------------------------------------------------------------------------
// Kernel handles: events, threads and completion ports share one handle space.
using LPTHREAD_START_ROUTINE=DWORD (*)(LPVOID);
using LPSECURITY_ATTRIBUTES=void*;
struct OVERLAPPED {
    ULONG_PTR Internal,InternalHigh;
    DWORD Offset,OffsetHigh;
    HANDLE hEvent;
};
using LPOVERLAPPED=OVERLAPPED*;
HANDLE CreateEvent(LPSECURITY_ATTRIBUTES,BOOL manual_reset,BOOL initial_state,LPCSTR name);
#define CreateEventA CreateEvent
BOOL SetEvent(HANDLE event);
BOOL ResetEvent(HANDLE event);
DWORD WaitForSingleObject(HANDLE handle,DWORD milliseconds);
DWORD WaitForMultipleObjects(DWORD count,const HANDLE* handles,BOOL wait_all,DWORD milliseconds);
BOOL CloseHandle(HANDLE handle);
HANDLE CreateThread(LPSECURITY_ATTRIBUTES,SIZE_T,LPTHREAD_START_ROUTINE start,LPVOID parameter,DWORD flags,LPDWORD thread_id);
DWORD ResumeThread(HANDLE thread);
DWORD SuspendThread(HANDLE thread);
BOOL GetExitCodeThread(HANDLE thread,LPDWORD code);
BOOL TerminateThread(HANDLE thread,DWORD code);
BOOL SetThreadPriority(HANDLE thread,int priority);
DWORD GetCurrentThreadId();
inline HANDLE GetCurrentThread() {return nullptr;}
inline DWORD GetCurrentProcessId() {return static_cast<DWORD>(getpid());}
BOOL PostThreadMessage(DWORD thread_id,UINT message,WPARAM,LPARAM);

HANDLE CreateIoCompletionPort(HANDLE file,HANDLE existing,ULONG_PTR key,DWORD threads);
BOOL GetQueuedCompletionStatus(HANDLE port,LPDWORD bytes,PULONG_PTR key,LPOVERLAPPED* overlapped,DWORD milliseconds);
// Original 32-bit sources pass DWORD completion keys.
BOOL GetQueuedCompletionStatus(HANDLE port,LPDWORD bytes,DWORD* key,LPOVERLAPPED* overlapped,DWORD milliseconds);
BOOL PostQueuedCompletionStatus(HANDLE port,DWORD bytes,ULONG_PTR key,LPOVERLAPPED overlapped);

struct SYSTEM_INFO {
    WORD wProcessorArchitecture,wReserved;DWORD dwPageSize;LPVOID lpMinimumApplicationAddress,lpMaximumApplicationAddress;
    DWORD_PTR dwActiveProcessorMask;DWORD dwNumberOfProcessors,dwProcessorType,dwAllocationGranularity;WORD wProcessorLevel,wProcessorRevision;
};
using LPSYSTEM_INFO=SYSTEM_INFO*;
void GetSystemInfo(LPSYSTEM_INFO info);
// ----------------------------------------------------------------------------
// Files (byte-oriented; paths are used as given, with '\\' mapped to '/').
#define GENERIC_READ 0x80000000u
#define GENERIC_WRITE 0x40000000u
#define FILE_SHARE_READ 0x1u
#define FILE_SHARE_WRITE 0x2u
#define CREATE_NEW 1u
#define CREATE_ALWAYS 2u
#define OPEN_EXISTING 3u
#define OPEN_ALWAYS 4u
#define TRUNCATE_EXISTING 5u
#define FILE_ATTRIBUTE_NORMAL 0x80u
#define FILE_ATTRIBUTE_DIRECTORY 0x10u
#define FILE_FLAG_RANDOM_ACCESS 0x10000000u
#define FILE_FLAG_SEQUENTIAL_SCAN 0x08000000u
#define FILE_BEGIN 0u
#define FILE_CURRENT 1u
#define FILE_END 2u
#define INVALID_SET_FILE_POINTER 0xFFFFFFFFu
#define INVALID_FILE_SIZE 0xFFFFFFFFu
#define INVALID_FILE_ATTRIBUTES 0xFFFFFFFFu
HANDLE CreateFile(LPCSTR path,DWORD access,DWORD share,LPSECURITY_ATTRIBUTES,DWORD disposition,DWORD flags,HANDLE);
#define CreateFileA CreateFile
BOOL ReadFile(HANDLE file,LPVOID buffer,DWORD bytes,LPDWORD read,LPOVERLAPPED);
BOOL WriteFile(HANDLE file,LPCVOID buffer,DWORD bytes,LPDWORD written,LPOVERLAPPED);
DWORD GetFileSize(HANDLE file,LPDWORD high);
DWORD SetFilePointer(HANDLE file,LONG distance,LONG* high,DWORD method);
BOOL FlushFileBuffers(HANDLE file);
BOOL DeleteFile(LPCSTR path);
BOOL CreateDirectory(LPCSTR path,LPSECURITY_ATTRIBUTES);
DWORD GetFileAttributes(LPCSTR path);
inline BOOL IsDebuggerPresent() {return FALSE;}
// The servers pass Win32 paths (backslashes) to the C runtime as well.
FILE* fourstory_fopen(const char* path,const char* mode);
#define fopen fourstory_fopen
// shlwapi PathRemoveFileSpec: strip the last path component (either separator).
inline BOOL PathRemoveFileSpec(LPSTR path) {
    if(!path) return FALSE;
    char* slash=std::strrchr(path,'/');char* back=std::strrchr(path,'\\');
    char* cut=back>slash?back:slash;
    if(!cut) return FALSE;
    *cut=0;return TRUE;
}
#define PathRemoveFileSpecA PathRemoveFileSpec
#define ATLTRACE2(...) ((void)0)

// ----------------------------------------------------------------------------
// Process/module information and diagnostics
DWORD GetModuleFileName(HMODULE,LPSTR path,DWORD size);
#define GetModuleFileNameA GetModuleFileName
DWORD GetCurrentDirectory(DWORD size,LPSTR path);
void OutputDebugString(LPCSTR text);
#define OutputDebugStringA OutputDebugString
inline HMODULE GetModuleHandle(LPCSTR) {return nullptr;}
inline void DebugBreak() {}
#define _ASSERT(expr) ((void)0)
#define ASSERT(expr) ((void)0)
#define ATLASSERT(expr) ((void)0)
#define ATLTRACE(...) ((void)0)
#define TRACE(...) ((void)0)
#define _ASSERTE(expr) ((void)0)

// ----------------------------------------------------------------------------
// Registry replacement backed by the file named in FOURSTORY_REGISTRY.
#define HKEY_LOCAL_MACHINE (reinterpret_cast<HKEY>(std::uintptr_t(0x80000002)))
#define HKEY_CURRENT_USER (reinterpret_cast<HKEY>(std::uintptr_t(0x80000001)))
#define REG_NONE 0u
#define REG_SZ 1u
#define REG_BINARY 3u
#define REG_DWORD 4u
#define KEY_READ 0x20019u
#define KEY_WRITE 0x20006u
#define KEY_ALL_ACCESS 0xF003Fu
#define KEY_QUERY_VALUE 0x0001u
#define KEY_SET_VALUE 0x0002u
#define REG_OPTION_NON_VOLATILE 0u
LONG RegCreateKey(HKEY root,LPCSTR path,PHKEY result);
LONG RegCreateKeyEx(HKEY root,LPCSTR path,DWORD,LPSTR,DWORD,REGSAM,LPSECURITY_ATTRIBUTES,PHKEY result,LPDWORD disposition);
LONG RegOpenKeyEx(HKEY root,LPCSTR path,DWORD,REGSAM,PHKEY result);
LONG RegOpenKey(HKEY root,LPCSTR path,PHKEY result);
LONG RegQueryValueEx(HKEY key,LPCSTR name,LPDWORD,LPDWORD type,LPBYTE data,LPDWORD size);
LONG RegSetValueEx(HKEY key,LPCSTR name,DWORD,DWORD type,const BYTE* data,DWORD size);
LONG RegDeleteValue(HKEY key,LPCSTR name);
LONG RegCloseKey(HKEY key);
#define RegCreateKeyA RegCreateKey
#define RegQueryValueExA RegQueryValueEx
#define RegOpenKeyExA RegOpenKeyEx
#define RegSetValueExA RegSetValueEx

// ----------------------------------------------------------------------------
// 32-bit handles for object pointers that original code passes through DWORD
// fields of in-process packets (the Map AI delayed-command chain). Handles are
// never reused; registered objects must outlive their pending messages.
namespace fourstory::win32 {
DWORD pointer_handle(void* pointer);
void* handle_pointer(DWORD handle);
}

// ----------------------------------------------------------------------------
// Service stop requests (ATL message loop replacement).
namespace fourstory::win32 {
// Startup readiness: roles whose worker threads finish initialization after
// OnEnter (Login opens per-worker databases) register the expected count.
void expect_ready_workers(unsigned count);
void worker_ready();
bool wait_ready_workers(DWORD milliseconds);
// Values of the form env:NAME resolve from the environment (database secrets
// stored in configuration tables); other values are returned unchanged.
std::string resolve_secret(const char* value);
void request_stop() noexcept;
bool stop_requested() noexcept;
// FOURSTORY_PACKET_TRACE=1 makes Login and Map log each client packet they
// dispatch (native-only lines in their receive paths).
inline bool packet_trace() {
    static const bool enabled=[]{const char* value=std::getenv("FOURSTORY_PACKET_TRACE");return value && *value && *value!='0';}();
    return enabled;
}
void wait_for_stop();
}

#include "winsock.hpp"
