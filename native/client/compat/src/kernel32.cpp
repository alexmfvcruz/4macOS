// Native kernel object subset (C4-P): events and threads over the C++
// standard library. Declarations: include/fourstory_kernel32.h.
#include <windows.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <sys/time.h>
#include <thread>
#include <unistd.h>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <mach/mach.h>
#include <sys/sysctl.h>
#include <sys/stat.h>
#include <cerrno>
#include <fcntl.h>
#include <spawn.h>
#include <ifaddrs.h>
#include <mach-o/dyld.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <sys/wait.h>
extern char** environ;

namespace {
// One condition variable for all objects: a wait on several objects wakes on any change.
std::mutex g_lock;
std::condition_variable g_changed;

struct Object {
    enum Kind { event, thread, file } kind = event;
    int descriptor = -1; // file
    int references = 1;
    // event
    bool manual = false, signaled = false;
    // thread
    std::thread worker;
    bool finished = false, started = false;
    DWORD exit_code = STILL_ACTIVE, id = 0;
    int priority = 0;
    LPTHREAD_START_ROUTINE start = nullptr; LPVOID parameter = nullptr;
};
std::set<Object*>& objects() { static std::set<Object*> s; return s; }
std::atomic<DWORD> g_next_thread_id{1000};
thread_local DWORD t_thread_id = 0;

Object* get(HANDLE h) { Object* o = static_cast<Object*>(h); return objects().count(o) ? o : nullptr; }
bool is_signaled(Object* o) { return o->kind == Object::event ? o->signaled : o->kind == Object::thread ? o->finished : true; }
void consume(Object* o) { if (o->kind == Object::event && !o->manual) o->signaled = false; }
void release(Object* o) {
    if (--o->references > 0) return;
    objects().erase(o);
    if (o->worker.joinable()) o->worker.detach();
    if (o->kind == Object::file && o->descriptor >= 0) ::close(o->descriptor);
    delete o;
}
void run(Object* o) {
    t_thread_id = o->id;
    const DWORD code = o->start(o->parameter);
    std::lock_guard<std::mutex> guard(g_lock);
    o->exit_code = code; o->finished = true;
    g_changed.notify_all();
    release(o); // the reference of the running thread
}
}  // namespace

HANDLE CreateEvent(LPSECURITY_ATTRIBUTES, BOOL manual_reset, BOOL initial_state, LPCSTR name) {
    if (name) { SetLastError(ERROR_NOT_SUPPORTED); return nullptr; } // named events are not shared between processes here
    std::lock_guard<std::mutex> guard(g_lock);
    Object* o = new Object; o->kind = Object::event;
    o->manual = manual_reset != 0; o->signaled = initial_state != 0;
    objects().insert(o);
    return o;
}
BOOL SetEvent(HANDLE h) {
    std::lock_guard<std::mutex> guard(g_lock);
    Object* o = get(h);
    if (!o || o->kind != Object::event) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
    o->signaled = true; g_changed.notify_all();
    return TRUE;
}
BOOL ResetEvent(HANDLE h) {
    std::lock_guard<std::mutex> guard(g_lock);
    Object* o = get(h);
    if (!o || o->kind != Object::event) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
    o->signaled = false;
    return TRUE;
}
// PulseEvent cannot release exactly the waiters present at the call with
// this implementation; the client does not call it. It fails.
BOOL PulseEvent(HANDLE) { SetLastError(ERROR_NOT_SUPPORTED); return FALSE; }

