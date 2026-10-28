// C4-P criterion 3: the SDL3 shell delivers window messages to an MFC message
// map. SDL events are pushed with SDL_PushEvent (synthetic input) and go
// through the real backend translation, the Win32 message rules of the
// compatibility layer, PeekMessage/TranslateMessage/DispatchMessage and the
// CWnd message map, as in CTachyonApp::MainProc. Run with
// SDL_VIDEO_DRIVER=dummy for a run without a visible window (the offscreen
// driver of this SDL build needs an OpenGL library). No drawing.
#include <afxwin.h>
#include <SDL3/SDL.h>
#include <fourstory_sdl_backend.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

// Shell tests exercise input translation without changing Mac keyboard mode.
extern "C" void fourstory_keyboard_focus(int) {}
extern "C" void fourstory_keyboard_observe(int) {}

static int g_checks = 0, g_failures = 0;
#define CHECK(c) do { ++g_checks; if (!(c)) { ++g_failures; std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

struct Call { std::string name; UINT a; UINT b; UINT c; int x; int y; };
static std::vector<Call> g_calls;
// WaitMessage blocks while no message arrives (Win32); this timer bounds each wait of run_loop.
static const UINT_PTR kLoopTimer = 99;
static HWND g_main = nullptr;
static void record(const char* n, UINT a = 0, UINT b = 0, UINT c = 0, int x = 0, int y = 0) { g_calls.push_back({n, a, b, c, x, y}); }

extern "C" void fourstory_display_focus_resumed(HWND) { record("focus-resumed"); }
extern "C" void fourstory_display_focus_lost(HWND) { record("focus-lost"); }
extern "C" void fourstory_display_mode_changed(HWND, int fullscreen) {
    record("fullscreen", fullscreen);
}

class CShellWnd : public CWnd {
public:
    afx_msg int OnCreate(LPCREATESTRUCT cs) { record("create"); return CWnd::OnCreate(cs); }
    afx_msg void OnKeyDown(UINT ch, UINT rep, UINT flags) { record("keydown", ch, rep, flags); }
    afx_msg void OnKeyUp(UINT ch, UINT rep, UINT flags) { record("keyup", ch, rep, flags); }
    afx_msg void OnSysKeyDown(UINT ch, UINT rep, UINT flags) { record("syskeydown", ch, rep, flags); }
    afx_msg void OnChar(UINT ch, UINT rep, UINT flags) { record("char", ch, rep, flags); }
    afx_msg void OnLButtonDown(UINT flags, CPoint p) { record("lbuttondown", flags, 0, 0, p.x, p.y); }
    afx_msg void OnLButtonUp(UINT flags, CPoint p) { record("lbuttonup", flags, 0, 0, p.x, p.y); }
    afx_msg void OnLButtonDblClk(UINT flags, CPoint p) { record("lbuttondblclk", flags, 0, 0, p.x, p.y); }
    afx_msg void OnRButtonDown(UINT flags, CPoint p) { record("rbuttondown", flags, 0, 0, p.x, p.y); }
    afx_msg void OnMouseMove(UINT flags, CPoint p) { record("mousemove", flags, 0, 0, p.x, p.y); }
    afx_msg BOOL OnMouseWheel(UINT flags, short delta, CPoint p) { record("mousewheel", flags, static_cast<UINT>(static_cast<int>(delta)), 0, p.x, p.y); return TRUE; }
    afx_msg BOOL OnSetCursor(CWnd* w, UINT hit, UINT msg) { record("setcursor", hit, msg, w == this); return TRUE; }
    afx_msg void OnSize(UINT type, int cx, int cy) { record("size", type, 0, 0, cx, cy); }
    afx_msg void OnActivate(UINT state, CWnd*, BOOL minimized) { record("activate", state, minimized); }
    afx_msg void OnTimer(UINT_PTR id) { if (id != kLoopTimer) record("timer", static_cast<UINT>(id)); }
    afx_msg void OnDestroy() { record("destroy"); CWnd::OnDestroy(); }
    DECLARE_MESSAGE_MAP()
};
BEGIN_MESSAGE_MAP(CShellWnd, CWnd)
    ON_WM_CREATE()
    ON_WM_KEYDOWN()
    ON_WM_KEYUP()
    ON_WM_SYSKEYDOWN()
    ON_WM_CHAR()
    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONUP()
    ON_WM_LBUTTONDBLCLK()
    ON_WM_RBUTTONDOWN()
    ON_WM_MOUSEMOVE()
    ON_WM_MOUSEWHEEL()
    ON_WM_SETCURSOR()
    ON_WM_SIZE()
    ON_WM_ACTIVATE()
    ON_WM_TIMER()
    ON_WM_DESTROY()
