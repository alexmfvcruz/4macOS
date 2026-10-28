// Checks the Win32/ATL compatibility layer used by FOURSTORY_NATIVE_MODULE
// builds, driving the original TNetLib CSession through the emulated
// AcceptEx/WSARecv/WSASend/completion-port path.
#include "StdAfx.h"
#include <cassert>
#include <csignal>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <unistd.h>

namespace {
void require(bool condition,const char* message) {
    if(!condition) {std::cerr<<"FAILED: "<<message<<'\n';std::exit(1);}
}
struct Completion {BOOL ok;DWORD bytes;DWORD key;LPOVERLAPPED overlapped;DWORD error;};
Completion next_completion(HANDLE port,DWORD timeout=5000) {
    Completion c{};
    c.ok=GetQueuedCompletionStatus(port,&c.bytes,&c.key,&c.overlapped,timeout);
    c.error=c.ok?0:GetLastError();
    return c;
}
WORD listen_port(CSession& listener) {
    sockaddr_in address{};socklen_t size=sizeof(address);
    require(getsockname(static_cast<int>(listener.m_sock),reinterpret_cast<sockaddr*>(&address),&size)==0,"listener address");
    return ntohs(address.sin_port);
}

void registry() {
    char path[]="/tmp/fourstory-registry-XXXXXX";
    const int fd=mkstemp(path);
    require(fd>=0,"temporary registry file");
    close(fd);
    {
        std::ofstream file(path);
        file<<"# comment\n[SYSTEM\\CurrentControlSet\\Services\\TMapSvr\\Config]\n"
            <<"GamePort=dword:5815\nGameDSN=sz:Native Game\nGamePasswd=env:FOURSTORY_TEST_SECRET\n";
    }
    setenv("FOURSTORY_REGISTRY",path,1);
    setenv("FOURSTORY_TEST_SECRET","not-a-real-secret",1);
    HKEY key=nullptr;
    require(RegCreateKey(HKEY_LOCAL_MACHINE,"SYSTEM\\CurrentControlSet\\Services\\TMapSvr\\Config",&key)==ERROR_SUCCESS,"open registry key");
    DWORD port=0,size=sizeof(port),type=0;
    require(RegQueryValueEx(key,"gameport",nullptr,&type,reinterpret_cast<LPBYTE>(&port),&size)==ERROR_SUCCESS,"case-insensitive DWORD");
    require(type==REG_DWORD && port==5815 && size==4,"DWORD value");
    char text[64];size=sizeof(text);
    require(RegQueryValueEx(key,"GameDSN",nullptr,nullptr,reinterpret_cast<LPBYTE>(text),&size)==ERROR_SUCCESS,"string value");
    require(std::string(text)=="Native Game" && size==12,"string bytes include terminator");
    char small[4];size=sizeof(small);
    require(RegQueryValueEx(key,"GameDSN",nullptr,nullptr,reinterpret_cast<LPBYTE>(small),&size)==ERROR_MORE_DATA && size==12,"short buffer");
    size=sizeof(text);
    require(RegQueryValueEx(key,"GamePasswd",nullptr,nullptr,reinterpret_cast<LPBYTE>(text),&size)==ERROR_SUCCESS &&
        std::string(text)=="not-a-real-secret","environment value");
    size=sizeof(text);
    require(RegQueryValueEx(key,"Missing",nullptr,nullptr,reinterpret_cast<LPBYTE>(text),&size)==ERROR_FILE_NOT_FOUND,"missing value");
    require(RegCloseKey(key)==ERROR_SUCCESS,"close key");
    unlink(path);
}

DWORD WINAPI worker(LPVOID parameter) {
    auto* event=static_cast<HANDLE*>(parameter);
    SetEvent(event[0]);
    return WaitForSingleObject(event[1],5000)==WAIT_OBJECT_0?42:7;
}
void threads() {
    HANDLE events[2]={CreateEvent(nullptr,FALSE,FALSE,nullptr),CreateEvent(nullptr,FALSE,FALSE,nullptr)};
    DWORD id=0;
    HANDLE thread=CreateThread(nullptr,0,worker,events,CREATE_SUSPENDED,&id);
    require(thread && id && id!=GetCurrentThreadId(),"thread created");
    require(WaitForSingleObject(events[0],100)==WAIT_TIMEOUT,"suspended thread has not started");
    require(ResumeThread(thread)==1,"resume count");
    require(WaitForSingleObject(events[0],5000)==WAIT_OBJECT_0,"thread started");
    require(WaitForSingleObject(events[0],50)==WAIT_TIMEOUT,"auto-reset event consumed");
    SetEvent(events[1]);
    require(WaitForMultipleObjects(1,&thread,TRUE,5000)==WAIT_OBJECT_0,"thread joined");
    DWORD code=0;
    require(GetExitCodeThread(thread,&code) && code==42,"thread exit code");
    for(HANDLE h:{events[0],events[1],thread}) require(CloseHandle(h),"close handle");
    require(!CloseHandle(thread),"double close rejected");
}

void sessions() {
    WSADATA data;
    require(WSAStartup(MAKEWORD(2,2),&data)==0 && LOBYTE(data.wVersion)==2,"WSAStartup");
    HANDLE accept_port=CreateIoCompletionPort(INVALID_HANDLE_VALUE,nullptr,0,0);
    HANDLE work_port=CreateIoCompletionPort(INVALID_HANDLE_VALUE,nullptr,0,0);
    require(accept_port && work_port,"completion ports");

    CSession listener;
    require(listener.Listen(0),"original Listen");
    require(CreateIoCompletionPort(reinterpret_cast<HANDLE>(listener.m_sock),accept_port,COMP_ACCEPT,0)==accept_port,"associate listener");
    SOCKET accepted=INVALID_SOCKET;
    require(CSession::CreateSocket(accepted),"original CreateSocket");
    CPacket accept_buffer;
    COverlappedEx accept_overlapped;
    DWORD read=0;
    require(!AcceptEx(listener.m_sock,accepted,accept_buffer.GetBuffer(),0,sizeof(SOCKADDR)+16,sizeof(SOCKADDR)+16,
        &read,reinterpret_cast<LPOVERLAPPED>(&accept_overlapped)) && WSAGetLastError()==ERROR_IO_PENDING,"AcceptEx pending");

    CSession client;
    require(client.Connect("127.0.0.1",listen_port(listener)),"original Connect");
    auto accepted_event=next_completion(accept_port);
    require(accepted_event.ok && accepted_event.key==COMP_ACCEPT &&
        accepted_event.overlapped==reinterpret_cast<LPOVERLAPPED>(&accept_overlapped),"accept completion");

    CSession server;
    require(server.Open(accepted,accept_buffer),"original Open");
    require(server.m_addr.sin_addr.s_addr==htonl(INADDR_LOOPBACK),"remote address at AcceptEx offset");
    server.m_Recv.ExpandIoBuffer(RECV_SVR_SIZE);
    require(CreateIoCompletionPort(reinterpret_cast<HANDLE>(server.m_sock),work_port,COMP_SESSION,0)==work_port,"associate session");
    require(server.WaitForMessage(),"original WaitForMessage");

    // A complete original packet crosses the emulated overlapped path.
    auto* packet=new CPacket();
    packet->SetID(0x1234)<<DWORD(0xA1B2C3D4)<<"native";
    require(CreateIoCompletionPort(reinterpret_cast<HANDLE>(client.m_sock),work_port,COMP_SESSION,0)==work_port,"associate client");
    require(client.Say(packet),"original Say");
    bool received=false,sent=false;
    for(int i=0;i<10 && !(received && sent);++i) {
        auto event=next_completion(work_port);
        require(event.ok,"session completion succeeds");
        auto* overlapped=reinterpret_cast<COverlappedEx*>(event.overlapped);
        if(overlapped==&client.m_ovSEND) {
            require(client.SendComplete(event.bytes)==FALSE,"send completion");sent=true;
        } else if(overlapped==&server.m_ovRECV) {
            require(server.Read(event.bytes),"original Read");
            if(server.CheckMessage()==PACKET_COMPLETE) {
                DWORD value=0;CString text;
                server.m_Recv>>value>>text;
                require(server.m_Recv.GetID()==0x1234 && value==0xA1B2C3D4 && text=="native","packet contents");
                received=true;
            } else require(server.WaitForMessage(),"continue receiving");
        } else require(false,"unexpected completion");
    }
    require(received && sent,"packet delivered");

    // Closing a socket aborts its pending receive, as on Windows.
    server.Flush();
    require(server.WaitForMessage(),"pending receive");
    SOCKET doomed=server.m_sock;
    require(closesocket(doomed)==0,"close with pending receive");
    server.m_sock=INVALID_SOCKET;
    auto aborted=next_completion(work_port);
    require(!aborted.ok && aborted.error==ERROR_OPERATION_ABORTED &&
        aborted.overlapped==reinterpret_cast<LPOVERLAPPED>(&server.m_ovRECV) && aborted.bytes==0,"aborted receive");

    // Graceful peer close completes a receive with zero bytes.
    require(client.WaitForMessage(),"client receive");
    auto eof=next_completion(work_port);
    require(eof.ok && eof.bytes==0 && eof.overlapped==reinterpret_cast<LPOVERLAPPED>(&client.m_ovRECV),"zero-byte EOF");

    // A refused nonblocking connect is not reported as connected.
    CSession probe;
    require(probe.Listen(0),"probe listen");
    const WORD closed_port=listen_port(probe);
    probe.Close();
    CSession refused;
    require(!refused.Connect("127.0.0.1",closed_port),"refused connect fails");

    // Windows bind semantics: a restarted listener reuses a port whose
    // server-closed connection is in TIME_WAIT; two live listeners conflict.
    {
        CSession first;
        require(first.Listen(0),"listener for TIME_WAIT check");
        const WORD port=listen_port(first);
        CSession peer;
        require(peer.Connect("127.0.0.1",port),"connect for TIME_WAIT check");
        sockaddr_in remote{};socklen_t size=sizeof(remote);
        int accepted_fd=-1;
        for(int i=0;i<200 && accepted_fd<0;++i) {
            accepted_fd=::accept(static_cast<int>(first.m_sock),reinterpret_cast<sockaddr*>(&remote),&size);
            if(accepted_fd<0) Sleep(5);
        }
        require(accepted_fd>=0,"accepted TIME_WAIT connection");
        ::close(accepted_fd);  // server closes first and keeps TIME_WAIT
        Sleep(50);
        first.Close();
        CSession restarted;
        require(restarted.Listen(port),"listen again despite TIME_WAIT");
        CSession duplicate;
        require(!duplicate.Listen(port),"second live listener rejected");
    }

    require(PostQueuedCompletionStatus(work_port,0,COMP_EXIT,nullptr),"post exit");
    auto exit_event=next_completion(work_port);
    require(exit_event.ok && exit_event.key==COMP_EXIT && !exit_event.overlapped,"posted completion");
    auto timeout=next_completion(work_port,20);
    require(!timeout.ok && timeout.error==WAIT_TIMEOUT && !timeout.overlapped,"completion timeout");
}

void handles_and_strings() {
    auto object=std::make_unique<int>(7);
    const DWORD handle=fourstory::win32::pointer_handle(object.get());
    require(handle && fourstory::win32::pointer_handle(object.get())==handle,"stable pointer handle");
    require(fourstory::win32::handle_pointer(handle)==object.get(),"pointer round trip");
    require(!fourstory::win32::handle_pointer(handle+1000),"unknown handle");
    require(reinterpret_cast<std::uintptr_t>(object.get())>0xFFFFFFFFull,"heap pointers exceed 32 bits on macOS");

    CString name("Hero");CString text;
    text.Format("%s:%d:%u:%08X",name,-5,7u,0xBEEFu);
    require(text=="Hero:-5:7:0000BEEF","ATL-compatible Format with CString");
    char buffer[64];
    wsprintf(buffer,"[%s]",name);
    require(std::string(buffer)=="[Hero]","wsprintf with CString");
    CString upper("mixed");
    require(upper.MakeUpper()=="MIXED","MakeUpper returns the string");
    CString time=CTime(2026,10,9,23,5,30).Format("%Y-%m-%d %H:%M:%S");
    require(time=="2026-10-09 23:05:30","CTime local calendar");
    require(CTimeSpan(1,2,3,4).GetTotalSeconds()==93784,"CTimeSpan");
}
}