DWORD WaitForMultipleObjects(DWORD count, const HANDLE* handles, BOOL wait_all, DWORD milliseconds) {
    if (!count || count > MAXIMUM_WAIT_OBJECTS || !handles) { SetLastError(ERROR_INVALID_PARAMETER); return WAIT_FAILED; }
    std::unique_lock<std::mutex> guard(g_lock);
    std::vector<Object*> list;
    for (DWORD i = 0; i < count; ++i) {
        Object* o = get(handles[i]);
        if (!o) { SetLastError(ERROR_INVALID_HANDLE); return WAIT_FAILED; }
        list.push_back(o);
    }
    for (Object* o : list) ++o->references; // the objects stay alive during the wait
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);
    DWORD result = WAIT_TIMEOUT;
    for (;;) {
        if (wait_all) {
            bool all = true;
            for (Object* o : list) all = all && is_signaled(o);
            if (all) { for (Object* o : list) consume(o); result = WAIT_OBJECT_0; break; }
        } else {
            for (DWORD i = 0; i < count; ++i) if (is_signaled(list[i])) { consume(list[i]); result = WAIT_OBJECT_0 + i; break; }
            if (result != WAIT_TIMEOUT) break;
        }
        if (milliseconds == 0) break;
        if (milliseconds == INFINITE) g_changed.wait(guard);
        else if (g_changed.wait_until(guard, deadline) == std::cv_status::timeout && std::chrono::steady_clock::now() >= deadline) {
            // Check once more after the time out.
            milliseconds = 0;
        }
    }
    for (Object* o : list) release(o);
    return result;
}
DWORD WaitForSingleObject(HANDLE h, DWORD milliseconds) { return WaitForMultipleObjects(1, &h, FALSE, milliseconds); }

BOOL CloseHandle(HANDLE h) {
    std::lock_guard<std::mutex> guard(g_lock);
    Object* o = get(h);
    if (!o) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
    // A suspended thread that never runs gives up the reference of its run as well.
    if (o->kind == Object::thread && !o->started && o->references == 2) release(o);
    release(o);
    return TRUE;
}
BOOL DuplicateHandle(HANDLE, HANDLE source, HANDLE, LPHANDLE target, DWORD, BOOL, DWORD) {
    std::lock_guard<std::mutex> guard(g_lock);
    Object* o = get(source);
    if (!o || !target) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
    ++o->references; *target = o;
    return TRUE;
}
HANDLE CreateThread(LPSECURITY_ATTRIBUTES, SIZE_T, LPTHREAD_START_ROUTINE start, LPVOID parameter, DWORD flags, LPDWORD thread_id) {
    if (!start) { SetLastError(ERROR_INVALID_PARAMETER); return nullptr; }
    std::lock_guard<std::mutex> guard(g_lock);
    Object* o = new Object; o->kind = Object::thread;
    o->start = start; o->parameter = parameter; o->id = g_next_thread_id++;
    o->references = 2; // the handle and the running thread
    objects().insert(o);
    if (thread_id) *thread_id = o->id;
    if (!(flags & CREATE_SUSPENDED)) {
        try { o->worker = std::thread(run, o); o->started = true; }
        catch (...) { objects().erase(o); delete o; SetLastError(ERROR_NOT_ENOUGH_MEMORY); return nullptr; }
    }
    return o;
}
DWORD ResumeThread(HANDLE h) {
    std::lock_guard<std::mutex> guard(g_lock);
    Object* o = get(h);
    if (!o || o->kind != Object::thread) { SetLastError(ERROR_INVALID_HANDLE); return static_cast<DWORD>(-1); }
    if (o->started) return 0;
    try { o->worker = std::thread(run, o); o->started = true; }
    catch (...) { SetLastError(ERROR_NOT_ENOUGH_MEMORY); return static_cast<DWORD>(-1); }
    return 1;
}
BOOL GetExitCodeThread(HANDLE h, LPDWORD code) {
    std::lock_guard<std::mutex> guard(g_lock);
    Object* o = get(h);
    if (!o || o->kind != Object::thread || !code) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
    *code = o->exit_code;
    return TRUE;
}
// Priorities of threads made by CreateThread are recorded; for other threads
// the call is accepted. macOS scheduling is not changed in either case.
BOOL SetThreadPriority(HANDLE h, int priority) {
    std::lock_guard<std::mutex> guard(g_lock);
    Object* o = get(h);
    if (o && o->kind == Object::thread) { o->priority = priority; return TRUE; }
    return h == GetCurrentThread() ? TRUE : FALSE;
}
int GetThreadPriority(HANDLE h) {
    std::lock_guard<std::mutex> guard(g_lock);
    Object* o = get(h);
    return o && o->kind == Object::thread ? o->priority : 0;
}
// A thread cannot be ended from outside safely; the call fails.
BOOL TerminateThread(HANDLE, DWORD) { SetLastError(ERROR_NOT_SUPPORTED); return FALSE; }
HANDLE GetCurrentThread() { return reinterpret_cast<HANDLE>(static_cast<LONG_PTR>(-2)); }
DWORD GetCurrentThreadId() { if (!t_thread_id) t_thread_id = g_next_thread_id++; return t_thread_id; }
HANDLE GetCurrentProcess() { return reinterpret_cast<HANDLE>(static_cast<LONG_PTR>(-1)); }
DWORD GetCurrentProcessId() { return static_cast<DWORD>(getpid()); }

