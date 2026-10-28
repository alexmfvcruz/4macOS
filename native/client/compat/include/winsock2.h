// Native replacement for <winsock2.h> (FOURSTORY_CLIENT_NATIVE) over BSD
// sockets. SOCKET is a file descriptor. The BSD functions socket, connect,
// send, recv, setsockopt, inet_addr, inet_ntoa and htons keep their names; after a failure GetLastError() and WSAGetLastError() return
// the Winsock code (windows.h, "Last error"). A non-blocking connect in
// progress reports WSAEWOULDBLOCK, as on Windows. Sockets made by WSASocket
// never raise SIGPIPE (SO_NOSIGPIPE); a send to a closed peer fails with
// WSAESHUTDOWN instead.
// Not provided: WSAAsyncSelect and the other window-message functions. The
// owner of a session must poll (board, C2 contract item 9).
#pragma once
#include "windows.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>

typedef int SOCKET;
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
typedef sockaddr SOCKADDR;
typedef sockaddr* PSOCKADDR;
typedef sockaddr* LPSOCKADDR;
typedef sockaddr_in SOCKADDR_IN;
typedef sockaddr_in* PSOCKADDR_IN;
typedef sockaddr_in* LPSOCKADDR_IN;
typedef hostent HOSTENT;
typedef hostent* LPHOSTENT;
typedef hostent* PHOSTENT;
typedef in_addr IN_ADDR;
typedef unsigned int GROUP;
struct WSAPROTOCOL_INFO;
typedef WSAPROTOCOL_INFO* LPWSAPROTOCOL_INFO;

#define WSAEINTR 10004L
#define WSAEFAULT 10014L
#define WSAEINVAL 10022L
#define WSAEMFILE 10024L
#define WSAEWOULDBLOCK 10035L
#define WSAEINPROGRESS 10036L
#define WSAEALREADY 10037L
#define WSAENOTSOCK 10038L
#define WSAEMSGSIZE 10040L
#define WSAEADDRINUSE 10048L
#define WSAEADDRNOTAVAIL 10049L
#define WSAENETDOWN 10050L
#define WSAENETUNREACH 10051L
#define WSAECONNABORTED 10053L
#define WSAECONNRESET 10054L
#define WSAENOBUFS 10055L
#define WSAEISCONN 10056L
#define WSAENOTCONN 10057L
#define WSAESHUTDOWN 10058L
#define WSAETIMEDOUT 10060L
#define WSAECONNREFUSED 10061L
#define WSAEHOSTUNREACH 10065L
#define WSANOTINITIALISED 10093L

inline int WSAGetLastError() { return static_cast<int>(GetLastError()); }
inline void WSASetLastError(int code) { SetLastError(static_cast<DWORD>(code)); }

// Winsock needs WSAStartup before the first socket; BSD sockets need nothing.
// The structure is filled with version 2.2 so that callers that check it pass.
#define WSADESCRIPTION_LEN 256
#define WSASYS_STATUS_LEN 128
struct WSADATA {
    WORD wVersion, wHighVersion;
    unsigned short iMaxSockets, iMaxUdpDg;
    char* lpVendorInfo;
    char szDescription[WSADESCRIPTION_LEN + 1];
    char szSystemStatus[WSASYS_STATUS_LEN + 1];
};
typedef WSADATA* LPWSADATA;
inline int WSAStartup(WORD, LPWSADATA data) {
    if (data) { std::memset(data, 0, sizeof(*data)); data->wVersion = data->wHighVersion = MAKEWORD(2, 2); std::strcpy(data->szDescription, "BSD sockets"); }
    return 0;
}
inline int WSACleanup() { return 0; }

