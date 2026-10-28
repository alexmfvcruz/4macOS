// Native replacement for <windows.h>, for the original client libraries
// (FOURSTORY_CLIENT_NATIVE). Only the names that the client sources use are
// provided. Widths follow Win32 on x64: LONG, ULONG and DWORD are 32 bits,
// pointer-sized types follow the pointer. See native/client/compat/README.md.
#pragma once
#ifndef FOURSTORY_CLIENT_NATIVE
#error "native/client/compat is only for FOURSTORY_CLIENT_NATIVE builds"
#endif
#include <cerrno>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <dirent.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

// ---- Scalar types -----------------------------------------------------------
typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned int DWORD;
typedef int BOOL;
typedef int INT;
typedef unsigned int UINT;
typedef int LONG;
typedef unsigned int ULONG;
typedef short SHORT;
typedef unsigned short USHORT;
typedef char CHAR;
typedef unsigned char UCHAR;
typedef float FLOAT;
typedef double DOUBLE;
typedef long long LONGLONG;
typedef unsigned long long ULONGLONG;
typedef long long INT64;
typedef unsigned long long UINT64;
typedef long long LONG64;
typedef unsigned long long ULONG64;
typedef unsigned long long DWORD64;
typedef int INT32;
typedef unsigned int UINT32;
typedef short INT16;
typedef unsigned short UINT16;
typedef signed char INT8;
typedef unsigned char UINT8;
// Pointer-sized integers are (unsigned) long long, as on Windows x64, and not
// (unsigned) long: CArchive stores the C type long as 32 bits, and these
// types must keep 64 bits there. std::size_t is unsigned long on this target
// and therefore goes through CArchive as 32 bits (README, "Type widths").
typedef unsigned long long DWORD_PTR;
typedef unsigned long long ULONG_PTR;
typedef unsigned long long UINT_PTR;
typedef long long LONG_PTR;
typedef long long INT_PTR;
typedef unsigned long long SIZE_T;
typedef long long LRESULT;
typedef unsigned long long WPARAM;
typedef long long LPARAM;
static_assert(sizeof(DWORD_PTR) == sizeof(void*) && sizeof(long long) == 8 && sizeof(int) == 4, "LP64 target required");
typedef int HRESULT;
typedef DWORD COLORREF;
typedef char TCHAR;
typedef char16_t WCHAR; // 16 bits as on Windows; wchar_t is 32 bits here.
// The client sources use __int64 as a type name and as a function-style cast,
// "__int64(value)". A typedef name supports both; the macro also replaces the
// Clang keyword of -fms-extensions, which does not support the cast. The form
// "unsigned __int64" is not supported (no use in Client, Engine Lib, TComp,
// TChart or TCML; write UINT64 or ULONGLONG).
typedef long long fourstory_compat_int64;
typedef int fourstory_compat_int32;
typedef short fourstory_compat_int16;
typedef char fourstory_compat_int8;
#define __int64 fourstory_compat_int64
#define __int32 fourstory_compat_int32
#define __int16 fourstory_compat_int16
#define __int8 fourstory_compat_int8
typedef long long __time64_t;

typedef BYTE* LPBYTE;
typedef BYTE* PBYTE;
typedef WORD* LPWORD;
typedef WORD* PWORD;
typedef DWORD* LPDWORD;
typedef DWORD* PDWORD;
typedef INT64* PINT64;
typedef UINT64* PUINT64;
typedef LONGLONG* PLONGLONG;
typedef ULONGLONG* PULONGLONG;
typedef int* LPINT;
typedef LONG* LPLONG;
typedef BOOL* LPBOOL;
typedef FLOAT* LPFLOAT;
typedef void* LPVOID;
typedef void* PVOID;
typedef const void* LPCVOID;
typedef char* LPSTR;
typedef char* PSTR;
typedef char* PCHAR;
typedef const char* LPCSTR;
typedef const char* PCSTR;
typedef char* LPTSTR;
typedef const char* LPCTSTR;
typedef WCHAR* LPWSTR;
typedef const WCHAR* LPCWSTR;