END_MESSAGE_MAP()

class CShellApp : public CWinApp { public: CShellApp() : CWinApp("ShellTest") {} };
static CShellApp g_app;

static SDL_WindowID g_id = 0;
static void push(SDL_Event e) { e.common.timestamp = SDL_GetTicksNS(); CHECK(SDL_PushEvent(&e)); }
static void key(SDL_Scancode sc, SDL_Keycode k, bool down, bool repeat = false) {
    SDL_Event e; SDL_zero(e);
    e.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    e.key.windowID = g_id; e.key.scancode = sc; e.key.key = k; e.key.down = down; e.key.repeat = repeat;
    push(e);
}
static void text(const char* t) { SDL_Event e; SDL_zero(e); e.type = SDL_EVENT_TEXT_INPUT; e.text.windowID = g_id; e.text.text = t; push(e); }
static void button(Uint8 b, bool down, float x, float y) {
    SDL_Event e; SDL_zero(e);
    e.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
    e.button.windowID = g_id; e.button.button = b; e.button.down = down; e.button.clicks = 1; e.button.x = x; e.button.y = y;
    push(e);
}
static void motion(float x, float y) { SDL_Event e; SDL_zero(e); e.type = SDL_EVENT_MOUSE_MOTION; e.motion.windowID = g_id; e.motion.x = x; e.motion.y = y; push(e); }
static void wheel(float dy, float x, float y) {
    SDL_Event e; SDL_zero(e); e.type = SDL_EVENT_MOUSE_WHEEL; e.wheel.windowID = g_id; e.wheel.y = dy; e.wheel.direction = SDL_MOUSEWHEEL_NORMAL;
    e.wheel.mouse_x = x; e.wheel.mouse_y = y; push(e);
}
static void window_event(SDL_EventType t, int d1 = 0, int d2 = 0) { SDL_Event e; SDL_zero(e); e.type = t; e.window.windowID = g_id; e.window.data1 = d1; e.window.data2 = d2; push(e); }
// The CTachyonApp::MainProc loop: PeekMessage, then TranslateMessage and DispatchMessage.
static bool run_loop(int milliseconds) {
    const Uint64 end = SDL_GetTicks() + static_cast<Uint64>(milliseconds);
    MSG msg;
    if (IsWindow(g_main)) SetTimer(g_main, kLoopTimer, 10, nullptr);
    bool quit = false;
    while (!quit && SDL_GetTicks() < end) {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) { quit = true; break; }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        if (!quit) {
            if (IsWindow(g_main)) WaitMessage();
            else SDL_Delay(5);
        }
    }
    if (IsWindow(g_main)) KillTimer(g_main, kLoopTimer);
    return quit;
}
static std::vector<Call> take(const char* name) {
    std::vector<Call> out;
    for (const Call& c : g_calls) if (c.name == name) out.push_back(c);
    return out;
}

