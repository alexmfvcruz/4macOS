#pragma once
// Native module build: Windows minidumps are replaced by macOS crash reports.
// CTMiniDump's methods are implemented as no-ops in native/server/win32/minidump.cpp.
#include "../atl.hpp"
enum _MINIDUMP_TYPE {MiniDumpNormal=0,MiniDumpWithFullMemory=2};
using MINIDUMP_TYPE=_MINIDUMP_TYPE;
struct _EXCEPTION_POINTERS;
