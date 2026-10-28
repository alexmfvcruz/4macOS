// Native implementation of the Win32 subset declared in windows.hpp/winsock.hpp.
#include "atl.hpp"
// The implementation calls the POSIX functions these wrappers replace.
#undef setsockopt
#undef select
#undef fopen
#include <chrono>
#include <condition_variable>
#include <deque>
#include <fstream>
#include <memory>
#include <mutex>
#include <poll.h>
#ifdef __linux__
#include <sys/epoll.h>
#else
#include <sys/event.h>
#endif
#include <sys/stat.h>
#include <sys/wait.h>
#include <csignal>
#include <sstream>
#include <thread>
#include <unordered_map>

// Linux has no SO_NOSIGPIPE: MSG_NOSIGNAL keeps a peer reset from raising
// SIGPIPE even if the process-wide SIG_IGN (module_main.cpp) ever changes.
#ifdef MSG_NOSIGNAL
#define FOURSTORY_SEND_FLAGS MSG_NOSIGNAL
#else
#define FOURSTORY_SEND_FLAGS 0
#endif

namespace fourstory::win32 {
namespace {std::atomic<bool> stop_flag{false};}
DWORD& last_error() {static thread_local DWORD value=0;return value;}

namespace {
struct Object {virtual ~Object()=default;};
std::mutex handles_mutex;
std::unordered_map<Object*,std::shared_ptr<Object>> handles;

HANDLE publish(std::shared_ptr<Object> object) {
    std::lock_guard lock(handles_mutex);
    auto* raw=object.get();handles.emplace(raw,std::move(object));return raw;
}
template<class T> std::shared_ptr<T> find(HANDLE handle) {
    std::lock_guard lock(handles_mutex);
    const auto it=handles.find(static_cast<Object*>(handle));
    if(it==handles.end()) return {};
    return std::dynamic_pointer_cast<T>(it->second);
}

struct Waitable : Object {
    std::mutex mutex;
    std::condition_variable changed;
    virtual bool signaled_locked() const=0;
    virtual void consume_locked() {}
};
struct Event final : Waitable {
    bool manual=false,signaled=false;
    bool signaled_locked() const override {return signaled;}
    void consume_locked() override {if(!manual) signaled=false;}
};
struct Thread final : Waitable {
    LPTHREAD_START_ROUTINE start=nullptr;
    LPVOID parameter=nullptr;
    DWORD suspend=0,exit_code=STILL_ACTIVE,id=0;
    bool finished=false;
    bool signaled_locked() const override {return finished;}
};
struct Completion {DWORD bytes;ULONG_PTR key;LPOVERLAPPED overlapped;bool ok;DWORD error;};
struct Port final : Object {
    std::mutex mutex;
    std::condition_variable ready;
    std::deque<Completion> queue;
    void post(Completion completion) {
        {std::lock_guard lock(mutex);queue.push_back(completion);}
        ready.notify_one();
    }
};

std::atomic<DWORD> next_thread_id{1};
DWORD& current_thread_id() {static thread_local DWORD id=next_thread_id++;return id;}

using Clock=std::chrono::steady_clock;
const Clock::time_point process_start=Clock::now();

// ----------------------------------------------------------------------------
// Socket reactor: performs pending overlapped operations and posts completions
// to the port associated with the socket, as Windows does.
struct Pending {
    enum Kind {recv,send,accept,connect} kind;
    LPOVERLAPPED overlapped=nullptr;
    char* buffer=nullptr;
    DWORD length=0,done=0;
    int accept_fd=-1;
};
struct SocketState {
    std::weak_ptr<Port> port;
    ULONG_PTR key=0;
    std::vector<Pending> pending;
    unsigned interest=0; // events registered with the reactor's poller
};
std::mutex sockets_mutex;
std::unordered_map<int,SocketState> sockets;
std::once_flag reactor_once;
std::atomic<int> poller{-1};

int fd_of(SOCKET socket) {
    if(socket==INVALID_SOCKET || socket>static_cast<SOCKET>(INT32_MAX)) return -1;
    return static_cast<int>(socket);
}
// The reactor keeps one persistent epoll/kqueue registration per socket with
// pending operations, so it never rescans every socket. Registrations change
// only when a socket's set of pending kinds changes (caller holds
// sockets_mutex); both pollers accept changes while the reactor is waiting.
enum : unsigned {want_read=1,want_write=2};
unsigned interest_of(const SocketState& state) {
    unsigned events=0;
    for(const auto& op:state.pending)
        events|=(op.kind==Pending::send || op.kind==Pending::connect)?want_write:want_read;
    return events;
}
// Removes any registration for fd. An epoll registration belongs to the open
// file description, so it outlives close() while a forked child (StartService)
// still holds the descriptor; delete it explicitly.
void drop_registration(int fd) {
    const int queue=poller.load();
    if(queue<0) return;
#ifdef __linux__
    epoll_event none{};::epoll_ctl(queue,EPOLL_CTL_DEL,fd,&none);
#else
    struct kevent changes[2],receipts[2];
    EV_SET(&changes[0],fd,EVFILT_READ,EV_DELETE|EV_RECEIPT,0,0,nullptr);
    EV_SET(&changes[1],fd,EVFILT_WRITE,EV_DELETE|EV_RECEIPT,0,0,nullptr);
    ::kevent(queue,changes,2,receipts,2,nullptr);
#endif
}
// Returns false when the poller rejects the socket (closed descriptor, watch
// limit); the caller must then fail the socket's pending operations.
bool update_interest(int fd,SocketState& state) {
    const unsigned events=interest_of(state);
    const int queue=poller.load();
    if(events==state.interest || queue<0) return true;
    if(!events) {drop_registration(fd);state.interest=0;return true;}
#ifdef __linux__
    epoll_event change{};change.data.fd=fd;
    change.events=((events&want_read)?EPOLLIN:0u)|((events&want_write)?EPOLLOUT:0u);
    // dup2 (AcceptEx) can drop a registration behind our back; fall back
    // between ADD and MOD.
    int result=::epoll_ctl(queue,state.interest?EPOLL_CTL_MOD:EPOLL_CTL_ADD,fd,&change);
    if(result<0 && errno==ENOENT) result=::epoll_ctl(queue,EPOLL_CTL_ADD,fd,&change);
    else if(result<0 && errno==EEXIST) result=::epoll_ctl(queue,EPOLL_CTL_MOD,fd,&change);
    const bool ok=result==0;
#else
    struct kevent changes[2],receipts[2];
    EV_SET(&changes[0],fd,EVFILT_READ,((events&want_read)?EV_ADD:EV_DELETE)|EV_RECEIPT,0,0,nullptr);
    EV_SET(&changes[1],fd,EVFILT_WRITE,((events&want_write)?EV_ADD:EV_DELETE)|EV_RECEIPT,0,0,nullptr);
    // EV_RECEIPT reports each change in receipts; ENOENT from deleting an
    // absent filter is expected, a failed EV_ADD is not.
    const int count=::kevent(queue,changes,2,receipts,2,nullptr);
    bool ok=count==2;
    for(int i=0;ok && i<count;++i)
        if((receipts[i].flags&EV_ERROR) && receipts[i].data &&
           ((i==0 && (events&want_read)) || (i==1 && (events&want_write)))) ok=false;
#endif
    if(!ok) {drop_registration(fd);state.interest=0;return false;}
    state.interest=events;
    return true;
}
DWORD error_from_errno(int error) {
    switch(error) {
    case ECONNRESET: case EPIPE: return ERROR_NETNAME_DELETED;
    case ECONNABORTED: return ERROR_CONNECTION_ABORTED;
    case ECONNREFUSED: return WSAECONNREFUSED;
    case ETIMEDOUT: return WSAETIMEDOUT;
    default: return static_cast<DWORD>(WSAECONNRESET);
    }
}
void complete(int fd,const Pending& op,DWORD bytes,bool ok,DWORD error) {
    auto it=sockets.find(fd);
    if(it==sockets.end()) return;
    if(auto port=it->second.port.lock()) port->post({bytes,it->second.key,op.overlapped,ok,error});
}
// Fails every pending operation of a socket the poller rejected.
void fail_pending(int fd) {
    auto it=sockets.find(fd);
    if(it==sockets.end()) return;
    auto pending=std::move(it->second.pending);it->second.pending.clear();
    for(const auto& op:pending) complete(fd,op,0,false,ERROR_INVALID_HANDLE);
    // A state left for a closed descriptor would capture the next socket that
    // reuses the number.
    if(::fcntl(fd,F_GETFD)<0) sockets.erase(fd);
}
// Returns true when the operation finished (successfully or not).
bool perform(int fd,Pending& op) {
    switch(op.kind) {
    case Pending::recv: {
        const auto n=::recv(fd,op.buffer,op.length,0);
        if(n>=0) {complete(fd,op,static_cast<DWORD>(n),true,0);return true;}
        if(errno==EAGAIN || errno==EWOULDBLOCK || errno==EINTR) return false;
        complete(fd,op,0,false,error_from_errno(errno));return true;
    }
    case Pending::send: {
        while(op.done<op.length) {
            const auto n=::send(fd,op.buffer+op.done,op.length-op.done,FOURSTORY_SEND_FLAGS);
            if(n>0) {op.done+=static_cast<DWORD>(n);continue;}
            if(n<0 && (errno==EAGAIN || errno==EWOULDBLOCK || errno==EINTR)) return false;
            complete(fd,op,op.done,false,error_from_errno(errno));return true;
        }
        complete(fd,op,op.done,true,0);return true;
    }
    case Pending::accept: {
        sockaddr_in remote{};socklen_t size=sizeof(remote);
        const int accepted=::accept(fd,reinterpret_cast<sockaddr*>(&remote),&size);
        if(accepted<0) {
            if(errno==EAGAIN || errno==EWOULDBLOCK || errno==EINTR || errno==ECONNABORTED) return false;
            complete(fd,op,0,false,error_from_errno(errno));return true;
        }
        // The connection takes over the caller's pre-created accept socket.
        const bool replaced=::dup2(accepted,op.accept_fd)>=0;
        ::close(accepted);
        if(!replaced) {complete(fd,op,0,false,ERROR_INVALID_HANDLE);return true;}
        const int flags=::fcntl(op.accept_fd,F_GETFL,0);
        ::fcntl(op.accept_fd,F_SETFL,flags|O_NONBLOCK);
#ifdef SO_NOSIGPIPE
        const int one=1;::setsockopt(op.accept_fd,SOL_SOCKET,SO_NOSIGPIPE,&one,sizeof(one));
#endif
        sockaddr_in local{};socklen_t local_size=sizeof(local);
        ::getsockname(op.accept_fd,reinterpret_cast<sockaddr*>(&local),&local_size);
        std::memcpy(op.buffer+6,&local,sizeof(local));
        std::memcpy(op.buffer+ACCEPTEX_REMOTE_OFFSET,&remote,sizeof(remote));
        complete(fd,op,0,true,0);return true;
    }
    case Pending::connect: {
        pollfd probe{fd,POLLOUT,0};
        if(::poll(&probe,1,0)<=0) return false;
        int error=0;socklen_t size=sizeof(error);
        ::getsockopt(fd,SOL_SOCKET,SO_ERROR,&error,&size);
        complete(fd,op,0,error==0,error?error_from_errno(error):0);return true;
    }
    }
    return true;
}
void reactor() {
    constexpr int capacity=256;
#ifdef __linux__
    epoll_event events[capacity];
#else
    struct kevent events[capacity];
#endif
    for(;;) {
#ifdef __linux__
        const int count=::epoll_wait(poller,events,capacity,-1);
#else
        const int count=::kevent(poller,nullptr,0,events,capacity,nullptr);
#endif
        if(count<0) {if(errno==EINTR) continue;std::abort();}
        std::lock_guard lock(sockets_mutex);
        for(int i=0;i<count;++i) {
#ifdef __linux__
            const int fd=events[i].data.fd;
#else
            const int fd=static_cast<int>(events[i].ident);
#endif
            auto it=sockets.find(fd);
            // A stale event for a closed (or reused) descriptor is harmless:
            // operations that are not ready return EAGAIN and stay pending.
            if(it==sockets.end()) {drop_registration(fd);continue;}
            auto& pending=it->second.pending;
            for(std::size_t at=0;at<pending.size();) {
                auto op=pending[at];
                const bool done=perform(fd,op);
                it=sockets.find(fd);
                if(it==sockets.end()) break;
                if(done) pending.erase(pending.begin()+static_cast<std::ptrdiff_t>(at));
                else {pending[at]=op;++at;}
            }
            if(it!=sockets.end() && !update_interest(fd,it->second)) fail_pending(fd);
        }
    }
}
void start_reactor() {
    std::call_once(reactor_once,[] {
#ifdef __linux__
        poller=::epoll_create1(EPOLL_CLOEXEC);
#else
        poller=::kqueue();
#endif
        if(poller.load()<0) throw std::runtime_error("Cannot create socket reactor poller");
        std::thread(reactor).detach();
    });
}
int queue_operation(SOCKET socket,Pending op) {
    const int fd=fd_of(socket);
    if(fd<0) {errno=EBADF;return SOCKET_ERROR;}
    start_reactor();
    bool registered;
    {
        std::lock_guard lock(sockets_mutex);
        auto& state=sockets[fd];
        state.pending.push_back(op);
        registered=update_interest(fd,state);
        // A closed descriptor is rejected at once, as Winsock rejects an
        // invalid socket, instead of leaving an operation that never runs.
        if(!registered) {state.pending.pop_back();fail_pending(fd);}
    }
    if(!registered) {errno=WSAENOTSOCK;return SOCKET_ERROR;}
    errno=WSA_IO_PENDING;return SOCKET_ERROR;
}

// ----------------------------------------------------------------------------
// Files
struct File final : Object {
    int fd=-1;
    ~File() override {if(fd>=0) ::close(fd);}
};
// Win32 path to POSIX: separators, and a drive prefix ("C:\\dir\\file", which
// the servers hardcode for some logs) becomes $FOURSTORY_DRIVE_ROOT/C/dir/file.
// Without FOURSTORY_DRIVE_ROOT a drive path stays unusable, like a missing
// drive on Windows, instead of creating oddly named files in the working
// directory. `create_parents` prepares the directories for a file about to be
// written.
std::string native_path(LPCSTR path,bool create_parents=false) {
    std::string result(path?path:"");
    for(auto& c:result) if(c=='\\') c='/';
    if(result.size()>=3 && std::isalpha(static_cast<unsigned char>(result[0])) && result[1]==':' && result[2]=='/') {
        const char* root=std::getenv("FOURSTORY_DRIVE_ROOT");
        if(!root || !*root) return std::string();
        result=std::string(root)+"/"+std::string(1,static_cast<char>(std::toupper(static_cast<unsigned char>(result[0]))))+result.substr(2);
        if(create_parents)
            for(std::size_t at=result.find('/',1);at!=std::string::npos;at=result.find('/',at+1))
                ::mkdir(result.substr(0,at).c_str(),0755);
    }
    return result;
}

// ----------------------------------------------------------------------------
// Registry file
struct RegistryValue {DWORD type;std::string data;};
struct Key final : Object {std::string path;};
std::mutex registry_mutex;
bool registry_loaded=false;
std::map<std::string,std::map<std::string,RegistryValue>> registry;

std::string lower(std::string text) {
    for(auto& c:text) c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}
std::string trim(const std::string& text) {
    const auto first=text.find_first_not_of(" \t\r\n");
    if(first==std::string::npos) return {};
    return text.substr(first,text.find_last_not_of(" \t\r\n")-first+1);
}
// Format: [KEY\PATH] sections and Name=sz:text | dword:number | env:VARIABLE.
// env: values resolve from the environment so secrets stay out of files.
void load_registry_locked() {
    if(registry_loaded) return;
    registry_loaded=true;
    const char* path=std::getenv("FOURSTORY_REGISTRY");
    if(!path) return;
    std::ifstream input(path);
    if(!input) throw std::runtime_error("Cannot open FOURSTORY_REGISTRY file");
    std::string line,section;int number=0;
    while(std::getline(input,line)) {
        ++number;line=trim(line);
        if(line.empty() || line[0]=='#' || line[0]==';') continue;
        if(line.front()=='[' && line.back()==']') {section=lower(line.substr(1,line.size()-2));continue;}
        const auto equals=line.find('=');
        const auto colon=line.find(':',equals);
        if(section.empty() || equals==std::string::npos || colon==std::string::npos)
            throw std::runtime_error("Invalid registry line "+std::to_string(number));
        const auto name=lower(trim(line.substr(0,equals)));
        const auto kind=trim(line.substr(equals+1,colon-equals-1));
        const auto value=line.substr(colon+1);
        if(kind=="sz") registry[section][name]={REG_SZ,value};
        else if(kind=="env") {
            const char* resolved=std::getenv(value.c_str());
            if(!resolved) throw std::runtime_error("Registry environment variable is missing: "+value);
            registry[section][name]={REG_SZ,resolved};
        } else if(kind=="dword") {
            char* end=nullptr;errno=0;
            const auto parsed=std::strtoull(value.c_str(),&end,0);
            if(errno || !end || *end || parsed>0xFFFFFFFFull)
                throw std::runtime_error("Invalid registry DWORD on line "+std::to_string(number));
            const DWORD dword=static_cast<DWORD>(parsed);
            registry[section][name]={REG_DWORD,std::string(reinterpret_cast<const char*>(&dword),4)};
        } else throw std::runtime_error("Unknown registry value type on line "+std::to_string(number));
    }
}
std::string key_path(HKEY root,LPCSTR path) {
    std::string prefix;
    if(root==HKEY_LOCAL_MACHINE) prefix="";
    else if(root==HKEY_CURRENT_USER) prefix="hkcu\\";
    else if(auto key=find<Key>(root)) prefix=key->path+"\\";
    else return {};
    return lower(prefix+(path?path:""));
}

}

namespace {
std::mutex pointer_handles_mutex;
std::unordered_map<void*,DWORD> pointer_ids;
std::vector<void*> handle_pointers{nullptr};
}
DWORD pointer_handle(void* pointer) {
    if(!pointer) return 0;
    std::lock_guard lock(pointer_handles_mutex);
    const auto found=pointer_ids.find(pointer);
    if(found!=pointer_ids.end()) return found->second;
    if(handle_pointers.size()>=0xFFFFFFFFu) throw std::length_error("Pointer handle space exhausted");
    const auto id=static_cast<DWORD>(handle_pointers.size());
    handle_pointers.push_back(pointer);pointer_ids.emplace(pointer,id);return id;
}
void* handle_pointer(DWORD handle) {
    std::lock_guard lock(pointer_handles_mutex);
    return handle<handle_pointers.size()?handle_pointers[handle]:nullptr;
}

namespace {
std::mutex ready_mutex;
std::condition_variable ready_changed;
unsigned ready_expected=0,ready_count=0;
}
void expect_ready_workers(unsigned count) {std::lock_guard lock(ready_mutex);ready_expected=count;}
void worker_ready() {
    {std::lock_guard lock(ready_mutex);++ready_count;}
    ready_changed.notify_all();
}
bool wait_ready_workers(DWORD milliseconds) {
    std::unique_lock lock(ready_mutex);
    return ready_changed.wait_for(lock,std::chrono::milliseconds(milliseconds),[]{
        return ready_count>=ready_expected || stop_flag.load();
    }) && ready_count>=ready_expected;
}
void log_event(const char* service,const char* text);
std::string resolve_secret(const char* value) {
    if(!value) return {};
    if(std::strncmp(value,"env:",4)!=0) return value;
    const char* resolved=std::getenv(value+4);
    if(resolved) return resolved;
    // An unresolved secret fails the connection like a wrong password would.
    log_event("compat",(std::string("Database secret environment variable is missing: ")+(value+4)).c_str());
    return {};
}

void request_stop() noexcept {stop_flag=true;}
bool stop_requested() noexcept {return stop_flag;}
void wait_for_stop() {while(!stop_flag) std::this_thread::sleep_for(std::chrono::milliseconds(50));}

void log_event(const char* service,const char* text) {
    char stamp[32];const auto now=std::time(nullptr);std::tm local{};localtime_r(&now,&local);
    std::strftime(stamp,sizeof(stamp),"%Y-%m-%d %H:%M:%S",&local);
    std::fprintf(stderr,"%s %s: %s\n",stamp,service,text);std::fflush(stderr);
}
}

