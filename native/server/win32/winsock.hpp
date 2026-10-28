#pragma once
// Winsock subset over POSIX sockets. Overlapped operations always report
// WSA_IO_PENDING and complete through the emulated completion port, which is
// a valid Win32 outcome that the original TNetLib callers already handle.
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>

using SOCKET=std::uintptr_t;
#define INVALID_SOCKET (~SOCKET(0))
#define SOCKET_ERROR (-1)
using SOCKADDR=struct sockaddr;
using SOCKADDR_IN=struct sockaddr_in;
using PSOCKADDR=SOCKADDR*;
using LPSOCKADDR=SOCKADDR*;
using PSOCKADDR_IN=SOCKADDR_IN*;
using LPSOCKADDR_IN=SOCKADDR_IN*;
using HOSTENT=struct hostent;
using LPHOSTENT=struct hostent*;
using IN_ADDR=struct in_addr;
struct WSABUF {DWORD len;char* buf;};
using LPWSABUF=WSABUF*;
struct WSADATA {WORD wVersion,wHighVersion;char szDescription[257];char szSystemStatus[129];};
using LPWSADATA=WSADATA*;
struct GUID {DWORD Data1;WORD Data2,Data3;BYTE Data4[8];};
using LPWSAOVERLAPPED_COMPLETION_ROUTINE=void (*)(DWORD,DWORD,LPOVERLAPPED,DWORD);
using LPFN_CONNECTEX=BOOL (*)(SOCKET,const struct sockaddr*,int,PVOID,DWORD,LPDWORD,LPOVERLAPPED);
#define WSAID_CONNECTEX {0x25a207b9,0xddf3,0x4660,{0x8e,0xe9,0x76,0xe5,0x8c,0x74,0x06,0x3e}}
#define SIO_GET_EXTENSION_FUNCTION_POINTER 0xC8000006u
#define WSA_FLAG_OVERLAPPED 0x01u
#define WSA_IO_PENDING 997
#define WSAEWOULDBLOCK 10035
#define WSAEINPROGRESS 10036
#define WSAEINVAL 10022
#define WSAEMSGSIZE 10040
#define WSAEADDRINUSE 10048
#define WSAENETDOWN 10050
#define WSAECONNABORTED 10053
#define WSAECONNRESET 10054
#define WSAENOTCONN 10057
#define WSAETIMEDOUT 10060
#define WSAECONNREFUSED 10061
#define WSAENOTSOCK 10038
#define SO_UPDATE_ACCEPT_CONTEXT 0x700B
#define SO_UPDATE_CONNECT_CONTEXT 0x7010
#define SD_RECEIVE SHUT_RD
#define SD_SEND SHUT_WR
#define SD_BOTH SHUT_RDWR

int WSAStartup(WORD version,LPWSADATA data);
int WSACleanup();
int WSAGetLastError();
void WSASetLastError(int error);
SOCKET WSASocket(int family,int type,int protocol,void* info,unsigned group,DWORD flags);
int closesocket(SOCKET socket);
int fourstory_ioctlsocket(SOCKET socket,long command,const void* argument);
#define ioctlsocket(s,c,a) fourstory_ioctlsocket((s),(c),(a))
#define FIONBIO_WIN32 0x8004667Eu
int fourstory_setsockopt(SOCKET socket,int level,int name,const void* value,int length);
#define setsockopt(s,l,n,v,len) fourstory_setsockopt((s),(l),(n),(v),(len))
int fourstory_select(int,fd_set* read,fd_set* write,fd_set* except,const struct timeval* timeout);
#define select(n,r,w,e,t) fourstory_select((n),(r),(w),(e),(t))
int WSASend(SOCKET socket,LPWSABUF buffers,DWORD count,LPDWORD sent,DWORD flags,LPOVERLAPPED overlapped,LPWSAOVERLAPPED_COMPLETION_ROUTINE);
int WSARecv(SOCKET socket,LPWSABUF buffers,DWORD count,LPDWORD received,LPDWORD flags,LPOVERLAPPED overlapped,LPWSAOVERLAPPED_COMPLETION_ROUTINE);
BOOL AcceptEx(SOCKET listen,SOCKET accept,PVOID output,DWORD receive_length,DWORD local_length,DWORD remote_length,LPDWORD received,LPOVERLAPPED overlapped);
void GetAcceptExSockaddrs(PVOID output,DWORD receive_length,DWORD local_length,DWORD remote_length,
    LPSOCKADDR* local,LPINT local_size,LPSOCKADDR* remote,LPINT remote_size);
int WSAIoctl(SOCKET socket,DWORD code,LPVOID in,DWORD in_size,LPVOID out,DWORD out_size,LPDWORD returned,LPOVERLAPPED,LPWSAOVERLAPPED_COMPLETION_ROUTINE);
// Address of a remote peer in AcceptEx output, where TNetLib reads it.
constexpr std::size_t ACCEPTEX_REMOTE_OFFSET=38;
// TNetLib.h has `using namespace std`; this overload is more specialized than
// std::bind for the original bind(SOCKET, sockaddr*, size) calls.
// Resolved before any using-directive, so it always names POSIX bind.
// Windows lets a listener bind a port whose old connections are in TIME_WAIT;
// BSD needs SO_REUSEADDR for that (it still rejects a second active listener).
inline int fourstory_posix_bind(int socket,const struct sockaddr* address,socklen_t length) {
    const int one=1;
    ::setsockopt(socket,SOL_SOCKET,SO_REUSEADDR,&one,sizeof(one));
    return ::bind(socket,address,length);
}
template<class Length> int bind(SOCKET socket,struct sockaddr* address,Length length) {
    return fourstory_posix_bind(static_cast<int>(socket),address,static_cast<socklen_t>(length));
}