// A reply written just before the socket is closed must still arrive: Login
// answers a rejected login and closes at once. Winsock hands the overlapped
// send to the transport immediately.
void send_then_close() {
    HANDLE port=CreateIoCompletionPort(INVALID_HANDLE_VALUE,nullptr,0,0);
    const int listener=::socket(AF_INET,SOCK_STREAM,0);
    sockaddr_in address{};address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    socklen_t size=sizeof(address);
    require(bind(static_cast<SOCKET>(listener),reinterpret_cast<sockaddr*>(&address),static_cast<int>(sizeof(address)))==0 && ::listen(listener,1)==0 &&
        getsockname(listener,reinterpret_cast<sockaddr*>(&address),&size)==0,"plain listener");
    const int peer=::socket(AF_INET,SOCK_STREAM,0);
    require(::connect(peer,reinterpret_cast<sockaddr*>(&address),sizeof(address))==0,"plain connect");
    const SOCKET server=static_cast<SOCKET>(::accept(listener,nullptr,nullptr));
    require(server!=INVALID_SOCKET,"plain accept");
    require(CreateIoCompletionPort(reinterpret_cast<HANDLE>(server),port,COMP_SESSION,0)==port,"associate plain socket");
    char reply[]="rejected";WSABUF buffer{sizeof(reply),reply};DWORD sent=0;OVERLAPPED overlapped{};
    WSASend(server,&buffer,1,&sent,0,&overlapped,nullptr);
    closesocket(server);
    char received[sizeof(reply)]{};std::size_t got=0;
    while(got<sizeof(reply)) {
        const auto n=::recv(peer,received+got,sizeof(reply)-got,0);
        if(n<=0) break;
        got+=static_cast<std::size_t>(n);
    }
    require(got==sizeof(reply) && std::memcmp(received,reply,sizeof(reply))==0,"send before close is delivered");
    auto event=next_completion(port);
    require(event.ok && event.bytes==sizeof(reply) && event.overlapped==&overlapped,"send completion is still posted");
    ::close(peer);::close(listener);CloseHandle(port);
}