// Only the plain form is supported: no protocol information, group or flags.
inline SOCKET WSASocket(int family, int type, int protocol, LPWSAPROTOCOL_INFO info, GROUP group, DWORD flags) {
    if (info || group || flags) { SetLastError(WSAEINVAL); return INVALID_SOCKET; }
    const int descriptor = ::socket(family, type, protocol);
    if (descriptor < 0) return INVALID_SOCKET;
    const int on = 1;
    if (::setsockopt(descriptor, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on)) != 0 ||
        ::fcntl(descriptor, F_SETFD, FD_CLOEXEC) != 0) {
        const int error = errno; ::close(descriptor); errno = error;
        return INVALID_SOCKET;
    }
    return descriptor;
}
inline int closesocket(SOCKET socket) { return ::close(socket) == 0 ? 0 : SOCKET_ERROR; }
// FIONBIO is the only command; *argument non-zero selects non-blocking mode.
inline int ioctlsocket(SOCKET socket, unsigned long command, unsigned long* argument) {
    if (command != static_cast<unsigned long>(FIONBIO) || !argument) { SetLastError(WSAEINVAL); return SOCKET_ERROR; }
    const int flags = ::fcntl(socket, F_GETFL, 0);
    if (flags < 0) return SOCKET_ERROR;
    return ::fcntl(socket, F_SETFL, *argument ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK)) == 0 ? 0 : SOCKET_ERROR;
}
// gethostbyname, IPv4 only, through getaddrinfo. The macOS function returns
// a misaligned h_addr_list for numeric names, which UndefinedBehaviorSanitizer
// reports when the original code reads *h_addr_list. The result is valid until
// the next call on the same thread, as on Windows.
#define WSAHOST_NOT_FOUND 11001L
namespace fourstory { namespace compat {
struct host_entry {
    hostent entry;
    char name[256];
    char* aliases[1];
    in_addr addresses[16];
    char* list[17];
};
inline hostent* gethostbyname(const char* name) {
    static thread_local host_entry result;
    if (!name) { SetLastError(WSAEFAULT); return nullptr; }
    addrinfo hints; std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; hints.ai_socktype = SOCK_STREAM; hints.ai_flags = AI_CANONNAME;
    addrinfo* found = nullptr;
    if (::getaddrinfo(name, nullptr, &hints, &found) != 0 || !found) { SetLastError(WSAHOST_NOT_FOUND); return nullptr; }
    std::memset(&result, 0, sizeof(result));
    std::strncpy(result.name, found->ai_canonname ? found->ai_canonname : name, sizeof(result.name) - 1);
    int count = 0;
    for (addrinfo* item = found; item && count < 16; item = item->ai_next) {
        if (item->ai_family != AF_INET || !item->ai_addr) continue;
        const in_addr address = reinterpret_cast<const sockaddr_in*>(item->ai_addr)->sin_addr;
        bool duplicate = false;
        for (int i = 0; i < count; ++i) duplicate = duplicate || result.addresses[i].s_addr == address.s_addr;
        if (!duplicate) result.addresses[count++] = address;
    }
    ::freeaddrinfo(found);
    if (!count) { SetLastError(WSAHOST_NOT_FOUND); return nullptr; }
    for (int i = 0; i < count; ++i) result.list[i] = reinterpret_cast<char*>(&result.addresses[i]);
    result.entry.h_name = result.name;
    result.entry.h_aliases = result.aliases;
    result.entry.h_addrtype = AF_INET;
    result.entry.h_length = sizeof(in_addr);
    result.entry.h_addr_list = result.list;
    return &result.entry;
}
}}
#define gethostbyname fourstory::compat::gethostbyname

#define SD_RECEIVE SHUT_RD
#define SD_SEND SHUT_WR
#define SD_BOTH SHUT_RDWR

// Winsock takes the address length as int*; BSD as socklen_t* (C4).
inline int getsockname(SOCKET s, sockaddr* address, int* length) {
    socklen_t n = length ? static_cast<socklen_t>(*length) : 0;
    const int r = ::getsockname(s, address, &n);
    if (length) *length = static_cast<int>(n);
    return r;
}
inline int getpeername(SOCKET s, sockaddr* address, int* length) {
    socklen_t n = length ? static_cast<socklen_t>(*length) : 0;
    const int r = ::getpeername(s, address, &n);
    if (length) *length = static_cast<int>(n);
    return r;
}
