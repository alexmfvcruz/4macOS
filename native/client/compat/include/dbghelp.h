// Windows SDK header name (C4). Types for the crash-dump declarations of the
// client (TClient/TMiniDump.h). No dbghelp function is provided: the native
// client replaces TMiniDump.cpp (native/client/app/src/minidump_native.cpp).
#pragma once
#include <windows.h>
struct _EXCEPTION_RECORD;
struct _CONTEXT;
struct _EXCEPTION_POINTERS { _EXCEPTION_RECORD* ExceptionRecord; _CONTEXT* ContextRecord; };
typedef _EXCEPTION_POINTERS EXCEPTION_POINTERS, *PEXCEPTION_POINTERS, *LPEXCEPTION_POINTERS;
enum _MINIDUMP_TYPE {
    MiniDumpNormal = 0x00000000, MiniDumpWithDataSegs = 0x00000001, MiniDumpWithFullMemory = 0x00000002,
    MiniDumpWithHandleData = 0x00000004, MiniDumpFilterMemory = 0x00000008, MiniDumpScanMemory = 0x00000010,
    MiniDumpWithIndirectlyReferencedMemory = 0x00000040, MiniDumpWithProcessThreadData = 0x00000100,
    MiniDumpWithPrivateReadWriteMemory = 0x00000200
};
typedef _MINIDUMP_TYPE MINIDUMP_TYPE;