using namespace fourstory::win32;
#ifdef __APPLE__
// <mach-o/dyld.h> defines TRUE/FALSE enumerators that clash with Win32 names.
extern "C" int _NSGetExecutablePath(char* buffer,uint32_t* size);
#endif

DWORD GetTickCount() {
    return static_cast<DWORD>(std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now()-process_start).count());
}
ULONGLONG GetTickCount64() {
    return static_cast<ULONGLONG>(std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now()-process_start).count());
}
void Sleep(DWORD milliseconds) {
    if(milliseconds==0) {std::this_thread::yield();return;}
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}
static void fill_system_time(LPSYSTEMTIME out,bool utc) {
    const auto now=std::chrono::system_clock::now();
    const auto seconds=std::chrono::system_clock::to_time_t(now);
    const auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()%1000;
    std::tm value{};
    if(utc) gmtime_r(&seconds,&value); else localtime_r(&seconds,&value);
    *out={static_cast<WORD>(value.tm_year+1900),static_cast<WORD>(value.tm_mon+1),static_cast<WORD>(value.tm_wday),
        static_cast<WORD>(value.tm_mday),static_cast<WORD>(value.tm_hour),static_cast<WORD>(value.tm_min),
        static_cast<WORD>(value.tm_sec),static_cast<WORD>(ms)};
}
void GetLocalTime(LPSYSTEMTIME time) {fill_system_time(time,false);}
void GetSystemTime(LPSYSTEMTIME time) {fill_system_time(time,true);}

