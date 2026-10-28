// Native kernel object subset (C4-P), part of <windows.h>: events, threads,
// waits, local time and TCHAR string names. Implementation:
// native/client/compat/src/kernel32.cpp (libcompat_ui). Handles are reference
// counted objects; CloseHandle releases one reference. A thread handle stays
// waitable after the thread ends until it is closed.
#pragma once

typedef DWORD (WINAPI* LPTHREAD_START_ROUTINE)(LPVOID);
struct SECURITY_ATTRIBUTES { DWORD nLength; LPVOID lpSecurityDescriptor; BOOL bInheritHandle; };
typedef SECURITY_ATTRIBUTES *LPSECURITY_ATTRIBUTES;
#define STILL_ACTIVE 259
#define WAIT_TIMEOUT 258
#define MAXIMUM_WAIT_OBJECTS 64

HANDLE CreateEvent(LPSECURITY_ATTRIBUTES attributes, BOOL manual_reset, BOOL initial_state, LPCSTR name);
BOOL SetEvent(HANDLE event);
BOOL ResetEvent(HANDLE event);
BOOL PulseEvent(HANDLE event);
DWORD WaitForSingleObject(HANDLE handle, DWORD milliseconds);
DWORD WaitForMultipleObjects(DWORD count, const HANDLE* handles, BOOL wait_all, DWORD milliseconds);
BOOL CloseHandle(HANDLE handle);
BOOL DuplicateHandle(HANDLE, HANDLE source, HANDLE, LPHANDLE target, DWORD, BOOL, DWORD);
HANDLE CreateThread(LPSECURITY_ATTRIBUTES attributes, SIZE_T stack, LPTHREAD_START_ROUTINE start, LPVOID parameter, DWORD flags, LPDWORD thread_id);
DWORD ResumeThread(HANDLE thread);
BOOL GetExitCodeThread(HANDLE thread, LPDWORD code);
BOOL SetThreadPriority(HANDLE thread, int priority);
int GetThreadPriority(HANDLE thread);
BOOL TerminateThread(HANDLE thread, DWORD code);
HANDLE GetCurrentThread();
DWORD GetCurrentThreadId();
HANDLE GetCurrentProcess();
DWORD GetCurrentProcessId();
BOOL QueryPerformanceCounter(LARGE_INTEGER* count);
BOOL QueryPerformanceFrequency(LARGE_INTEGER* frequency);
void GetLocalTime(SYSTEMTIME* time);
void GetSystemTime(SYSTEMTIME* time);
struct SYSTEM_INFO {
    WORD wProcessorArchitecture, wReserved; DWORD dwPageSize; LPVOID lpMinimumApplicationAddress, lpMaximumApplicationAddress;
    DWORD_PTR dwActiveProcessorMask; DWORD dwNumberOfProcessors, dwProcessorType, dwAllocationGranularity; WORD wProcessorLevel, wProcessorRevision;
};
typedef SYSTEM_INFO *LPSYSTEM_INFO;
void GetSystemInfo(LPSYSTEM_INFO info);

// Portable executable section header (TModuleProtector.h declarations only).
#define IMAGE_SIZEOF_SHORT_NAME 8
struct IMAGE_SECTION_HEADER {
    BYTE Name[IMAGE_SIZEOF_SHORT_NAME]; union { DWORD PhysicalAddress; DWORD VirtualSize; } Misc;
    DWORD VirtualAddress, SizeOfRawData, PointerToRawData, PointerToRelocations, PointerToLinenumbers;
    WORD NumberOfRelocations, NumberOfLinenumbers; DWORD Characteristics;
};
typedef IMAGE_SECTION_HEADER *PIMAGE_SECTION_HEADER;

// Local memory handles of LocalAlloc are plain pointers (windows.h).
inline LPVOID LocalLock(HLOCAL memory) { return memory; }
inline BOOL LocalUnlock(HLOCAL) { return FALSE; }
#define LHND 0x0042
// itoa and _itoa: the Microsoft CRT conversion of an int to text in a radix of 2 to 36.
inline char* _itoa(int value, char* text, int radix) {
    if (!text || radix < 2 || radix > 36) return text;
    char digits[34]; int n = 0;
    unsigned int u = (radix == 10 && value < 0) ? static_cast<unsigned int>(-(long long)value) : static_cast<unsigned int>(value);
    do { const unsigned d = u % static_cast<unsigned>(radix); digits[n++] = static_cast<char>(d < 10 ? '0' + d : 'a' + d - 10); u /= static_cast<unsigned>(radix); } while (u);
    char* p = text;
    if (radix == 10 && value < 0) *p++ = '-';
    while (n) *p++ = digits[--n];
    *p = 0;
    return text;
}
inline char* itoa(int value, char* text, int radix) { return _itoa(value, text, radix); }

