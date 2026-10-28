// Native replacement for the role TMiniDump.cpp files: macOS records crash
// reports itself, so the Windows minidump options are accepted and ignored.
#include "atl.hpp"
#include "../../../source/4Story 5.0 Source Files/Server/TMapSvr/TMiniDump.h"

CTMiniDump theTMiniDump;
CTMiniDump::CTMiniDump() {}
CTMiniDump::~CTMiniDump() {}
void CTMiniDump::SetOption(_MINIDUMP_TYPE) {}
void CTMiniDump::SetUserInfo(const char*) {}
void CTMiniDump::SetHWND(HWND) {}
void CTMiniDump::SnapMemoryDump() {}
LONG WINAPI CTMiniDump::TopLevelExceptionFilter(_EXCEPTION_POINTERS*) {return 0;}
char* CTMiniDump::GetFaultReason(_EXCEPTION_POINTERS*) {return nullptr;}