int main() {
    setvbuf(stdout, nullptr, _IONBF, 0);
    fourstory::shell::SdlOptions options;
    options.virtual_game_size = std::getenv("FOURSTORY_TEST_VIRTUAL_SIZE") != nullptr;
    if (!fourstory::shell::install_sdl_backend(options)) { std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError()); return 2; }
    std::printf("SDL video driver: %s\n", SDL_GetCurrentVideoDriver());
    CHECK(!fourstory::shell::set_game_fullscreen(nullptr, true));
    CHECK(!fourstory::shell::game_fullscreen(nullptr));
    if (options.virtual_game_size)
        CHECK(!SDL_GetHintBoolean(SDL_HINT_MOUSE_EMULATE_WARP_WITH_RELATIVE, true));
    CShellWnd* wnd = new CShellWnd;
    RECT r = {0, 0, 640, 360};
    AdjustWindowRect(&r, WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN, FALSE);
    const BOOL created = wnd->CreateEx(0, AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS), "Shell", WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN, r, nullptr, 0);
    CHECK(created);
    if (!created) { std::fprintf(stderr, "window creation failed: %s\n", SDL_GetError()); return 1; }
    g_app.m_pMainWnd = wnd;
    g_main = wnd->m_hWnd;
    SDL_Window* native = static_cast<SDL_Window*>(fourstory::platform::native_window(wnd->m_hWnd));
    CHECK(native != nullptr);
    g_id = SDL_GetWindowID(native);
    window_event(SDL_EVENT_WINDOW_ENTER_FULLSCREEN);
    window_event(SDL_EVENT_WINDOW_LEAVE_FULLSCREEN);
    run_loop(30);
    auto modes=take("fullscreen");
    CHECK(modes.size()==2 && modes[0].a==1 && modes[1].a==0);
    int w = 0, h = 0;
    SDL_GetWindowSize(native, &w, &h);
    CHECK(w == 640 && h == 360);
    CHECK(!take("create").empty());
    wnd->SetFocus();
    window_event(SDL_EVENT_WINDOW_FOCUS_GAINED);
    run_loop(50);
    CHECK(::GetForegroundWindow() == wnd->m_hWnd);
    g_calls.clear();

    // Cmd-Tab may leave downs in SDL's batch before focus loss is dispatched.
    // They must not survive the reset or let background clicks move the camera.
    using namespace fourstory::platform;
    input_key(g_main,VK_LWIN,0,false,true,false);
    run_loop(20);
    CHECK(GetKeyState(VK_LWIN) & 0x8000);
    g_calls.clear();
    input_key(g_main,VK_TAB,0,false,true,false);
    input_mouse_button(g_main,1,true,40,50);
    input_mouse_move(g_main,45,55);
    input_focus(g_main,false);
    CHECK(!(GetKeyState(VK_LWIN) & 0x8000));
    CHECK(!(GetAsyncKeyState(VK_TAB) & 0x8000));
    CHECK(!SetCursorPos(200,200));
    input_mouse_button(g_main,1,true,60,70);
    input_mouse_move(g_main,65,75);
    input_mouse_wheel(g_main,1,0,65,75);
    input_key(g_main,'W',0,false,true,false);
    run_loop(20);
    CHECK(take("rbuttondown").empty());
    CHECK(take("mousemove").empty());
    CHECK(take("mousewheel").empty());
    CHECK(take("keydown").empty());
    input_focus(g_main,true);
    input_mouse_move(g_main,70,80);
    run_loop(20);
    CHECK(take("mousemove").size()==1);
    g_calls.clear();

    if(options.virtual_game_size) {
        CHECK(SDL_GetWindowFlags(native) & SDL_WINDOW_RESIZABLE);
        CHECK(SDL_SetWindowSize(native,1280,720)); CHECK(SDL_SyncWindow(native)); run_loop(60);
        RECT client; GetClientRect(g_main,&client); CHECK(client.right==640 && client.bottom==360);
        g_calls.clear();
        SetCapture(g_main);
        motion(0,0); run_loop(30); motion(1278,718); run_loop(30); motion(-20,740); run_loop(30);
        button(SDL_BUTTON_RIGHT,true,1278,718); button(SDL_BUTTON_RIGHT,false,1278,718);
        wheel(1,640,360); run_loop(60);
        auto points=take("mousemove");
        for(auto point:points)std::printf("mapped mouse %d,%d\n",point.x,point.y);
        CHECK(points.size()==3 && points[0].x==0 && points[0].y==0);
        CHECK(points.size()==3 && points[1].x==639 && points[1].y==359);
        CHECK(points.size()==3 && points[2].x==-10 && points[2].y==370);
        auto right=take("rbuttondown"); CHECK(right.size()==1 && right[0].x==639 && right[0].y==359);
        auto wheels=take("mousewheel"); POINT origin={0,0}; ClientToScreen(g_main,&origin);
        CHECK(wheels.size()==1 && wheels[0].x==origin.x+320 && wheels[0].y==origin.y+180);
        ReleaseCapture();
        auto* backend=fourstory::platform::backend();
        backend->set_client_rect(native,100,100,640,360); CHECK(SDL_SyncWindow(native));
        SDL_GetWindowSize(native,&w,&h); CHECK(w==1280 && h==720); // move-only preserves native extent
        backend->set_client_rect(native,100,100,800,450); CHECK(SDL_SyncWindow(native)); run_loop(60);
        SDL_GetWindowSize(native,&w,&h); CHECK(w==800 && h==450);
        GetClientRect(g_main,&client); CHECK(client.right==800 && client.bottom==450);
        // Simulate MoveWindow publishing a requested origin when Cocoa does not
        // physically move a fullscreen window. A same-mode request must repair
        // the virtual origin even though no native MOVED event is emitted.
        fourstory::platform::input_moved(g_main,7,28);
        CHECK(fourstory::shell::set_game_fullscreen(g_main,false));
        int actualX=0,actualY=0; CHECK(SDL_GetWindowPosition(native,&actualX,&actualY));
        POINT repaired={0,0}; ClientToScreen(g_main,&repaired);
        CHECK(repaired.x==actualX && repaired.y==actualY);
        // Cmd-Tab/focus recovery must repair the same invariant without a
        // fullscreen toggle, backbuffer resize or forced pointer warp.
        key(SDL_SCANCODE_W, SDLK_W, true);
        button(SDL_BUTTON_RIGHT,true,400,225); run_loop(20);
        SetCapture(g_main);
        window_event(SDL_EVENT_WINDOW_FOCUS_LOST); run_loop(20);
        CHECK(GetCapture()==nullptr);
        CHECK(!(GetAsyncKeyState('W') & 0x8000));
        CHECK(!(GetAsyncKeyState(VK_RBUTTON) & 0x8000));
        fourstory::platform::input_moved(g_main,93,57);
        window_event(SDL_EVENT_WINDOW_FOCUS_GAINED); run_loop(30);
        repaired={0,0}; ClientToScreen(g_main,&repaired);
        CHECK(SDL_GetWindowPosition(native,&actualX,&actualY));
        CHECK(repaired.x==actualX && repaired.y==actualY);
        GetClientRect(g_main,&client); CHECK(client.right==800 && client.bottom==450);
        CHECK(!take("focus-resumed").empty());
        CHECK(!take("focus-lost").empty());
        if(std::string(SDL_GetCurrentVideoDriver()) != "dummy") {
            SDL_RaiseWindow(native); run_loop(80);
            CHECK(SDL_GetKeyboardFocus()==native);
            CHECK(backend->warp_mouse(native,400,225)); run_loop(30);
            int gx=0,gy=0; CHECK(backend->cursor_position(&gx,&gy));
            POINT top={0,0}; ClientToScreen(g_main,&top);
            CHECK(std::abs(gx-top.x-400)<=1 && std::abs(gy-top.y-225)<=1);
            CHECK(fourstory::shell::set_game_fullscreen(g_main,true)); CHECK(SDL_SyncWindow(native)); run_loop(100);
            SDL_GetWindowSize(native,&w,&h); g_calls.clear();
            motion(w*.5f,h*.5f);run_loop(30);
            auto full=take("mousemove"); for(auto point:full)std::printf("fullscreen mouse %d,%d\n",point.x,point.y); CHECK(!full.empty() && full.back().x==400 && full.back().y==225);
            CHECK(backend->warp_mouse(native,400,225));run_loop(30);
            CHECK(backend->cursor_position(&gx,&gy)); top={0,0};ClientToScreen(g_main,&top);
            CHECK(std::abs(gx-top.x-400)<=1 && std::abs(gy-top.y-225)<=1);
            // The legacy camera hides and repeatedly recenters the pointer.
            // SDL must retain absolute mouse mode and matching query/warp space.
            backend->show_cursor(false);
            for (int i=0; i<12; ++i) {
                CHECK(backend->warp_mouse(native,400,225)); run_loop(10);
            }
            CHECK(!SDL_GetWindowRelativeMouseMode(native));
            CHECK(backend->cursor_position(&gx,&gy)); top={0,0};ClientToScreen(g_main,&top);
            CHECK(std::abs(gx-top.x-400)<=1 && std::abs(gy-top.y-225)<=1);
            backend->show_cursor(true);
            // Test all corners after the mode transition, not just the center.
            g_calls.clear();
            motion(0,0); run_loop(20); motion(w-2.f,h-2.f); run_loop(20);
            auto corners=take("mousemove");
            CHECK(corners.size()>=2 && corners.front().x==0 && corners.front().y==0);
            CHECK(corners.back().x>=797 && corners.back().x<800 && corners.back().y>=447 && corners.back().y<450);
            fourstory::platform::input_moved(g_main,93,57);
            window_event(SDL_EVENT_WINDOW_FOCUS_LOST); window_event(SDL_EVENT_WINDOW_FOCUS_GAINED);run_loop(30);
            top={0,0};ClientToScreen(g_main,&top);
            CHECK(SDL_GetWindowPosition(native,&actualX,&actualY));
            CHECK(top.x==actualX && top.y==actualY);
            CHECK(backend->warp_mouse(native,400,225));run_loop(30);
            CHECK(backend->cursor_position(&gx,&gy));
            CHECK(std::abs(gx-top.x-400)<=1 && std::abs(gy-top.y-225)<=1);
            CHECK(fourstory::shell::game_fullscreen(g_main));
            CHECK(fourstory::shell::set_game_fullscreen(g_main,false));CHECK(SDL_SyncWindow(native));run_loop(100);
            CHECK(!fourstory::shell::game_fullscreen(g_main));
            GetClientRect(g_main,&client);CHECK(client.right==800 && client.bottom==450);
        }
        window_event(SDL_EVENT_WINDOW_CLOSE_REQUESTED); CHECK(run_loop(500)); delete wnd;
        fourstory::shell::uninstall_sdl_backend();
        std::printf("%s virtual-size: %d checks, %d failures\n",g_failures?"FAIL":"PASS",g_checks,g_failures);
        return g_failures?1:0;
    }

    // Keys: VK code, repeat count, scan code and the key-up flags; text as WM_CHAR in code page 1252.
    key(SDL_SCANCODE_A, SDLK_A, true);
    text("a");
    key(SDL_SCANCODE_A, SDLK_A, false);
    key(SDL_SCANCODE_RETURN, SDLK_RETURN, true);
    key(SDL_SCANCODE_RETURN, SDLK_RETURN, false);
    text("\xC3\xBC");   // u umlaut
    key(SDL_SCANCODE_LEFT, SDLK_LEFT, true);
    key(SDL_SCANCODE_LEFT, SDLK_LEFT, true, true);
    key(SDL_SCANCODE_LEFT, SDLK_LEFT, false);
    key(SDL_SCANCODE_LALT, SDLK_LALT, true);
    key(SDL_SCANCODE_1, SDLK_1, true);
    key(SDL_SCANCODE_1, SDLK_1, false);
    key(SDL_SCANCODE_LALT, SDLK_LALT, false);
    run_loop(100);
    auto downs = take("keydown");
    CHECK(downs.size() == 4);
    CHECK(downs.size() == 4 && downs[0].a == 'A' && downs[0].b == 1 && (downs[0].c & 0xFF) == 0x1E);
    CHECK(downs.size() == 4 && downs[1].a == VK_RETURN);
    CHECK(downs.size() == 4 && downs[2].a == VK_LEFT && (downs[2].c & KF_EXTENDED) && !(downs[2].c & KF_REPEAT) && (downs[3].c & KF_REPEAT));
    auto ups = take("keyup");
    CHECK(ups.size() >= 3 && (ups[0].c & KF_UP) && ups[0].a == 'A');
    auto sys = take("syskeydown");
    CHECK(sys.size() == 2 && sys[0].a == VK_MENU && sys[1].a == '1' && (sys[1].c & KF_ALTDOWN));
    auto chars = take("char");
    CHECK(chars.size() == 3 && chars[0].a == 'a' && chars[1].a == 0x0D && chars[2].a == 0xFC);
    g_calls.clear();

    // Mouse: WM_SETCURSOR before each mouse message, client coordinates, double click (CS_DBLCLKS).
    motion(100, 50);
    button(SDL_BUTTON_LEFT, true, 100, 50);
    button(SDL_BUTTON_LEFT, false, 100, 50);
    button(SDL_BUTTON_LEFT, true, 101, 51);
    button(SDL_BUTTON_LEFT, false, 101, 51);
    button(SDL_BUTTON_RIGHT, true, 7, 8);
    wheel(1.0f, 10, 10);
    run_loop(100);
    auto moves = take("mousemove");
    CHECK(moves.size() == 1 && moves[0].x == 100 && moves[0].y == 50);
    CHECK(take("lbuttondown").size() == 1);
    auto dbl = take("lbuttondblclk");
    CHECK(dbl.size() == 1 && dbl[0].x == 101 && dbl[0].y == 51 && (dbl[0].a & MK_LBUTTON));
    auto right = take("rbuttondown");
    CHECK(right.size() == 1 && right[0].x == 7 && right[0].y == 8 && (right[0].a & MK_RBUTTON));
    auto cursor = take("setcursor");
    CHECK(cursor.size() == 6 && cursor[0].a == HTCLIENT && cursor[0].b == WM_MOUSEMOVE && cursor[0].c == 1);
    auto wheels = take("mousewheel");
    CHECK(wheels.size() == 1 && static_cast<int>(wheels[0].b) == WHEEL_DELTA);
    g_calls.clear();

    // Window events: resize, focus loss and gain (WM_ACTIVATE), timers.
    window_event(SDL_EVENT_WINDOW_RESIZED, 800, 450);
    window_event(SDL_EVENT_WINDOW_FOCUS_LOST);
    window_event(SDL_EVENT_WINDOW_FOCUS_GAINED);
    wnd->SetTimer(7, 20, nullptr);
    run_loop(120);
    wnd->KillTimer(7);
    auto sizes = take("size");
    CHECK(sizes.size() == 1 && sizes[0].x == 800 && sizes[0].y == 450 && sizes[0].a == SIZE_RESTORED);
    RECT client; wnd->GetClientRect(&client);
    CHECK(client.right == 800 && client.bottom == 450);
    auto act = take("activate");
    CHECK(act.size() == 2 && act[0].a == WA_INACTIVE && act[1].a == WA_ACTIVE);
    CHECK(take("timer").size() >= 2);
    g_calls.clear();

    // Real SDL frame changes must follow the Win32 style, including a request
    // made while fullscreen (the backend defers that request until exit).

    CHECK(!SDL_GetHintBoolean(SDL_HINT_MAC_PRESS_AND_HOLD, true));
    for (int i = 0; i < 3; ++i) {
        SetWindowLongPtr(wnd->m_hWnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        CHECK(SDL_GetWindowFlags(native) & SDL_WINDOW_BORDERLESS);
        if (std::getenv("FOURSTORY_TEST_FULLSCREEN")) {
            CHECK(SDL_SetWindowFullscreen(native, true));
            CHECK(SDL_SyncWindow(native));
            CHECK(SDL_GetWindowFlags(native) & SDL_WINDOW_FULLSCREEN);
        }
        SetWindowLongPtr(wnd->m_hWnd, GWL_STYLE, WS_OVERLAPPED | WS_CAPTION | WS_VISIBLE);
        if (std::getenv("FOURSTORY_TEST_FULLSCREEN")) {
            CHECK(SDL_SetWindowFullscreen(native, false));
            CHECK(SDL_SyncWindow(native));
        }
        run_loop(100);
        // The dummy driver has no native set-border operation.
        if (std::string(SDL_GetCurrentVideoDriver()) != "dummy") {
            CHECK(!(SDL_GetWindowFlags(native) & SDL_WINDOW_BORDERLESS));
            RECT requested = {0, 0, 800, 450};
            AdjustWindowRect(&requested, DWORD(GetWindowLongPtr(wnd->m_hWnd, GWL_STYLE)), FALSE);
            CHECK(MoveWindow(wnd->m_hWnd, 100, 100, requested.right - requested.left,
                             requested.bottom - requested.top, TRUE));
            CHECK(SDL_SyncWindow(native));
            run_loop(100);
            int x, y, width, height;
            CHECK(SDL_GetWindowPosition(native, &x, &y));
            CHECK(SDL_GetWindowSize(native, &width, &height));
            POINT origin = {0, 0}; ClientToScreen(wnd->m_hWnd, &origin);
            RECT client; GetClientRect(wnd->m_hWnd, &client);
            CHECK(origin.x == x && origin.y == y);
            CHECK(client.right == width && client.bottom == height && width == 800 && height == 450);
            motion(100, 50); run_loop(50);
            const auto after = take("mousemove");
            CHECK(!after.empty() && after.back().x == 100 && after.back().y == 50);
        }
    }

    // Close button: WM_SYSCOMMAND SC_CLOSE, WM_CLOSE, DestroyWindow, WM_DESTROY, then WM_QUIT
    // from CWnd::OnNcDestroy of the main window.
    window_event(SDL_EVENT_WINDOW_CLOSE_REQUESTED);
    const bool quit = run_loop(500);
    CHECK(quit);
    CHECK(take("destroy").size() == 1 && wnd->m_hWnd == nullptr && AfxGetMainWnd() == nullptr);
    delete wnd;
    fourstory::shell::uninstall_sdl_backend();
    std::printf("%s c4-shell-test: %d checks, %d failures\n", g_failures ? "FAIL" : "PASS", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