// ---- TCHAR names (TCHAR is char) ------------------------------------------------
#define _tcslen strlen
#define _tcscpy strcpy
#define _tcscpy_s strcpy_s
#define _tcsncpy strncpy
#define _tcscat strcat
#define _tcscmp strcmp
#define _tcsicmp _stricmp
#define _tcsncmp strncmp
#define _tcschr strchr
#define _tcsrchr strrchr
#define _tcsstr strstr
#define _tcstok strtok
#define _tstoi atoi
#define _tstof atof
#define _stprintf sprintf
#define _stprintf_s sprintf_s
#define _tprintf printf
#define _istspace isspace
#define _istdigit isdigit
#define _istalpha isalpha
// Lower and upper case of ASCII letters only (README, CString).
inline errno_t _strlwr_s(char* text, std::size_t size) {
    if (!text || strnlen(text, size) == size) return EINVAL;
    for (; *text; ++text) if (*text >= 'A' && *text <= 'Z') *text = static_cast<char>(*text - 'A' + 'a');
    return 0;
}
template<std::size_t N> inline errno_t _strlwr_s(char (&text)[N]) { return _strlwr_s(text, N); }
inline errno_t _strupr_s(char* text, std::size_t size) {
    if (!text || strnlen(text, size) == size) return EINVAL;
    for (; *text; ++text) if (*text >= 'a' && *text <= 'z') *text = static_cast<char>(*text - 'a' + 'A');
    return 0;
}
template<std::size_t N> inline errno_t _strupr_s(char (&text)[N]) { return _strupr_s(text, N); }
inline char* _strlwr(char* text) { for (char* p = text; p && *p; ++p) if (*p >= 'A' && *p <= 'Z') *p = static_cast<char>(*p - 'A' + 'a'); return text; }
inline char* _strupr(char* text) { for (char* p = text; p && *p; ++p) if (*p >= 'a' && *p <= 'z') *p = static_cast<char>(*p - 'a' + 'A'); return text; }

// ---- Resources (C4) ------------------------------------------------------------
// The native executable has no resource section. A generated table
// (native/client/app/gen_resources.py, from TClient.rc) registers the
// resources at start; FindResource searches that table. The module handle is
// ignored (one module). Resource types are names ("SHADER") or RT_ numbers.
#define RT_CURSOR MAKEINTRESOURCE(1)
#define RT_BITMAP MAKEINTRESOURCE(2)
#define RT_ICON MAKEINTRESOURCE(3)
#define RT_MENU MAKEINTRESOURCE(4)
#define RT_DIALOG MAKEINTRESOURCE(5)
#define RT_STRING MAKEINTRESOURCE(6)
#define RT_ACCELERATOR MAKEINTRESOURCE(9)
#define RT_RCDATA MAKEINTRESOURCE(10)
#define RT_GROUP_CURSOR MAKEINTRESOURCE(12)
#define RT_GROUP_ICON MAKEINTRESOURCE(14)
namespace fourstory { namespace resources {
// One resource: type and name are a string or a number (the other is null/0).
// For CURSOR and ICON the data is the .cur/.ico file (RT_GROUP_CURSOR/RT_GROUP_ICON).
struct Resource {
    const char* type_name; unsigned type_id;
    const char* name; unsigned id;
    const unsigned char* data; DWORD size;
};
void add(const Resource* table, std::size_t count);  // the table must stay valid
const Resource* find(LPCSTR name, LPCSTR type);
}}
HRSRC FindResource(HMODULE module, LPCSTR name, LPCSTR type);
#define FindResourceA FindResource
HGLOBAL LoadResource(HMODULE module, HRSRC resource);
LPVOID LockResource(HGLOBAL data);
DWORD SizeofResource(HMODULE module, HRSRC resource);
BOOL FreeResource(HGLOBAL data);
int LoadString(HINSTANCE instance, UINT id, LPSTR buffer, int size);
#define LoadStringA LoadString

// ---- Memory status (C4) -----------------------------------------------------
struct MEMORYSTATUSEX {
    DWORD dwLength, dwMemoryLoad;
    ULONGLONG ullTotalPhys, ullAvailPhys, ullTotalPageFile, ullAvailPageFile, ullTotalVirtual, ullAvailVirtual, ullAvailExtendedVirtual;
};
typedef MEMORYSTATUSEX *LPMEMORYSTATUSEX;
// ullTotalPhys is hw.memsize. ullAvailPhys counts free and inactive pages.
// The page file and virtual fields are not measured and are 0.
BOOL GlobalMemoryStatusEx(LPMEMORYSTATUSEX status);