// A send on a connection reset by the peer must fail at once, as Winsock does.
// A pending result made CSession re-post the failed send forever, so sessions
// of players who dropped together never closed (stuck TCURRENTUSER rows).
void send_after_reset() {
    std::signal(SIGPIPE,SIG_IGN);
    HANDLE port=CreateIoCompletionPort(INVALID_HANDLE_VALUE,nullptr,0,0);
    const int listener=::socket(AF_INET,SOCK_STREAM,0);
    sockaddr_in address{};address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    socklen_t size=sizeof(address);
    require(bind(static_cast<SOCKET>(listener),reinterpret_cast<sockaddr*>(&address),static_cast<int>(sizeof(address)))==0 && ::listen(listener,1)==0 &&
        getsockname(listener,reinterpret_cast<sockaddr*>(&address),&size)==0,"reset listener");
    const int peer=::socket(AF_INET,SOCK_STREAM,0);
    require(::connect(peer,reinterpret_cast<sockaddr*>(&address),sizeof(address))==0,"reset connect");
    const SOCKET server=static_cast<SOCKET>(::accept(listener,nullptr,nullptr));
    require(server!=INVALID_SOCKET,"reset accept");
    require(CreateIoCompletionPort(reinterpret_cast<HANDLE>(server),port,COMP_SESSION,0)==port,"associate reset socket");
    linger hard{1,0};::setsockopt(peer,SOL_SOCKET,SO_LINGER,&hard,sizeof(hard));
    ::close(peer);
    char data[]="x";WSABUF buffer{sizeof(data),data};DWORD sent=0;OVERLAPPED overlapped{};
    int result=0;
    for(int attempt=0;attempt<50 && result!=SOCKET_ERROR;++attempt) {
        ::usleep(10000);
        result=WSASend(server,&buffer,1,&sent,0,&overlapped,nullptr);
        if(result==0) next_completion(port);
    }
    require(result==SOCKET_ERROR && WSAGetLastError()==WSAECONNRESET,"send on a reset connection fails at once");
    require(!next_completion(port,200).overlapped,"a failed send posts no completion");
    closesocket(server);
    // An operation posted on a closed socket fails at once instead of waiting
    // forever in the reactor.
    char space[4];WSABUF into{sizeof(space),space};DWORD got=0,flags=0;
    require(WSARecv(server,&into,1,&got,&flags,&overlapped,nullptr)==SOCKET_ERROR && WSAGetLastError()==WSAENOTSOCK,
        "a receive on a closed socket fails at once");
    ::close(listener);CloseHandle(port);
}

