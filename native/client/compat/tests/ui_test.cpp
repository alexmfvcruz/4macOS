// Unit tests of the C4-P window system (libcompat_ui): Win32 message rules,
// MFC message maps, profile store, code pages and GDI objects. A scripted
// backend stands in for SDL3. Expected values are the documented Win32 and MFC
// behavior (cited per check); they were not recorded from a Windows program.
#include <afxwin.h>
#include <gdiplus.h>
#include <atomic>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

static int g_failures = 0, g_checks = 0;
#define CHECK(cond) do { ++g_checks; if (!(cond)) { ++g_failures; std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

// ---- Scripted backend ----------------------------------------------------------
class TestBackend : public fourstory::platform::Backend {
public:
    struct NativeWindow { int x, y, w, h; bool decorated, visible; std::string title; };
    std::vector<NativeWindow*> windows;
    std::atomic<int> wakes{0};
    bool captured = false, cursor_shown = true, topmost = false;
    int system_cursor = 0;
    std::string clip;
    const char* name() const override { return "test"; }
    void* create_window(HWND, const fourstory::platform::WindowDesc& d) override {
        NativeWindow* n = new NativeWindow{d.x, d.y, d.width, d.height, d.decorated, d.visible, d.title_utf8 ? d.title_utf8 : ""};
        windows.push_back(n); return n;
    }
    void destroy_window(void* native) override {
        for (auto it = windows.begin(); it != windows.end(); ++it) if (*it == native) { delete *it; windows.erase(it); break; }
    }
    bool set_decorated(void* native, bool decorated) override { static_cast<NativeWindow*>(native)->decorated = decorated; return true; }
    void set_title(void* native, const char* utf8) override { static_cast<NativeWindow*>(native)->title = utf8; }
    void set_client_rect(void* native, int x, int y, int w, int h) override { NativeWindow* n = static_cast<NativeWindow*>(native); n->x = x; n->y = y; n->w = w; n->h = h; }
    void show(void* native, int command) override { static_cast<NativeWindow*>(native)->visible = command != SW_HIDE; }
    void raise(void*) override {}
    bool set_topmost(void*, bool on) override { topmost = on; return true; }
    void pump() override {}
    void wait(DWORD ms) override { std::this_thread::sleep_for(std::chrono::milliseconds(ms > 5 ? 5 : ms)); }
    void wake() override { ++wakes; }
    void capture_mouse(bool on) override { captured = on; }
    void show_cursor(bool show) override { cursor_shown = show; }
    void set_system_cursor(int idc) override { system_cursor = idc; }
    bool warp_mouse(void*, int, int) override { return true; }
    bool cursor_position(int*, int*) override { return false; }
    void screen_size(int* w, int* h) override { *w = 1920; *h = 1080; }
    int frame_caption_height() const override { return 28; }
    bool set_clipboard(const char* utf8) override { clip = utf8; return true; }
    bool clipboard(std::string* utf8) override { *utf8 = clip; return true; }
    int message_box(const char*, const char*, UINT) override { return IDOK; }
};
static TestBackend g_backend;

// ---- A window class that records what it receives ----------------------------------
struct Event { UINT message; WPARAM w; LPARAM l; };
static std::vector<Event> g_events;
static LRESULT CALLBACK RecordProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m != WM_SETCURSOR || true) g_events.push_back({m, w, l});
    return DefWindowProc(h, m, w, l);
}
static HWND make_window(UINT class_style, DWORD style = WS_POPUP | WS_VISIBLE, int w = 800, int h = 600) {
    static int n = 0;
    static std::vector<std::string> names;
    names.push_back("Record" + std::to_string(n++));
    WNDCLASS wc = {};
    wc.style = class_style; wc.lpfnWndProc = RecordProc; wc.lpszClassName = names.back().c_str();
    RegisterClass(&wc);
    return CreateWindowEx(0, wc.lpszClassName, "test", style, 100, 50, w, h, nullptr, nullptr, nullptr, nullptr);
}
static void drain(HWND, bool translate = true) {
    MSG m;
    while (PeekMessage(&m, nullptr, 0, 0, PM_REMOVE)) { if (m.message == WM_QUIT) break; if (translate) TranslateMessage(&m); DispatchMessage(&m); }
}
static std::vector<Event> take(UINT a, UINT b = 0) {
    std::vector<Event> out;
    for (const Event& e : g_events) if (e.message == a || (b && e.message == b)) out.push_back(e);
    g_events.clear();
    return out;
}

