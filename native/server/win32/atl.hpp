#pragma once
// ATL subset used by the original server modules: CTime/CTimeSpan and the
// service module base. Windows service installation and COM registration are
// excluded from native builds; process supervision replaces the SCM.
#include "windows.hpp"

class CTimeSpan {
    __time64_t span_=0;
public:
    CTimeSpan()=default;
    CTimeSpan(__time64_t seconds):span_(seconds) {}
    CTimeSpan(LONG days,int hours,int minutes,int seconds)
        :span_(seconds+60*(minutes+60*(hours+24*static_cast<__time64_t>(days)))) {}
    __time64_t GetTimeSpan() const {return span_;}
    LONGLONG GetDays() const {return span_/(24*3600);}
    LONGLONG GetTotalHours() const {return span_/3600;}
    LONG GetHours() const {return static_cast<LONG>(GetTotalHours()-GetDays()*24);}
    LONGLONG GetTotalMinutes() const {return span_/60;}
    LONG GetMinutes() const {return static_cast<LONG>(GetTotalMinutes()-GetTotalHours()*60);}
    LONGLONG GetTotalSeconds() const {return span_;}
    LONG GetSeconds() const {return static_cast<LONG>(GetTotalSeconds()-GetTotalMinutes()*60);}
    CTimeSpan operator+(CTimeSpan other) const {return span_+other.span_;}
    CTimeSpan operator-(CTimeSpan other) const {return span_-other.span_;}
    CTimeSpan& operator+=(CTimeSpan other) {span_+=other.span_;return *this;}
    CTimeSpan& operator-=(CTimeSpan other) {span_-=other.span_;return *this;}
    bool operator==(CTimeSpan other) const {return span_==other.span_;}
    bool operator!=(CTimeSpan other) const {return span_!=other.span_;}
    bool operator<(CTimeSpan other) const {return span_<other.span_;}
    bool operator>(CTimeSpan other) const {return span_>other.span_;}
    bool operator<=(CTimeSpan other) const {return span_<=other.span_;}
    bool operator>=(CTimeSpan other) const {return span_>=other.span_;}
};

// Local-calendar time, matching ATL CTime (localtime/mktime).
class CTime {
    __time64_t time_=0;
    std::tm local() const {
        std::tm value{};const std::time_t t=static_cast<std::time_t>(time_);
        if(!localtime_r(&t,&value)) throw std::out_of_range("CTime outside local calendar range");
        return value;
    }
public:
    CTime()=default;
    CTime(__time64_t value):time_(value) {}
    CTime(int year,int month,int day,int hour,int minute,int second,int dst=-1) {
        std::tm value{};value.tm_year=year-1900;value.tm_mon=month-1;value.tm_mday=day;
        value.tm_hour=hour;value.tm_min=minute;value.tm_sec=second;value.tm_isdst=dst;
        time_=std::mktime(&value);
    }
    CTime(const SYSTEMTIME& value,int dst=-1)
        :CTime(value.wYear,value.wMonth,value.wDay,value.wHour,value.wMinute,value.wSecond,dst) {}
    static CTime GetCurrentTime() {return CTime(static_cast<__time64_t>(std::time(nullptr)));}
    __time64_t GetTime() const {return time_;}
    int GetYear() const {return local().tm_year+1900;}
    int GetMonth() const {return local().tm_mon+1;}
    int GetDay() const {return local().tm_mday;}
    int GetHour() const {return local().tm_hour;}
    int GetMinute() const {return local().tm_min;}
    int GetSecond() const {return local().tm_sec;}
    int GetDayOfWeek() const {return local().tm_wday+1;}
    std::tm* GetLocalTm(std::tm* out) const {if(out) *out=local();return out;}
    bool GetAsSystemTime(SYSTEMTIME& out) const {
        const auto value=local();
        out={static_cast<WORD>(value.tm_year+1900),static_cast<WORD>(value.tm_mon+1),static_cast<WORD>(value.tm_wday),
            static_cast<WORD>(value.tm_mday),static_cast<WORD>(value.tm_hour),static_cast<WORD>(value.tm_min),
            static_cast<WORD>(value.tm_sec),0};
        return true;
    }
    CString Format(const char* format) const {
        const auto value=local();char buffer[512];
        const auto size=std::strftime(buffer,sizeof(buffer),format,&value);
        return CString(std::string(buffer,size));
    }
    CTime operator+(CTimeSpan span) const {return time_+span.GetTimeSpan();}
    CTime operator-(CTimeSpan span) const {return time_-span.GetTimeSpan();}
    CTimeSpan operator-(CTime other) const {return time_-other.time_;}
    CTime& operator+=(CTimeSpan span) {time_+=span.GetTimeSpan();return *this;}
    CTime& operator-=(CTimeSpan span) {time_-=span.GetTimeSpan();return *this;}
    bool operator==(CTime other) const {return time_==other.time_;}
    bool operator!=(CTime other) const {return time_!=other.time_;}
    bool operator<(CTime other) const {return time_<other.time_;}
    bool operator>(CTime other) const {return time_>other.time_;}
    bool operator<=(CTime other) const {return time_<=other.time_;}
    bool operator>=(CTime other) const {return time_>=other.time_;}
};