typedef void* HANDLE;
typedef HANDLE* LPHANDLE;
#define FOURSTORY_COMPAT_HANDLE(name) struct name##__ { int unused; }; typedef struct name##__* name
FOURSTORY_COMPAT_HANDLE(HWND);
FOURSTORY_COMPAT_HANDLE(HINSTANCE);
FOURSTORY_COMPAT_HANDLE(HDC);
FOURSTORY_COMPAT_HANDLE(HFONT);
FOURSTORY_COMPAT_HANDLE(HBITMAP);
FOURSTORY_COMPAT_HANDLE(HICON);
FOURSTORY_COMPAT_HANDLE(HMENU);
FOURSTORY_COMPAT_HANDLE(HKEY);
typedef HINSTANCE HMODULE;
typedef HICON HCURSOR;

#define VOID void
#define CONST const
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#define WINAPI
#define CALLBACK
#define APIENTRY
#define PASCAL
#define FAR
#define NEAR
#define IN
#define OUT
#define OPTIONAL
#define __stdcall
#define __cdecl
#define __fastcall
#define __forceinline inline
#define UNREFERENCED_PARAMETER(P) ((void)(P))
#define _T(value) value
#define TEXT(value) value
#define MAX_PATH 260
#define _MAX_PATH 260
#define _MAX_DRIVE 3
#define _MAX_DIR 256
#define _MAX_FNAME 256
#define _MAX_EXT 256
#define INFINITE 0xFFFFFFFFu
#define INVALID_HANDLE_VALUE (reinterpret_cast<HANDLE>(~std::uintptr_t(0)))
#define _countof(array) (sizeof(array)/sizeof((array)[0]))

#define S_OK ((HRESULT)0)
#define S_FALSE ((HRESULT)1)
#define E_FAIL ((HRESULT)0x80004005)
#define E_INVALIDARG ((HRESULT)0x80070057)
#define E_OUTOFMEMORY ((HRESULT)0x8007000E)
#define SUCCEEDED(hr) (((HRESULT)(hr))>=0)
#define FAILED(hr) (((HRESULT)(hr))<0)

#define LOBYTE(w) ((BYTE)(((DWORD_PTR)(w))&0xff))
#define HIBYTE(w) ((BYTE)((((DWORD_PTR)(w))>>8)&0xff))
#define LOWORD(l) ((WORD)(((DWORD_PTR)(l))&0xffff))
#define HIWORD(l) ((WORD)((((DWORD_PTR)(l))>>16)&0xffff))
#define MAKEWORD(a,b) ((WORD)(((BYTE)(((DWORD_PTR)(a))&0xff))|((WORD)((BYTE)(((DWORD_PTR)(b))&0xff)))<<8))
#define MAKELONG(a,b) ((LONG)(((WORD)(((DWORD_PTR)(a))&0xffff))|((DWORD)((WORD)(((DWORD_PTR)(b))&0xffff)))<<16))
#define MAKEWPARAM(l,h) ((WPARAM)(DWORD)MAKELONG(l,h))
#define MAKELPARAM(l,h) ((LPARAM)(DWORD)MAKELONG(l,h))
#define RGB(r,g,b) ((COLORREF)(((BYTE)(r)|((WORD)((BYTE)(g))<<8))|(((DWORD)(BYTE)(b))<<16)))
#define GetRValue(rgb) (LOBYTE(rgb))
#define GetGValue(rgb) (LOBYTE(((WORD)(rgb))>>8))
#define GetBValue(rgb) (LOBYTE((rgb)>>16))
#define __min(a,b) (((a)<(b))?(a):(b))
#define __max(a,b) (((a)>(b))?(a):(b))

struct POINT { LONG x, y; };
struct SIZE { LONG cx, cy; };
struct RECT { LONG left, top, right, bottom; };
typedef POINT* LPPOINT;
typedef SIZE* LPSIZE;
typedef RECT* LPRECT;
typedef const RECT* LPCRECT;
struct SYSTEMTIME { WORD wYear, wMonth, wDayOfWeek, wDay, wHour, wMinute, wSecond, wMilliseconds; };
struct FILETIME { DWORD dwLowDateTime, dwHighDateTime; };
union LARGE_INTEGER { struct { DWORD LowPart; LONG HighPart; }; LONGLONG QuadPart; };
union ULARGE_INTEGER { struct { DWORD LowPart; DWORD HighPart; }; ULONGLONG QuadPart; };

