// Shared by the isolated probe, production manager and recovery helper.
#pragma once
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/hidsystem/IOHIDLib.h>
#include <IOKit/hidsystem/IOHIDParameter.h>
#include <IOKit/hidsystem/IOHIDShared.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <spawn.h>
#include <poll.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <algorithm>
#include <chrono>
extern char** environ;
namespace fourstory_keyboard_lease {
struct Reply { int32_t status, original, observed; };
inline int transport=-1; inline pid_t helper=-1;
inline volatile sig_atomic_t stopping=0;
inline void stopHelper(int){stopping=1;}
inline bool transfer(int fd,void* data,size_t count,bool writing,int timeout=2000) {
    const auto start=std::chrono::steady_clock::now();
    auto* p=static_cast<char*>(data);
    while(count) {
        pollfd poller{fd,short(writing?POLLOUT:POLLIN),0};
        int wait=-1;
        if(timeout>=0) {
            const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count();
            if(elapsed>=timeout)return false;
            wait=timeout-int(elapsed);
        }
        int ready=poll(&poller,1,wait);
        if(ready<0 && errno==EINTR && !stopping)continue;
        if(ready<=0 || stopping)return false;
        ssize_t n=writing?write(fd,p,count):read(fd,p,count);
        if(n<0 && errno==EINTR)continue;
        if(n<=0)return false;
        p+=n;count-=size_t(n);
    }
    return true;
}
#ifdef FOURSTORY_KEYBOARD_PROBE_TEST
inline int fakeRead(int* mode) {
    const char* path=std::getenv("FOURSTORY_KEYBOARD_FAKE_STATE");if(!path)return int(kIOReturnNotFound);
    FILE* f=std::fopen(path,"r");if(!f)return int(kIOReturnNotFound);
    bool ok=std::fscanf(f,"%d",mode)==1 && (*mode==0 || *mode==1);std::fclose(f);
    return ok?0:int(kIOReturnBadArgument);
}
inline int fakeWrite(int mode) {
    if(!mode && std::getenv("FOURSTORY_KEYBOARD_FAKE_FAIL_RESTORE"))return int(kIOReturnNotPermitted);
    if(mode && std::getenv("FOURSTORY_KEYBOARD_FAKE_ENABLE_DELAY_MS"))usleep(1000*std::atoi(std::getenv("FOURSTORY_KEYBOARD_FAKE_ENABLE_DELAY_MS")));
    const char* path=std::getenv("FOURSTORY_KEYBOARD_FAKE_STATE");if(!path)return int(kIOReturnNotFound);
    FILE* f=std::fopen(path,"w");if(!f)return int(kIOReturnNotWritable);
    std::fprintf(f,"%d\n",mode);return std::fclose(f)?int(kIOReturnError):0;
}
#endif
inline void disconnect(io_connect_t connection) {
#ifndef FOURSTORY_KEYBOARD_PROBE_TEST
    if(connection)IOServiceClose(connection);
#else
    (void)connection;
#endif
}
inline int readMode(io_connect_t connection,int* mode) {
#ifdef FOURSTORY_KEYBOARD_PROBE_TEST
    (void)connection;return fakeRead(mode);
#else
    CFTypeRef value=nullptr;
    auto status=IOHIDCopyCFTypeParameter(connection,CFSTR(kIOHIDFKeyModeKey),&value);
    bool valid=!status && value && CFGetTypeID(value)==CFNumberGetTypeID()
        && CFNumberGetValue((CFNumberRef)value,kCFNumberIntType,mode) && (*mode==0 || *mode==1);
    if(value)CFRelease(value);
    return status?int(status):valid?0:int(kIOReturnBadArgument);
#endif
}
inline int writeMode(io_connect_t connection,int mode) {
#ifdef FOURSTORY_KEYBOARD_PROBE_TEST
    (void)connection;return fakeWrite(mode);
#else
    CFNumberRef value=CFNumberCreate(kCFAllocatorDefault,kCFNumberIntType,&mode);
    if(!value)return int(kIOReturnNoMemory);
    int status=int(IOHIDSetCFTypeParameter(connection,CFSTR(kIOHIDFKeyModeKey),value));
    CFRelease(value);return status;
#endif
}
inline int connect(io_connect_t* connection) {
#ifdef FOURSTORY_KEYBOARD_PROBE_TEST
    *connection=1;return 0;
#else
    io_service_t service=IOServiceGetMatchingService(kIOMainPortDefault,IOServiceMatching("IOHIDSystem"));
    if(!service)return int(kIOReturnNotFound);
    int result=int(IOServiceOpen(service,mach_task_self(),kIOHIDParamConnectType,connection));
    IOObjectRelease(service);return result;
#endif
}
inline int watch(int fd,const char* lockPath) {
    struct sigaction action{};action.sa_handler=stopHelper;sigemptyset(&action.sa_mask);
    sigaction(SIGTERM,&action,nullptr);sigaction(SIGINT,&action,nullptr);
    int lock=open(lockPath,O_RDWR|O_CREAT|O_NOFOLLOW,0600);
    struct stat info{};
    bool leased=lock>=0 && fstat(lock,&info)==0 && S_ISREG(info.st_mode)
        && info.st_uid==getuid() && flock(lock,LOCK_EX|LOCK_NB)==0;
    io_connect_t connection=IO_OBJECT_NULL;
    Reply reply{leased?connect(&connection):int(kIOReturnExclusiveAccess),-1,-1};
    if(!reply.status)reply.status=readMode(connection,&reply.original);
    reply.observed=reply.original;
    std::fprintf(stderr,"POC helper ready uid=%u status=0x%08x original=%d\n",getuid(),unsigned(reply.status),reply.original);
    bool ready=transfer(fd,&reply,sizeof(reply),true);
    bool changed=false;int lastApplied=reply.original;
    while(ready && !reply.status && !stopping) {
        char command=0;if(!transfer(fd,&command,1,false,-1))break;
        if(command=='Q')break;
        int desired=command=='E'?1:reply.original;
        if(command!='E' && command!='R' && command!='P') {reply.status=int(kIOReturnBadArgument);}
        else {
            int before=-1;reply.status=readMode(connection,&before);
            // Never overwrite a distinguishable external change on restore.
            if(!reply.status && command=='R' && !changed) {
                reply.observed=before;
                if(before!=reply.original)reply.status=int(kIOReturnBusy);
            } else if(!reply.status && ((command=='R' && before!=lastApplied) || (command=='P' && before!=reply.original) || (command=='E' && before!=(changed?lastApplied:reply.original)))) reply.status=int(kIOReturnBusy);
            else if(!reply.status) {
                // Mark the attempted lease before writing: even a failed API may
                // have partially applied its request and still needs recovery.
                if(command=='E') {changed=true;lastApplied=desired;}
                reply.status=writeMode(connection,desired);
                int observed=-1;int readStatus=readMode(connection,&observed);reply.observed=observed;
                if(!reply.status)reply.status=readStatus;
                if(!reply.status && observed!=desired)reply.status=int(kIOReturnNotReady);
                if(!reply.status && command=='R')changed=false;
            }
        }
        std::fprintf(stderr,"POC request=%c status=0x%08x readback=%d (not physical proof)\n",command,unsigned(reply.status),reply.observed);
        ready=transfer(fd,&reply,sizeof(reply),true);
        // An operation failure is reported, then the helper goes to recovery.
    }
    int recoveryExit=0;
    if(changed && connection) {
        int current=-1;int result=readMode(connection,&current);
        if(!result && current==lastApplied) {
            result=writeMode(connection,reply.original);
            int observed=-1;int readStatus=readMode(connection,&observed);
            if(!result)result=readStatus;
            if(!result && observed!=reply.original)result=int(kIOReturnNotReady);
            recoveryExit=result?1:0;
            std::fprintf(stderr,"POC recovery status=0x%08x readback=%d original=%d\n",unsigned(result),observed,reply.original);
        } else {
            recoveryExit=2;
            std::fprintf(stderr,"POC recovery conflict/unavailable status=0x%08x current=%d; not overwriting external state\n",unsigned(result),current);
        }
    }
    disconnect(connection);
    close(fd);if(lock>=0)close(lock);return recoveryExit;
}
inline bool launch(const char* executable,const char* lockPath,Reply* initial) {
    int sockets[2];if(socketpair(AF_UNIX,SOCK_STREAM,0,sockets))return false;
    int yes=1;setsockopt(sockets[0],SOL_SOCKET,SO_NOSIGPIPE,&yes,sizeof(yes));setsockopt(sockets[1],SOL_SOCKET,SO_NOSIGPIPE,&yes,sizeof(yes));
    fcntl(sockets[0],F_SETFD,FD_CLOEXEC);fcntl(sockets[1],F_SETFD,FD_CLOEXEC);
    // Use a descriptor outside the socket pair; dup2 clears close-on-exec.
    const int childFD=std::max(sockets[0],sockets[1])+1;
    posix_spawn_file_actions_t actions;posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions,sockets[1],childFD);
    posix_spawn_file_actions_addclose(&actions,sockets[0]);posix_spawn_file_actions_addclose(&actions,sockets[1]);
    char fdText[16];std::snprintf(fdText,sizeof(fdText),"%d",childFD);
    char* args[]={const_cast<char*>(executable),const_cast<char*>("--watchdog"),fdText,const_cast<char*>(lockPath),nullptr};
    int status=posix_spawn(&helper,executable,&actions,nullptr,args,environ);
    posix_spawn_file_actions_destroy(&actions);close(sockets[1]);
    if(status){close(sockets[0]);return false;}
    transport=sockets[0];pollfd p{transport,POLLIN,0};
    if(poll(&p,1,3000)<=0 || !transfer(transport,initial,sizeof(*initial),false)) {close(transport);transport=-1;return false;}
    return true;
}
inline void invalidateTransport() {
    if(transport>=0){shutdown(transport,SHUT_RDWR);close(transport);transport=-1;}
}
inline Reply request(char command) {
    Reply reply{int(kIOReturnOffline),-1,-1};
    if(transport<0 || !transfer(transport,&command,1,true)){invalidateTransport();return reply;}
    pollfd p{transport,POLLIN,0};
    if(poll(&p,1,3000)<=0 || !transfer(transport,&reply,sizeof(reply),false)) {
        invalidateTransport();return Reply{int(kIOReturnOffline),-1,-1};
    }
    if(reply.status)invalidateTransport();
    return reply;
}
inline int lastRecovery=0;
inline int finish() {
    if(transport>=0){char q='Q';transfer(transport,&q,1,true);invalidateTransport();}
    if(helper<=0)return lastRecovery;
    const auto start=std::chrono::steady_clock::now();
    for(;;) {
        int status=0;pid_t result=waitpid(helper,&status,WNOHANG);
        if(result==helper){helper=-1;lastRecovery=WIFEXITED(status)?WEXITSTATUS(status):128+(WIFSIGNALED(status)?WTERMSIG(status):0);return lastRecovery;}
        if(result<0 && errno!=EINTR){helper=-1;lastRecovery=3;return lastRecovery;}
        if(std::chrono::steady_clock::now()-start>std::chrono::seconds(5)){lastRecovery=3;return lastRecovery;}
        usleep(10000); // Worker only; never blocks the AppKit main thread.
    }
}
}
