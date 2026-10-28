// Native process entry for an original server module. The module's own
// _tWinMain (renamed WinMain by windows.hpp) runs the ATL service sequence:
// PreMessageLoop -> OnEnter, RunMessageLoop until SIGTERM/SIGINT or an
// original WM_QUIT request, then PostMessageLoop -> OnExit.
#include "atl.hpp"
#include <csignal>
#include <exception>
#include <execinfo.h>
#include <unistd.h>

extern "C" int WINAPI WinMain(HINSTANCE,HINSTANCE,LPSTR,int);

extern "C" void fourstory_module_signal(int) {fourstory::win32::request_stop();}

// Crash evidence for tools/crash_report.py: a marker line and the stack of
// the failing thread go to stderr (the role log) before the default action
// produces the macOS crash report. Only async-signal-safe calls are used.
namespace {
void write_text(const char* text) {[[maybe_unused]] auto ignored=::write(STDERR_FILENO,text,std::strlen(text));}
void write_stack() {
    void* frames[64];
    const int count=::backtrace(frames,64);
    ::backtrace_symbols_fd(frames,count,STDERR_FILENO);
    write_text("Native crash: end of stack\n");
}
extern "C" void fourstory_fatal_signal(int number) {
    write_text("Native crash: " FOURSTORY_SERVICE_NAME " fatal signal ");
    write_text(number==SIGSEGV?"SIGSEGV (invalid memory access)":number==SIGBUS?"SIGBUS (invalid memory access)":
        number==SIGILL?"SIGILL":number==SIGFPE?"SIGFPE (arithmetic error)":"SIGABRT");
    write_text("\n");
    write_stack();
    ::signal(number,SIG_DFL);
    ::raise(number);
}
void fourstory_terminate() {
    write_text("Native crash: " FOURSTORY_SERVICE_NAME " uncaught C++ exception");
    if(auto current=std::current_exception()) {
        try {std::rethrow_exception(current);}
        catch(const std::exception& error) {write_text(": ");write_text(error.what());}
        catch(...) {write_text(": unknown type");}
    }
    write_text("\n");
    write_stack();
    ::signal(SIGABRT,SIG_DFL);
    std::abort();
}
}

int main() {
    struct sigaction action{};
    action.sa_handler=fourstory_module_signal;
    sigemptyset(&action.sa_mask);
    sigaction(SIGTERM,&action,nullptr);
    sigaction(SIGINT,&action,nullptr);
    struct sigaction ignore{};
    ignore.sa_handler=SIG_IGN;
    sigemptyset(&ignore.sa_mask);
    sigaction(SIGPIPE,&ignore,nullptr);
    // Sanitizer builds keep the sanitizer's own, more detailed reports.
#if !defined(__has_feature) || !__has_feature(address_sanitizer)
    for(int number:{SIGSEGV,SIGBUS,SIGILL,SIGFPE,SIGABRT}) {
        struct sigaction fatal{};
        fatal.sa_handler=fourstory_fatal_signal;
        sigemptyset(&fatal.sa_mask);
        sigaction(number,&fatal,nullptr);
    }
    std::set_terminate(fourstory_terminate);
#endif
    char empty[]="";
    int code=1;
    try {
        code=WinMain(nullptr,nullptr,empty,1);
    } catch(const std::exception& error) {
        fourstory::win32::log_event(FOURSTORY_SERVICE_NAME,(std::string("Unhandled exception: ")+error.what()).c_str());
    }
    std::fflush(stdout);std::fflush(stderr);
    // Worker threads are joined by the module's own OnExit; skip static
    // destructors that Windows service processes never run either.
    std::_Exit(code==0?0:1);
}
