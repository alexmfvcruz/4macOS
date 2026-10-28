// Native Win32 window, message, input and GDI-handle subset (C4-P), part of
// <windows.h>. Implementation: native/client/compat/src (library
// libcompat_ui.a). The window system runs on one GUI thread, the thread that
// creates the first window. A platform backend (SDL3 in native/client/shell,
// or the scripted test backend) supplies native windows and input; the rules
// of Win32 (message order, key state, double clicks, character messages) are
// implemented here, independent of the backend. See compat/README.md,
// section "Window system (C4-P)".
#pragma once
#include "fourstory_win32_constants.h"
#include <string>
#include <vector>

// ---- Handles ------------------------------------------------------------------
FOURSTORY_COMPAT_HANDLE(HACCEL);
FOURSTORY_COMPAT_HANDLE(HHOOK);
FOURSTORY_COMPAT_HANDLE(HBRUSH);
FOURSTORY_COMPAT_HANDLE(HPEN);
FOURSTORY_COMPAT_HANDLE(HRGN);
FOURSTORY_COMPAT_HANDLE(HPALETTE);
FOURSTORY_COMPAT_HANDLE(HKL);
FOURSTORY_COMPAT_HANDLE(HRSRC);
FOURSTORY_COMPAT_HANDLE(HDROP);
typedef void* HGDIOBJ;
typedef HANDLE HGLOBAL;
typedef COLORREF* LPCOLORREF;
typedef WORD ATOM;
typedef DWORD LCID;
typedef WORD LANGID;

// ---- Procedures and structures ------------------------------------------------
typedef LRESULT (CALLBACK* WNDPROC)(HWND, UINT, WPARAM, LPARAM);
typedef LRESULT (CALLBACK* HOOKPROC)(int, WPARAM, LPARAM);
typedef BOOL (CALLBACK* WNDENUMPROC)(HWND, LPARAM);
typedef VOID (CALLBACK* TIMERPROC)(HWND, UINT, UINT_PTR, DWORD);

struct MSG { HWND hwnd; UINT message; WPARAM wParam; LPARAM lParam; DWORD time; POINT pt; };
typedef MSG *PMSG, *LPMSG;
struct CREATESTRUCT {
    LPVOID lpCreateParams; HINSTANCE hInstance; HMENU hMenu; HWND hwndParent;
    int cy, cx, y, x; LONG style; LPCSTR lpszName; LPCSTR lpszClass; DWORD dwExStyle;
};
typedef CREATESTRUCT *LPCREATESTRUCT;
struct WNDCLASS {
    UINT style; WNDPROC lpfnWndProc; int cbClsExtra, cbWndExtra; HINSTANCE hInstance;
    HICON hIcon; HCURSOR hCursor; HBRUSH hbrBackground; LPCSTR lpszMenuName; LPCSTR lpszClassName;
};
typedef WNDCLASS *LPWNDCLASS;
struct NMHDR { HWND hwndFrom; UINT_PTR idFrom; UINT code; };
typedef NMHDR *LPNMHDR;
struct PAINTSTRUCT { HDC hdc; BOOL fErase; RECT rcPaint; BOOL fRestore, fIncUpdate; BYTE rgbReserved[32]; };
typedef PAINTSTRUCT *LPPAINTSTRUCT;
struct WINDOWPOS { HWND hwnd, hwndInsertAfter; int x, y, cx, cy; UINT flags; };
typedef WINDOWPOS *LPWINDOWPOS;
struct MINMAXINFO { POINT ptReserved, ptMaxSize, ptMaxPosition, ptMinTrackSize, ptMaxTrackSize; };
struct ACCEL { BYTE fVirt; WORD key; WORD cmd; };
typedef ACCEL *LPACCEL;
struct POINTS { SHORT x, y; };