namespace {
void fill_time(SYSTEMTIME* t, bool local) {
    if (!t) return;
    timeval tv; gettimeofday(&tv, nullptr);
    const time_t seconds = tv.tv_sec;
    tm parts;
    if (local) localtime_r(&seconds, &parts); else gmtime_r(&seconds, &parts);
    t->wYear = static_cast<WORD>(parts.tm_year + 1900); t->wMonth = static_cast<WORD>(parts.tm_mon + 1);
    t->wDayOfWeek = static_cast<WORD>(parts.tm_wday); t->wDay = static_cast<WORD>(parts.tm_mday);
    t->wHour = static_cast<WORD>(parts.tm_hour); t->wMinute = static_cast<WORD>(parts.tm_min);
    t->wSecond = static_cast<WORD>(parts.tm_sec); t->wMilliseconds = static_cast<WORD>(tv.tv_usec / 1000);
}
}
// Nanoseconds of the monotonic clock; the frequency is 10^9 per second.
BOOL QueryPerformanceCounter(LARGE_INTEGER* count) {
    if (!count) return FALSE;
    count->QuadPart = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    return TRUE;
}
BOOL QueryPerformanceFrequency(LARGE_INTEGER* frequency) { if (!frequency) return FALSE; frequency->QuadPart = 1000000000LL; return TRUE; }
void GetLocalTime(SYSTEMTIME* time) { fill_time(time, true); }
void GetSystemTime(SYSTEMTIME* time) { fill_time(time, false); }
void GetSystemInfo(LPSYSTEM_INFO info) {
    if (!info) return;
    std::memset(info, 0, sizeof(*info));
    info->wProcessorArchitecture = 12; // PROCESSOR_ARCHITECTURE_ARM64
    info->dwPageSize = static_cast<DWORD>(getpagesize());
    info->dwAllocationGranularity = 65536;
    const unsigned n = std::thread::hardware_concurrency();
    info->dwNumberOfProcessors = n ? n : 1;
    info->dwActiveProcessorMask = info->dwNumberOfProcessors >= 64 ? ~0ULL : ((1ULL << info->dwNumberOfProcessors) - 1);
}