HANDLE CreateEvent(LPSECURITY_ATTRIBUTES,BOOL manual_reset,BOOL initial_state,LPCSTR) {
    auto event=std::make_shared<Event>();event->manual=manual_reset;event->signaled=initial_state;
    return publish(event);
}
BOOL SetEvent(HANDLE handle) {
    auto event=find<Event>(handle);
    if(!event) {SetLastError(ERROR_INVALID_HANDLE);return FALSE;}
    {std::lock_guard lock(event->mutex);event->signaled=true;}
    event->changed.notify_all();return TRUE;
}
BOOL ResetEvent(HANDLE handle) {
    auto event=find<Event>(handle);
    if(!event) {SetLastError(ERROR_INVALID_HANDLE);return FALSE;}
    std::lock_guard lock(event->mutex);event->signaled=false;return TRUE;
}
DWORD WaitForSingleObject(HANDLE handle,DWORD milliseconds) {
    auto object=find<Waitable>(handle);
    if(!object) {SetLastError(ERROR_INVALID_HANDLE);return WAIT_FAILED;}
    std::unique_lock lock(object->mutex);
    auto ready=[&]{return object->signaled_locked();};
    if(milliseconds==INFINITE) object->changed.wait(lock,ready);
    else if(!object->changed.wait_for(lock,std::chrono::milliseconds(milliseconds),ready)) return WAIT_TIMEOUT;
    object->consume_locked();return WAIT_OBJECT_0;
}
DWORD WaitForMultipleObjects(DWORD count,const HANDLE* list,BOOL wait_all,DWORD milliseconds) {
    if(!count || !list) {SetLastError(ERROR_INVALID_HANDLE);return WAIT_FAILED;}
    const auto deadline=Clock::now()+std::chrono::milliseconds(milliseconds==INFINITE?0:milliseconds);
    // Bounded polling keeps this simple; callers use it for shutdown joins.
    for(;;) {
        DWORD signaled=0;
        for(DWORD i=0;i<count;++i) {
            auto object=find<Waitable>(list[i]);
            if(!object) {SetLastError(ERROR_INVALID_HANDLE);return WAIT_FAILED;}
            std::lock_guard lock(object->mutex);
            if(object->signaled_locked()) {
                if(!wait_all) {object->consume_locked();return WAIT_OBJECT_0+i;}
                ++signaled;
            }
        }
        if(wait_all && signaled==count) {
            for(DWORD i=0;i<count;++i) {auto object=find<Waitable>(list[i]);std::lock_guard lock(object->mutex);object->consume_locked();}
            return WAIT_OBJECT_0;
        }
        if(milliseconds!=INFINITE && Clock::now()>=deadline) return WAIT_TIMEOUT;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}
BOOL CloseHandle(HANDLE handle) {
    std::lock_guard lock(handles_mutex);
    if(!handles.erase(static_cast<Object*>(handle))) {SetLastError(ERROR_INVALID_HANDLE);return FALSE;}
    return TRUE;
}
HANDLE CreateThread(LPSECURITY_ATTRIBUTES,SIZE_T,LPTHREAD_START_ROUTINE start,LPVOID parameter,DWORD flags,LPDWORD thread_id) {
    auto thread=std::make_shared<Thread>();
    thread->start=start;thread->parameter=parameter;
    thread->suspend=(flags&CREATE_SUSPENDED)?1:0;
    std::mutex id_mutex;std::condition_variable id_ready;bool have_id=false;
    std::thread([thread,&id_mutex,&id_ready,&have_id] {
        // Notify under the lock: the creator's stack objects die once it returns.
        {std::lock_guard lock(id_mutex);thread->id=current_thread_id();have_id=true;id_ready.notify_one();}
        {std::unique_lock lock(thread->mutex);thread->changed.wait(lock,[&]{return thread->suspend==0;});}
        const DWORD code=thread->start(thread->parameter);
        {std::lock_guard lock(thread->mutex);thread->exit_code=code;thread->finished=true;}
        thread->changed.notify_all();
    }).detach();
    {std::unique_lock lock(id_mutex);id_ready.wait(lock,[&]{return have_id;});}
    if(thread_id) *thread_id=thread->id;
    return publish(thread);
}
DWORD ResumeThread(HANDLE handle) {
    auto thread=find<Thread>(handle);
    if(!thread) {SetLastError(ERROR_INVALID_HANDLE);return 0xFFFFFFFFu;}
    DWORD previous;
    {std::lock_guard lock(thread->mutex);previous=thread->suspend;if(thread->suspend) --thread->suspend;}
    thread->changed.notify_all();return previous;
}
DWORD SuspendThread(HANDLE handle) {
    auto thread=find<Thread>(handle);
    if(!thread) {SetLastError(ERROR_INVALID_HANDLE);return 0xFFFFFFFFu;}
    std::lock_guard lock(thread->mutex);
    // Only a not-yet-started thread can be suspended natively.
    if(thread->finished) return 0xFFFFFFFFu;
    return thread->suspend++;
}
BOOL GetExitCodeThread(HANDLE handle,LPDWORD code) {
    auto thread=find<Thread>(handle);
    if(!thread || !code) {SetLastError(ERROR_INVALID_HANDLE);return FALSE;}
    std::lock_guard lock(thread->mutex);*code=thread->exit_code;return TRUE;
}
BOOL TerminateThread(HANDLE,DWORD) {
    // Forced termination is not supported; the original callers use it only
    // after a cooperative shutdown wait has timed out.
    SetLastError(ERROR_INVALID_HANDLE);return FALSE;
}
BOOL SetThreadPriority(HANDLE,int) {return TRUE;}
DWORD GetCurrentThreadId() {return current_thread_id();}
BOOL PostThreadMessage(DWORD,UINT message,WPARAM,LPARAM) {
    if(message==WM_QUIT) request_stop();
    return TRUE;
}

HANDLE CreateIoCompletionPort(HANDLE file,HANDLE existing,ULONG_PTR key,DWORD) {
    if(file==INVALID_HANDLE_VALUE) {
        if(existing) {SetLastError(ERROR_INVALID_HANDLE);return nullptr;}
        return publish(std::make_shared<Port>());
    }
    auto port=find<Port>(existing);
    const int fd=fd_of(reinterpret_cast<SOCKET>(file));
    if(!port || fd<0) {SetLastError(ERROR_INVALID_HANDLE);return nullptr;}
    std::lock_guard lock(sockets_mutex);
    auto& state=sockets[fd];state.port=port;state.key=key;
    return existing;
}
BOOL GetQueuedCompletionStatus(HANDLE handle,LPDWORD bytes,PULONG_PTR key,LPOVERLAPPED* overlapped,DWORD milliseconds) {
    auto port=find<Port>(handle);
    if(!port) {SetLastError(ERROR_INVALID_HANDLE);if(overlapped) *overlapped=nullptr;return FALSE;}
    std::unique_lock lock(port->mutex);
    auto ready=[&]{return !port->queue.empty();};
    if(milliseconds==INFINITE) port->ready.wait(lock,ready);
    else if(!port->ready.wait_for(lock,std::chrono::milliseconds(milliseconds),ready)) {
        if(overlapped) *overlapped=nullptr;
        SetLastError(WAIT_TIMEOUT);return FALSE;
    }
    const auto completion=port->queue.front();port->queue.pop_front();
    if(bytes) *bytes=completion.bytes;
    if(key) *key=completion.key;
    if(overlapped) *overlapped=completion.overlapped;
    if(!completion.ok) {SetLastError(completion.error);return FALSE;}
    return TRUE;
}
BOOL GetQueuedCompletionStatus(HANDLE port,LPDWORD bytes,DWORD* key,LPOVERLAPPED* overlapped,DWORD milliseconds) {
    ULONG_PTR wide=0;
    const BOOL result=GetQueuedCompletionStatus(port,bytes,&wide,overlapped,milliseconds);
    if(key) *key=static_cast<DWORD>(wide);
    return result;
}
BOOL PostQueuedCompletionStatus(HANDLE handle,DWORD bytes,ULONG_PTR key,LPOVERLAPPED overlapped) {
    auto port=find<Port>(handle);
    if(!port) {SetLastError(ERROR_INVALID_HANDLE);return FALSE;}
    port->post({bytes,key,overlapped,true,0});return TRUE;
}

FILE* fourstory_fopen(const char* path,const char* mode) {
    const bool writes=mode && (std::strchr(mode,'w') || std::strchr(mode,'a') || std::strchr(mode,'+'));
    const auto result=native_path(path,writes);
    if(result.empty()) {errno=ENOENT;return nullptr;}
    return std::fopen(result.c_str(),mode);
}
DWORD GetModuleFileName(HMODULE,LPSTR path,DWORD size) {
    if(!path || !size) return 0;
    char buffer[4096];
#ifdef __APPLE__
    uint32_t length=sizeof(buffer);
    if(_NSGetExecutablePath(buffer,&length)!=0) return 0;
#else
    ssize_t length=::readlink("/proc/self/exe",buffer,sizeof(buffer)-1);
    if(length<=0) return 0;
    buffer[length]=0;
    // An executable replaced while running reads back as "<path> (deleted)".
    static const char deleted[]=" (deleted)";
    const ssize_t suffix=sizeof(deleted)-1;
    if(length>suffix && std::strcmp(buffer+length-suffix,deleted)==0) buffer[length-suffix]=0;
#endif
    // Win32 separators: the servers locate sibling files with strrchr('\\')
    // (Login's TClient.exe reference, log files). With POSIX separators that
    // search fails and they open the executable itself. File calls and fopen
    // translate the separators back.
    for(char* c=buffer;*c;++c) if(*c=='/') *c='\\';
    const auto actual=std::strlen(buffer);
    if(actual>=size) {std::memcpy(path,buffer,size-1);path[size-1]=0;return size;}
    std::memcpy(path,buffer,actual+1);return static_cast<DWORD>(actual);
}
DWORD GetCurrentDirectory(DWORD size,LPSTR path) {
    char buffer[4096];
    if(!::getcwd(buffer,sizeof(buffer))) return 0;
    const auto length=std::strlen(buffer);
    if(!path || length>=size) return static_cast<DWORD>(length+1);
    std::memcpy(path,buffer,length+1);return static_cast<DWORD>(length);
}
void OutputDebugString(LPCSTR) {}
HANDLE CreateFile(LPCSTR path,DWORD access,DWORD,LPSECURITY_ATTRIBUTES,DWORD disposition,DWORD,HANDLE) {
    int flags=0;
    if((access&GENERIC_READ) && (access&GENERIC_WRITE)) flags=O_RDWR;
    else if(access&GENERIC_WRITE) flags=O_WRONLY;
    else flags=O_RDONLY;
    switch(disposition) {
    case CREATE_NEW: flags|=O_CREAT|O_EXCL;break;
    case CREATE_ALWAYS: flags|=O_CREAT|O_TRUNC;break;
    case OPEN_EXISTING: break;
    case OPEN_ALWAYS: flags|=O_CREAT;break;
    case TRUNCATE_EXISTING: flags|=O_TRUNC;break;
    default: SetLastError(ERROR_INVALID_HANDLE);return INVALID_HANDLE_VALUE;
    }
    const int fd=::open(native_path(path).c_str(),flags|O_CLOEXEC,0644);
    if(fd<0) {SetLastError(ERROR_FILE_NOT_FOUND);return INVALID_HANDLE_VALUE;}
    auto file=std::make_shared<File>();file->fd=fd;return publish(file);
}
BOOL ReadFile(HANDLE handle,LPVOID buffer,DWORD bytes,LPDWORD read,LPOVERLAPPED) {
    auto file=find<File>(handle);
    if(!file) {SetLastError(ERROR_INVALID_HANDLE);return FALSE;}
    const auto n=::read(file->fd,buffer,bytes);
    if(n<0) {if(read) *read=0;return FALSE;}
    if(read) *read=static_cast<DWORD>(n);
    return TRUE;
}
BOOL WriteFile(HANDLE handle,LPCVOID buffer,DWORD bytes,LPDWORD written,LPOVERLAPPED) {
    auto file=find<File>(handle);
    if(!file) {SetLastError(ERROR_INVALID_HANDLE);return FALSE;}
    DWORD done=0;
    while(done<bytes) {
        const auto n=::write(file->fd,static_cast<const char*>(buffer)+done,bytes-done);
        if(n<=0) {if(written) *written=done;return FALSE;}
        done+=static_cast<DWORD>(n);
    }
    if(written) *written=done;
    return TRUE;
}
DWORD GetFileSize(HANDLE handle,LPDWORD high) {
    auto file=find<File>(handle);
    struct stat info{};
    if(!file || ::fstat(file->fd,&info)<0) {SetLastError(ERROR_INVALID_HANDLE);return INVALID_FILE_SIZE;}
    const auto size=static_cast<std::uint64_t>(info.st_size);
    if(high) *high=static_cast<DWORD>(size>>32);
    return static_cast<DWORD>(size);
}
DWORD SetFilePointer(HANDLE handle,LONG distance,LONG* high,DWORD method) {
    auto file=find<File>(handle);
    if(!file) {SetLastError(ERROR_INVALID_HANDLE);return INVALID_SET_FILE_POINTER;}
    std::int64_t offset=distance;
    if(high) offset=(static_cast<std::int64_t>(*high)<<32)|static_cast<std::uint32_t>(distance);
    const int whence=method==FILE_BEGIN?SEEK_SET:method==FILE_CURRENT?SEEK_CUR:SEEK_END;
    const auto result=::lseek(file->fd,offset,whence);
    if(result<0) return INVALID_SET_FILE_POINTER;
    if(high) *high=static_cast<LONG>(result>>32);
    return static_cast<DWORD>(result);
}
BOOL FlushFileBuffers(HANDLE handle) {
    auto file=find<File>(handle);
    return file && ::fsync(file->fd)==0;
}
BOOL DeleteFile(LPCSTR path) {return ::unlink(native_path(path).c_str())==0;}
BOOL CreateDirectory(LPCSTR path,LPSECURITY_ATTRIBUTES) {return ::mkdir(native_path(path).c_str(),0755)==0;}
DWORD GetFileAttributes(LPCSTR path) {
    struct stat info{};
    if(::stat(native_path(path).c_str(),&info)<0) return INVALID_FILE_ATTRIBUTES;
    return S_ISDIR(info.st_mode)?FILE_ATTRIBUTE_DIRECTORY:FILE_ATTRIBUTE_NORMAL;
}
void GetSystemInfo(LPSYSTEM_INFO info) {
    if(!info) return;
    std::memset(info,0,sizeof(*info));
    const auto cores=std::thread::hardware_concurrency();
    info->dwNumberOfProcessors=cores?cores:1;
    info->dwPageSize=static_cast<DWORD>(::getpagesize());
    info->dwAllocationGranularity=65536;
}

LONG RegCreateKeyEx(HKEY root,LPCSTR path,DWORD,LPSTR,DWORD,REGSAM,LPSECURITY_ATTRIBUTES,PHKEY result,LPDWORD disposition) {
    const auto full=key_path(root,path);
    if(full.empty() || !result) return ERROR_INVALID_HANDLE;
    {
        std::lock_guard lock(registry_mutex);
        load_registry_locked();
        if(disposition) *disposition=registry.count(full)?2:1;
        registry[full];
    }
    auto key=std::make_shared<Key>();key->path=full;*result=static_cast<HKEY>(publish(key));
    return ERROR_SUCCESS;
}
LONG RegCreateKey(HKEY root,LPCSTR path,PHKEY result) {
    return RegCreateKeyEx(root,path,0,nullptr,0,KEY_ALL_ACCESS,nullptr,result,nullptr);
}
LONG RegOpenKeyEx(HKEY root,LPCSTR path,DWORD,REGSAM,PHKEY result) {
    const auto full=key_path(root,path);
    if(full.empty() || !result) return ERROR_INVALID_HANDLE;
    {
        std::lock_guard lock(registry_mutex);
        load_registry_locked();
        if(!registry.count(full)) return ERROR_FILE_NOT_FOUND;
    }
    auto key=std::make_shared<Key>();key->path=full;*result=static_cast<HKEY>(publish(key));
    return ERROR_SUCCESS;
}
LONG RegOpenKey(HKEY root,LPCSTR path,PHKEY result) {return RegOpenKeyEx(root,path,0,KEY_ALL_ACCESS,result);}
LONG RegQueryValueEx(HKEY handle,LPCSTR name,LPDWORD,LPDWORD type,LPBYTE data,LPDWORD size) {
    auto key=find<Key>(handle);
    if(!key) return ERROR_INVALID_HANDLE;
    std::lock_guard lock(registry_mutex);
    load_registry_locked();
    auto& values=registry[key->path];
    const auto it=values.find(lower(name?name:""));
    if(it==values.end()) return ERROR_FILE_NOT_FOUND;
    const auto& value=it->second;
    if(type) *type=value.type;
    const DWORD needed=static_cast<DWORD>(value.data.size()+(value.type==REG_SZ?1:0));
    if(!data) {if(size) *size=needed;return ERROR_SUCCESS;}
    if(!size || *size<needed) {if(size) *size=needed;return ERROR_MORE_DATA;}
    std::memcpy(data,value.data.data(),value.data.size());
    if(value.type==REG_SZ) data[value.data.size()]=0;
    *size=needed;return ERROR_SUCCESS;
}
LONG RegSetValueEx(HKEY handle,LPCSTR name,DWORD,DWORD type,const BYTE* data,DWORD size) {
    auto key=find<Key>(handle);
    if(!key || (!data && size)) return ERROR_INVALID_HANDLE;
    std::string bytes(reinterpret_cast<const char*>(data),size);
    if(type==REG_SZ) bytes=std::string(bytes.c_str());
    // In-process only: native configuration files are not rewritten.
    std::lock_guard lock(registry_mutex);
    registry[key->path][lower(name?name:"")]={type,bytes};
    return ERROR_SUCCESS;
}
LONG RegDeleteValue(HKEY handle,LPCSTR name) {
    auto key=find<Key>(handle);
    if(!key) return ERROR_INVALID_HANDLE;
    std::lock_guard lock(registry_mutex);
    return registry[key->path].erase(lower(name?name:""))?ERROR_SUCCESS:ERROR_FILE_NOT_FOUND;
}
LONG RegCloseKey(HKEY key) {return CloseHandle(key)?ERROR_SUCCESS:ERROR_INVALID_HANDLE;}

// ----------------------------------------------------------------------------
// Winsock
int WSAStartup(WORD,LPWSADATA data) {
    if(data) {std::memset(data,0,sizeof(*data));data->wVersion=data->wHighVersion=MAKEWORD(2,2);}
    start_reactor();return 0;
}
int WSACleanup() {return 0;}
int WSAGetLastError() {
    switch(errno) {
    case EAGAIN: case EINPROGRESS: return WSAEWOULDBLOCK;
    case ECONNRESET: return WSAECONNRESET;
    case ECONNREFUSED: return WSAECONNREFUSED;
    case ECONNABORTED: return WSAECONNABORTED;
    case ETIMEDOUT: return WSAETIMEDOUT;
    case ENOTCONN: return WSAENOTCONN;
    case EADDRINUSE: return WSAEADDRINUSE;
    case EINVAL: return WSAEINVAL;
    case ENOTSOCK: case EBADF: return WSAENOTSOCK;
    default: return errno;
    }
}
void WSASetLastError(int error) {errno=error;}
SOCKET WSASocket(int family,int type,int protocol,void*,unsigned,DWORD) {
    const int fd=::socket(family,type,protocol);
    if(fd<0) return INVALID_SOCKET;
    // Linux has no SO_NOSIGPIPE; module_main ignores SIGPIPE for the process.
#ifdef SO_NOSIGPIPE
    const int one=1;::setsockopt(fd,SOL_SOCKET,SO_NOSIGPIPE,&one,sizeof(one));
#endif
    return static_cast<SOCKET>(fd);
}
int closesocket(SOCKET socket) {
    const int fd=fd_of(socket);
    if(fd<0) {errno=EBADF;return SOCKET_ERROR;}
    {
        // Pending overlapped operations complete with ERROR_OPERATION_ABORTED.
        std::lock_guard lock(sockets_mutex);
        auto it=sockets.find(fd);
        if(it!=sockets.end()) {
            auto pending=std::move(it->second.pending);it->second.pending.clear();
            for(const auto& op:pending) complete(fd,op,0,false,ERROR_OPERATION_ABORTED);
            if(it->second.interest) drop_registration(fd);
            sockets.erase(fd);
        }
        // An AcceptEx waiting to reuse this socket is aborted as well.
        for(auto& [listen_fd,state]:sockets) {
            for(auto op=state.pending.begin();op!=state.pending.end();) {
                if(op->kind==Pending::accept && op->accept_fd==fd) {
                    complete(listen_fd,*op,0,false,ERROR_OPERATION_ABORTED);op=state.pending.erase(op);
                } else ++op;
            }
            update_interest(listen_fd,state);
        }
        // Closing the descriptor drops its epoll/kqueue registration.
        ::close(fd);
    }
    return 0;
}
int fourstory_ioctlsocket(SOCKET socket,long command,const void* argument) {
    const int fd=fd_of(socket);
    if(fd<0 || !argument) {errno=EBADF;return SOCKET_ERROR;}
    if(static_cast<unsigned long>(command)!=static_cast<unsigned long>(FIONBIO)) {errno=EINVAL;return SOCKET_ERROR;}
    // Win32 u_long is 32 bits; read exactly that much from the caller.
    std::uint32_t enable;std::memcpy(&enable,argument,sizeof(enable));
    const int flags=::fcntl(fd,F_GETFL,0);
    return ::fcntl(fd,F_SETFL,enable?(flags|O_NONBLOCK):(flags&~O_NONBLOCK))<0?SOCKET_ERROR:0;
}
int fourstory_setsockopt(SOCKET socket,int level,int name,const void* value,int length) {
    const int fd=fd_of(socket);
    if(fd<0) {errno=EBADF;return SOCKET_ERROR;}
    if(level==SOL_SOCKET && (name==SO_UPDATE_ACCEPT_CONTEXT || name==SO_UPDATE_CONNECT_CONTEXT)) return 0;
    // Win32 SO_SNDBUF=0 selects direct overlapped sends; POSIX keeps its buffer.
    if(level==SOL_SOCKET && name==SO_SNDBUF && length==sizeof(int) && value) {
        int size;std::memcpy(&size,value,sizeof(size));
        if(size==0) return 0;
    }
    return ::setsockopt(fd,level,name,value,static_cast<socklen_t>(length))<0?SOCKET_ERROR:0;
}
int fourstory_select(int,fd_set* read,fd_set* write,fd_set* except,const struct timeval* timeout) {
    timeval copy{};timeval* wait=nullptr;
    if(timeout) {copy=*timeout;wait=&copy;}
    const int result=::select(FD_SETSIZE,read,write,except,wait);
    if(result<=0 || !write) return result;
    // Win32 reports a failed nonblocking connect in exceptfds, not writefds.
    int remaining=result;
    for(int fd=0;fd<FD_SETSIZE;++fd) {
        if(!FD_ISSET(fd,write)) continue;
        int error=0;socklen_t size=sizeof(error);
        if(::getsockopt(fd,SOL_SOCKET,SO_ERROR,&error,&size)==0 && error) {
            FD_CLR(fd,write);--remaining;
            if(except) FD_SET(fd,except);
        }
    }
    return remaining;
}
int WSASend(SOCKET socket,LPWSABUF buffers,DWORD count,LPDWORD sent,DWORD,LPOVERLAPPED overlapped,LPWSAOVERLAPPED_COMPLETION_ROUTINE) {
    if(count!=1 || !buffers || !overlapped) {errno=EINVAL;return SOCKET_ERROR;}
    if(sent) *sent=0;
    Pending op{Pending::send,overlapped,buffers->buf,buffers->len,0,-1};
    // Winsock hands an overlapped send to the transport at once, so the data
    // is still delivered when the socket is closed right afterwards (Login
    // answers a rejected login and closes). Send what the kernel accepts now;
    // only a remainder waits for the reactor, and a completion is posted. A
    // connection that is already dead fails at once with no completion, as
    // Winsock does: a pending result there made CSession::OnSendComplete
    // re-post forever, so a session dropped by the peer never reached
    // Close() (stuck logout after a mass disconnect).
    const int fd=fd_of(socket);
    bool dead=false;
    if(fd>=0) {
        std::lock_guard lock(sockets_mutex);
        auto it=sockets.find(fd);
        bool queued=false;
        if(it!=sockets.end())
            for(const auto& pending:it->second.pending) if(pending.kind==Pending::send) queued=true;
        if(it!=sockets.end() && !queued) {
            while(op.done<op.length) {
                const auto n=::send(fd,op.buffer+op.done,op.length-op.done,MSG_DONTWAIT|FOURSTORY_SEND_FLAGS);
                if(n<0 && (errno==ECONNRESET || errno==EPIPE || errno==ENOTCONN ||
                           errno==ECONNABORTED || errno==ETIMEDOUT)) {dead=true;break;}
                if(n<=0) break;
                op.done+=static_cast<DWORD>(n);
            }
            if(op.done==op.length) {
                if(sent) *sent=op.done;
                complete(fd,op,op.done,true,0);
                return 0;
            }
        }
    }
    if(dead) {errno=ECONNRESET;return SOCKET_ERROR;}
    return queue_operation(socket,op);
}
int WSARecv(SOCKET socket,LPWSABUF buffers,DWORD count,LPDWORD received,LPDWORD,LPOVERLAPPED overlapped,LPWSAOVERLAPPED_COMPLETION_ROUTINE) {
    if(count!=1 || !buffers || !overlapped) {errno=EINVAL;return SOCKET_ERROR;}
    if(received) *received=0;
    Pending op{Pending::recv,overlapped,buffers->buf,buffers->len,0,-1};
    return queue_operation(socket,op);
}
BOOL AcceptEx(SOCKET listen,SOCKET accept,PVOID output,DWORD receive_length,DWORD local_length,DWORD remote_length,LPDWORD received,LPOVERLAPPED overlapped) {
    // The supplied servers request no initial receive data.
    if(receive_length || !output || !overlapped || fd_of(accept)<0 ||
       local_length+remote_length<ACCEPTEX_REMOTE_OFFSET+sizeof(sockaddr_in)) {errno=EINVAL;return FALSE;}
    if(received) *received=0;
    Pending op{Pending::accept,overlapped,static_cast<char*>(output),0,0,fd_of(accept)};
    queue_operation(listen,op);
    return FALSE;
}
void GetAcceptExSockaddrs(PVOID output,DWORD,DWORD,DWORD,LPSOCKADDR* local,LPINT local_size,LPSOCKADDR* remote,LPINT remote_size) {
    auto* bytes=static_cast<char*>(output);
    if(local) *local=reinterpret_cast<LPSOCKADDR>(bytes+6);
    if(local_size) *local_size=sizeof(sockaddr_in);
    if(remote) *remote=reinterpret_cast<LPSOCKADDR>(bytes+ACCEPTEX_REMOTE_OFFSET);
    if(remote_size) *remote_size=sizeof(sockaddr_in);
}
static BOOL connect_ex(SOCKET socket,const struct sockaddr* address,int length,PVOID,DWORD,LPDWORD,LPOVERLAPPED overlapped) {
    const int fd=fd_of(socket);
    if(fd<0 || !overlapped) {errno=EINVAL;return FALSE;}
    if(::connect(fd,address,static_cast<socklen_t>(length))<0 && errno!=EINPROGRESS) return FALSE;
    Pending op{Pending::connect,overlapped,nullptr,0,0,-1};
    queue_operation(socket,op);
    return FALSE;
}
int WSAIoctl(SOCKET,DWORD code,LPVOID in,DWORD in_size,LPVOID out,DWORD out_size,LPDWORD returned,LPOVERLAPPED,LPWSAOVERLAPPED_COMPLETION_ROUTINE) {
    const GUID connect_ex_id=WSAID_CONNECTEX;
    if(code!=SIO_GET_EXTENSION_FUNCTION_POINTER || in_size!=sizeof(GUID) || !in ||
       std::memcmp(in,&connect_ex_id,sizeof(GUID)) || out_size!=sizeof(LPFN_CONNECTEX) || !out) {
        errno=EINVAL;return SOCKET_ERROR;
    }
    const LPFN_CONNECTEX function=connect_ex;
    std::memcpy(out,&function,sizeof(function));
    if(returned) *returned=sizeof(function);
    return 0;
}

// ----------------------------------------------------------------------------
// Service Control Manager over FOURSTORY_SCM_DIR status files.
namespace {
struct ServiceManager final : Object {std::string directory;};
struct ServiceHandle final : Object {std::string directory,name;};
bool valid_service_name(const std::string& name) {
    if(name.empty() || name.size()>64) return false;
    for(const char c:name) if(!std::isalnum(static_cast<unsigned char>(c)) && c!='_' && c!='-') return false;
    return true;
}
struct ServiceRecord {DWORD state=SERVICE_STOPPED;pid_t pid=0;};
ServiceRecord read_service(const ServiceHandle& service) {
    ServiceRecord record;
    std::ifstream input(service.directory+"/"+service.name+".status");
    unsigned long state=0;long pid=0;
    if(!(input>>state>>pid) || pid<=0) return {};
    // A record whose process is gone describes a stopped service.
    if(::kill(static_cast<pid_t>(pid),0)!=0 && errno==ESRCH) return {};
    record.state=static_cast<DWORD>(state);record.pid=static_cast<pid_t>(pid);
    return record;
}
}
namespace fourstory::win32 {
void publish_service_status(const char* service,DWORD state) {
    const char* directory=std::getenv("FOURSTORY_SCM_DIR");
    if(!directory || !service || !valid_service_name(service)) return;
    const std::string path=std::string(directory)+"/"+service+".status";
    const std::string temporary=path+".tmp";
    {
        std::ofstream output(temporary,std::ios::trunc);
        output<<state<<' '<<::getpid()<<'\n';
        if(!output) return;
    }
    std::rename(temporary.c_str(),path.c_str());
}
}
SC_HANDLE OpenSCManager(LPCSTR,LPCSTR,DWORD) {
    const char* directory=std::getenv("FOURSTORY_SCM_DIR");
    if(!directory) {SetLastError(ERROR_INVALID_HANDLE);return nullptr;}
    auto manager=std::make_shared<ServiceManager>();manager->directory=directory;
    return publish(manager);
}
SC_HANDLE OpenService(SC_HANDLE manager_handle,LPCSTR name,DWORD) {
    auto manager=find<ServiceManager>(manager_handle);
    if(!manager || !name || !valid_service_name(name)) {SetLastError(ERROR_INVALID_HANDLE);return nullptr;}
    auto service=std::make_shared<ServiceHandle>();service->directory=manager->directory;service->name=name;
    return publish(service);
}
BOOL QueryServiceStatus(SC_HANDLE handle,SERVICE_STATUS* status) {
    auto service=find<ServiceHandle>(handle);
    if(!service || !status) {SetLastError(ERROR_INVALID_HANDLE);return FALSE;}
    std::memset(status,0,sizeof(*status));
    status->dwCurrentState=read_service(*service).state;
    return TRUE;
}
BOOL ControlService(SC_HANDLE handle,DWORD control,SERVICE_STATUS* status) {
    auto service=find<ServiceHandle>(handle);
    if(!service) {SetLastError(ERROR_INVALID_HANDLE);return FALSE;}
    const auto record=read_service(*service);
    if(status) {std::memset(status,0,sizeof(*status));status->dwCurrentState=record.state;}
    if(control==SERVICE_CONTROL_INTERROGATE) return TRUE;
    if(control!=SERVICE_CONTROL_STOP || !record.pid || record.state==SERVICE_STOPPED) {SetLastError(ERROR_INVALID_HANDLE);return FALSE;}
    if(::kill(record.pid,SIGTERM)!=0) {SetLastError(ERROR_INVALID_HANDLE);return FALSE;}
    if(status) status->dwCurrentState=SERVICE_STOP_PENDING;
    return TRUE;
}
BOOL StartService(SC_HANDLE handle,DWORD,LPCSTR*) {
    auto service=find<ServiceHandle>(handle);
    const char* command=std::getenv("FOURSTORY_SCM_START");
    if(!service || !command) {SetLastError(ERROR_INVALID_HANDLE);return FALSE;}
    if(read_service(*service).state!=SERVICE_STOPPED) {SetLastError(ERROR_INVALID_HANDLE);return FALSE;}
    const pid_t child=::fork();
    if(child<0) {SetLastError(ERROR_INVALID_HANDLE);return FALSE;}
    if(child==0) {
        // Detach so the started service outlives the requesting Control process.
        ::setsid();
        ::execl("/bin/sh","sh","-c",command,"sh",service->name.c_str(),static_cast<char*>(nullptr));
        ::_exit(127);
    }
    int code=0;
    // The command is expected to launch the service in the background.
    return ::waitpid(child,&code,0)==child && WIFEXITED(code) && WEXITSTATUS(code)==0;
}
BOOL CloseServiceHandle(SC_HANDLE handle) {return CloseHandle(handle);}