#define HWND_TOP ((HWND)0)
#define HWND_BOTTOM ((HWND)1)
#define HWND_TOPMOST ((HWND)-1)
#define HWND_NOTOPMOST ((HWND)-2)
#define HWND_DESKTOP ((HWND)0)
#define HWND_BROADCAST ((HWND)0xffff)
#define HWND_MESSAGE ((HWND)-3)
#define MAKEINTRESOURCE(i) ((LPSTR)((ULONG_PTR)((WORD)(i))))
#define MAKEINTRESOURCEA MAKEINTRESOURCE
#define IS_INTRESOURCE(r) ((((ULONG_PTR)(r)) >> 16) == 0)
#define IDC_ARROW MAKEINTRESOURCE(32512)
#define IDC_IBEAM MAKEINTRESOURCE(32513)
#define IDC_WAIT MAKEINTRESOURCE(32514)
#define IDC_CROSS MAKEINTRESOURCE(32515)
#define IDC_SIZEALL MAKEINTRESOURCE(32646)
#define IDC_NO MAKEINTRESOURCE(32648)
#define IDC_HAND MAKEINTRESOURCE(32649)
#define IDI_APPLICATION MAKEINTRESOURCE(32512)
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#define GET_WHEEL_DELTA_WPARAM(wParam) ((short)HIWORD(wParam))
#define GET_KEYSTATE_WPARAM(wParam) (LOWORD(wParam))

// ---- Window classes and windows ---------------------------------------------
ATOM RegisterClass(const WNDCLASS* window_class);
BOOL UnregisterClass(LPCSTR class_name, HINSTANCE instance);
BOOL GetClassInfo(HINSTANCE instance, LPCSTR class_name, LPWNDCLASS window_class);
HWND CreateWindowEx(DWORD ex_style, LPCSTR class_name, LPCSTR window_name, DWORD style, int x, int y, int width, int height,
                    HWND parent, HMENU menu, HINSTANCE instance, LPVOID param);
inline HWND CreateWindow(LPCSTR class_name, LPCSTR window_name, DWORD style, int x, int y, int width, int height,
                         HWND parent, HMENU menu, HINSTANCE instance, LPVOID param) {
    return CreateWindowEx(0, class_name, window_name, style, x, y, width, height, parent, menu, instance, param);
}
BOOL DestroyWindow(HWND window);
BOOL IsWindow(HWND window);
BOOL ShowWindow(HWND window, int command);
BOOL UpdateWindow(HWND window);
BOOL MoveWindow(HWND window, int x, int y, int width, int height, BOOL repaint);
BOOL SetWindowPos(HWND window, HWND insert_after, int x, int y, int cx, int cy, UINT flags);
BOOL GetWindowRect(HWND window, LPRECT rect);
BOOL GetClientRect(HWND window, LPRECT rect);
BOOL ClientToScreen(HWND window, LPPOINT point);
BOOL ScreenToClient(HWND window, LPPOINT point);
int MapWindowPoints(HWND from, HWND to, LPPOINT points, UINT count);
BOOL AdjustWindowRect(LPRECT rect, DWORD style, BOOL menu);
BOOL AdjustWindowRectEx(LPRECT rect, DWORD style, BOOL menu, DWORD ex_style);
HWND GetParent(HWND window);
HWND SetParent(HWND child, HWND parent);
HWND GetWindow(HWND window, UINT command);
HWND GetDesktopWindow();
BOOL IsChild(HWND parent, HWND window);
BOOL IsWindowVisible(HWND window);
BOOL IsWindowEnabled(HWND window);
BOOL EnableWindow(HWND window, BOOL enable);
BOOL IsIconic(HWND window);
BOOL IsZoomed(HWND window);
int GetWindowText(HWND window, LPSTR text, int size);
int GetWindowTextLength(HWND window);
BOOL SetWindowText(HWND window, LPCSTR text);
LONG_PTR GetWindowLongPtr(HWND window, int index);
LONG_PTR SetWindowLongPtr(HWND window, int index, LONG_PTR value);
inline LONG GetWindowLong(HWND window, int index) { return static_cast<LONG>(GetWindowLongPtr(window, index)); }
inline LONG SetWindowLong(HWND window, int index, LONG value) { return static_cast<LONG>(SetWindowLongPtr(window, index, value)); }
LRESULT CallWindowProc(WNDPROC procedure, HWND window, UINT message, WPARAM wparam, LPARAM lparam);
LRESULT DefWindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
BOOL EnumChildWindows(HWND parent, WNDENUMPROC procedure, LPARAM lparam);
HWND ChildWindowFromPointEx(HWND parent, POINT point, UINT flags);
inline HWND ChildWindowFromPoint(HWND parent, POINT point) { return ChildWindowFromPointEx(parent, point, 0); }
HWND WindowFromPoint(POINT point);
HWND GetActiveWindow();
HWND SetActiveWindow(HWND window);
HWND GetForegroundWindow();
BOOL SetForegroundWindow(HWND window);
HWND GetFocus();
HWND SetFocus(HWND window);
HWND SetCapture(HWND window);
BOOL ReleaseCapture();
HWND GetCapture();
HANDLE GetProp(HWND window, LPCSTR name);
BOOL SetProp(HWND window, LPCSTR name, HANDLE data);
HANDLE RemoveProp(HWND window, LPCSTR name);
BOOL InvalidateRect(HWND window, const RECT* rect, BOOL erase);
BOOL ValidateRect(HWND window, const RECT* rect);
HDC BeginPaint(HWND window, LPPAINTSTRUCT paint);
BOOL EndPaint(HWND window, const PAINTSTRUCT* paint);
BOOL FlashWindow(HWND window, BOOL invert);
BOOL SetWindowIcon(HWND window, HICON icon);