// ---- Memory helpers ---------------------------------------------------------
inline void ZeroMemory(void* pointer, std::size_t bytes) { std::memset(pointer, 0, bytes); }
inline void CopyMemory(void* to, const void* from, std::size_t bytes) { std::memcpy(to, from, bytes); }
inline void MoveMemory(void* to, const void* from, std::size_t bytes) { std::memmove(to, from, bytes); }
inline void FillMemory(void* to, std::size_t bytes, BYTE value) { std::memset(to, value, bytes); }

// ---- printf-style argument adapter -----------------------------------------
// MFC passes CString objects through "..."; the object is one pointer to its
// text, so printf reads it as "%s". A C++ object cannot pass through "..." in
// Clang. Every printf-style entry point of this layer is a template that
// converts CString arguments to const char* first.
class CString;
const char* fourstory_compat_cstr(const CString& text);
namespace fourstory { namespace compat {
template<class T> inline const T& printf_arg(const T& value) { return value; }
inline const char* printf_arg(const CString& value) { return fourstory_compat_cstr(value); }
// Converts a Microsoft format string to one with the same meaning here:
// "%ld" and the other integer conversions with one "l" read a 32-bit LONG on
// Windows, so the "l" is removed; "I64" becomes "ll", "I32" is removed and a
// single "I" (pointer size) becomes "ll". Other text is copied.
inline std::string printf_format(const char* format) {
    std::string out;
    if (!format) return out;
    for (const char* p = format; *p; ) {
        if (*p != '%') { out += *p++; continue; }
        out += *p++;
        if (*p == '%') { out += *p++; continue; }
        while (*p && std::strchr("-+ #0", *p)) out += *p++;
        while ((*p >= '0' && *p <= '9') || *p == '*') out += *p++;
        if (*p == '.') { out += *p++; while ((*p >= '0' && *p <= '9') || *p == '*') out += *p++; }
        if (p[0] == 'I' && p[1] == '6' && p[2] == '4') { out += "ll"; p += 3; }
        else if (p[0] == 'I' && p[1] == '3' && p[2] == '2') p += 3;
        else if (p[0] == 'I' && p[1] && std::strchr("diouxX", p[1])) { out += "ll"; p += 1; }
        else if (p[0] == 'l' && p[1] && std::strchr("diouxX", p[1])) p += 1;
        else while (*p && std::strchr("hlLjzt", *p)) out += *p++;
        if (*p) out += *p++;
    }
    return out;
}
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-security"
#pragma clang diagnostic ignored "-Wformat-nonliteral"
#endif
template<class... Args> inline int snprintf_args(char* to, std::size_t size, const char* format, const Args&... args) {
    return std::snprintf(to, size, printf_format(format).c_str(), printf_arg(args)...);
}
inline int vsnprintf_args(char* to, std::size_t size, const char* format, va_list args) {
    return std::vsnprintf(to, size, printf_format(format).c_str(), args);
}
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
}}

