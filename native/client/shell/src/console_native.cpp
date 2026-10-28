// Native stand-in for CTConsole and TRACE (C4). The original
// (Lib/Own/Engine Lib/Engine Lib/TConsole.cpp) opens a Win32 console window.
// macOS has no such console: Create fails, the handle stays null and every
// output call returns at the null check, as the original does without a
// console. TRACE writes nothing in a build without USE_CONSOLE and _DEBUG,
// which is the Release x64 configuration that the native build follows.
#include <fourstory_engine_ui.h>

CTConsole::CTConsole() : m_hConsole(NULL) {}
CTConsole::~CTConsole() {}
CTConsole* CTConsole::GetInstance() { static CTConsole obj; return &obj; }
bool CTConsole::Create(LPCSTR, bool) { return false; } // AllocConsole is not available
void CTConsole::Color(WORD) {}
void CTConsole::Output(LPCSTR, ...) {}
void CTConsole::SetTitle(LPCSTR) {}
char* CTConsole::GetTitle() { static char szWindowTitle[256] = ""; return szWindowTitle; }
HWND CTConsole::GetHWND() { return NULL; }
HANDLE CTConsole::GetHandle() { return m_hConsole; }
void CTConsole::Show(bool) {}
void CTConsole::DisableClose() {}
void CTConsole::Clear() {}
void CTConsole::Close() { m_hConsole = NULL; }
void TRACE_NATIVE(LPCSTR, ...) {}