// ---- Files (C4) ---------------------------------------------------------------
#ifndef FILE_FLAG_RANDOM_ACCESS
#define FILE_FLAG_RANDOM_ACCESS 0x10000000
#endif
#ifndef FILE_FLAG_SEQUENTIAL_SCAN
#define FILE_FLAG_SEQUENTIAL_SCAN 0x08000000
#endif
#ifndef INVALID_FILE_SIZE
#define INVALID_FILE_SIZE ((DWORD)0xFFFFFFFF)
#endif
#ifndef ERROR_WRITE_FAULT
#define ERROR_WRITE_FAULT 29L
#endif
#ifndef ERROR_READ_FAULT
#define ERROR_READ_FAULT 30L
#endif
#ifndef ERROR_FILE_EXISTS
#define ERROR_FILE_EXISTS 80L
#endif
#ifndef ERROR_NEGATIVE_SEEK
#define ERROR_NEGATIVE_SEEK 131L
#endif
HANDLE CreateFile(LPCSTR name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES security, DWORD disposition, DWORD flags, HANDLE template_file);
#define CreateFileA CreateFile
BOOL ReadFile(HANDLE file, LPVOID buffer, DWORD bytes, LPDWORD read, LPVOID overlapped);
BOOL WriteFile(HANDLE file, LPCVOID buffer, DWORD bytes, LPDWORD written, LPVOID overlapped);
DWORD SetFilePointer(HANDLE file, LONG distance, LPLONG high, DWORD method);
DWORD GetFileSize(HANDLE file, LPDWORD high);

// ---- Low-level keyboard hook data (C4) -------------------------------------------
struct KBDLLHOOKSTRUCT { DWORD vkCode, scanCode, flags, time; ULONG_PTR dwExtraInfo; };
typedef KBDLLHOOKSTRUCT *LPKBDLLHOOKSTRUCT, *PKBDLLHOOKSTRUCT;

// ---- Shell (C4) ---------------------------------------------------------------------
// ShellExecute with the "open" verb (or none) hands a URL or file to
// /usr/bin/open (default browser or application). Other verbs fail.
// Returns a value above 32 on success, as Windows.
HINSTANCE ShellExecute(HWND window, LPCSTR verb, LPCSTR file, LPCSTR parameters, LPCSTR directory, int show);
#define ShellExecuteA ShellExecute

// ---- Wide-character conversion (C4) -------------------------------------------
inline int _wtoi(const WCHAR* text) {
    if (!text) return 0;
    while (*text == ' ' || *text == '\t') ++text;
    bool negative = false;
    if (*text == '-' || *text == '+') negative = *text++ == '-';
    long long value = 0;
    while (*text >= '0' && *text <= '9') { value = value * 10 + (*text++ - '0'); if (value > 0x80000000LL) break; }
    return static_cast<int>(negative ? -value : value);
}

// ---- Processes and file deletion (C4) -------------------------------------------
struct STARTUPINFO {
    DWORD cb; LPSTR lpReserved, lpDesktop, lpTitle; DWORD dwX, dwY, dwXSize, dwYSize, dwXCountChars, dwYCountChars, dwFillAttribute, dwFlags;
    WORD wShowWindow, cbReserved2; LPBYTE lpReserved2; HANDLE hStdInput, hStdOutput, hStdError;
};
typedef STARTUPINFO *LPSTARTUPINFO;
struct PROCESS_INFORMATION { HANDLE hProcess, hThread; DWORD dwProcessId, dwThreadId; };
typedef PROCESS_INFORMATION *LPPROCESS_INFORMATION;
#ifndef STARTF_USESHOWWINDOW
#define STARTF_USESHOWWINDOW 0x00000001
#endif
// Windows programs cannot be started on macOS: CreateProcess fails
// (ERROR_NOT_SUPPORTED) and starts nothing.
BOOL CreateProcess(LPCSTR application, LPSTR command_line, LPSECURITY_ATTRIBUTES process_attributes, LPSECURITY_ATTRIBUTES thread_attributes,
                   BOOL inherit_handles, DWORD flags, LPVOID environment, LPCSTR directory, LPSTARTUPINFO startup, LPPROCESS_INFORMATION information);
#define CreateProcessA CreateProcess
BOOL DeleteFile(LPCSTR name);
#define DeleteFileA DeleteFile

// ---- Module, directories and UUIDs (C4) ----------------------------------------
// GetModuleFileName returns the POSIX path of the native executable ('/'
// separators); the module handle must be null (the program itself).
DWORD GetModuleFileName(HMODULE module, LPSTR file_name, DWORD size);
#define GetModuleFileNameA GetModuleFileName
BOOL CreateDirectory(LPCSTR path, LPSECURITY_ATTRIBUTES security);
#define CreateDirectoryA CreateDirectory
#ifndef GUID_DEFINED
#define GUID_DEFINED
struct GUID { DWORD Data1; WORD Data2, Data3; BYTE Data4[8]; };
#endif
typedef GUID UUID;
#define RPC_S_OK 0L
#define RPC_S_UUID_LOCAL_ONLY 1824L
// Version 1 UUID as UuidCreateSequential makes it: 100 ns time since
// 1582-10-15 and, in Data4[2..7], the hardware address of en0 (else the first
// non-loopback Ethernet-type interface). Without one the node is random with the
// multicast bit set and the result is RPC_S_UUID_LOCAL_ONLY, as on Windows.
long UuidCreateSequential(UUID* uuid);