// ---- Microsoft CRT names ----------------------------------------------------
// The secure functions return 0 on success and an errno value on failure. On
// failure the destination holds an empty string, as in the Microsoft CRT. The
// invalid-parameter handler (process termination by default) is not emulated.
#define _TRUNCATE ((std::size_t)-1)
typedef int errno_t;
inline errno_t memcpy_s(void* to, std::size_t size, const void* from, std::size_t count) {
    if (!count) return 0;
    if (!to) return EINVAL;
    if (!from || size < count) { std::memset(to, 0, size); return from ? ERANGE : EINVAL; }
    std::memcpy(to, from, count); return 0;
}
inline errno_t memmove_s(void* to, std::size_t size, const void* from, std::size_t count) {
    if (!count) return 0;
    if (!to || !from) return EINVAL;
    if (size < count) return ERANGE;
    std::memmove(to, from, count); return 0;
}
inline errno_t strcpy_s(char* to, std::size_t size, const char* from) {
    if (!to || !size) return EINVAL;
    if (!from) { to[0] = 0; return EINVAL; }
    const std::size_t length = std::strlen(from);
    if (length >= size) { to[0] = 0; return ERANGE; }
    std::memcpy(to, from, length + 1); return 0;
}
template<std::size_t N> inline errno_t strcpy_s(char (&to)[N], const char* from) { return strcpy_s(to, N, from); }
inline errno_t strncpy_s(char* to, std::size_t size, const char* from, std::size_t count) {
    if (!to || !size) return EINVAL;
    if (!from) { to[0] = 0; return EINVAL; }
    std::size_t length = 0;
    while (length < count && from[length]) ++length;
    if (length >= size) {
        if (count != _TRUNCATE) { to[0] = 0; return ERANGE; }
        length = size - 1;
        std::memcpy(to, from, length); to[length] = 0; return 80; // STRUNCATE
    }
    std::memcpy(to, from, length); to[length] = 0; return 0;
}
template<std::size_t N> inline errno_t strncpy_s(char (&to)[N], const char* from, std::size_t count) { return strncpy_s(to, N, from, count); }
inline errno_t strcat_s(char* to, std::size_t size, const char* from) {
    if (!to || !size) return EINVAL;
    const std::size_t used = strnlen(to, size);
    if (used == size) { to[0] = 0; return EINVAL; }
    if (!from) { to[0] = 0; return EINVAL; }
    const std::size_t length = std::strlen(from);
    if (length >= size - used) { to[0] = 0; return ERANGE; }
    std::memcpy(to + used, from, length + 1); return 0;
}
template<std::size_t N> inline errno_t strcat_s(char (&to)[N], const char* from) { return strcat_s(to, N, from); }
template<class... Args> inline int sprintf_s(char* to, std::size_t size, const char* format, const Args&... args) {
    if (!to || !size) return -1;
    const int length = fourstory::compat::snprintf_args(to, size, format, args...);
    if (length < 0 || static_cast<std::size_t>(length) >= size) { to[0] = 0; return -1; }
    return length;
}
template<std::size_t N, class... Args> inline int sprintf_s(char (&to)[N], const char* format, const Args&... args) {
    return sprintf_s(static_cast<char*>(to), N, format, args...);
}
// _snprintf does not write a terminator when the text fills the buffer.
template<class... Args> inline int _snprintf(char* to, std::size_t size, const char* format, const Args&... args) {
    const int length = fourstory::compat::snprintf_args(nullptr, 0, format, args...);
    if (length < 0) return -1;
    if (static_cast<std::size_t>(length) < size) return fourstory::compat::snprintf_args(to, size, format, args...);
    char* whole = static_cast<char*>(std::malloc(static_cast<std::size_t>(length) + 1));
    if (!whole) return -1;
    fourstory::compat::snprintf_args(whole, static_cast<std::size_t>(length) + 1, format, args...);
    std::memcpy(to, whole, size); std::free(whole);
    return static_cast<std::size_t>(length) == size ? length : -1;
}
inline int vsprintf_s(char* to, std::size_t size, const char* format, va_list args) {
    if (!to || !size) return -1;
    const int length = fourstory::compat::vsnprintf_args(to, size, format, args);
    if (length < 0 || static_cast<std::size_t>(length) >= size) { to[0] = 0; return -1; }
    return length;
}
inline int _vsnprintf(char* to, std::size_t size, const char* format, va_list args) {
    va_list copy; va_copy(copy, args);
    const int length = fourstory::compat::vsnprintf_args(nullptr, 0, format, copy); va_end(copy);
    if (length < 0) return -1;
    if (static_cast<std::size_t>(length) < size) return fourstory::compat::vsnprintf_args(to, size, format, args);
    char* whole = static_cast<char*>(std::malloc(static_cast<std::size_t>(length) + 1));
    if (!whole) return -1;
    fourstory::compat::vsnprintf_args(whole, static_cast<std::size_t>(length) + 1, format, args);
    std::memcpy(to, whole, size); std::free(whole);
    return static_cast<std::size_t>(length) == size ? length : -1;
}
inline int _vscprintf(const char* format, va_list args) {
    va_list copy; va_copy(copy, args);
    const int length = fourstory::compat::vsnprintf_args(nullptr, 0, format, copy); va_end(copy);
    return length;
}
inline int _stricmp(const char* a, const char* b) { return strcasecmp(a, b); }
inline int _strnicmp(const char* a, const char* b, std::size_t count) { return strncasecmp(a, b, count); }
inline int stricmp(const char* a, const char* b) { return strcasecmp(a, b); }
inline int strnicmp(const char* a, const char* b, std::size_t count) { return strncasecmp(a, b, count); }
// Low-level file functions that the Microsoft CRT declares in <stdio.h> and <io.h>.
#define _fileno fileno
#define _isatty isatty
#define _open open
#define _close close
#define _read read
#define _write write
#define _lseek lseek
// Win32 byte-string functions. A null pointer is an empty string, as in Win32.
inline int lstrlen(const char* text) { return text ? static_cast<int>(std::strlen(text)) : 0; }
inline int lstrcmp(const char* a, const char* b) { return std::strcmp(a ? a : "", b ? b : ""); }
inline int lstrcmpi(const char* a, const char* b) { return strcasecmp(a ? a : "", b ? b : ""); }
inline char* lstrcpy(char* to, const char* from) { return std::strcpy(to, from ? from : ""); }
inline char* lstrcat(char* to, const char* from) { return std::strcat(to, from ? from : ""); }
inline long long _atoi64(const char* text) { return std::strtoll(text, nullptr, 10); }
inline int _ttoi(const char* text) { return std::atoi(text); }