static void test_create_and_frame() {
    // AdjustWindowRect and CreateWindowEx share one frame model: a captioned window of
    // the adjusted size has the requested client size.
    RECT r = {0, 0, 640, 480};
    AdjustWindowRect(&r, WS_OVERLAPPED | WS_CAPTION, FALSE);
    CHECK(r.top == -28 && r.bottom == 480 && r.left == 0 && r.right == 640);
    g_events.clear();
    HWND w = make_window(0, WS_OVERLAPPED | WS_CAPTION, r.right - r.left, r.bottom - r.top);
    CHECK(w != nullptr);
    RECT c; GetClientRect(w, &c);
    CHECK(c.right == 640 && c.bottom == 480);
    // Creation order: WM_NCCREATE, WM_CREATE, WM_SIZE, WM_MOVE (Win32 CreateWindowEx).
    CHECK(g_events.size() >= 4 && g_events[0].message == WM_NCCREATE && g_events[1].message == WM_CREATE && g_events[2].message == WM_SIZE);
    POINT p = {0, 0}; ClientToScreen(w, &p);
    CHECK(p.x == 100 && p.y == 50 + 28);
    CHECK(g_backend.windows.back()->w == 640 && g_backend.windows.back()->h == 480 && g_backend.windows.back()->decorated);
    for (int i = 0; i < 3; ++i) {
        SetWindowLongPtr(w, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        CHECK(!g_backend.windows.back()->decorated);
        GetClientRect(w, &c); CHECK(c.right == 640 && c.bottom == 480);
        POINT q = {0, 0}; ClientToScreen(w, &q); CHECK(q.x == p.x && q.y == p.y);
        SetWindowLongPtr(w, GWL_STYLE, WS_OVERLAPPED | WS_CAPTION);
        CHECK(g_backend.windows.back()->decorated);
        GetClientRect(w, &c); CHECK(c.right == 640 && c.bottom == 480);
    }
    DestroyWindow(w);
    CHECK(!IsWindow(w));
    g_events.clear();
}

static void test_keys() {
    HWND w = make_window(0);
    SetFocus(w);
    drain(w); g_events.clear();
    fourstory::platform::input_key(w, 'A', 0, false, true, false);
    fourstory::platform::input_text(w, "a");
    fourstory::platform::input_key(w, 'A', 0, false, false, false);
    drain(w);
    auto ev = take(WM_KEYDOWN, WM_KEYUP);
    std::vector<Event> all = ev;
    CHECK(all.size() == 2);
    // lParam: repeat 1, scan code 0x1E in bits 16-23; key up sets bits 30 and 31.
    CHECK(all.size() == 2 && all[0].w == 'A' && (all[0].l & 0xFFFF) == 1 && ((all[0].l >> 16) & 0xFF) == 0x1E && !((all[0].l >> 30) & 1));
    CHECK(all.size() == 2 && ((all[1].l >> 30) & 3) == 3);
    // Text arrives as WM_CHAR with the code page byte.
    fourstory::platform::input_key(w, 'E', 0, false, true, false);
    fourstory::platform::input_text(w, "\xC3\xA9");   // e acute in UTF-8
    fourstory::platform::input_text(w, "\xE2\x82\xAC"); // euro sign
    fourstory::platform::input_key(w, 'E', 0, false, false, false);
    drain(w);
    auto chars = take(WM_CHAR);
    CHECK(chars.size() == 2 && chars[0].w == 0xE9 && chars[1].w == 0x80);
    // Shift: the message names VK_SHIFT; GetKeyState follows message retrieval.
    fourstory::platform::input_key(w, VK_LSHIFT, 0, false, true, false);
    CHECK(!(GetKeyState(VK_SHIFT) & 0x8000));
    CHECK(GetAsyncKeyState(VK_SHIFT) & 0x8000);
    drain(w);
    CHECK(GetKeyState(VK_SHIFT) & 0x8000);
    CHECK(GetKeyState(VK_LSHIFT) & 0x8000);
    auto shift = take(WM_KEYDOWN);
    CHECK(shift.size() == 1 && shift[0].w == VK_SHIFT);
    fourstory::platform::input_key(w, VK_LSHIFT, 0, false, false, false);
    drain(w); g_events.clear();
    CHECK(!(GetKeyState(VK_SHIFT) & 0x8000));
    // Control characters come from TranslateMessage: Enter, Backspace, Ctrl+C.
    fourstory::platform::input_key(w, VK_RETURN, 0, false, true, false);
    fourstory::platform::input_key(w, VK_BACK, 0, false, true, false);
    fourstory::platform::input_key(w, VK_LCONTROL, 0, false, true, false);
    drain(w);
    fourstory::platform::input_key(w, 'C', 0, false, true, false);
    drain(w);
    fourstory::platform::input_key(w, VK_LCONTROL, 0, false, false, false);
    drain(w);
    auto control = take(WM_CHAR);
    CHECK(control.size() == 3 && control[0].w == 0x0D && control[1].w == 0x08 && control[2].w == 0x03);
    // Alt combinations are system keys (WM_SYSKEYDOWN, context bit 29).
    fourstory::platform::input_key(w, VK_LMENU, 0, false, true, false);
    fourstory::platform::input_key(w, '1', 0, false, true, false);
    drain(w);
    auto sys = take(WM_SYSKEYDOWN);
    CHECK(sys.size() == 2 && sys[0].w == VK_MENU && sys[1].w == '1' && ((sys[1].l >> 29) & 1));
    fourstory::platform::input_key(w, '1', 0, false, false, false);
    fourstory::platform::input_key(w, VK_LMENU, 0, false, false, false);
    drain(w); g_events.clear();
    // Auto-repeat sets the previous-state bit 30.
    fourstory::platform::input_key(w, VK_LEFT, 0, true, true, false);
    fourstory::platform::input_key(w, VK_LEFT, 0, true, true, true);
    drain(w);
    auto rep = take(WM_KEYDOWN);
    CHECK(rep.size() == 2 && !((rep[0].l >> 30) & 1) && ((rep[1].l >> 30) & 1) && ((rep[1].l >> 24) & 1));
    fourstory::platform::input_key(w, VK_LEFT, 0, true, false, false);
    drain(w);
    DestroyWindow(w); g_events.clear();
}

static void test_mouse() {
    HWND w = make_window(CS_DBLCLKS);
    drain(w); g_events.clear();
    // Moves are coalesced: only the newest queued WM_MOUSEMOVE remains.
    fourstory::platform::input_mouse_move(w, 10, 10);
    fourstory::platform::input_mouse_move(w, 20, 30);
    drain(w);
    auto moves = take(WM_MOUSEMOVE, WM_SETCURSOR);
    CHECK(moves.size() == 2 && moves[0].message == WM_SETCURSOR && LOWORD(moves[0].l) == HTCLIENT && HIWORD(moves[0].l) == WM_MOUSEMOVE);
    CHECK(moves.size() == 2 && GET_X_LPARAM(moves[1].l) == 20 && GET_Y_LPARAM(moves[1].l) == 30);
    // Second press within 500 ms and 4 pixels: WM_LBUTTONDBLCLK (class has CS_DBLCLKS).
    fourstory::platform::input_mouse_button(w, 0, true, 20, 30);
    fourstory::platform::input_mouse_button(w, 0, false, 20, 30);
    fourstory::platform::input_mouse_button(w, 0, true, 21, 31);
    fourstory::platform::input_mouse_button(w, 0, false, 21, 31);
    drain(w);
    auto clicks = take(WM_LBUTTONDOWN, WM_LBUTTONDBLCLK);
    CHECK(clicks.size() == 2 && clicks[0].message == WM_LBUTTONDOWN && clicks[1].message == WM_LBUTTONDBLCLK && (clicks[1].w & MK_LBUTTON));
    // Without CS_DBLCLKS the second press stays WM_LBUTTONDOWN.
    HWND plain = make_window(0);
    drain(plain); g_events.clear();
    fourstory::platform::input_mouse_button(plain, 1, true, 5, 5);
    fourstory::platform::input_mouse_button(plain, 1, false, 5, 5);
    fourstory::platform::input_mouse_button(plain, 1, true, 5, 5);
    fourstory::platform::input_mouse_button(plain, 1, false, 5, 5);
    drain(plain);
    CHECK(take(WM_RBUTTONDOWN, WM_RBUTTONDBLCLK).size() == 2);
    // Wheel: 120 per notch, accumulated for fractional deltas; lParam holds screen coordinates.
    SetFocus(w);
    drain(w); g_events.clear();
    fourstory::platform::input_mouse_wheel(w, 0.5f, 0, 20, 30);
    fourstory::platform::input_mouse_wheel(w, 0.5f, 0, 20, 30);
    fourstory::platform::input_mouse_wheel(w, -1.0f, 0, 20, 30);
    drain(w);
    auto wheel = take(WM_MOUSEWHEEL);
    CHECK(wheel.size() == 3 && static_cast<short>(HIWORD(wheel[0].w)) == 60 && static_cast<short>(HIWORD(wheel[2].w)) == -120);
    CHECK(wheel.size() == 3 && GET_X_LPARAM(wheel[0].l) == 100 + 20 && GET_Y_LPARAM(wheel[0].l) == 50 + 30);
    // Capture: messages go to the capture window, outside its client area too, without WM_SETCURSOR.
    SetCapture(w);
    CHECK(g_backend.captured && GetCapture() == w);
    fourstory::platform::input_mouse_move(w, -5, 900);
    drain(w);
    auto captured = take(WM_MOUSEMOVE, WM_SETCURSOR);
    CHECK(captured.size() == 1 && captured[0].message == WM_MOUSEMOVE && GET_X_LPARAM(captured[0].l) == -5 && GET_Y_LPARAM(captured[0].l) == 900);
    ReleaseCapture();
    CHECK(!g_backend.captured && GetCapture() == nullptr);
    DestroyWindow(plain); DestroyWindow(w); g_events.clear();
}

static void test_order_timers_quit() {
    HWND w = make_window(0);
    drain(w); g_events.clear();
    // Posted messages before input messages; WM_QUIT after posted, before input.
    fourstory::platform::input_key(w, 'Q', 0, false, true, false);
    PostMessage(w, WM_USER + 1, 0, 0);
    PostQuitMessage(7);
    MSG m;
    CHECK(PeekMessage(&m, nullptr, 0, 0, PM_REMOVE) && m.message == WM_USER + 1);
    CHECK(PeekMessage(&m, nullptr, 0, 0, PM_REMOVE) && m.message == WM_QUIT && m.wParam == 7);
    CHECK(PeekMessage(&m, nullptr, 0, 0, PM_REMOVE) && m.message == WM_KEYDOWN);
    fourstory::platform::input_key(w, 'Q', 0, false, false, false);
    drain(w); g_events.clear();
    // A filter range selects messages; PM_NOREMOVE keeps them.
    PostMessage(w, WM_USER + 2, 0, 0);
    PostMessage(w, WM_USER + 3, 0, 0);
    CHECK(PeekMessage(&m, nullptr, WM_USER + 3, WM_USER + 3, PM_NOREMOVE) && m.message == WM_USER + 3);
    CHECK(PeekMessage(&m, nullptr, WM_USER + 3, WM_USER + 3, PM_REMOVE) && m.message == WM_USER + 3);
    CHECK(PeekMessage(&m, nullptr, 0, 0, PM_REMOVE) && m.message == WM_USER + 2);
    // Timers: WM_TIMER only when nothing else is queued; one message per expiry.
    CHECK(SetTimer(w, 42, 20, nullptr) == 42);
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    int timers = 0;
    while (PeekMessage(&m, nullptr, 0, 0, PM_REMOVE)) if (m.message == WM_TIMER && m.wParam == 42) ++timers;
    CHECK(timers == 1);
    CHECK(KillTimer(w, 42) && !KillTimer(w, 42));
    // GetMessage waits for a message posted by another thread.
    std::thread poster([w] { std::this_thread::sleep_for(std::chrono::milliseconds(20)); PostMessage(w, WM_USER + 9, 1, 2); });
    CHECK(GetMessage(&m, nullptr, 0, 0) == TRUE && m.message == WM_USER + 9 && m.lParam == 2);
    poster.join();
    // SendMessage from another thread runs on the GUI thread while it retrieves messages.
    std::atomic<LRESULT> sent_result{-1};
    std::thread sender([w, &sent_result] { sent_result = SendMessage(w, WM_GETTEXTLENGTH, 0, 0); });
    for (int i = 0; i < 200 && sent_result == -1; ++i) { PeekMessage(&m, nullptr, 0, 0, PM_REMOVE); std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
    sender.join();
    CHECK(sent_result == 4); // "test"
    DestroyWindow(w); g_events.clear();
}

// ---- MFC ---------------------------------------------------------------------------
struct Calls { int create = 0, lbutton = 0, key = 0, wheel = 0, destroy = 0, ncdestroy_seen = 0, message = 0; CPoint point; UINT ch = 0; short delta = 0; };
static Calls g_calls;
class CBase : public CWnd {
public:
    afx_msg void OnLButtonDown(UINT flags, CPoint point) { ++g_calls.lbutton; g_calls.point = point; (void)flags; }
    afx_msg void OnDestroy() { ++g_calls.destroy; CWnd::OnDestroy(); }
    DECLARE_MESSAGE_MAP()
};
BEGIN_MESSAGE_MAP(CBase, CWnd)
    ON_WM_LBUTTONDOWN()
    ON_WM_DESTROY()
END_MESSAGE_MAP()
class CDerived : public CBase {
public:
    afx_msg int OnCreate(LPCREATESTRUCT cs) { ++g_calls.create; return CWnd::OnCreate(cs); }
    afx_msg void OnKeyDown(UINT ch, UINT repeat, UINT flags) { ++g_calls.key; g_calls.ch = ch; (void)repeat; (void)flags; }
    afx_msg BOOL OnMouseWheel(UINT flags, short delta, CPoint point) { ++g_calls.wheel; g_calls.delta = delta; (void)flags; (void)point; return TRUE; }
    afx_msg LRESULT OnCustom(WPARAM w, LPARAM l) { ++g_calls.message; return static_cast<LRESULT>(w + l); }
    DECLARE_MESSAGE_MAP()
};
BEGIN_MESSAGE_MAP(CDerived, CBase)
    ON_WM_CREATE()
    ON_WM_KEYDOWN()
    ON_WM_MOUSEWHEEL()
    ON_MESSAGE(WM_USER + 20, OnCustom)
END_MESSAGE_MAP()
class CTestApp : public CWinApp {
public:
    CTestApp() : CWinApp("UiTest") {}
};
static CTestApp g_test_app;

static void test_mfc() {
    CDerived* wnd = new CDerived;
    CRect rect(0, 0, 320, 200);
    CHECK(wnd->CreateEx(0, AfxRegisterWndClass(CS_DBLCLKS), "mfc", WS_POPUP | WS_VISIBLE, rect, nullptr, 0));
    CHECK(g_calls.create == 1);
    CHECK(CWnd::FromHandlePermanent(wnd->m_hWnd) == wnd);
    CHECK(wnd->IsKindOf(RUNTIME_CLASS(CWnd)) && !wnd->IsKindOf(RUNTIME_CLASS(CWinApp)));
    g_test_app.m_pMainWnd = wnd;
    wnd->SetFocus();
    MSG m;
    while (PeekMessage(&m, nullptr, 0, 0, PM_REMOVE)) DispatchMessage(&m);
    // A handler of the base class map is found from the derived class.
    fourstory::platform::input_mouse_button(wnd->m_hWnd, 0, true, 33, 44);
    fourstory::platform::input_mouse_button(wnd->m_hWnd, 0, false, 33, 44);
    fourstory::platform::input_key(wnd->m_hWnd, VK_F5, 0, false, true, false);
    fourstory::platform::input_key(wnd->m_hWnd, VK_F5, 0, false, false, false);
    fourstory::platform::input_mouse_wheel(wnd->m_hWnd, -2.0f, 0, 1, 1);
    while (PeekMessage(&m, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&m); DispatchMessage(&m); }
    CHECK(g_calls.lbutton == 1 && g_calls.point.x == 33 && g_calls.point.y == 44);
    CHECK(g_calls.key == 1 && g_calls.ch == VK_F5);
    CHECK(g_calls.wheel == 1 && g_calls.delta == -240);
    CHECK(wnd->SendMessage(WM_USER + 20, 3, 4) == 7 && g_calls.message == 1);
    // Unhandled messages reach DefWindowProc (WM_GETTEXTLENGTH).
    CHECK(wnd->SendMessage(WM_GETTEXTLENGTH) == 3);
    // Destroying the main window posts WM_QUIT (CWnd::OnNcDestroy) and detaches the CWnd.
    HWND h = wnd->m_hWnd;
    CHECK(wnd->DestroyWindow());
    CHECK(g_calls.destroy == 1 && wnd->m_hWnd == nullptr && CWnd::FromHandlePermanent(h) == nullptr && !IsWindow(h));
    bool quit = false;
    while (PeekMessage(&m, nullptr, 0, 0, PM_REMOVE)) if (m.message == WM_QUIT) quit = true;
    CHECK(quit && AfxGetMainWnd() == nullptr);
    delete wnd;
}

static void test_profile() {
    char dir[] = "/tmp/fourstory-ui-test-XXXXXX";
    CHECK(mkdtemp(dir) != nullptr);
    setenv("FOURSTORY_SETTINGS_DIR", dir, 1);
    g_test_app.SetRegistryKey("4Story");
    CHECK(std::string(g_test_app.m_pszProfileName) == "UiTest");
    CHECK(g_test_app.GetProfileInt("Settings", "ScreenX", 5) == 5);
    CHECK(g_test_app.GetProfileString("Settings", "WindowedMode", "NOT FOUND") == "NOT FOUND");
    CHECK(g_test_app.WriteProfileString("Settings", "WindowedMode", "TRUE"));
    CHECK(g_test_app.WriteProfileInt("Settings", "PresentationInterval", static_cast<int>(0x80000000u)));
    CHECK(g_test_app.WriteProfileString("ChatInterface", "CHATTITLE0", "All \"quoted\" \\ \xE4"));
    // Type is kept: an integer read of a string value gives the default (MFC registry path).
    CHECK(g_test_app.GetProfileInt("Settings", "WindowedMode", 9) == 9);
    CHECK(g_test_app.GetProfileInt("Settings", "PresentationInterval", 0) == 0x80000000u);
    const std::string path = fourstory::mfc::profile_store_path();
    CHECK(path == std::string(dir) + "/4Story/UiTest/settings.reg");
    FILE* f = std::fopen(path.c_str(), "rb");
    std::string text;
    if (f) { char b[4096]; size_t n; while ((n = std::fread(b, 1, sizeof(b), f)) > 0) text.append(b, n); std::fclose(f); }
    CHECK(text.compare(0, 8, "REGEDIT4") == 0);
    CHECK(text.find("[HKEY_CURRENT_USER\\Software\\4Story\\UiTest\\Settings]\r\n") != std::string::npos);
    CHECK(text.find("\"PresentationInterval\"=dword:80000000\r\n") != std::string::npos);
    CHECK(text.find("\"CHATTITLE0\"=\"All \\\"quoted\\\" \\\\ \xE4\"") != std::string::npos);
    // A UTF-16 export of reg.exe (Version 5.00) is read too.
    const std::string v5 = std::string("Windows Registry Editor Version 5.00\r\n\r\n[HKEY_CURRENT_USER\\Software\\4Story\\UiTest\\Settings]\r\n\"ScreenY\"=dword:0000095c\r\n\"UseShader\"=\"FALSE\"\r\n");
    std::string utf16 = "\xFF\xFE";
    for (char c : v5) { utf16 += c; utf16 += '\0'; }
    f = std::fopen(path.c_str(), "wb");
    if (f) { std::fwrite(utf16.data(), 1, utf16.size(), f); std::fclose(f); }
    g_test_app.SetRegistryKey("4Story"); // same path: force a reload by switching away and back
    setenv("FOURSTORY_SETTINGS_DIR", "/tmp", 1); g_test_app.GetProfileInt("x", "y", 0);
    setenv("FOURSTORY_SETTINGS_DIR", dir, 1);
    CHECK(g_test_app.GetProfileInt("Settings", "ScreenY", 0) == 2396);
    CHECK(g_test_app.GetProfileString("Settings", "UseShader", "") == "FALSE");
    CHECK(g_test_app.GetProfileString("Settings", "WindowedMode", "gone") == "gone");
}

static void test_code_pages_and_gdi() {
    WCHAR wide[16];
    CHECK(MultiByteToWideChar(CP_ACP, 0, "\x80\xE9\x9F", 3, wide, 16) == 3 && wide[0] == 0x20AC && wide[1] == 0xE9 && wide[2] == 0x178);
    char narrow[16]; BOOL used = FALSE;
    const WCHAR in[] = {0x20AC, 0x4E2D, 0};
    CHECK(WideCharToMultiByte(CP_ACP, 0, in, -1, narrow, 16, nullptr, &used) == 3 && static_cast<BYTE>(narrow[0]) == 0x80 && narrow[1] == '?' && used);
    CHECK(fourstory::platform::acp_to_utf8("\xE4\x80") == "\xC3\xA4\xE2\x82\xAC");
    // GDI: a selected object cannot be deleted; GetObject returns the LOGFONT.
    LOGFONT lf = {}; lf.lfHeight = -13; strcpy_s(lf.lfFaceName, "2002L");
    HFONT font = CreateFontIndirect(&lf);
    HDC dc = CreateCompatibleDC(nullptr);
    HGDIOBJ old = SelectObject(dc, font);
    CHECK(old != nullptr && !DeleteObject(font));
    LOGFONT back = {};
    CHECK(GetObject(font, sizeof(back), &back) == sizeof(LOGFONT) && back.lfHeight == -13 && std::string(back.lfFaceName) == "2002L");
    char face[64];
    CHECK(GetTextFace(dc, sizeof(face), face) > 0 && std::string(face) == "Arial"); // GDI substitution, as Windows reports
    SelectObject(dc, old);
    CHECK(DeleteObject(font));
    BITMAPINFO bmi = {};
    bmi.bmiHeader = {sizeof(BITMAPINFOHEADER), 4, -2, 1, 32, BI_RGB, 0, 0, 0, 0, 0};
    void* bits = nullptr;
    HBITMAP dib = CreateDIBSection(dc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    CHECK(dib && bits);
    bmi.bmiHeader.biBitCount = 24;
    CHECK(CreateDIBSection(dc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0) == nullptr && bits == nullptr);
    CHECK(DeleteObject(dib));
    CHECK(DeleteDC(dc));
    // Clipboard: CF_TEXT through the backend in UTF-8.
    CHECK(OpenClipboard(nullptr) && EmptyClipboard());
    HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, 4);
    std::memcpy(GlobalLock(g), "\xE9t\xE9", 4); GlobalUnlock(g);
    CHECK(SetClipboardData(CF_TEXT, g) != nullptr && g_backend.clip == "\xC3\xA9t\xC3\xA9");
    HANDLE got = GetClipboardData(CF_TEXT);
    CHECK(got && std::string(static_cast<char*>(GlobalLock(got))) == "\xE9t\xE9");
    CHECK(CloseClipboard());
}

// Review fixes (critic, 2026-10-16).
static void test_review_fixes() {
    // Destroy order: WM_DESTROY parent then child; WM_NCDESTROY child then parent.
    HWND parent = make_window(0);
    WNDCLASS wc = {}; wc.lpfnWndProc = RecordProc; wc.lpszClassName = "RecordChild";
    RegisterClass(&wc);
    HWND child = CreateWindowEx(0, "RecordChild", "c", WS_CHILD | WS_VISIBLE, 0, 0, 10, 10, parent, nullptr, nullptr, nullptr);
    CHECK(child != nullptr);
    SetFocus(child);
    g_events.clear();
    DestroyWindow(parent);
    std::vector<UINT> seq;
    for (const Event& e : g_events) if (e.message == WM_DESTROY || e.message == WM_NCDESTROY || e.message == WM_KILLFOCUS) seq.push_back(e.message);
    CHECK(seq.size() == 5 && seq[0] == WM_KILLFOCUS && seq[1] == WM_DESTROY && seq[2] == WM_DESTROY && seq[3] == WM_NCDESTROY && seq[4] == WM_NCDESTROY);
    CHECK(!IsWindow(child) && GetFocus() == nullptr);
    g_events.clear();
    // SetCursor(NULL) hides the cursor; a system cursor shows it again.
    SetCursor(nullptr);
    CHECK(!g_backend.cursor_shown);
    SetCursor(LoadCursor(nullptr, IDC_ARROW));
    CHECK(g_backend.cursor_shown && g_backend.system_cursor == 32512);
    // Topmost through SetWindowPos; z order behind another window is refused.
    HWND w = make_window(0);
    CHECK(SetWindowPos(w, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE) && g_backend.topmost);
    CHECK(!SetWindowPos(w, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE));
    // Both Shift keys: every physical key sends its message; VK_SHIFT stays down until both are up.
    SetFocus(w); drain(w); g_events.clear();
    fourstory::platform::input_key(w, VK_LSHIFT, 0, false, true, false);
    fourstory::platform::input_key(w, VK_RSHIFT, 0, false, true, false);
    fourstory::platform::input_key(w, VK_LSHIFT, 0, false, false, false);
    drain(w);
    CHECK((GetKeyState(VK_SHIFT) & 0x8000) && !(GetKeyState(VK_LSHIFT) & 0x8000) && (GetKeyState(VK_RSHIFT) & 0x8000));
    fourstory::platform::input_key(w, VK_RSHIFT, 0, false, false, false);
    drain(w);
    CHECK(!(GetKeyState(VK_SHIFT) & 0x8000));
    CHECK(take(WM_KEYUP).size() == 2);
    DestroyWindow(w); g_events.clear();
    // Profile store: names are compared without case; line breaks are refused.
    CHECK(g_test_app.WriteProfileInt("Settings", "MixedCase", 3) && g_test_app.GetProfileInt("SETTINGS", "mixedcase", 0) == 3);
    CHECK(!g_test_app.WriteProfileString("Settings", "Bad", "two\nlines"));
    CHECK(g_test_app.GetProfileString("Settings", "Bad", "absent") == "absent");
}

static void test_without_backend() {
    fourstory::platform::set_backend(nullptr);
    WNDCLASS wc = {}; wc.lpfnWndProc = RecordProc; wc.lpszClassName = "NoBackend";
    RegisterClass(&wc);
    SetLastError(0);
    CHECK(CreateWindowEx(0, "NoBackend", "x", WS_POPUP, 0, 0, 10, 10, nullptr, nullptr, nullptr, nullptr) == nullptr && GetLastError() == ERROR_NOT_SUPPORTED);
    CHECK(LoadAccelerators(nullptr, MAKEINTRESOURCE(128)) == nullptr);
    CHECK(SetWindowsHookEx(WH_KEYBOARD_LL, nullptr, nullptr, 0) == nullptr);
    CHECK(!GetDeviceGammaRamp(nullptr, nullptr));
    fourstory::platform::set_backend(&g_backend);
}

// C4: cursor files (.cur ICONDIR with a DIB, animated RIFF ACON) and the
// resource table that LoadCursor and FindResource search.
static void put16(std::vector<unsigned char>& v, unsigned x) { v.push_back(x & 255); v.push_back((x >> 8) & 255); }
static void put32(std::vector<unsigned char>& v, unsigned x) { put16(v, x & 0xFFFF); put16(v, x >> 16); }
static std::vector<unsigned char> cursor_file(unsigned hot_x, unsigned hot_y, unsigned char blue, unsigned clr_used) {
    // 2x2, 8 bpp with a 256-entry palette (clr_used may claim more), AND mask: top-left transparent.
    std::vector<unsigned char> dib;
    put32(dib, 40); put32(dib, 2); put32(dib, 4); put16(dib, 1); put16(dib, 8); put32(dib, 0); put32(dib, 0);
    put32(dib, 0); put32(dib, 0); put32(dib, clr_used); put32(dib, 0);
    for (unsigned i = 0; i < 256; ++i) { dib.push_back(i == 1 ? blue : 0); dib.push_back(0); dib.push_back(i == 1 ? 0 : 0); dib.push_back(0); }
    const unsigned char xor_rows[2][4] = {{1, 1, 0, 0}, {0, 1, 0, 0}};  // bottom row first, 4-byte stride
    for (auto& r : xor_rows) dib.insert(dib.end(), r, r + 4);
    const unsigned char and_rows[2][4] = {{0, 0, 0, 0}, {0x80, 0, 0, 0}};  // top row: x=0 transparent
    for (auto& r : and_rows) dib.insert(dib.end(), r, r + 4);
    std::vector<unsigned char> f;
    put16(f, 0); put16(f, 2); put16(f, 1);
    f.push_back(2); f.push_back(2); f.push_back(0); f.push_back(0); put16(f, hot_x); put16(f, hot_y); put32(f, static_cast<unsigned>(dib.size())); put32(f, 22);
    f.insert(f.end(), dib.begin(), dib.end());
    return f;
}
static void test_cursors() {
    using fourstory::platform::CursorImage;
    CursorImage image;
    std::vector<unsigned char> cur = cursor_file(1, 0, 200, 0);
    CHECK(fourstory::platform::decode_cursor(cur.data(), cur.size(), &image));
    CHECK(image.width == 2 && image.height == 2 && image.hot_x == 1 && image.hot_y == 0 && image.frames.size() == 1 && image.durations_ms.empty());
    if (image.frames.size() == 1) {
        const std::vector<unsigned char>& p = image.frames[0];
        CHECK(p[3] == 0);                             // top-left: AND bit, black XOR -> transparent
        CHECK(p[4 + 2] == 200 && p[4 + 3] == 255);    // top-right: palette 1 (blue 200)
        CHECK(p[8 + 2] == 200 && p[8 + 3] == 255);    // bottom-left: palette 1 (bottom file row 1,1)
    }
    // A palette count beyond 2^bpp is clamped: no read past the file.
    std::vector<unsigned char> wide = cursor_file(0, 0, 50, 0x40000001u);
    CHECK(fourstory::platform::decode_cursor(wide.data(), wide.size(), &image) && image.frames.size() == 1);
    CHECK(!fourstory::platform::decode_cursor(cur.data(), 30, &image));  // truncated
    // Animated cursor: two frames, sequence 1,0,1, rate 6 jiffies (100 ms).
    std::vector<unsigned char> f0 = cursor_file(1, 1, 10, 0), f1 = cursor_file(1, 1, 20, 0);
    std::vector<unsigned char> body;
    auto chunk = [&](std::vector<unsigned char>& to, const char* id, const std::vector<unsigned char>& data) {
        to.insert(to.end(), id, id + 4); put32(to, static_cast<unsigned>(data.size())); to.insert(to.end(), data.begin(), data.end()); if (data.size() & 1) to.push_back(0); };
    std::vector<unsigned char> anih; put32(anih, 36); put32(anih, 2); put32(anih, 3); for (int i = 0; i < 4; ++i) put32(anih, 0); put32(anih, 6); put32(anih, 3);
    std::vector<unsigned char> seq; put32(seq, 1); put32(seq, 0); put32(seq, 1);
    std::vector<unsigned char> fram = {'f', 'r', 'a', 'm'};
    chunk(fram, "icon", f0); chunk(fram, "icon", f1);
    chunk(body, "anih", anih); chunk(body, "seq ", seq); chunk(body, "LIST", fram);
    std::vector<unsigned char> ani = {'R', 'I', 'F', 'F'}; put32(ani, static_cast<unsigned>(body.size() + 4));
    ani.insert(ani.end(), {'A', 'C', 'O', 'N'}); ani.insert(ani.end(), body.begin(), body.end());
    CHECK(fourstory::platform::decode_cursor(ani.data(), ani.size(), &image));
    CHECK(image.frames.size() == 3 && image.durations_ms.size() == 3 && image.durations_ms[0] == 100);
    if (image.frames.size() == 3) CHECK(image.frames[0][4 + 2] == 20 && image.frames[1][4 + 2] == 10 && image.frames[2][4 + 2] == 20);
    // Resource table: names and numbers, case-insensitive type names.
    static const unsigned char text[] = {'h', 'i'};
    static const fourstory::resources::Resource table[] = {{"SHADER", 0, nullptr, 7, text, 2}, {nullptr, 6, nullptr, 9, text, 2}};
    fourstory::resources::add(table, 2);
    HRSRC r = FindResource(nullptr, MAKEINTRESOURCE(7), "shader");
    CHECK(r && SizeofResource(nullptr, r) == 2 && LockResource(LoadResource(nullptr, r)) == text);
    CHECK(!FindResource(nullptr, MAKEINTRESOURCE(8), "SHADER") && GetLastError() == 1814);
    char buffer[8];
    CHECK(LoadString(nullptr, 9, buffer, sizeof(buffer)) == 2 && std::strcmp(buffer, "hi") == 0);
    CHECK(LoadString(nullptr, 9, buffer, 2) == 1 && std::strcmp(buffer, "h") == 0);
}

int main() {
    fourstory::platform::set_backend(&g_backend);
    test_create_and_frame();
    test_keys();
    test_mouse();
    test_order_timers_quit();
    test_mfc();
    test_profile();
    test_code_pages_and_gdi();
    test_review_fixes();
    test_without_backend();
    test_cursors();
    std::printf("%s c4-compat-ui-test: %d checks, %d failures\n", g_failures ? "FAIL" : "PASS", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
