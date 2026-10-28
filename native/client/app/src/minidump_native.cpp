// Native replacement of Client/TClient/TMiniDump.cpp (C4).
// The Windows unit installs an unhandled-exception filter, writes a minidump
// with dbghelp, shows a report dialog and mails the report. macOS has no
// structured exceptions, dbghelp or dialog templates. This unit keeps the
// recorded state (option, user, log, character, socket, window) so callers
// behave the same, and does not write a dump: crash reports on macOS come
// from the system crash reporter (~/Library/Logs/DiagnosticReports).
#include "StdAfx.h"
#include "TMiniDump.h"

namespace {
MINIDUMP_TYPE g_eDumpType = MiniDumpNormal;
std::string g_strUserData = "tttttt";
std::string g_strLogID;
std::string g_strCharID;
HWND g_hWnd = NULL;
SOCKET g_sock = INVALID_SOCKET;
}
CTMiniDump theTMiniDump;

CTMiniDump::CTMiniDump() {}
CTMiniDump::~CTMiniDump() {}
void CTMiniDump::SetOption(_MINIDUMP_TYPE nDumpType) { g_eDumpType = nDumpType; }
void CTMiniDump::SetUserInfo(const char* pszTemp) { g_strUserData = pszTemp ? pszTemp : ""; }
void CTMiniDump::SetLogID(const char* pszID) { g_strLogID = pszID ? pszID : ""; }
void CTMiniDump::SetCharID(const char* pszID) { g_strCharID = pszID ? pszID : ""; }
void CTMiniDump::SetSocket(SOCKET sock) { g_sock = sock; }
void CTMiniDump::SetHWND(HWND hWnd) { g_hWnd = hWnd; }
// Windows writes a memory dump of the running process here (Win32 only).
void CTMiniDump::SnapMemoryDump() {}
void CTMiniDump::LogPlayerInfo(CString& outResult) {
    outResult.Format("LogID: %s\nCharID: %s\n", g_strLogID.c_str(), g_strCharID.c_str());
}
BOOL CTMiniDump::GetExceptionDetail(_EXCEPTION_POINTERS*, CString& outResult, CString& outFirstStack) {
    outResult.Empty(); outFirstStack.Empty();
    return FALSE;
}
LONG WINAPI CTMiniDump::TopLevelExceptionFilter(_EXCEPTION_POINTERS*) { return 0; } // EXCEPTION_CONTINUE_SEARCH
char* CTMiniDump::GetFaultReason(_EXCEPTION_POINTERS*) { return nullptr; }