// ---- File names ---------------------------------------------------------------
// Windows file names use '\\' and ignore case. native_path returns a path that
// open() can use: separators converted, and each component matched without
// regard to case when the exact name does not exist. A path that cannot be
// resolved is returned with converted separators (for a file to be created).
namespace fourstory { namespace compat {
inline char* native_path(const char* windows_path, char* out, std::size_t size) {
    if (!out || !size) return out;
    out[0] = 0;
    if (!windows_path) return out;
    std::string path(windows_path);
    for (std::size_t i = 0; i < path.size(); ++i) if (path[i] == '\\') path[i] = '/';
    struct stat info;
    if (stat(path.c_str(), &info) != 0) {
        // Match each component without regard to case. Components after the
        // first one that has no match are appended unchanged, so that a new
        // file can be created in a directory whose name differs in case.
        std::string resolved = path[0] == '/' ? "/" : "";
        std::size_t position = path[0] == '/' ? 1 : 0;
        bool matching = true;
        while (position < path.size()) {
            std::size_t next = path.find('/', position);
            if (next == std::string::npos) next = path.size();
            const std::string name = path.substr(position, next - position);
            position = next + 1;
            if (name.empty() || name == ".") continue;
            std::string found = name;
            if (matching && name != ".." && stat((resolved + name).c_str(), &info) != 0) {
                matching = false;
                if (DIR* directory = opendir(resolved.empty() ? "." : resolved.c_str())) {
                    while (dirent* entry = readdir(directory)) {
                        if (strcasecmp(entry->d_name, name.c_str()) == 0) { found = entry->d_name; matching = true; break; }
                    }
                    closedir(directory);
                }
            }
            resolved += found;
            if (next < path.size()) resolved += "/";
        }
        if (!resolved.empty()) path = resolved;
    }
    std::strncpy(out, path.c_str(), size - 1); out[size - 1] = 0;
    return out;
}
inline std::string native_path(const char* windows_path) {
    char buffer[4096];
    return std::string(native_path(windows_path, buffer, sizeof(buffer)));
}
}}
inline errno_t fopen_s(FILE** file, const char* path, const char* mode) {
    if (!file) return EINVAL;
    *file = nullptr;
    if (!path || !mode) return EINVAL;
    *file = std::fopen(fourstory::compat::native_path(path).c_str(), mode);
    return *file ? 0 : errno;
}
// fopen with the same path rules (C4): the client opens Windows paths such as
// ".\\Intro\\IntroList.txt" with fopen (TClientWnd.cpp:3954). The macro
// applies to the code after this header; std::fopen names the same function.
inline FILE* fourstory_fopen(const char* path, const char* mode) {
    if (!path || !mode) { errno = EINVAL; return nullptr; }
    return std::fopen(fourstory::compat::native_path(path).c_str(), mode);
}
namespace std { using ::fourstory_fopen; }
#define fopen fourstory_fopen