// ---- Resources (C4) ------------------------------------------------------------
namespace {
std::vector<std::pair<const fourstory::resources::Resource*, std::size_t>>& resource_tables() {
    static std::vector<std::pair<const fourstory::resources::Resource*, std::size_t>> tables;
    return tables;
}
bool resource_key_matches(LPCSTR key, const char* name, unsigned id) {
    if (IS_INTRESOURCE(key)) return !name && id == static_cast<unsigned>(reinterpret_cast<ULONG_PTR>(key));
    if (key[0] == '#') return !name && id == static_cast<unsigned>(std::strtoul(key + 1, nullptr, 10));
    return name && _stricmp(name, key) == 0;
}
}
namespace fourstory { namespace resources {
void add(const Resource* table, std::size_t count) { if (table && count) resource_tables().emplace_back(table, count); }
const Resource* find(LPCSTR name, LPCSTR type) {
    if (!name || !type) return nullptr;
    for (const auto& t : resource_tables())
        for (std::size_t i = 0; i < t.second; ++i) {
            const Resource& r = t.first[i];
            if (resource_key_matches(type, r.type_name, r.type_id) && resource_key_matches(name, r.name, r.id)) return &r;
        }
    return nullptr;
}
}}
HRSRC FindResource(HMODULE, LPCSTR name, LPCSTR type) {
    const fourstory::resources::Resource* r = fourstory::resources::find(name, type);
    if (!r) SetLastError(1814); // ERROR_RESOURCE_NAME_NOT_FOUND
    return reinterpret_cast<HRSRC>(const_cast<fourstory::resources::Resource*>(r));
}
// The resource data stays in the table: the global handle is the resource and
// LockResource returns its bytes (Windows: read-only mapped image data).
HGLOBAL LoadResource(HMODULE, HRSRC resource) {
    if (!resource) { SetLastError(ERROR_INVALID_HANDLE); return nullptr; }
    return reinterpret_cast<HGLOBAL>(resource);
}
LPVOID LockResource(HGLOBAL data) {
    if (!data) return nullptr;
    return const_cast<unsigned char*>(reinterpret_cast<const fourstory::resources::Resource*>(data)->data);
}
DWORD SizeofResource(HMODULE, HRSRC resource) {
    if (!resource) { SetLastError(ERROR_INVALID_HANDLE); return 0; }
    return reinterpret_cast<const fourstory::resources::Resource*>(resource)->size;
}
BOOL FreeResource(HGLOBAL) { return FALSE; }
// STRINGTABLE entries are RT_STRING resources with their id; the data is the
// text in CP_ACP without a terminator. Returns the copied length, as Windows.
int LoadString(HINSTANCE, UINT id, LPSTR buffer, int size) {
    const fourstory::resources::Resource* r = fourstory::resources::find(MAKEINTRESOURCE(id), RT_STRING);
    if (!buffer || size <= 0) { SetLastError(ERROR_INVALID_PARAMETER); return 0; }
    if (!r) { buffer[0] = 0; SetLastError(1814); return 0; }
    const int n = static_cast<int>(std::min<DWORD>(r->size, static_cast<DWORD>(size - 1)));
    std::memcpy(buffer, r->data, static_cast<std::size_t>(n));
    buffer[n] = 0;
    return n;
}