namespace fourstory::win32 {
void log_event(const char* service,const char* text);
// Service status published for the emulated Service Control Manager.
void publish_service_status(const char* service,DWORD state);
}

#define DECLARE_LIBID(x)
#define DECLARE_REGISTRY_APPID_RESOURCEID(x,y)
#define _ATL_QUOTES_SPACE 2
#define SERVICE_STOPPED 1u
#define SERVICE_START_PENDING 2u
#define SERVICE_STOP_PENDING 3u
#define SERVICE_RUNNING 4u
#define SERVICE_CONTINUE_PENDING 5u
#define SERVICE_PAUSE_PENDING 6u
#define SERVICE_PAUSED 7u
struct SERVICE_STATUS {
    DWORD dwServiceType,dwCurrentState,dwControlsAccepted,dwWin32ExitCode;
    DWORD dwServiceSpecificExitCode,dwCheckPoint,dwWaitHint;
};
#ifndef FOURSTORY_SERVICE_NAME
#error "Native module builds define FOURSTORY_SERVICE_NAME"
#endif

// The ATL Run sequence: PreMessageLoop; RunMessageLoop only on S_OK;
// PostMessageLoop whenever PreMessageLoop succeeded.
template<class T,UINT ServiceNameId> class CAtlServiceModuleT {
public:
    TCHAR m_szServiceName[256]{};
    SERVICE_STATUS m_status{};
    DWORD m_dwThreadID=0;
    bool m_bService=true;
    CAtlServiceModuleT() {
        std::strncpy(m_szServiceName,FOURSTORY_SERVICE_NAME,sizeof(m_szServiceName)-1);
    }
    virtual ~CAtlServiceModuleT()=default;
    template<class... Args> void LogEvent(LPCTSTR format,const Args&... args) {
        fourstory::win32::log_event(m_szServiceName,fourstory::win32::printf_string(format,args...).c_str());
    }
    void SetServiceStatus(DWORD state) {
        m_status.dwCurrentState=state;
        fourstory::win32::publish_service_status(m_szServiceName,state);
    }
    HRESULT PreMessageLoop(int) {return S_OK;}
    HRESULT PostMessageLoop() {return S_OK;}
    void RunMessageLoop() {fourstory::win32::wait_for_stop();}
    void OnStop() {SetServiceStatus(SERVICE_STOP_PENDING);fourstory::win32::request_stop();}
    void OnShutdown() {OnStop();}
    HRESULT Run(int show=1) {
        T* module=static_cast<T*>(this);
        HRESULT result=module->PreMessageLoop(show);
        if(result==S_OK) {
            // Native readiness marker for tools/server/native_cluster.py: the
            // original OnEnter succeeded and registered workers initialized.
            if(fourstory::win32::wait_ready_workers(120000)) {
                LogEvent("Service started");
                SetServiceStatus(SERVICE_RUNNING);
                LogEvent("Native startup complete");
            } else if(!fourstory::win32::stop_requested()) {
                LogEvent("Native startup incomplete: worker initialization timed out");
                fourstory::win32::request_stop();
            }
            module->RunMessageLoop();
            SetServiceStatus(SERVICE_STOP_PENDING);
        }
        if(SUCCEEDED(result)) result=module->PostMessageLoop();
        return result;
    }
    // ServiceMain: START_PENDING, Run, then STOPPED with the run result.
    int WinMain(int show) {
        SetServiceStatus(SERVICE_START_PENDING);
        m_status.dwWin32ExitCode=static_cast<DWORD>(Run(show));
        SetServiceStatus(SERVICE_STOPPED);
        return static_cast<int>(m_status.dwWin32ExitCode);
    }
};

// Service Control Manager over a status directory (FOURSTORY_SCM_DIR): each
// native service publishes "<state> <pid>" as <name>.status from
// SetServiceStatus; QueryServiceStatus reads it and checks the process,
// ControlService(STOP) sends SIGTERM, and StartService runs the command in
// FOURSTORY_SCM_START with the service name as $1. Without FOURSTORY_SCM_DIR
// OpenSCManager fails, as for an unreachable Windows machine.
using SC_HANDLE=void*;
#define SC_MANAGER_ALL_ACCESS 0xF003Fu
#define SERVICE_ALL_ACCESS 0xF01FFu
#define SERVICE_CONTROL_STOP 0x1u
#define SERVICE_CONTROL_INTERROGATE 0x4u
SC_HANDLE OpenSCManager(LPCSTR machine,LPCSTR database,DWORD access);
SC_HANDLE OpenService(SC_HANDLE manager,LPCSTR name,DWORD access);
BOOL StartService(SC_HANDLE service,DWORD argc,LPCSTR* argv);
BOOL ControlService(SC_HANDLE service,DWORD control,SERVICE_STATUS* status);
BOOL QueryServiceStatus(SC_HANDLE service,SERVICE_STATUS* status);
BOOL CloseServiceHandle(SC_HANDLE handle);