// ---- min and max --------------------------------------------------------------
// On Windows min and max are macros, ((a)<(b)?(a):(b)), so they accept two
// different arithmetic types and give the common type. With "using namespace
// std" std::min/std::max cover equal types; these templates take only the
// mixed case, so the two never compete.
#include <type_traits>
template<class A, class B, class = typename std::enable_if<!std::is_same<A, B>::value &&
         (std::is_arithmetic<A>::value || std::is_enum<A>::value) && (std::is_arithmetic<B>::value || std::is_enum<B>::value)>::type>
inline auto min(A a, B b) -> decltype(a < b ? a : b) { return a < b ? a : b; }
template<class A, class B, class = typename std::enable_if<!std::is_same<A, B>::value &&
         (std::is_arithmetic<A>::value || std::is_enum<A>::value) && (std::is_arithmetic<B>::value || std::is_enum<B>::value)>::type>
inline auto max(A a, B b) -> decltype(a > b ? a : b) { return a > b ? a : b; }

// MulDiv: (number * numerator) / denominator with a 64-bit product, rounded to
// the nearest integer (halves away from zero); -1 when the denominator is 0
// or the result does not fit in 32 bits, as in Win32.
inline int MulDiv(int number, int numerator, int denominator) {
    if (!denominator) return -1;
    const long long product = (long long)number * numerator;
    const long long magnitude = ((product < 0 ? -product : product) + (denominator < 0 ? -(long long)denominator : denominator) / 2) /
                                (denominator < 0 ? -(long long)denominator : denominator);
    const long long result = ((product < 0) != (denominator < 0)) ? -magnitude : magnitude;
    if (result > 0x7fffffffLL || result < -0x80000000LL) return -1;
    return int(result);
}

// ---- Time -------------------------------------------------------------------
// Milliseconds from a monotonic clock, truncated to 32 bits as on Windows.
inline DWORD GetTickCount() {
    timespec now{}; clock_gettime(CLOCK_MONOTONIC, &now);
    return static_cast<DWORD>(static_cast<unsigned long long>(now.tv_sec) * 1000ull + static_cast<unsigned long long>(now.tv_nsec) / 1000000ull);
}
inline DWORD timeGetTime() { return GetTickCount(); }
// Sleep(INFINITE) waits about 49.7 days here; Windows never returns.
inline void Sleep(DWORD milliseconds) {
    timespec left{static_cast<time_t>(milliseconds / 1000u), static_cast<long>(milliseconds % 1000u) * 1000000L};
    while (nanosleep(&left, &left) != 0 && errno == EINTR) {}
}
inline void OutputDebugString(const char*) {}
inline void OutputDebugStringA(const char*) {}