// The servers find sibling files with strrchr(path,'\\') on the module path
// and hardcode drive paths for some logs.
void paths() {
    char module[1024];
    require(GetModuleFileName(nullptr,module,sizeof(module))>0,"module path");
    require(std::strrchr(module,'\\') && !std::strchr(module,'/'),"module path uses Win32 separators");
    char* slash=std::strrchr(module,'\\');
    std::strcpy(slash+1,"\\TClient.exe");
    require(CreateFile(module,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr)==INVALID_HANDLE_VALUE,
        "a missing client reference is not replaced by the executable");

    char directory[]="/tmp/fourstory-paths-XXXXXX";
    require(mkdtemp(directory),"temporary directory");
    std::string windows=std::string(directory)+"/log.txt";
    for(auto& c:windows) if(c=='/') c='\\';
    FILE* file=fopen(windows.c_str(),"a+");
    require(file && std::fputs("line",file)>=0 && std::fclose(file)==0,"fopen accepts Win32 separators");
    require(std::ifstream(std::string(directory)+"/log.txt").good(),"file created at the translated path");

    unsetenv("FOURSTORY_DRIVE_ROOT");
    require(!fopen("C:\\TServices_GSP\\Probe.log","a+"),"drive paths need FOURSTORY_DRIVE_ROOT");
    setenv("FOURSTORY_DRIVE_ROOT",directory,1);
    file=fopen("C:\\TServices_GSP\\Probe.log","a+");
    require(file && std::fclose(file)==0,"drive path opens under the drive root");
    require(std::ifstream(std::string(directory)+"/C/TServices_GSP/Probe.log").good(),"drive path location");
    unsetenv("FOURSTORY_DRIVE_ROOT");
    const std::string cleanup=std::string("rm -rf '")+directory+"'";
    require(std::system(cleanup.c_str())==0,"cleanup");
}

int main() {
    send_then_close();
    send_after_reset();
    paths();
    registry();
    threads();
    sessions();
    handles_and_strings();
    std::cout<<"win32 compatibility checks passed\n";
    return 0;
}