// ---- Messages ---------------------------------------------------------------
BOOL PeekMessage(LPMSG message, HWND window, UINT filter_min, UINT filter_max, UINT remove);
BOOL GetMessage(LPMSG message, HWND window, UINT filter_min, UINT filter_max);
BOOL PostMessage(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
BOOL PostThreadMessage(DWORD thread, UINT message, WPARAM wparam, LPARAM lparam);
LRESULT SendMessage(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
void PostQuitMessage(int exit_code);
BOOL TranslateMessage(const MSG* message);
LRESULT DispatchMessage(const MSG* message);
BOOL WaitMessage();
LONG GetMessageTime();
DWORD GetMessagePos();
DWORD GetQueueStatus(UINT flags);
UINT RegisterWindowMessage(LPCSTR name);
UINT_PTR SetTimer(HWND window, UINT_PTR id, UINT elapse, TIMERPROC procedure);
BOOL KillTimer(HWND window, UINT_PTR id);
HACCEL LoadAccelerators(HINSTANCE instance, LPCSTR name);
int TranslateAccelerator(HWND window, HACCEL table, LPMSG message);
HHOOK SetWindowsHookEx(int id, HOOKPROC procedure, HINSTANCE module, DWORD thread);
BOOL UnhookWindowsHookEx(HHOOK hook);
LRESULT CallNextHookEx(HHOOK hook, int code, WPARAM wparam, LPARAM lparam);

// ---- Keyboard and mouse -----------------------------------------------------
SHORT GetKeyState(int key);
SHORT GetAsyncKeyState(int key);
BOOL GetKeyboardState(PBYTE state);
UINT MapVirtualKey(UINT code, UINT map_type);
HKL GetKeyboardLayout(DWORD thread);
// macOS input sources are not switched by the program: fails, returns null.
HKL ActivateKeyboardLayout(HKL layout, UINT flags);
UINT GetDoubleClickTime();
BOOL GetCursorPos(LPPOINT point);
BOOL SetCursorPos(int x, int y);
int ShowCursor(BOOL show);
HCURSOR SetCursor(HCURSOR cursor);
HCURSOR GetCursor();
HCURSOR LoadCursor(HINSTANCE instance, LPCSTR name);
HCURSOR LoadCursorFromFile(LPCSTR file_name);
HICON LoadIcon(HINSTANCE instance, LPCSTR name);
BOOL DestroyCursor(HCURSOR cursor);
BOOL DestroyIcon(HICON icon);
BOOL ClipCursor(const RECT* rect);

// ---- System -----------------------------------------------------------------
int GetSystemMetrics(int index);
BOOL SystemParametersInfo(UINT action, UINT uparam, PVOID pvparam, UINT win_ini);
int MessageBox(HWND window, LPCSTR text, LPCSTR caption, UINT type);
BOOL MessageBeep(UINT type);

// ---- Clipboard (CF_TEXT only) and global memory -----------------------------
BOOL OpenClipboard(HWND owner);
BOOL CloseClipboard();
BOOL EmptyClipboard();
HANDLE SetClipboardData(UINT format, HANDLE memory);
HANDLE GetClipboardData(UINT format);
BOOL IsClipboardFormatAvailable(UINT format);
HGLOBAL GlobalAlloc(UINT flags, SIZE_T bytes);
LPVOID GlobalLock(HGLOBAL memory);
BOOL GlobalUnlock(HGLOBAL memory);
HGLOBAL GlobalFree(HGLOBAL memory);
SIZE_T GlobalSize(HGLOBAL memory);

// ---- Rectangles -------------------------------------------------------------
inline BOOL SetRect(LPRECT rect, int left, int top, int right, int bottom) {
    if (!rect) return FALSE;
    rect->left = left; rect->top = top; rect->right = right; rect->bottom = bottom; return TRUE;
}
inline BOOL SetRectEmpty(LPRECT rect) { return SetRect(rect, 0, 0, 0, 0); }
inline BOOL CopyRect(LPRECT to, const RECT* from) { if (!to || !from) return FALSE; *to = *from; return TRUE; }
inline BOOL OffsetRect(LPRECT rect, int dx, int dy) {
    if (!rect) return FALSE;
    rect->left += dx; rect->right += dx; rect->top += dy; rect->bottom += dy; return TRUE;
}
inline BOOL InflateRect(LPRECT rect, int dx, int dy) {
    if (!rect) return FALSE;
    rect->left -= dx; rect->right += dx; rect->top -= dy; rect->bottom += dy; return TRUE;
}
inline BOOL IsRectEmpty(const RECT* rect) { return !rect || rect->right <= rect->left || rect->bottom <= rect->top; }
// The right and bottom edges are outside the rectangle.
inline BOOL PtInRect(const RECT* rect, POINT point) {
    return rect && point.x >= rect->left && point.x < rect->right && point.y >= rect->top && point.y < rect->bottom;
}
inline BOOL EqualRect(const RECT* a, const RECT* b) {
    return a && b && a->left == b->left && a->top == b->top && a->right == b->right && a->bottom == b->bottom;
}
// An empty result is set to (0,0,0,0) and returns FALSE, as on Windows.
inline BOOL IntersectRect(LPRECT to, const RECT* a, const RECT* b) {
    if (!to || !a || !b) return FALSE;
    RECT r = { a->left > b->left ? a->left : b->left, a->top > b->top ? a->top : b->top,
               a->right < b->right ? a->right : b->right, a->bottom < b->bottom ? a->bottom : b->bottom };
    if (r.left >= r.right || r.top >= r.bottom) { SetRectEmpty(to); return FALSE; }
    *to = r; return TRUE;
}
inline BOOL UnionRect(LPRECT to, const RECT* a, const RECT* b) {
    if (!to || !a || !b) return FALSE;
    if (IsRectEmpty(a) && IsRectEmpty(b)) { SetRectEmpty(to); return FALSE; }
    if (IsRectEmpty(a)) { *to = *b; return TRUE; }
    if (IsRectEmpty(b)) { *to = *a; return TRUE; }
    RECT r = { a->left < b->left ? a->left : b->left, a->top < b->top ? a->top : b->top,
               a->right > b->right ? a->right : b->right, a->bottom > b->bottom ? a->bottom : b->bottom };
    *to = r; return TRUE;
}

// ---- Code pages -------------------------------------------------------------
// CP_ACP is the ANSI code page of the client data: 1252 by default; the
// environment variable FOURSTORY_ACP selects 1250, 1251, 1253 or 1254 (through
// iconv). Double-byte code pages are NOT SUPPORTED. WCHAR is UTF-16.
UINT GetACP();
int MultiByteToWideChar(UINT code_page, DWORD flags, LPCSTR text, int bytes, WCHAR* wide, int wide_size);
int WideCharToMultiByte(UINT code_page, DWORD flags, const WCHAR* wide, int wide_size, LPSTR text, int bytes,
                        LPCSTR default_char, BOOL* used_default);
BOOL IsDBCSLeadByte(BYTE value);
BOOL IsDBCSLeadByteEx(UINT code_page, BYTE value);
BOOL SetThreadLocale(LCID locale);
LCID GetThreadLocale();
LCID GetUserDefaultLCID();
int GetLocaleInfo(LCID locale, DWORD type, LPSTR data, int size);
#define MAKELCID(lgid, srtid) ((DWORD)((((DWORD)((WORD)(srtid))) << 16) | ((DWORD)((WORD)(lgid)))))
#define PRIMARYLANGID(lgid) ((WORD)(lgid) & 0x3ff)
#define SUBLANGID(lgid) ((WORD)(lgid) >> 10)

// ---- Native window system interface (not Win32) ------------------------------
namespace fourstory { namespace platform {
// Abstract input events, reported by a backend from Backend::pump on the GUI
// thread. Coordinates are client coordinates of the top-level window in
// window units (points on macOS). Keys are Windows virtual-key codes; the
// backend reports the left/right variants (VK_LSHIFT, VK_RCONTROL, ...).
void input_key(HWND top, UINT vk, UINT scan_code, bool extended, bool down, bool repeat);
void input_text(HWND top, const char* utf8);
void input_mouse_move(HWND top, int x, int y);
void input_mouse_button(HWND top, int button, bool down, int x, int y); // 0 left, 1 right, 2 middle
void input_mouse_wheel(HWND top, float delta_y, float delta_x, int x, int y);
void input_focus(HWND top, bool gained);
void input_moved(HWND top, int x, int y);       // client origin in screen coordinates
void input_resized(HWND top, int width, int height);
void input_shown(HWND top, int command);        // SW_MINIMIZE, SW_MAXIMIZE, SW_RESTORE
void input_close(HWND top);
void input_quit();

struct WindowDesc { int x, y, width, height; bool decorated, visible, topmost; const char* title_utf8; };
// A decoded cursor (.cur or animated .ani): frames of width x height RGBA
// pixels (straight alpha, top row first) with their display time; hot spot in pixels.
struct CursorImage {
    int width = 0, height = 0, hot_x = 0, hot_y = 0;
    std::vector<std::vector<unsigned char>> frames;  // width*height*4 bytes each
    std::vector<DWORD> durations_ms;                  // per frame; empty for a static cursor
};
// Decodes a cursor file (ICONDIR type 2 with DIB images) or an animated
// cursor (RIFF ACON: anih, optional rate/seq, fram LIST). The largest image of
// each frame is used. Monochrome inverted pixels become opaque black (no screen
// inversion). Returns false for a format it does not decode (PNG frames).
bool decode_cursor(const unsigned char* data, std::size_t size, CursorImage* image);
class Backend {
public:
    virtual ~Backend() {}
    virtual const char* name() const = 0;
    virtual void* create_window(HWND window, const WindowDesc& desc) = 0; // nullptr on failure
    virtual void destroy_window(void* native) = 0;
    virtual void set_title(void* native, const char* utf8) = 0;
    virtual bool set_decorated(void* native, bool decorated) = 0;
    virtual void set_client_rect(void* native, int x, int y, int width, int height) = 0;
    virtual void show(void* native, int command) = 0;
    virtual void raise(void* native) = 0;
    virtual bool set_topmost(void* native, bool topmost) = 0;
    virtual void pump() = 0;                  // translate pending native events (GUI thread)
    virtual void wait(DWORD milliseconds) = 0; // block until a native event, wake() or the time
    virtual void wake() = 0;                  // any thread
    virtual void capture_mouse(bool on) = 0;
    virtual void show_cursor(bool show) = 0;
    virtual void set_system_cursor(int idc) = 0;
    virtual bool warp_mouse(void* native, int x, int y) = 0;
    virtual bool cursor_position(int* x, int* y) = 0;  // screen coordinates
    virtual void screen_size(int* width, int* height) = 0;
    virtual int frame_caption_height() const = 0;      // title bar height of a decorated window
    virtual bool set_clipboard(const char* utf8) = 0;
    virtual bool clipboard(std::string* utf8) = 0;
    virtual int message_box(const char* text_utf8, const char* caption_utf8, UINT type) = 0;
    // Program cursors (LoadCursor with a resource). Default: not supported.
    virtual void* create_cursor(const CursorImage&) { return nullptr; }
    virtual void set_cursor(void*) {}
    virtual void destroy_cursor(void*) {}
};
// Installs the backend; the caller keeps ownership. nullptr removes it. Window
// creation fails with ERROR_NOT_SUPPORTED without a backend.
void set_backend(Backend* backend);
Backend* backend();
void* native_window(HWND window);        // the backend handle of a top-level window, or nullptr
HWND window_from_native(void* native);
// Called by CreateWindowEx after the handle exists and before WM_NCCREATE.
// MFC uses it to attach the CWnd that is being created (AfxHookWindowCreate).
typedef void (*CreateHook)(HWND window);
void set_create_hook(CreateHook hook);
// Text conversion between CP_ACP and UTF-8 (backends, message boxes).
std::string acp_to_utf8(const char* text);
std::string utf8_to_acp(const char* text);
}}
// The backend window handle of a top-level window as an HWND, for APIs that
// take the native window (DXVK Native: SDL_Window*). nullptr without one.
inline HWND fourstory_native_window(HWND window) { return static_cast<HWND>(fourstory::platform::native_window(window)); }