// ---- Last error -------------------------------------------------------------
// Win32 keeps one error code per thread. Here a failed POSIX call leaves its
// code in errno, and GetLastError() maps errno to the Win32/Winsock code, so
// GetLastError() and WSAGetLastError() work directly after connect, send or
// recv, as on Windows. SetLastError() stores a Win32 code (used by this layer,
// for example by the CryptoAPI functions) and marks errno with a tag; a later
// failing POSIX call replaces the tag. Codes without a Win32 equivalent are
// returned as 0x20000000 | errno (the customer bit of Win32 error codes).
#define ERROR_SUCCESS 0L
#define NO_ERROR 0L
#define ERROR_FILE_NOT_FOUND 2L
#define ERROR_PATH_NOT_FOUND 3L
#define ERROR_ACCESS_DENIED 5L
#define ERROR_INVALID_HANDLE 6L
#define ERROR_NOT_ENOUGH_MEMORY 8L
#define ERROR_INVALID_PARAMETER 87L
#define ERROR_BUFFER_OVERFLOW 111L
#define ERROR_INSUFFICIENT_BUFFER 122L
#define ERROR_MORE_DATA 234L
#define ERROR_NOT_SUPPORTED 50L
namespace fourstory { namespace compat {
enum : int { last_error_tag = 0x46534C45 }; // errno value that no POSIX call sets
inline DWORD& stored_last_error() { static thread_local DWORD value = 0; return value; }
inline DWORD errno_to_win32(int code) {
    switch (code) {
    case 0: return 0;
    case ENOENT: return ERROR_FILE_NOT_FOUND;
    case ENOTDIR: return ERROR_PATH_NOT_FOUND;
    case EACCES: case EPERM: return ERROR_ACCESS_DENIED;
    case ENOMEM: return ERROR_NOT_ENOUGH_MEMORY;
    case EINTR: return 10004;          // WSAEINTR
    case EBADF: case ENOTSOCK: return 10038; // WSAENOTSOCK
    case EFAULT: return 10014;         // WSAEFAULT
    case EINVAL: return 10022;         // WSAEINVAL
    case EMFILE: return 10024;         // WSAEMFILE
    case EAGAIN: case EINPROGRESS: return 10035; // WSAEWOULDBLOCK (non-blocking connect too)
    case EALREADY: return 10037;       // WSAEALREADY
    case EDESTADDRREQ: return 10039;   // WSAEDESTADDRREQ
    case EMSGSIZE: return 10040;       // WSAEMSGSIZE
    case EPROTOTYPE: return 10041;     // WSAEPROTOTYPE
    case ENOPROTOOPT: return 10042;    // WSAENOPROTOOPT
    case EPROTONOSUPPORT: return 10043;// WSAEPROTONOSUPPORT
    case EOPNOTSUPP: return 10045;     // WSAEOPNOTSUPP
    case EAFNOSUPPORT: return 10047;   // WSAEAFNOSUPPORT
    case EADDRINUSE: return 10048;     // WSAEADDRINUSE
    case EADDRNOTAVAIL: return 10049;  // WSAEADDRNOTAVAIL
    case ENETDOWN: return 10050;       // WSAENETDOWN
    case ENETUNREACH: return 10051;    // WSAENETUNREACH
    case ECONNABORTED: return 10053;   // WSAECONNABORTED
    case ECONNRESET: return 10054;     // WSAECONNRESET
    case ENOBUFS: return 10055;        // WSAENOBUFS
    case EISCONN: return 10056;        // WSAEISCONN
    case ENOTCONN: return 10057;       // WSAENOTCONN
    case EPIPE: case ESHUTDOWN: return 10058; // WSAESHUTDOWN
    case ETIMEDOUT: return 10060;      // WSAETIMEDOUT
    case ECONNREFUSED: return 10061;   // WSAECONNREFUSED
    case EHOSTDOWN: return 10064;      // WSAEHOSTDOWN
    case EHOSTUNREACH: return 10065;   // WSAEHOSTUNREACH
    default: return 0x20000000u | static_cast<DWORD>(code);
    }
}
}}
inline void SetLastError(DWORD code) { fourstory::compat::stored_last_error() = code; errno = fourstory::compat::last_error_tag; }
inline DWORD GetLastError() {
    return errno == fourstory::compat::last_error_tag ? fourstory::compat::stored_last_error() : fourstory::compat::errno_to_win32(errno);
}