// ---- Memory status (C4) -----------------------------------------------------
BOOL GlobalMemoryStatusEx(LPMEMORYSTATUSEX status) {
    if (!status || status->dwLength != sizeof(MEMORYSTATUSEX)) { SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
    const DWORD length = status->dwLength;
    std::memset(status, 0, sizeof(*status));
    status->dwLength = length;
    std::uint64_t total = 0; std::size_t size = sizeof(total);
    if (sysctlbyname("hw.memsize", &total, &size, nullptr, 0) != 0) { SetLastError(ERROR_NOT_SUPPORTED); return FALSE; }
    status->ullTotalPhys = total;
    vm_statistics64_data_t vm; mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    if (host_statistics64(mach_host_self(), HOST_VM_INFO64, reinterpret_cast<host_info64_t>(&vm), &count) == KERN_SUCCESS) {
        const std::uint64_t page = static_cast<std::uint64_t>(getpagesize());
        status->ullAvailPhys = (static_cast<std::uint64_t>(vm.free_count) + vm.inactive_count) * page;
        if (total) status->dwMemoryLoad = static_cast<DWORD>(100 - (status->ullAvailPhys * 100) / total);
    }
    return TRUE;
}

// ---- Files (C4) ---------------------------------------------------------------
// Synchronous files on a POSIX descriptor. Paths go through native_path
// (separators and case, windows.h). Overlapped I/O, sharing modes and
// security attributes are not implemented: an OVERLAPPED argument fails.
HANDLE CreateFile(LPCSTR name, DWORD access, DWORD, LPSECURITY_ATTRIBUTES, DWORD disposition, DWORD flags, HANDLE) {
    if (!name) { SetLastError(ERROR_INVALID_PARAMETER); return INVALID_HANDLE_VALUE; }
    if (flags & 0x40000000u) { SetLastError(ERROR_NOT_SUPPORTED); return INVALID_HANDLE_VALUE; } // FILE_FLAG_OVERLAPPED
    int mode = (access & GENERIC_WRITE) ? ((access & GENERIC_READ) ? O_RDWR : O_WRONLY) : O_RDONLY;
    switch (disposition) {
    case CREATE_NEW: mode |= O_CREAT | O_EXCL; break;
    case CREATE_ALWAYS: mode |= O_CREAT | O_TRUNC; break;
    case OPEN_EXISTING: break;
    case OPEN_ALWAYS: mode |= O_CREAT; break;
    case TRUNCATE_EXISTING: mode |= O_TRUNC; break;
    default: SetLastError(ERROR_INVALID_PARAMETER); return INVALID_HANDLE_VALUE;
    }
    const int fd = ::open(fourstory::compat::native_path(name).c_str(), mode | O_CLOEXEC, 0644);
    if (fd < 0) { SetLastError(errno == ENOENT ? (disposition == OPEN_EXISTING || disposition == TRUNCATE_EXISTING ? ERROR_FILE_NOT_FOUND : 3L /* ERROR_PATH_NOT_FOUND */) : errno == ENOTDIR || errno == ENAMETOOLONG || errno == ELOOP ? 3L : errno == EEXIST ? ERROR_FILE_EXISTS : ERROR_ACCESS_DENIED); return INVALID_HANDLE_VALUE; }
    std::lock_guard<std::mutex> guard(g_lock);
    Object* o = new Object; o->kind = Object::file; o->descriptor = fd;
    objects().insert(o);
    return o;
}
namespace {
// Holds a reference to the file object during one call, so a concurrent
// CloseHandle cannot close (and the system reuse) the descriptor meanwhile.
struct FileUse {
    Object* object = nullptr;
    explicit FileUse(HANDLE h) {
        std::lock_guard<std::mutex> guard(g_lock);
        Object* o = get(h);
        if (o && o->kind == Object::file) { object = o; ++o->references; }
    }
    ~FileUse() { if (object) { std::lock_guard<std::mutex> guard(g_lock); release(object); } }
    int fd() const { return object ? object->descriptor : -1; }
};
}
BOOL ReadFile(HANDLE file, LPVOID buffer, DWORD bytes, LPDWORD read, LPVOID overlapped) {
    if (read) *read = 0;
    FileUse use(file);
    const int fd = use.fd();
    if (fd < 0) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
    if (overlapped) { SetLastError(ERROR_NOT_SUPPORTED); return FALSE; }
    DWORD done = 0;
    while (done < bytes) {
        const ssize_t n = ::read(fd, static_cast<char*>(buffer) + done, bytes - done);
        if (n < 0) { if (errno == EINTR) continue; SetLastError(ERROR_READ_FAULT); return FALSE; }
        if (n == 0) break; // end of file: success with fewer bytes, as Windows
        done += static_cast<DWORD>(n);
    }
    if (read) *read = done;
    return TRUE;
}
BOOL WriteFile(HANDLE file, LPCVOID buffer, DWORD bytes, LPDWORD written, LPVOID overlapped) {
    if (written) *written = 0;
    FileUse use(file);
    const int fd = use.fd();
    if (fd < 0) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
    if (overlapped) { SetLastError(ERROR_NOT_SUPPORTED); return FALSE; }
    DWORD done = 0;
    while (done < bytes) {
        const ssize_t n = ::write(fd, static_cast<const char*>(buffer) + done, bytes - done);
        if (n < 0) { if (errno == EINTR) continue; SetLastError(ERROR_WRITE_FAULT); return FALSE; }
        done += static_cast<DWORD>(n);
    }
    if (written) *written = done;
    return TRUE;
}
DWORD SetFilePointer(HANDLE file, LONG distance, LPLONG high, DWORD method) {
    FileUse use(file);
    const int fd = use.fd();
    if (fd < 0) { SetLastError(ERROR_INVALID_HANDLE); return INVALID_SET_FILE_POINTER; }
    // Without a high part the distance is a signed 32-bit value; with it the
    // two parts form a signed 64-bit distance.
    const long long offset = high ? static_cast<long long>((static_cast<unsigned long long>(static_cast<DWORD>(*high)) << 32) | static_cast<DWORD>(distance)) : distance;
    const off_t r = ::lseek(fd, offset, method == FILE_BEGIN ? SEEK_SET : method == FILE_CURRENT ? SEEK_CUR : SEEK_END);
    if (r < 0) { SetLastError(ERROR_NEGATIVE_SEEK); return INVALID_SET_FILE_POINTER; }
    // Windows: without a high part a position beyond 32 bits fails.
    if (!high && static_cast<unsigned long long>(r) > 0xFFFFFFFEull) { SetLastError(ERROR_INVALID_PARAMETER); return INVALID_SET_FILE_POINTER; }
    if (high) *high = static_cast<LONG>(static_cast<unsigned long long>(r) >> 32);
    SetLastError(0); // callers test GetLastError() after INVALID_SET_FILE_POINTER (0xFFFFFFFF is a valid low part)
    return static_cast<DWORD>(r);
}
DWORD GetFileSize(HANDLE file, LPDWORD high) {
    FileUse use(file);
    const int fd = use.fd();
    struct stat st;
    if (fd < 0 || fstat(fd, &st) != 0) { SetLastError(ERROR_INVALID_HANDLE); return INVALID_FILE_SIZE; }
    if (high) *high = static_cast<DWORD>(static_cast<unsigned long long>(st.st_size) >> 32);
    return static_cast<DWORD>(st.st_size);
}

// ---- Processes, shell and file deletion (C4) -------------------------------------
BOOL CreateProcess(LPCSTR, LPSTR, LPSECURITY_ATTRIBUTES, LPSECURITY_ATTRIBUTES, BOOL, DWORD, LPVOID, LPCSTR, LPSTARTUPINFO, LPPROCESS_INFORMATION information) {
    if (information) std::memset(information, 0, sizeof(*information));
    SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
}
BOOL DeleteFile(LPCSTR name) {
    if (!name) { SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
    if (::unlink(fourstory::compat::native_path(name).c_str()) != 0) { SetLastError(errno == ENOENT ? ERROR_FILE_NOT_FOUND : ERROR_ACCESS_DENIED); return FALSE; }
    return TRUE;
}
// "open" (or no verb): /usr/bin/open with the URL or the resolved file path.
// URLs keep their text; other names go through native_path. The call waits
// for open, which returns after handing the target to the default
// application (a caller may delete the file right after, TClient.cpp:794).
// Returns 33 when open succeeded; SE_ERR_NOASSOC (31) for other verbs or
// parameters, ERROR_FILE_NOT_FOUND (2) when open fails or cannot start.
HINSTANCE ShellExecute(HWND, LPCSTR verb, LPCSTR file, LPCSTR parameters, LPCSTR, int) {
    if (!file || (verb && _stricmp(verb, "open") != 0) || (parameters && *parameters))
        return reinterpret_cast<HINSTANCE>(static_cast<ULONG_PTR>(31));
    const bool url = std::strstr(file, "://") != nullptr;
    std::string target = url ? std::string(file) : fourstory::compat::native_path(file);
    if (target.empty() || target[0] == '-') target = "./" + target;  // never an option of open
    char open_program[] = "/usr/bin/open";
    char* argv[] = {open_program, const_cast<char*>(target.c_str()), nullptr};
    pid_t pid = 0;
    int status = 0;
    if (posix_spawn(&pid, open_program, nullptr, nullptr, argv, environ) != 0 ||
        waitpid(pid, &status, 0) != pid || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
        return reinterpret_cast<HINSTANCE>(static_cast<ULONG_PTR>(ERROR_FILE_NOT_FOUND));
    return reinterpret_cast<HINSTANCE>(static_cast<ULONG_PTR>(33));
}

// ---- Module, directories and UUIDs (C4) ----------------------------------------
DWORD GetModuleFileName(HMODULE module, LPSTR file_name, DWORD size) {
    if (module || !file_name || !size) { SetLastError(ERROR_INVALID_PARAMETER); return 0; }
    uint32_t n = 0;
    _NSGetExecutablePath(nullptr, &n);
    std::vector<char> path(n + 1);
    if (_NSGetExecutablePath(path.data(), &n) != 0) { SetLastError(ERROR_NOT_SUPPORTED); file_name[0] = 0; return 0; }
    const std::size_t length = std::strlen(path.data());
    if (length >= size) {  // truncated and terminated, as Windows XP and later
        std::memcpy(file_name, path.data(), size - 1); file_name[size - 1] = 0;
        SetLastError(122); // ERROR_INSUFFICIENT_BUFFER
        return size;
    }
    std::memcpy(file_name, path.data(), length + 1);
    return static_cast<DWORD>(length);
}
BOOL CreateDirectory(LPCSTR path, LPSECURITY_ATTRIBUTES) {
    if (!path) { SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
    if (::mkdir(fourstory::compat::native_path(path).c_str(), 0755) != 0) {
        SetLastError(errno == EEXIST ? 183L /* ERROR_ALREADY_EXISTS */ : errno == ENOENT ? 3L /* ERROR_PATH_NOT_FOUND */ : ERROR_ACCESS_DENIED);
        return FALSE;
    }
    return TRUE;
}
long UuidCreateSequential(UUID* uuid) {
    if (!uuid) return 1783L; // RPC_X_NULL_REF_POINTER
    static std::mutex lock;
    static std::uint16_t clock_sequence = static_cast<std::uint16_t>(arc4random() & 0x3FFF);
    static std::uint64_t last = 0;
    std::lock_guard<std::mutex> guard(lock);
    // 100 ns intervals between 1582-10-15 and 1970-01-01: 0x01B21DD213814000.
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    std::uint64_t t = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(now).count() / 100) + 0x01B21DD213814000ULL;
    if (t <= last) t = last + 1;
    last = t;
    uuid->Data1 = static_cast<DWORD>(t & 0xFFFFFFFFu);
    uuid->Data2 = static_cast<WORD>((t >> 32) & 0xFFFF);
    uuid->Data3 = static_cast<WORD>(((t >> 48) & 0x0FFF) | 0x1000);
    uuid->Data4[0] = static_cast<BYTE>(((clock_sequence >> 8) & 0x3F) | 0x80);
    uuid->Data4[1] = static_cast<BYTE>(clock_sequence & 0xFF);
    bool found = false;
    ifaddrs* list = nullptr;
    if (getifaddrs(&list) == 0) {
        // The built-in interface en0 first, then any other Ethernet-type link.
        for (int pass = 0; pass < 2 && !found; ++pass)
        for (ifaddrs* a = list; a && !found; a = a->ifa_next) {
            if (!a->ifa_addr || a->ifa_addr->sa_family != AF_LINK || (a->ifa_flags & IFF_LOOPBACK)) continue;
            if (pass == 0 && std::strcmp(a->ifa_name, "en0") != 0) continue;
            const sockaddr_dl* link = reinterpret_cast<const sockaddr_dl*>(a->ifa_addr);
            if (link->sdl_type != IFT_ETHER || link->sdl_alen != 6) continue;
            std::memcpy(&uuid->Data4[2], LLADDR(link), 6);
            found = true;
        }
        freeifaddrs(list);
    }
    if (!found) { arc4random_buf(&uuid->Data4[2], 6); uuid->Data4[2] |= 0x01; return RPC_S_UUID_LOCAL_ONLY; }
    return RPC_S_OK;
}