// ---- Messages and local memory --------------------------------------------
#define LANG_NEUTRAL 0x00
#define SUBLANG_DEFAULT 0x01
#define MAKELANGID(p, s) ((((WORD)(s)) << 10) | (WORD)(p))
#define FORMAT_MESSAGE_ALLOCATE_BUFFER 0x00000100
#define FORMAT_MESSAGE_IGNORE_INSERTS 0x00000200
#define FORMAT_MESSAGE_FROM_STRING 0x00000400
#define FORMAT_MESSAGE_FROM_SYSTEM 0x00001000
typedef HANDLE HLOCAL;
#define LMEM_FIXED 0x0000
#define LMEM_ZEROINIT 0x0040
#define LPTR (LMEM_FIXED | LMEM_ZEROINIT)
inline HLOCAL LocalAlloc(UINT flags, SIZE_T bytes) {
    void* memory = (flags & LMEM_ZEROINIT) ? std::calloc(1, bytes ? bytes : 1) : std::malloc(bytes ? bytes : 1);
    if (!memory) SetLastError(ERROR_NOT_ENOUGH_MEMORY);
    return memory;
}
inline HLOCAL LocalFree(HLOCAL memory) { std::free(memory); return nullptr; }
// Only FORMAT_MESSAGE_FROM_SYSTEM is supported. The text is "Error <code>."
// followed by CR LF, not the localized Windows text. Inserts are not expanded.
inline DWORD FormatMessage(DWORD flags, LPCVOID, DWORD code, DWORD, LPSTR buffer, DWORD size, va_list*) {
    if (!(flags & FORMAT_MESSAGE_FROM_SYSTEM) || !buffer) { SetLastError(ERROR_INVALID_PARAMETER); return 0; }
    char text[64];
    const int length = std::snprintf(text, sizeof(text), "Error %u.\r\n", code);
    if (flags & FORMAT_MESSAGE_ALLOCATE_BUFFER) {
        char* memory = static_cast<char*>(LocalAlloc(LMEM_FIXED, static_cast<SIZE_T>(length) + 1));
        if (!memory) return 0;
        std::memcpy(memory, text, static_cast<std::size_t>(length) + 1);
        *reinterpret_cast<char**>(buffer) = memory;
        return static_cast<DWORD>(length);
    }
    if (size <= static_cast<DWORD>(length)) { SetLastError(ERROR_INSUFFICIENT_BUFFER); return 0; }
    std::memcpy(buffer, text, static_cast<std::size_t>(length) + 1);
    return static_cast<DWORD>(length);
}
#define FormatMessageA FormatMessage

// ---- Computer name ----------------------------------------------------------
// The host name stands for the NetBIOS and the DNS name. On input *size is the
// buffer size in characters; on success it is the length without terminator.
inline BOOL GetComputerName(LPSTR buffer, LPDWORD size) {
    if (!buffer || !size) { SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
    char name[256] = {};
    if (gethostname(name, sizeof(name) - 1) != 0) { SetLastError(fourstory::compat::errno_to_win32(errno)); return FALSE; }
    const DWORD length = static_cast<DWORD>(std::strlen(name));
    if (*size <= length) { *size = length + 1; SetLastError(ERROR_BUFFER_OVERFLOW); return FALSE; }
    std::memcpy(buffer, name, length + 1); *size = length;
    return TRUE;
}
#define GetComputerNameA GetComputerName

// ---- Critical sections ------------------------------------------------------
// A Windows critical section is recursive: the owning thread may enter it
// again and must leave it as often. Here it is a recursive POSIX mutex.
// Spin counts are accepted and ignored. Debug fields of Win32 are not provided.
#include <pthread.h>
struct CRITICAL_SECTION { pthread_mutex_t mutex; };
typedef CRITICAL_SECTION* LPCRITICAL_SECTION;
inline void InitializeCriticalSection(LPCRITICAL_SECTION section) {
    pthread_mutexattr_t attributes;
    pthread_mutexattr_init(&attributes);
    pthread_mutexattr_settype(&attributes, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&section->mutex, &attributes);
    pthread_mutexattr_destroy(&attributes);
}
inline BOOL InitializeCriticalSectionAndSpinCount(LPCRITICAL_SECTION section, DWORD) { InitializeCriticalSection(section); return TRUE; }
inline void DeleteCriticalSection(LPCRITICAL_SECTION section) { pthread_mutex_destroy(&section->mutex); }
inline void EnterCriticalSection(LPCRITICAL_SECTION section) { pthread_mutex_lock(&section->mutex); }
inline BOOL TryEnterCriticalSection(LPCRITICAL_SECTION section) { return pthread_mutex_trylock(&section->mutex) == 0; }
inline void LeaveCriticalSection(LPCRITICAL_SECTION section) { pthread_mutex_unlock(&section->mutex); }

// ---- Window system and GDI (C4-P) ----------------------------------------------
#include <string>
// Float to int as the x86/x64 truncating conversion (cvttss2si, cvttsd2si):
// NaN and values outside the int range give INT_MIN (0x80000000). ARM64
// saturates instead; branches that convert possibly infinite values use this.
inline int fourstory_x86_float_to_int(double value) {
    return (value != value || value >= 2147483648.0 || value < -2147483648.0) ? static_cast<int>(0x80000000u) : static_cast<int>(value);
}
#include "fourstory_user32.h"
#include "fourstory_gdi.h"
#include "fourstory_kernel32.h"
