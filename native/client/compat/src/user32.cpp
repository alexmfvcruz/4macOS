// Native Win32 window and message subset (C4-P). Declarations and the scope
// are in include/fourstory_user32.h; differences from Windows are listed in
// README.md, section "Window system (C4-P)".
#include <windows.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace {
using fourstory::platform::Backend;

struct Window : HWND__ {
    std::string class_name, text;
    DWORD style = 0, ex_style = 0;
    HWND parent = nullptr;          // parent window (WS_CHILD) or owner
    std::vector<HWND> children;     // in creation order
    RECT rect = {};                 // window rectangle: screen (top level) or parent client coordinates (child)
    RECT client = {};               // client rectangle relative to the window rectangle origin
    WNDPROC procedure = nullptr;
    LONG_PTR user_data = 0;
    HMENU id = nullptr;
    HINSTANCE instance = nullptr;
    HICON icon = nullptr;
    UINT class_style = 0;
    HCURSOR class_cursor = nullptr;
    bool visible = false, enabled = true, minimized = false, maximized = false;
    bool destroying = false, needs_paint = false;
    std::map<std::string, HANDLE> props;
    void* native = nullptr;         // backend window of a top-level window
};

struct Timer { HWND window; UINT_PTR id; UINT elapse; TIMERPROC procedure; std::chrono::steady_clock::time_point due; };
// popped: the GUI thread took the entry and runs it; the sender must wait for done.
struct SentMessage { MSG msg; LRESULT result = 0; bool done = false, popped = false; };

struct State {
    std::recursive_mutex lock;          // window tree, queues, timers
    std::condition_variable_any posted; // a message arrived (threads other than the GUI thread use wake())
    std::thread::id gui_thread;
    bool gui_thread_set = false;
    std::set<Window*> windows;
    std::map<std::string, WNDCLASS> classes;
    std::deque<MSG> posted_queue, input_queue;
    std::deque<SentMessage*> sent_queue;
    std::vector<Timer> timers;
    UINT_PTR next_timer_id = 1;
    bool quit = false; int quit_code = 0;
    HWND focus = nullptr, active = nullptr, capture = nullptr;
    Backend* backend = nullptr;
    bool input_suspended = false;
    fourstory::platform::CreateHook create_hook = nullptr;
    BYTE key_sync[256] = {}, key_async[256] = {};
    LONG message_time = 0; DWORD message_pos = 0;
    POINT cursor = {};                  // last known cursor position, screen coordinates
    int cursor_count = 0;               // ShowCursor display counter
    HCURSOR cursor_current = nullptr;
    LPARAM last_key_lparam = 0;
    // Double clicks: the last button press.
    UINT last_click_message = 0; HWND last_click_window = nullptr; DWORD last_click_time = 0; POINT last_click_point = {};
    float wheel_rest_y = 0, wheel_rest_x = 0;
    std::map<std::string, UINT> registered_messages; UINT next_registered = 0xC000;
};
State& state() { static State s; return s; }

Window* get(HWND window) {
    State& s = state();
    Window* w = static_cast<Window*>(window);
    return window && s.windows.count(w) ? w : nullptr;
}
bool on_gui_thread() {
    State& s = state();
    return !s.gui_thread_set || s.gui_thread == std::this_thread::get_id();
}
HWND top_level(HWND window) {
    Window* w = get(window);
    while (w && (w->style & WS_CHILD) && get(w->parent)) w = get(w->parent);
    return w;
}
// Screen position of the client origin of a window.
POINT client_origin(HWND window) {
    POINT p = {0, 0};
    for (Window* w = get(window); w; ) {
        p.x += w->rect.left + w->client.left; p.y += w->rect.top + w->client.top;
        if (!(w->style & WS_CHILD)) break;
        w = get(w->parent);
    }
    return p;
}
void wake_gui() {
    State& s = state();
    s.posted.notify_all();
    if (s.backend) s.backend->wake();
}
// Frame model shared by AdjustWindowRect and CreateWindowEx, so that a
// rectangle computed by one gives the requested client size in the other.
void frame_insets(DWORD style, RECT* insets) {
    insets->left = insets->top = insets->right = insets->bottom = 0;
    if ((style & WS_CHILD) || (style & WS_CAPTION) != WS_CAPTION) return;
    State& s = state();
    insets->top = s.backend ? s.backend->frame_caption_height() : 0;
}
LPARAM point_lparam(int x, int y) { return MAKELPARAM(static_cast<WORD>(static_cast<SHORT>(x)), static_cast<WORD>(static_cast<SHORT>(y))); }
DWORD now_ticks() { return GetTickCount(); }

void make_msg(MSG* m, HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    State& s = state();
    m->hwnd = window; m->message = message; m->wParam = wparam; m->lParam = lparam;
    m->time = now_ticks(); m->pt = s.cursor;
}
void post_input(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    State& s = state();
    std::lock_guard<std::recursive_mutex> guard(s.lock);
    // Windows keeps only the newest mouse move: a move replaces a move that is
    // still the last queued input message for the same window.
    if (message == WM_MOUSEMOVE && !s.input_queue.empty()) {
        MSG& last = s.input_queue.back();
        if (last.message == WM_MOUSEMOVE && last.hwnd == window) { make_msg(&last, window, message, wparam, lparam); return; }
    }
    MSG m; make_msg(&m, window, message, wparam, lparam);
    s.input_queue.push_back(m);
}
bool matches(const MSG& m, HWND window, UINT low, UINT high) {
    if (window && m.hwnd != window && !IsChild(window, m.hwnd)) return false;
    if (low == 0 && high == 0) return true;
    return m.message >= low && m.message <= high;
}
void update_sync_keys(const MSG& m) {
    State& s = state();
    auto set = [&](int vk, bool down) {
        if (down) { if (!(s.key_sync[vk] & 0x80)) s.key_sync[vk] ^= 0x01; s.key_sync[vk] |= 0x80; }
        else s.key_sync[vk] &= static_cast<BYTE>(~0x80);
    };
    switch (m.message) {
    case WM_KEYDOWN: case WM_SYSKEYDOWN: case WM_KEYUP: case WM_SYSKEYUP: {
        const bool down = m.message == WM_KEYDOWN || m.message == WM_SYSKEYDOWN;
        const int vk = static_cast<int>(m.wParam & 0xFF);
        const UINT scan = (static_cast<UINT>(m.lParam) >> 16) & 0xFF; const bool ext = (m.lParam >> 24) & 1;
        // The generic Shift, Ctrl and Alt keys are down while either side is down.
        if (vk == VK_SHIFT || vk == VK_CONTROL || vk == VK_MENU) {
            const int side = vk == VK_SHIFT ? (scan == 0x36 ? VK_RSHIFT : VK_LSHIFT) : vk == VK_CONTROL ? (ext ? VK_RCONTROL : VK_LCONTROL) : (ext ? VK_RMENU : VK_LMENU);
            const int other = side == VK_LSHIFT ? VK_RSHIFT : side == VK_RSHIFT ? VK_LSHIFT : side == VK_LCONTROL ? VK_RCONTROL : side == VK_RCONTROL ? VK_LCONTROL : side == VK_LMENU ? VK_RMENU : VK_LMENU;
            set(side, down);
            set(vk, down || (s.key_sync[other] & 0x80));
        } else set(vk, down);
        break; }
    case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK: set(VK_LBUTTON, true); break;
    case WM_LBUTTONUP: set(VK_LBUTTON, false); break;
    case WM_RBUTTONDOWN: case WM_RBUTTONDBLCLK: set(VK_RBUTTON, true); break;
    case WM_RBUTTONUP: set(VK_RBUTTON, false); break;
    case WM_MBUTTONDOWN: case WM_MBUTTONDBLCLK: set(VK_MBUTTON, true); break;
    case WM_MBUTTONUP: set(VK_MBUTTON, false); break;
    }
}
bool is_mouse(UINT message) { return message >= WM_MOUSEFIRST && message <= WM_MOUSELAST && message != WM_MOUSEWHEEL && message != WM_MOUSEHWHEEL; }

void process_sent() {
    State& s = state();
    for (;;) {
        SentMessage* sent = nullptr;
        { std::lock_guard<std::recursive_mutex> guard(s.lock);
          if (s.sent_queue.empty()) return;
          sent = s.sent_queue.front(); s.sent_queue.pop_front(); sent->popped = true; }
        LRESULT result = 0;
        if (Window* w = get(sent->msg.hwnd)) if (w->procedure) result = w->procedure(sent->msg.hwnd, sent->msg.message, sent->msg.wParam, sent->msg.lParam);
        { std::lock_guard<std::recursive_mutex> guard(s.lock); sent->result = result; sent->done = true; }
        s.posted.notify_all();
    }
}

// Message retrieval in the Windows order: sent, posted, quit, input, paint,
// timer. Returns true and fills *out when a message matches.
bool retrieve(MSG* out, HWND window, UINT low, UINT high, bool remove) {
    State& s = state();
    process_sent();
    std::unique_lock<std::recursive_mutex> guard(s.lock);
    for (auto it = s.posted_queue.begin(); it != s.posted_queue.end(); ++it)
        if (matches(*it, window, low, high)) { *out = *it; if (remove) s.posted_queue.erase(it); return true; }
    if (s.quit && ((low == 0 && high == 0) || (WM_QUIT >= low && WM_QUIT <= high))) {
        make_msg(out, nullptr, WM_QUIT, static_cast<WPARAM>(s.quit_code), 0);
        if (remove) s.quit = false;
        return true;
    }
    for (auto it = s.input_queue.begin(); it != s.input_queue.end(); ++it) {
        if (!matches(*it, window, low, high)) continue;
        MSG m = *it;
        if (remove) s.input_queue.erase(it);
        if (remove) {
            update_sync_keys(m);
            // WM_SETCURSOR precedes a mouse message for a window that does not hold the capture.
            if (is_mouse(m.message) && s.capture != m.hwnd && get(m.hwnd)) {
                guard.unlock();
                SendMessage(m.hwnd, WM_SETCURSOR, reinterpret_cast<WPARAM>(m.hwnd), MAKELPARAM(HTCLIENT, m.message));
                guard.lock();
            }
        }
        *out = m; return true;
    }
    for (Window* w : s.windows)
        if (w->needs_paint && w->visible && matches(MSG{w, WM_PAINT, 0, 0, 0, {}}, window, low, high)) {
            make_msg(out, w, WM_PAINT, 0, 0); return true;
        }
    const auto now = std::chrono::steady_clock::now();
    for (Timer& t : s.timers) {
        if (t.due > now) continue;
        MSG m; make_msg(&m, t.window, WM_TIMER, t.id, reinterpret_cast<LPARAM>(t.procedure));
        if (!matches(m, window, low, high)) continue;
        if (remove) t.due = now + std::chrono::milliseconds(t.elapse);
        *out = m; return true;
    }
    return false;
}
DWORD next_timer_wait() {
    State& s = state();
    std::lock_guard<std::recursive_mutex> guard(s.lock);
    const auto now = std::chrono::steady_clock::now();
    long long wait = 100;
    for (const Timer& t : s.timers) {
        const long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(t.due - now).count();
        wait = std::min(wait, std::max(0LL, ms));
    }
    return static_cast<DWORD>(wait);
}
void pump_backend() { State& s = state(); if (s.backend) s.backend->pump(); }

// Windows order: WM_DESTROY to the window, then to its children (parents
// first); WM_NCDESTROY to the children, then to the window. A window that
// failed WM_NCCREATE gets only WM_NCDESTROY.
void send_destroy(Window* w) {
    SendMessage(w, WM_DESTROY, 0, 0);
    for (HWND child : std::vector<HWND>(w->children)) if (Window* c = get(child)) send_destroy(c);
}
void destroy_tree(Window* w, bool send_wm_destroy = true) {
    State& s = state();
    // The focus leaves a window that is destroyed (WM_KILLFOCUS).
    for (HWND f = s.focus; f; f = (get(f) && (static_cast<Window*>(f)->style & WS_CHILD)) ? static_cast<Window*>(f)->parent : nullptr)
        if (f == w) { SetFocus(nullptr); break; }
    if (send_wm_destroy) send_destroy(w);
    for (HWND child : std::vector<HWND>(w->children)) if (Window* c = get(child)) destroy_tree(c, false);
    SendMessage(w, WM_NCDESTROY, 0, 0);
    std::lock_guard<std::recursive_mutex> guard(s.lock);
    if (Window* p = get(w->parent)) p->children.erase(std::remove(p->children.begin(), p->children.end(), static_cast<HWND>(w)), p->children.end());
    if (s.focus == w) s.focus = nullptr;
    if (s.active == w) s.active = nullptr;
    if (s.capture == w) { s.capture = nullptr; if (s.backend) s.backend->capture_mouse(false); }
    s.timers.erase(std::remove_if(s.timers.begin(), s.timers.end(), [&](const Timer& t) { return t.window == w; }), s.timers.end());
    s.posted_queue.erase(std::remove_if(s.posted_queue.begin(), s.posted_queue.end(), [&](const MSG& m) { return m.hwnd == w; }), s.posted_queue.end());
    s.input_queue.erase(std::remove_if(s.input_queue.begin(), s.input_queue.end(), [&](const MSG& m) { return m.hwnd == w; }), s.input_queue.end());
    if (w->native && s.backend) s.backend->destroy_window(w->native);
    s.windows.erase(w);
    delete w;
}

// The deepest visible, enabled child under a point given in client coordinates of window.
HWND hit_child(HWND window, POINT* point) {
    Window* w = get(window);
    while (w) {
        Window* found = nullptr;
        for (auto it = w->children.rbegin(); it != w->children.rend(); ++it) {
            Window* c = get(*it);
            if (!c || !c->visible || !c->enabled) continue;
            if (point->x >= c->rect.left && point->x < c->rect.right && point->y >= c->rect.top && point->y < c->rect.bottom) { found = c; break; }
        }
        if (!found) return w;
        point->x -= found->rect.left + found->client.left; point->y -= found->rect.top + found->client.top;
        w = found;
    }
    return nullptr;
}
WPARAM mouse_key_flags() {
    State& s = state();
    WPARAM f = 0;
    if (s.key_async[VK_LBUTTON] & 0x80) f |= MK_LBUTTON;
    if (s.key_async[VK_RBUTTON] & 0x80) f |= MK_RBUTTON;
    if (s.key_async[VK_MBUTTON] & 0x80) f |= MK_MBUTTON;
    if (s.key_async[VK_SHIFT] & 0x80) f |= MK_SHIFT;
    if (s.key_async[VK_CONTROL] & 0x80) f |= MK_CONTROL;
    return f;
}
// Target window and its client coordinates for a mouse event at top-level client (x, y).
HWND mouse_target(HWND top, int x, int y, POINT* client) {
    State& s = state();
    POINT origin = client_origin(top);
    POINT screen = {origin.x + x, origin.y + y};
    s.cursor = screen;
    if (Window* c = get(s.capture)) {
        POINT o = client_origin(c);
        client->x = screen.x - o.x; client->y = screen.y - o.y;
        return c;
    }
    *client = {x, y};
    return hit_child(top, client);
}

void set_async(int vk, bool down) {
    State& s = state();
    if (down) { if (!(s.key_async[vk] & 0x80)) s.key_async[vk] ^= 0x01; s.key_async[vk] |= 0x80; }
    else s.key_async[vk] &= static_cast<BYTE>(~0x80);
}

// Code page 1252 bytes 0x80-0x9F (other bytes are Latin-1). 0 means undefined.
const char16_t cp1252_high[32] = {
    0x20AC, 0, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0, 0x017D, 0,
    0, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0, 0x017E, 0x0178 };
UINT active_code_page() {
    static const UINT page = [] {
        const char* v = std::getenv("FOURSTORY_ACP");
        const UINT p = v ? static_cast<UINT>(std::atoi(v)) : 1252;
        return (p == 1250 || p == 1251 || p == 1252 || p == 1253 || p == 1254) ? p : 1252;
    }();
    return page;
}
}  // namespace

#include <iconv.h>
namespace {
// Single-byte table of a Windows code page: byte -> UTF-16 (0 = undefined).
const char16_t* code_page_table(UINT page) {
    static std::map<UINT, std::vector<char16_t>> tables;
    static std::mutex m;
    std::lock_guard<std::mutex> guard(m);
    auto it = tables.find(page);
    if (it != tables.end()) return it->second.data();
    std::vector<char16_t> table(256, 0);
    for (int i = 0; i < 128; ++i) table[i] = static_cast<char16_t>(i);
    if (page == 1252) {
        for (int i = 0x80; i < 0xA0; ++i) table[i] = cp1252_high[i - 0x80];
        for (int i = 0xA0; i < 0x100; ++i) table[i] = static_cast<char16_t>(i);
    } else {
        const std::string name = "CP" + std::to_string(page);
        iconv_t cd = iconv_open("UTF-16LE", name.c_str());
        if (cd != reinterpret_cast<iconv_t>(-1)) {
            for (int i = 0x80; i < 0x100; ++i) {
                char in = static_cast<char>(i); char out[4] = {};
                char* ip = &in; char* op = out; size_t il = 1, ol = sizeof(out);
                iconv(cd, nullptr, nullptr, nullptr, nullptr);
                if (iconv(cd, &ip, &il, &op, &ol) != static_cast<size_t>(-1) && ol == 2)
                    table[i] = static_cast<char16_t>(static_cast<BYTE>(out[0]) | (static_cast<BYTE>(out[1]) << 8));
            }
            iconv_close(cd);
        }
    }
    return tables.emplace(page, std::move(table)).first->second.data();
}
UINT resolve_page(UINT page) { return (page == CP_ACP || page == CP_THREAD_ACP || page == CP_OEMCP) ? active_code_page() : page; }
void append_utf8(std::string* out, char32_t c) {
    if (c < 0x80) *out += static_cast<char>(c);
    else if (c < 0x800) { *out += static_cast<char>(0xC0 | (c >> 6)); *out += static_cast<char>(0x80 | (c & 0x3F)); }
    else if (c < 0x10000) { *out += static_cast<char>(0xE0 | (c >> 12)); *out += static_cast<char>(0x80 | ((c >> 6) & 0x3F)); *out += static_cast<char>(0x80 | (c & 0x3F)); }
    else { *out += static_cast<char>(0xF0 | (c >> 18)); *out += static_cast<char>(0x80 | ((c >> 12) & 0x3F)); *out += static_cast<char>(0x80 | ((c >> 6) & 0x3F)); *out += static_cast<char>(0x80 | (c & 0x3F)); }
}
// Decodes UTF-8; invalid sequences give U+FFFD.
std::u32string decode_utf8(const char* text) {
    std::u32string out;
    const unsigned char* p = reinterpret_cast<const unsigned char*>(text);
    while (p && *p) {
        char32_t c = *p; int extra = 0;
        if (c >= 0xF0 && c < 0xF8) { c &= 0x07; extra = 3; } else if (c >= 0xE0) { c &= 0x0F; extra = 2; }
        else if (c >= 0xC0) { c &= 0x1F; extra = 1; } else if (c >= 0x80) { out += 0xFFFD; ++p; continue; }
        ++p;
        bool ok = true;
        for (int i = 0; i < extra; ++i) { if ((*p & 0xC0) != 0x80) { ok = false; break; } c = (c << 6) | (*p++ & 0x3F); }
        out += ok ? c : 0xFFFD;
    }
    return out;
}
}  // namespace

// ---- Window classes and windows -------------------------------------------------
ATOM RegisterClass(const WNDCLASS* window_class) {
    if (!window_class || !window_class->lpszClassName) { SetLastError(ERROR_INVALID_PARAMETER); return 0; }
    State& s = state();
    std::lock_guard<std::recursive_mutex> guard(s.lock);
    if (s.classes.count(window_class->lpszClassName)) { SetLastError(1410); return 0; } // ERROR_CLASS_ALREADY_EXISTS
    s.classes[window_class->lpszClassName] = *window_class;
    return static_cast<ATOM>(0xC000 + s.classes.size());
}
BOOL UnregisterClass(LPCSTR class_name, HINSTANCE) {
    State& s = state();
    std::lock_guard<std::recursive_mutex> guard(s.lock);
    return class_name && s.classes.erase(class_name) ? TRUE : FALSE;
}
BOOL GetClassInfo(HINSTANCE, LPCSTR class_name, LPWNDCLASS window_class) {
    State& s = state();
    std::lock_guard<std::recursive_mutex> guard(s.lock);
    auto it = class_name ? s.classes.find(class_name) : s.classes.end();
    if (it == s.classes.end()) { SetLastError(1411); return FALSE; } // ERROR_CLASS_DOES_NOT_EXIST
    if (window_class) *window_class = it->second;
    return TRUE;
}

HWND CreateWindowEx(DWORD ex_style, LPCSTR class_name, LPCSTR window_name, DWORD style, int x, int y, int width, int height,
                    HWND parent, HMENU menu, HINSTANCE instance, LPVOID param) {
    State& s = state();
    WNDCLASS wc = {};
    {
        std::lock_guard<std::recursive_mutex> guard(s.lock);
        auto it = class_name ? s.classes.find(class_name) : s.classes.end();
        if (it == s.classes.end()) { SetLastError(1407); return nullptr; } // ERROR_CANNOT_FIND_WND_CLASS
        wc = it->second;
        if (!s.gui_thread_set) { s.gui_thread = std::this_thread::get_id(); s.gui_thread_set = true; }
        else if (!on_gui_thread()) { SetLastError(ERROR_NOT_SUPPORTED); return nullptr; }
    }
    const bool child = (style & WS_CHILD) != 0;
    if (child && !get(parent)) { SetLastError(1406); return nullptr; } // ERROR_TLW_WITH_WSCHILD
    if (!child && !s.backend) { SetLastError(ERROR_NOT_SUPPORTED); return nullptr; }
    Window* w = new Window;
    w->class_name = class_name; w->text = window_name ? window_name : "";
    w->style = style & ~WS_VISIBLE; w->ex_style = ex_style; w->parent = parent; w->id = menu; w->instance = instance;
    w->procedure = wc.lpfnWndProc; w->class_style = wc.style; w->class_cursor = wc.hCursor; w->icon = wc.hIcon;
    if (static_cast<UINT>(x) == CW_USEDEFAULT) { x = 0; y = 0; }
    if (static_cast<UINT>(width) == CW_USEDEFAULT) { width = 640; height = 480; }
    RECT insets; frame_insets(style, &insets);
    w->rect = {x, y, x + width, y + height};
    w->client = {insets.left, insets.top, std::max<LONG>(insets.left, width - insets.right), std::max<LONG>(insets.top, height - insets.bottom)};
    {
        std::lock_guard<std::recursive_mutex> guard(s.lock);
        s.windows.insert(w);
        if (Window* p = get(parent)) if (child) p->children.push_back(w);
    }
    if (!child) {
        fourstory::platform::WindowDesc desc;
        desc.x = x + w->client.left; desc.y = y + w->client.top;
        desc.width = w->client.right - w->client.left; desc.height = w->client.bottom - w->client.top;
        desc.decorated = (style & WS_CAPTION) == WS_CAPTION; desc.visible = false; desc.topmost = (ex_style & WS_EX_TOPMOST) != 0;
        const std::string title = fourstory::platform::acp_to_utf8(w->text.c_str());
        desc.title_utf8 = title.c_str();
        w->native = s.backend->create_window(w, desc);
        if (!w->native) {
            std::lock_guard<std::recursive_mutex> guard(s.lock);
            s.windows.erase(w); delete w;
            SetLastError(ERROR_NOT_SUPPORTED);
            return nullptr;
        }
    }
    if (s.create_hook) s.create_hook(w);
    CREATESTRUCT cs = {param, instance, menu, parent, height, width, y, x, static_cast<LONG>(style), window_name, class_name, ex_style};
    const bool nccreate = SendMessage(w, WM_NCCREATE, 0, reinterpret_cast<LPARAM>(&cs)) != 0;
    if (!nccreate || SendMessage(w, WM_CREATE, 0, reinterpret_cast<LPARAM>(&cs)) == -1) {
        if (get(w)) destroy_tree(w, nccreate);
        SetLastError(1400); // ERROR_INVALID_WINDOW_HANDLE
        return nullptr;
    }
    SendMessage(w, WM_SIZE, SIZE_RESTORED, MAKELPARAM(w->client.right - w->client.left, w->client.bottom - w->client.top));
    SendMessage(w, WM_MOVE, 0, point_lparam(w->rect.left + w->client.left, w->rect.top + w->client.top));
    if (style & WS_VISIBLE) ShowWindow(w, SW_SHOW);
    return w;
}

BOOL DestroyWindow(HWND window) {
    Window* w = get(window);
    if (!w) { SetLastError(1400); return FALSE; }
    if (w->destroying) return TRUE;
    w->destroying = true;
    if (w->visible) ShowWindow(window, SW_HIDE);
    destroy_tree(w);
    return TRUE;
}
BOOL IsWindow(HWND window) { std::lock_guard<std::recursive_mutex> guard(state().lock); return get(window) ? TRUE : FALSE; }

BOOL ShowWindow(HWND window, int command) {
    State& s = state();
    Window* w = get(window);
    if (!w) return FALSE;
    const BOOL was_visible = w->visible ? TRUE : FALSE;
    const bool show = command != SW_HIDE;
    if (show != w->visible) {
        w->visible = show;
        if (show) { w->style |= WS_VISIBLE; w->needs_paint = true; } else w->style &= ~WS_VISIBLE;
        SendMessage(window, WM_SHOWWINDOW, show ? TRUE : FALSE, 0);
    }
    if (w->native && s.backend) s.backend->show(w->native, command);
    if (command == SW_MINIMIZE || command == SW_SHOWMINIMIZED) w->minimized = true;
    else if (command == SW_MAXIMIZE) { w->maximized = true; w->minimized = false; }
    else if (command == SW_RESTORE || command == SW_SHOWNORMAL) { w->minimized = false; w->maximized = false; }
    if (show && !(w->style & WS_CHILD) && command != SW_SHOWNA && command != SW_SHOWNOACTIVATE && command != SW_MINIMIZE) SetActiveWindow(window);
    return was_visible;
}
BOOL UpdateWindow(HWND window) {
    Window* w = get(window);
    if (!w) return FALSE;
    if (w->needs_paint && w->visible) SendMessage(window, WM_PAINT, 0, 0);
    return TRUE;
}
BOOL SetWindowPos(HWND window, HWND insert_after, int x, int y, int cx, int cy, UINT flags) {
    State& s = state();
    Window* w = get(window);
    if (!w) { SetLastError(1400); return FALSE; }
    // Z order of top-level windows: topmost state and raising go to the backend.
    // Placing a window behind another one (HWND_BOTTOM, a window handle) is NOT SUPPORTED.
    if (!(flags & SWP_NOZORDER)) {
        if (insert_after == HWND_TOPMOST || insert_after == HWND_NOTOPMOST) {
            if (!w->native || !s.backend || !s.backend->set_topmost(w->native, insert_after == HWND_TOPMOST)) { SetLastError(ERROR_NOT_SUPPORTED); return FALSE; }
            if (insert_after == HWND_TOPMOST) w->ex_style |= WS_EX_TOPMOST; else w->ex_style &= ~static_cast<DWORD>(WS_EX_TOPMOST);
        } else if (insert_after == HWND_TOP) {
            if (w->native && s.backend) s.backend->raise(w->native);
        } else { SetLastError(ERROR_NOT_SUPPORTED); return FALSE; }
    }
    RECT r = w->rect;
    if (!(flags & SWP_NOMOVE)) { r.right += x - r.left; r.bottom += y - r.top; r.left = x; r.top = y; }
    if (!(flags & SWP_NOSIZE)) { r.right = r.left + cx; r.bottom = r.top + cy; }
    const bool moved = r.left != w->rect.left || r.top != w->rect.top;
    const bool sized = (r.right - r.left) != (w->rect.right - w->rect.left) || (r.bottom - r.top) != (w->rect.bottom - w->rect.top);
    RECT insets; frame_insets(w->style, &insets);
    w->rect = r;
    w->client = {insets.left, insets.top, std::max<LONG>(insets.left, (r.right - r.left) - insets.right), std::max<LONG>(insets.top, (r.bottom - r.top) - insets.bottom)};
    if (w->native && s.backend && (moved || sized))
        s.backend->set_client_rect(w->native, r.left + w->client.left, r.top + w->client.top, w->client.right - w->client.left, w->client.bottom - w->client.top);
    if (sized) SendMessage(window, WM_SIZE, w->maximized ? SIZE_MAXIMIZED : SIZE_RESTORED, MAKELPARAM(w->client.right - w->client.left, w->client.bottom - w->client.top));
    if (moved) SendMessage(window, WM_MOVE, 0, point_lparam(r.left + w->client.left, r.top + w->client.top));
    if (flags & SWP_SHOWWINDOW) ShowWindow(window, SW_SHOW);
    if (flags & SWP_HIDEWINDOW) ShowWindow(window, SW_HIDE);
    if (!(flags & SWP_NOREDRAW) && sized) w->needs_paint = true;
    return TRUE;
}
BOOL MoveWindow(HWND window, int x, int y, int width, int height, BOOL repaint) {
    return SetWindowPos(window, nullptr, x, y, width, height, SWP_NOZORDER | (repaint ? 0 : SWP_NOREDRAW));
}
BOOL GetWindowRect(HWND window, LPRECT rect) {
    Window* w = get(window);
    if (!w || !rect) { SetLastError(1400); return FALSE; }
    POINT o = {0, 0};
    if (w->style & WS_CHILD) { o = client_origin(w->parent); }
    *rect = {w->rect.left + o.x, w->rect.top + o.y, w->rect.right + o.x, w->rect.bottom + o.y};
    return TRUE;
}
BOOL GetClientRect(HWND window, LPRECT rect) {
    Window* w = get(window);
    if (!w || !rect) { SetLastError(1400); return FALSE; }
    *rect = {0, 0, w->client.right - w->client.left, w->client.bottom - w->client.top};
    return TRUE;
}
BOOL ClientToScreen(HWND window, LPPOINT point) {
    if (!get(window) || !point) return FALSE;
    POINT o = client_origin(window); point->x += o.x; point->y += o.y; return TRUE;
}
BOOL ScreenToClient(HWND window, LPPOINT point) {
    if (!get(window) || !point) return FALSE;
    POINT o = client_origin(window); point->x -= o.x; point->y -= o.y; return TRUE;
}
int MapWindowPoints(HWND from, HWND to, LPPOINT points, UINT count) {
    POINT a = from ? client_origin(from) : POINT{0, 0}, b = to ? client_origin(to) : POINT{0, 0};
    const LONG dx = a.x - b.x, dy = a.y - b.y;
    for (UINT i = 0; i < count; ++i) { points[i].x += dx; points[i].y += dy; }
    return MAKELONG(static_cast<WORD>(static_cast<SHORT>(dx)), static_cast<WORD>(static_cast<SHORT>(dy)));
}
BOOL AdjustWindowRectEx(LPRECT rect, DWORD style, BOOL, DWORD) {
    if (!rect) return FALSE;
    RECT insets; frame_insets(style, &insets);
    rect->left -= insets.left; rect->top -= insets.top; rect->right += insets.right; rect->bottom += insets.bottom;
    return TRUE;
}
BOOL AdjustWindowRect(LPRECT rect, DWORD style, BOOL menu) { return AdjustWindowRectEx(rect, style, menu, 0); }
HWND GetParent(HWND window) { Window* w = get(window); return w ? w->parent : nullptr; }
HWND SetParent(HWND child, HWND parent) {
    State& s = state();
    std::lock_guard<std::recursive_mutex> guard(s.lock);
    Window* w = get(child);
    if (!w || (parent && !get(parent))) { SetLastError(1400); return nullptr; }
    HWND old = w->parent;
    if (Window* p = get(old)) p->children.erase(std::remove(p->children.begin(), p->children.end(), child), p->children.end());
    w->parent = parent;
    if (Window* p = get(parent)) if (w->style & WS_CHILD) p->children.push_back(child);
    return old ? old : GetDesktopWindow();
}
HWND GetWindow(HWND window, UINT command) {
    Window* w = get(window);
    if (!w) return nullptr;
    if (command == GW_CHILD) return w->children.empty() ? nullptr : w->children.front();
    if (command == GW_OWNER) return (w->style & WS_CHILD) ? nullptr : w->parent;
    Window* p = get(w->parent);
    if (!p || !(w->style & WS_CHILD)) return nullptr;
    auto it = std::find(p->children.begin(), p->children.end(), window);
    if (command == GW_HWNDNEXT) return (it != p->children.end() && it + 1 != p->children.end()) ? *(it + 1) : nullptr;
    if (command == GW_HWNDPREV) return (it != p->children.begin() && it != p->children.end()) ? *(it - 1) : nullptr;
    if (command == GW_HWNDFIRST) return p->children.front();
    if (command == GW_HWNDLAST) return p->children.back();
    return nullptr;
}
HWND GetDesktopWindow() { static HWND__ desktop; return &desktop; }
BOOL IsChild(HWND parent, HWND window) {
    for (Window* w = get(window); w && (w->style & WS_CHILD); w = get(w->parent)) if (w->parent == parent) return TRUE;
    return FALSE;
}
BOOL IsWindowVisible(HWND window) {
    for (Window* w = get(window); w; w = (w->style & WS_CHILD) ? get(w->parent) : nullptr) if (!w->visible) return FALSE;
    return get(window) ? TRUE : FALSE;
}
BOOL IsWindowEnabled(HWND window) { Window* w = get(window); return w && w->enabled ? TRUE : FALSE; }
BOOL EnableWindow(HWND window, BOOL enable) {
    Window* w = get(window);
    if (!w) return FALSE;
    const BOOL was_disabled = w->enabled ? FALSE : TRUE;
    if (w->enabled != (enable != 0)) { w->enabled = enable != 0; SendMessage(window, WM_ENABLE, enable, 0); }
    return was_disabled;
}
BOOL IsIconic(HWND window) { Window* w = get(window); return w && w->minimized ? TRUE : FALSE; }
BOOL IsZoomed(HWND window) { Window* w = get(window); return w && w->maximized ? TRUE : FALSE; }
int GetWindowText(HWND window, LPSTR text, int size) {
    Window* w = get(window);
    if (!text || size <= 0) return 0;
    if (!w) { text[0] = 0; return 0; }
    const int n = std::min<int>(size - 1, static_cast<int>(w->text.size()));
    std::memcpy(text, w->text.data(), static_cast<size_t>(n)); text[n] = 0;
    return n;
}
int GetWindowTextLength(HWND window) { Window* w = get(window); return w ? static_cast<int>(w->text.size()) : 0; }
BOOL SetWindowText(HWND window, LPCSTR text) {
    State& s = state();
    Window* w = get(window);
    if (!w) { SetLastError(1400); return FALSE; }
    w->text = text ? text : "";
    if (w->native && s.backend) s.backend->set_title(w->native, fourstory::platform::acp_to_utf8(w->text.c_str()).c_str());
    return TRUE;
}
LONG_PTR GetWindowLongPtr(HWND window, int index) {
    Window* w = get(window);
    if (!w) { SetLastError(1400); return 0; }
    switch (index) {
    case GWLP_WNDPROC: return reinterpret_cast<LONG_PTR>(w->procedure);
    case GWL_STYLE: return static_cast<LONG>(w->style);
    case GWL_EXSTYLE: return static_cast<LONG>(w->ex_style);
    case GWLP_USERDATA: return w->user_data;
    case GWLP_ID: return reinterpret_cast<LONG_PTR>(w->id);
    case GWLP_HINSTANCE: return reinterpret_cast<LONG_PTR>(w->instance);
    case GWLP_HWNDPARENT: return reinterpret_cast<LONG_PTR>(w->parent);
    }
    SetLastError(1413); // ERROR_INVALID_INDEX
    return 0;
}
LONG_PTR SetWindowLongPtr(HWND window, int index, LONG_PTR value) {
    Window* w = get(window);
    if (!w) { SetLastError(1400); return 0; }
    LONG_PTR old = GetWindowLongPtr(window, index);
    switch (index) {
    case GWLP_WNDPROC: w->procedure = reinterpret_cast<WNDPROC>(value); break;
    case GWL_STYLE: {
        const DWORD style = static_cast<DWORD>(value);
        State& s = state();
        const bool decorated = !(style & WS_CHILD) && (style & WS_CAPTION) == WS_CAPTION;
        if (w->native && s.backend && !s.backend->set_decorated(w->native, decorated)) {
            SetLastError(ERROR_NOT_SUPPORTED); return 0;
        }
        // Keep the client origin and size stable while replacing the frame.
        const POINT origin = client_origin(window);
        const LONG width = w->client.right - w->client.left;
        const LONG height = w->client.bottom - w->client.top;
        w->style = style;
        RECT insets; frame_insets(style, &insets);
        if (w->native) {
            w->rect = {origin.x - insets.left, origin.y - insets.top,
                       origin.x + width + insets.right, origin.y + height + insets.bottom};
            w->client = {insets.left, insets.top, insets.left + width, insets.top + height};
        }
        break;
    }
    case GWL_EXSTYLE: w->ex_style = static_cast<DWORD>(value); break;
    case GWLP_USERDATA: w->user_data = value; break;
    case GWLP_ID: w->id = reinterpret_cast<HMENU>(value); break;
    default: SetLastError(1413); return 0;
    }
    return old;
}
LRESULT CallWindowProc(WNDPROC procedure, HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    return procedure ? procedure(window, message, wparam, lparam) : 0;
}

LRESULT DefWindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    State& s = state();
    Window* w = get(window);
    if (!w) return 0;
    switch (message) {
    case WM_NCCREATE: return TRUE;
    case WM_CLOSE: DestroyWindow(window); return 0;
    case WM_NCHITTEST: {
        POINT p = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        ScreenToClient(window, &p);
        RECT c; GetClientRect(window, &c);
        return PtInRect(&c, p) ? HTCLIENT : HTNOWHERE; }
    case WM_SETCURSOR:
        if ((w->style & WS_CHILD) && get(w->parent) && SendMessage(w->parent, WM_SETCURSOR, wparam, lparam)) return TRUE;
        if (LOWORD(lparam) == HTCLIENT && w->class_cursor) { SetCursor(w->class_cursor); return TRUE; }
        return FALSE;
    case WM_MOUSEWHEEL: case WM_MOUSEHWHEEL:
        if ((w->style & WS_CHILD) && get(w->parent)) return SendMessage(w->parent, message, wparam, lparam);
        return 0;
    case WM_SYSKEYDOWN:
        if (wparam == VK_F4 && (s.key_sync[VK_MENU] & 0x80)) PostMessage(top_level(window), WM_SYSCOMMAND, SC_CLOSE, 0);
        return 0;
    case WM_SYSCOMMAND:
        switch (wparam & 0xFFF0) {
        case SC_CLOSE: SendMessage(window, WM_CLOSE, 0, 0); return 0;
        case SC_MINIMIZE: ShowWindow(window, SW_MINIMIZE); return 0;
        case SC_MAXIMIZE: ShowWindow(window, SW_MAXIMIZE); return 0;
        case SC_RESTORE: ShowWindow(window, SW_RESTORE); return 0;
        }
        return 0;
    case WM_PAINT: { PAINTSTRUCT ps; BeginPaint(window, &ps); EndPaint(window, &ps); return 0; }
    case WM_ERASEBKGND: return 0;
    case WM_ACTIVATE: if (LOWORD(wparam) != WA_INACTIVE && !HIWORD(wparam)) SetFocus(window); return 0;
    case WM_GETTEXT: return GetWindowText(window, reinterpret_cast<LPSTR>(lparam), static_cast<int>(wparam));
    case WM_GETTEXTLENGTH: return GetWindowTextLength(window);
    case WM_SETTEXT: return SetWindowText(window, reinterpret_cast<LPCSTR>(lparam));
    case WM_MOUSEACTIVATE: return MA_ACTIVATE;
    }
    return 0;
}

BOOL EnumChildWindows(HWND parent, WNDENUMPROC procedure, LPARAM lparam) {
    Window* w = get(parent);
    if (!w || !procedure) return FALSE;
    std::vector<HWND> all;
    std::vector<HWND> stack(w->children.rbegin(), w->children.rend());
    while (!stack.empty()) {
        HWND h = stack.back(); stack.pop_back(); all.push_back(h);
        if (Window* c = get(h)) stack.insert(stack.end(), c->children.rbegin(), c->children.rend());
    }
    for (HWND h : all) if (get(h) && !procedure(h, lparam)) return FALSE;
    return TRUE;
}
HWND ChildWindowFromPointEx(HWND parent, POINT point, UINT flags) {
    Window* w = get(parent);
    if (!w) return nullptr;
    RECT c; GetClientRect(parent, &c);
    if (!PtInRect(&c, point)) return nullptr;
    for (auto it = w->children.rbegin(); it != w->children.rend(); ++it) {
        Window* ch = get(*it);
        if (!ch) continue;
        if ((flags & CWP_SKIPINVISIBLE) && !ch->visible) continue;
        if ((flags & CWP_SKIPDISABLED) && !ch->enabled) continue;
        if ((flags & CWP_SKIPTRANSPARENT) && (ch->ex_style & WS_EX_TRANSPARENT)) continue;
        if (PtInRect(&ch->rect, point)) return ch;
    }
    return parent;
}
HWND WindowFromPoint(POINT point) {
    State& s = state();
    for (Window* w : s.windows) {
        if ((w->style & WS_CHILD) || !w->visible) continue;
        if (!PtInRect(&w->rect, point)) continue;
        POINT p = point; POINT o = client_origin(w); p.x -= o.x; p.y -= o.y;
        return hit_child(w, &p);
    }
    return nullptr;
}
HWND GetActiveWindow() { return state().active; }
HWND SetActiveWindow(HWND window) {
    State& s = state();
    HWND old = s.active;
    window = top_level(window);
    if (!window || window == old) return old;
    s.active = window;
    if (get(old)) SendMessage(old, WM_ACTIVATE, MAKEWPARAM(WA_INACTIVE, IsIconic(old)), reinterpret_cast<LPARAM>(window));
    SendMessage(window, WM_ACTIVATE, MAKEWPARAM(WA_ACTIVE, IsIconic(window)), reinterpret_cast<LPARAM>(old));
    return old;
}
// The foreground window is the active window while the application has the
// keyboard focus of the desktop (backend focus event), otherwise none.
namespace { bool g_application_focused = false; }
HWND GetForegroundWindow() { return g_application_focused ? state().active : nullptr; }
BOOL SetForegroundWindow(HWND window) {
    State& s = state();
    Window* w = get(top_level(window));
    if (!w) return FALSE;
    if (w->native && s.backend) s.backend->raise(w->native);
    SetActiveWindow(w);
    return TRUE;
}
HWND GetFocus() { return state().focus; }
HWND SetFocus(HWND window) {
    State& s = state();
    HWND old = s.focus;
    if (window && !get(window)) { SetLastError(1400); return nullptr; }
    if (window == old) return old;
    s.focus = window;
    if (get(old)) SendMessage(old, WM_KILLFOCUS, reinterpret_cast<WPARAM>(window), 0);
    if (window) SendMessage(window, WM_SETFOCUS, reinterpret_cast<WPARAM>(old), 0);
    return old;
}
HWND SetCapture(HWND window) {
    State& s = state();
    HWND old = s.capture;
    if (!get(window)) return old;
    s.capture = window;
    if (s.backend) s.backend->capture_mouse(true);
    return old;
}
BOOL ReleaseCapture() {
    State& s = state();
    HWND old = s.capture;
    s.capture = nullptr;
    if (s.backend) s.backend->capture_mouse(false);
    if (get(old)) SendMessage(old, WM_CAPTURECHANGED, 0, 0);
    return TRUE;
}
HWND GetCapture() { return state().capture; }
HANDLE GetProp(HWND window, LPCSTR name) {
    Window* w = get(window);
    if (!w || !name) return nullptr;
    auto it = w->props.find(IS_INTRESOURCE(name) ? "#" + std::to_string(reinterpret_cast<ULONG_PTR>(name)) : std::string(name));
    return it == w->props.end() ? nullptr : it->second;
}
BOOL SetProp(HWND window, LPCSTR name, HANDLE data) {
    Window* w = get(window);
    if (!w || !name) return FALSE;
    w->props[IS_INTRESOURCE(name) ? "#" + std::to_string(reinterpret_cast<ULONG_PTR>(name)) : std::string(name)] = data;
    return TRUE;
}
HANDLE RemoveProp(HWND window, LPCSTR name) {
    Window* w = get(window);
    if (!w || !name) return nullptr;
    const std::string key = IS_INTRESOURCE(name) ? "#" + std::to_string(reinterpret_cast<ULONG_PTR>(name)) : std::string(name);
    auto it = w->props.find(key);
    if (it == w->props.end()) return nullptr;
    HANDLE data = it->second; w->props.erase(it); return data;
}
BOOL InvalidateRect(HWND window, const RECT*, BOOL) {
    Window* w = get(window);
    if (!w) return FALSE;
    w->needs_paint = true; wake_gui(); return TRUE;
}
BOOL ValidateRect(HWND window, const RECT*) { Window* w = get(window); if (!w) return FALSE; w->needs_paint = false; return TRUE; }
// The window device context of the native layer has no pixels: GDI drawing
// to a window is not presented (the client draws with Direct3D).
HDC BeginPaint(HWND window, LPPAINTSTRUCT paint) {
    Window* w = get(window);
    if (!w || !paint) return nullptr;
    std::memset(paint, 0, sizeof(*paint));
    GetClientRect(window, &paint->rcPaint);
    paint->hdc = GetDC(window);
    w->needs_paint = false;
    return paint->hdc;
}
BOOL EndPaint(HWND window, const PAINTSTRUCT* paint) { if (paint && paint->hdc) ReleaseDC(window, paint->hdc); return TRUE; }
BOOL FlashWindow(HWND, BOOL) { return FALSE; }
BOOL SetWindowIcon(HWND window, HICON icon) { Window* w = get(window); if (!w) return FALSE; w->icon = icon; return TRUE; }

// ---- Messages -------------------------------------------------------------------
BOOL PeekMessage(LPMSG message, HWND window, UINT low, UINT high, UINT remove) {
    if (!message || !on_gui_thread()) return FALSE;
    pump_backend();
    MSG m;
    if (!retrieve(&m, window, low, high, (remove & PM_REMOVE) != 0)) return FALSE;
    State& s = state();
    s.message_time = static_cast<LONG>(m.time); s.message_pos = static_cast<DWORD>(MAKELONG(m.pt.x, m.pt.y));
    *message = m;
    return TRUE;
}
BOOL GetMessage(LPMSG message, HWND window, UINT low, UINT high) {
    if (!message || !on_gui_thread()) return -1;
    for (;;) {
        if (PeekMessage(message, window, low, high, PM_REMOVE)) return message->message == WM_QUIT ? FALSE : TRUE;
        WaitMessage();
    }
}
BOOL PostMessage(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    State& s = state();
    {
        std::lock_guard<std::recursive_mutex> guard(s.lock);
        if (window && !get(window)) { SetLastError(1400); return FALSE; }
        MSG m; make_msg(&m, window, message, wparam, lparam);
        s.posted_queue.push_back(m);
    }
    wake_gui();
    return TRUE;
}
// One GUI thread: a thread message goes to the queue of the GUI thread.
BOOL PostThreadMessage(DWORD, UINT message, WPARAM wparam, LPARAM lparam) { return PostMessage(nullptr, message, wparam, lparam); }
LRESULT SendMessage(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    State& s = state();
    if (on_gui_thread()) {
        Window* w = get(window);
        if (!w || !w->procedure) return 0;
        return w->procedure(window, message, wparam, lparam);
    }
    // Another thread: the GUI thread runs the procedure while it retrieves messages.
    SentMessage sent; make_msg(&sent.msg, window, message, wparam, lparam);
    std::unique_lock<std::recursive_mutex> guard(s.lock);
    if (!get(window)) return 0;
    s.sent_queue.push_back(&sent);
    wake_gui();
    s.posted.wait(guard, [&] { return sent.done || (!sent.popped && !get(window)); });
    if (!sent.done) s.sent_queue.erase(std::remove(s.sent_queue.begin(), s.sent_queue.end(), &sent), s.sent_queue.end());
    return sent.result;
}
void PostQuitMessage(int exit_code) {
    State& s = state();
    { std::lock_guard<std::recursive_mutex> guard(s.lock); s.quit = true; s.quit_code = exit_code; }
    wake_gui();
}
// Character messages: printable text comes from the backend text events
// (input_text). TranslateMessage adds the control characters that Windows
// derives from key presses: Backspace, Tab, Enter, Escape and Ctrl+letter.
BOOL TranslateMessage(const MSG* message) {
    if (!message || message->message != WM_KEYDOWN) return FALSE;
    State& s = state();
    const bool ctrl = (s.key_sync[VK_CONTROL] & 0x80) != 0, alt = (s.key_sync[VK_MENU] & 0x80) != 0;
    UINT ch = 0;
    switch (message->wParam) {
    case VK_BACK: ch = 0x08; break;
    case VK_TAB: ch = 0x09; break;
    case VK_RETURN: ch = ctrl ? 0x0A : 0x0D; break;
    case VK_ESCAPE: ch = 0x1B; break;
    default:
        if (ctrl && !alt && message->wParam >= 'A' && message->wParam <= 'Z') ch = static_cast<UINT>(message->wParam - 'A' + 1);
    }
    if (!ch) return FALSE;
    std::lock_guard<std::recursive_mutex> guard(s.lock);
    MSG m; make_msg(&m, message->hwnd, WM_CHAR, ch, message->lParam);
    s.posted_queue.push_back(m);
    return TRUE;
}
LRESULT DispatchMessage(const MSG* message) {
    if (!message) return 0;
    if (message->message == WM_TIMER && message->lParam) {
        reinterpret_cast<TIMERPROC>(message->lParam)(message->hwnd, WM_TIMER, message->wParam, now_ticks());
        return 0;
    }
    Window* w = get(message->hwnd);
    if (!w || !w->procedure) return 0;
    return w->procedure(message->hwnd, message->message, message->wParam, message->lParam);
}
BOOL WaitMessage() {
    State& s = state();
    if (!on_gui_thread()) return FALSE;
    for (;;) {
        pump_backend();
        {
            std::unique_lock<std::recursive_mutex> guard(s.lock);
            if (!s.posted_queue.empty() || !s.input_queue.empty() || !s.sent_queue.empty() || s.quit) return TRUE;
            for (Window* w : s.windows) if (w->needs_paint && w->visible) return TRUE;
        }
        const DWORD wait = next_timer_wait();
        if (wait == 0) return TRUE;
        if (s.backend) s.backend->wait(wait);
        else { std::unique_lock<std::recursive_mutex> guard(s.lock); s.posted.wait_for(guard, std::chrono::milliseconds(wait)); }
    }
}
LONG GetMessageTime() { return state().message_time; }
DWORD GetMessagePos() { return state().message_pos; }
DWORD GetQueueStatus(UINT flags) {
    State& s = state();
    std::lock_guard<std::recursive_mutex> guard(s.lock);
    DWORD status = 0;
    if (!s.posted_queue.empty()) status |= QS_POSTMESSAGE;
    for (const MSG& m : s.input_queue) status |= (m.message >= WM_KEYFIRST && m.message <= WM_KEYLAST) ? QS_KEY : (m.message == WM_MOUSEMOVE ? QS_MOUSEMOVE : QS_MOUSEBUTTON);
    if (!s.sent_queue.empty()) status |= QS_SENDMESSAGE;
    return (status & flags) << 16 | (status & flags);
}
UINT RegisterWindowMessage(LPCSTR name) {
    State& s = state();
    std::lock_guard<std::recursive_mutex> guard(s.lock);
    if (!name || !*name) return 0;
    auto it = s.registered_messages.find(name);
    if (it != s.registered_messages.end()) return it->second;
    return s.registered_messages[name] = s.next_registered++;
}
UINT_PTR SetTimer(HWND window, UINT_PTR id, UINT elapse, TIMERPROC procedure) {
    State& s = state();
    std::lock_guard<std::recursive_mutex> guard(s.lock);
    if (window && !get(window)) { SetLastError(1400); return 0; }
    elapse = std::max<UINT>(elapse, USER_TIMER_MINIMUM);
    if (!window) id = s.next_timer_id++;
    const auto due = std::chrono::steady_clock::now() + std::chrono::milliseconds(elapse);
    for (Timer& t : s.timers) if (t.window == window && t.id == id) { t.elapse = elapse; t.procedure = procedure; t.due = due; return window ? 1 : id; }
    s.timers.push_back({window, id, elapse, procedure, due});
    wake_gui();
    return window ? (id ? id : 1) : id;
}
BOOL KillTimer(HWND window, UINT_PTR id) {
    State& s = state();
    std::lock_guard<std::recursive_mutex> guard(s.lock);
    auto it = std::find_if(s.timers.begin(), s.timers.end(), [&](const Timer& t) { return t.window == window && t.id == id; });
    if (it == s.timers.end()) { SetLastError(1402); return FALSE; } // ERROR_INVALID_TIMER_ID... (Windows: 1402 = ERROR_INVALID_CURSOR_HANDLE?) see README
    s.timers.erase(it);
    for (auto q = s.posted_queue.begin(); q != s.posted_queue.end();) q = (q->message == WM_TIMER && q->hwnd == window && q->wParam == id) ? s.posted_queue.erase(q) : q + 1;
    return TRUE;
}
// Accelerator tables are resources. The native build has no resource
// section yet (C4): LoadAccelerators fails, TranslateAccelerator translates nothing.
HACCEL LoadAccelerators(HINSTANCE, LPCSTR) { SetLastError(1814); return nullptr; } // ERROR_RESOURCE_NAME_NOT_FOUND
int TranslateAccelerator(HWND, HACCEL table, LPMSG) { if (!table) SetLastError(1400); return 0; }
// Global hooks do not exist on macOS.
HHOOK SetWindowsHookEx(int, HOOKPROC, HINSTANCE, DWORD) { SetLastError(ERROR_NOT_SUPPORTED); return nullptr; }
BOOL UnhookWindowsHookEx(HHOOK) { SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
LRESULT CallNextHookEx(HHOOK, int, WPARAM, LPARAM) { return 0; }

// ---- Keyboard and mouse -----------------------------------------------------------
SHORT GetKeyState(int key) {
    const BYTE v = state().key_sync[key & 0xFF];
    return static_cast<SHORT>(((v & 0x80) ? 0x8000 : 0) | (v & 0x01));
}
SHORT GetAsyncKeyState(int key) { return static_cast<SHORT>((state().key_async[key & 0xFF] & 0x80) ? 0x8000 : 0); }
BOOL GetKeyboardState(PBYTE out) { if (!out) return FALSE; std::memcpy(out, state().key_sync, 256); return TRUE; }
// Set-1 scan codes of the US layout (the layout of the original key names).
namespace {
const BYTE vk_scan[256] = {
    /*00*/ 0,0,0,0x46,0,0,0,0, 0x0E,0x0F,0,0,0x4C,0x1C,0,0,
    /*10*/ 0x2A,0x1D,0x38,0x45,0x3A,0,0,0, 0,0,0,0x01,0,0,0,0,
    /*20*/ 0x39,0x49,0x51,0x4F,0x47,0x4B,0x48,0x4D, 0x50,0,0,0,0x54,0x52,0x53,0,
    /*30*/ 0x0B,0x02,0x03,0x04,0x05,0x06,0x07,0x08, 0x09,0x0A,0,0,0,0,0,0,
    /*40*/ 0,0x1E,0x30,0x2E,0x20,0x12,0x21,0x22, 0x23,0x17,0x24,0x25,0x26,0x32,0x31,0x18,
    /*50*/ 0x19,0x10,0x13,0x1F,0x14,0x16,0x2F,0x11, 0x2D,0x15,0x2C,0x5B,0x5C,0x5D,0,0x5F,
    /*60*/ 0x52,0x4F,0x50,0x51,0x4B,0x4C,0x4D,0x47, 0x48,0x49,0x37,0x4E,0,0x4A,0x53,0x35,
    /*70*/ 0x3B,0x3C,0x3D,0x3E,0x3F,0x40,0x41,0x42, 0x43,0x44,0x57,0x58,0x64,0x65,0x66,0x67,
    /*80*/ 0x68,0x69,0x6A,0x6B,0x6C,0x6D,0x6E,0x76, 0,0,0,0,0,0,0,0,
    /*90*/ 0x45,0x46,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
    /*A0*/ 0x2A,0x36,0x1D,0x1D,0x38,0x38,0,0, 0,0,0,0,0,0,0,0,
    /*B0*/ 0,0,0,0,0,0,0,0, 0,0,0x27,0x0D,0x33,0x0C,0x34,0x35,
    /*C0*/ 0x29,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
    /*D0*/ 0,0,0,0,0,0,0,0, 0,0,0,0x1A,0x2B,0x1B,0x28,0,
    /*E0*/ 0,0,0x56,0,0,0,0,0, 0,0,0,0,0,0,0,0,
    /*F0*/ 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0 };
}
UINT MapVirtualKey(UINT code, UINT map_type) {
    switch (map_type) {
    case MAPVK_VK_TO_VSC: return code < 256 ? vk_scan[code] : 0;
    case MAPVK_VSC_TO_VK: case MAPVK_VSC_TO_VK_EX:
        for (UINT vk = 1; vk < 256; ++vk) if (vk_scan[vk] == (code & 0xFF)) {
            if (map_type == MAPVK_VSC_TO_VK && (vk == VK_LSHIFT || vk == VK_RSHIFT)) return VK_SHIFT;
            if (map_type == MAPVK_VSC_TO_VK && (vk == VK_LCONTROL || vk == VK_RCONTROL)) return VK_CONTROL;
            if (map_type == MAPVK_VSC_TO_VK && (vk == VK_LMENU || vk == VK_RMENU)) return VK_MENU;
            if (vk >= VK_NUMPAD0 && vk <= VK_DIVIDE) continue;
            return vk;
        }
        return 0;
    case MAPVK_VK_TO_CHAR:
        if ((code >= '0' && code <= '9') || (code >= 'A' && code <= 'Z') || code == VK_SPACE) return code;
        return 0;
    }
    return 0;
}
// The keyboard layout identifier encodes the language of LCID (1033 English
// US by default). Layout switching is not supported.
HKL GetKeyboardLayout(DWORD) { return reinterpret_cast<HKL>(static_cast<ULONG_PTR>(0x04090409)); }
UINT GetDoubleClickTime() { return 500; }
BOOL GetCursorPos(LPPOINT point) {
    State& s = state();
    if (!point) return FALSE;
    int x, y;
    if (!s.input_suspended && s.backend && s.backend->cursor_position(&x, &y)) { s.cursor = {x, y}; }
    *point = s.cursor;
    return TRUE;
}
HKL ActivateKeyboardLayout(HKL, UINT) { SetLastError(ERROR_NOT_SUPPORTED); return nullptr; }
BOOL SetCursorPos(int x, int y) {
    State& s = state();
    Window* w = get(s.active);
    if (s.input_suspended || !w || !w->native || !s.backend) { SetLastError(ERROR_NOT_SUPPORTED); return FALSE; }
    POINT o = client_origin(w);
    if (!s.backend->warp_mouse(w->native, x - o.x, y - o.y)) return FALSE;
    s.cursor = {x, y};
    return TRUE;
}
// The cursor is visible when the ShowCursor counter is >= 0 and a cursor is
// set: SetCursor(NULL) removes the cursor shape, as on Windows.
namespace { void apply_cursor_visibility() {
    State& s = state();
    if (s.backend) s.backend->show_cursor(s.cursor_count >= 0 && s.cursor_current != nullptr);
} }
int ShowCursor(BOOL show) {
    State& s = state();
    s.cursor_count += show ? 1 : -1;
    apply_cursor_visibility();
    return s.cursor_count;
}
// System cursors are handles 32512..32651 (the IDC_ resource numbers).
// Program cursors (LoadCursor with an instance and a cursor resource) are
// ProgramCursor objects; the backend shows them. LoadCursorFromFile fails.
namespace {
struct ProgramCursor { unsigned magic; void* native; };
constexpr unsigned kCursorMagic = 0x43555253u; // "CURS"
ProgramCursor* program_cursor(HCURSOR cursor) {
    const ULONG_PTR id = reinterpret_cast<ULONG_PTR>(cursor);
    if (!cursor || id <= 0xFFFF) return nullptr;
    ProgramCursor* c = reinterpret_cast<ProgramCursor*>(cursor);
    return c->magic == kCursorMagic ? c : nullptr;
}
std::uint16_t rd16(const unsigned char* p) { return static_cast<std::uint16_t>(p[0] | (p[1] << 8)); }
std::uint32_t rd32(const unsigned char* p) { return static_cast<std::uint32_t>(p[0] | (p[1] << 8) | (p[2] << 16)) | (static_cast<std::uint32_t>(p[3]) << 24); }
// One DIB cursor/icon image: BITMAPINFOHEADER, palette, XOR bitmap, AND mask (bottom-up).
bool decode_dib(const unsigned char* d, std::size_t size, int* width, int* height, std::vector<unsigned char>* rgba) {
    if (size < 40 || rd32(d) < 40) return false;
    const int w = static_cast<int>(rd32(d + 4)), h2 = static_cast<int>(rd32(d + 8)), bpp = rd16(d + 14);
    const std::uint32_t compression = rd32(d + 16), header = rd32(d);
    if (w <= 0 || w > 256 || h2 <= 0 || h2 > 512 || compression != 0) return false;
    if (bpp != 1 && bpp != 4 && bpp != 8 && bpp != 24 && bpp != 32) return false;
    const int h = h2 / 2;
    std::uint32_t colors = rd32(d + 32);
    if (bpp <= 8 && (colors == 0 || colors > (1u << bpp))) colors = 1u << bpp;
    if (bpp > 8) colors = 0;
    if (header > size) return false;
    const std::size_t palette = header, xor_off = palette + static_cast<std::size_t>(colors) * 4u;
    const std::size_t xor_stride = ((static_cast<std::size_t>(w) * bpp + 31) / 32) * 4, and_stride = ((static_cast<std::size_t>(w) + 31) / 32) * 4;
    const std::size_t and_off = xor_off + xor_stride * static_cast<std::size_t>(h);
    if (and_off + and_stride * static_cast<std::size_t>(h) > size) return false;
    rgba->assign(static_cast<std::size_t>(w) * h * 4, 0);
    bool any_alpha = false;
    if (bpp == 32)
        for (int y = 0; y < h && !any_alpha; ++y)
            for (int x = 0; x < w; ++x) if (d[xor_off + y * xor_stride + x * 4 + 3]) { any_alpha = true; break; }
    for (int y = 0; y < h; ++y) {
        const unsigned char* row = d + xor_off + static_cast<std::size_t>(h - 1 - y) * xor_stride;
        const unsigned char* mask = d + and_off + static_cast<std::size_t>(h - 1 - y) * and_stride;
        for (int x = 0; x < w; ++x) {
            unsigned char b = 0, g = 0, r = 0, a = 255;
            if (bpp == 32) { b = row[x * 4]; g = row[x * 4 + 1]; r = row[x * 4 + 2]; a = row[x * 4 + 3]; }
            else if (bpp == 24) { b = row[x * 3]; g = row[x * 3 + 1]; r = row[x * 3 + 2]; }
            else {
                unsigned index = 0;
                if (bpp == 8) index = row[x];
                else if (bpp == 4) index = (row[x / 2] >> ((x & 1) ? 0 : 4)) & 15u;
                else index = (row[x / 8] >> (7 - (x & 7))) & 1u;
                if (index < colors) { const unsigned char* c = d + palette + index * 4u; b = c[0]; g = c[1]; r = c[2]; }
            }
            const bool and_bit = (mask[x / 8] >> (7 - (x & 7))) & 1u;
            if (!(bpp == 32 && any_alpha)) {
                if (and_bit) {
                    // Transparent where the XOR color is black; inverted otherwise (shown opaque black).
                    if (r | g | b) { r = g = b = 0; a = 255; } else a = 0;
                } else a = 255;
            }
            unsigned char* o = &(*rgba)[(static_cast<std::size_t>(y) * w + x) * 4];
            o[0] = r; o[1] = g; o[2] = b; o[3] = a;
        }
    }
    *width = w; *height = h;
    return true;
}
// ICONDIR (type 1 icon, type 2 cursor): the largest image; the cursor hot spot.
bool decode_icondir(const unsigned char* d, std::size_t size, fourstory::platform::CursorImage* image, std::vector<unsigned char>* rgba) {
    if (size < 6 || rd16(d) != 0 || (rd16(d + 2) != 1 && rd16(d + 2) != 2)) return false;
    const unsigned count = rd16(d + 4);
    if (!count || 6 + 16u * count > size) return false;
    unsigned best = 0; int best_area = -1;
    for (unsigned i = 0; i < count; ++i) {
        const unsigned char* e = d + 6 + 16 * i;
        const int w = e[0] ? e[0] : 256, h = e[1] ? e[1] : 256;
        if (w * h > best_area) { best_area = w * h; best = i; }
    }
    const unsigned char* e = d + 6 + 16 * best;
    const std::uint32_t bytes = rd32(e + 8), offset = rd32(e + 12);
    if (offset > size || bytes > size - offset) return false;
    if (bytes >= 8 && d[offset] == 0x89 && d[offset + 1] == 'P') return false; // PNG image: not decoded
    int w = 0, h = 0;
    if (!decode_dib(d + offset, bytes, &w, &h, rgba)) return false;
    image->width = w; image->height = h;
    if (rd16(d + 2) == 2) { image->hot_x = rd16(e + 4); image->hot_y = rd16(e + 6); }
    return true;
}
}
namespace fourstory { namespace platform {
bool decode_cursor(const unsigned char* d, std::size_t size, CursorImage* image) {
    if (!d || !image) return false;
    *image = CursorImage();
    if (size >= 12 && std::memcmp(d, "RIFF", 4) == 0 && std::memcmp(d + 8, "ACON", 4) == 0) {
        std::uint32_t frames = 0, steps = 0, rate = 0, flags = 0;
        std::vector<std::uint32_t> rates, sequence;
        std::vector<std::vector<unsigned char>> images;
        std::size_t i = 12;
        const std::size_t end = std::min<std::size_t>(size, 8 + static_cast<std::size_t>(rd32(d + 4)));
        while (i + 8 <= end) {
            const unsigned char* id = d + i; const std::uint32_t n = rd32(d + i + 4);
            if (n > end - i - 8) return false;
            const unsigned char* body = d + i + 8;
            if (std::memcmp(id, "LIST", 4) == 0) { i += 12; continue; }  // descend into "fram"
            if (std::memcmp(id, "anih", 4) == 0 && n >= 36) { frames = rd32(body + 4); steps = rd32(body + 8); rate = rd32(body + 28); flags = rd32(body + 32); }
            else if (std::memcmp(id, "rate", 4) == 0) for (std::uint32_t k = 0; k + 4 <= n; k += 4) rates.push_back(rd32(body + k));
            else if (std::memcmp(id, "seq ", 4) == 0) for (std::uint32_t k = 0; k + 4 <= n; k += 4) sequence.push_back(rd32(body + k));
            else if (std::memcmp(id, "icon", 4) == 0) {
                CursorImage frame; std::vector<unsigned char> rgba;
                if (!(flags & 1u) || !decode_icondir(body, n, &frame, &rgba)) return false; // raw-bitmap frames: not decoded
                if (images.empty()) { image->width = frame.width; image->height = frame.height; image->hot_x = frame.hot_x; image->hot_y = frame.hot_y; }
                else if (frame.width != image->width || frame.height != image->height) return false;
                images.push_back(std::move(rgba));
            }
            i += 8 + n + (n & 1u);
        }
        if (images.empty() || (frames && images.size() != frames)) return false;
        if (!steps) steps = static_cast<std::uint32_t>(images.size());
        for (std::uint32_t k = 0; k < steps; ++k) {
            const std::uint32_t index = (flags & 2u) && k < sequence.size() ? sequence[k] : k;
            if (index >= images.size()) return false;
            const std::uint32_t jiffies = k < rates.size() ? rates[k] : rate;
            image->frames.push_back(images[index]);
            image->durations_ms.push_back(std::max<DWORD>(1, static_cast<DWORD>((jiffies * 1000u + 30u) / 60u))); // 1/60 s units
        }
        return true;
    }
    std::vector<unsigned char> rgba;
    if (!decode_icondir(d, size, image, &rgba)) return false;
    image->frames.push_back(std::move(rgba));
    return true;
}
}}
HCURSOR SetCursor(HCURSOR cursor) {
    State& s = state();
    HCURSOR old = s.cursor_current;
    s.cursor_current = cursor;
    const ULONG_PTR id = reinterpret_cast<ULONG_PTR>(cursor);
    if (s.backend && cursor && id >= 32512 && id <= 32651) s.backend->set_system_cursor(static_cast<int>(id));
    else if (ProgramCursor* c = program_cursor(cursor)) { if (s.backend && c->native) s.backend->set_cursor(c->native); }
    apply_cursor_visibility();
    return old;
}
HCURSOR GetCursor() { return state().cursor_current; }
// RT_ANICURSOR (21, the resource compiler's type for RIFF cursor files) is
// searched before RT_GROUP_CURSOR, as LoadCursor on Windows.
HCURSOR LoadCursor(HINSTANCE instance, LPCSTR name) {
    if (!instance && IS_INTRESOURCE(name)) {
        const ULONG_PTR id = reinterpret_cast<ULONG_PTR>(name);
        if (id >= 32512 && id <= 32651) return reinterpret_cast<HCURSOR>(id);
    }
    if (instance) {
        const fourstory::resources::Resource* r = fourstory::resources::find(name, MAKEINTRESOURCE(21));
        if (!r) r = fourstory::resources::find(name, RT_GROUP_CURSOR);
        State& s = state();
        fourstory::platform::CursorImage image;
        // Program cursors are shared, as on Windows: one handle per resource.
        static std::map<const fourstory::resources::Resource*, HCURSOR> loaded;
        if (r) { auto it = loaded.find(r); if (it != loaded.end()) return it->second; }
        if (r && s.backend && fourstory::platform::decode_cursor(r->data, r->size, &image)) {
            if (void* native = s.backend->create_cursor(image)) return loaded[r] = reinterpret_cast<HCURSOR>(new ProgramCursor{kCursorMagic, native});
            SetLastError(ERROR_NOT_SUPPORTED);
            return nullptr;
        }
    }
    SetLastError(1814);
    return nullptr;
}
HCURSOR LoadCursorFromFile(LPCSTR) { SetLastError(ERROR_NOT_SUPPORTED); return nullptr; }
HICON LoadIcon(HINSTANCE instance, LPCSTR name) {
    if (!instance && reinterpret_cast<ULONG_PTR>(name) == 32512) return reinterpret_cast<HICON>(static_cast<ULONG_PTR>(32512));
    SetLastError(1814);
    return nullptr;
}
// LoadCursor cursors are shared: DestroyCursor leaves them (Windows does the same).
BOOL DestroyCursor(HCURSOR) { return TRUE; }
BOOL DestroyIcon(HICON) { return TRUE; }
BOOL ClipCursor(const RECT*) { SetLastError(ERROR_NOT_SUPPORTED); return FALSE; }

// ---- System -------------------------------------------------------------------------
int GetSystemMetrics(int index) {
    State& s = state();
    int w = 0, h = 0;
    if (s.backend) s.backend->screen_size(&w, &h);
    switch (index) {
    case SM_CXSCREEN: case SM_CXFULLSCREEN: case SM_CXVIRTUALSCREEN: case SM_CXMAXIMIZED: return w;
    case SM_CYSCREEN: case SM_CYVIRTUALSCREEN: case SM_CYMAXIMIZED: return h;
    case SM_CYFULLSCREEN: return h - (s.backend ? s.backend->frame_caption_height() : 0);
    case SM_CYCAPTION: return s.backend ? s.backend->frame_caption_height() : 0;
    case SM_CXDOUBLECLK: case SM_CYDOUBLECLK: return 4;
    case SM_CXDRAG: case SM_CYDRAG: return 4;
    case SM_CMONITORS: return 1;
    case SM_MOUSEPRESENT: return 1;
    case SM_CMOUSEBUTTONS: return 3;
    case SM_MOUSEWHEELPRESENT: return 1;
    case SM_CXBORDER: case SM_CYBORDER: case SM_CXFRAME: case SM_CYFRAME: case SM_CXFIXEDFRAME: case SM_CYFIXEDFRAME: return 0;
    }
    return 0;
}
BOOL SystemParametersInfo(UINT action, UINT, PVOID data, UINT) {
    if (action == SPI_GETWORKAREA && data) {
        int w = 0, h = 0;
        if (state().backend) state().backend->screen_size(&w, &h);
        *static_cast<RECT*>(data) = {0, 0, w, h};
        return TRUE;
    }
    if (action == SPI_GETWHEELSCROLLLINES && data) { *static_cast<UINT*>(data) = 3; return TRUE; }
    SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
}
int MessageBox(HWND, LPCSTR text, LPCSTR caption, UINT type) {
    State& s = state();
    const std::string t = fourstory::platform::acp_to_utf8(text ? text : ""), c = fourstory::platform::acp_to_utf8(caption ? caption : "");
    std::fprintf(stderr, "MessageBox: [%s] %s\n", c.c_str(), t.c_str());
    if (s.backend) return s.backend->message_box(t.c_str(), c.c_str(), type);
    // No display: only a box with the single answer OK can be answered.
    if ((type & MB_TYPEMASK) == MB_OK) return IDOK;
    SetLastError(ERROR_NOT_SUPPORTED);
    return 0;
}
BOOL MessageBeep(UINT) { return TRUE; }

// ---- Clipboard and global memory ------------------------------------------------------
namespace {
struct Global { std::vector<char> bytes; int locks = 0; };
std::set<Global*>& globals() { static std::set<Global*> g; return g; }
bool g_clipboard_open = false;
}
HGLOBAL GlobalAlloc(UINT flags, SIZE_T bytes) {
    Global* g = new (std::nothrow) Global;
    if (!g) { SetLastError(ERROR_NOT_ENOUGH_MEMORY); return nullptr; }
    try { g->bytes.assign(static_cast<size_t>(bytes ? bytes : 1), 0); } catch (...) { delete g; SetLastError(ERROR_NOT_ENOUGH_MEMORY); return nullptr; }
    (void)flags;
    globals().insert(g);
    return g;
}
LPVOID GlobalLock(HGLOBAL memory) {
    Global* g = static_cast<Global*>(memory);
    if (!globals().count(g)) { SetLastError(ERROR_INVALID_HANDLE); return nullptr; }
    ++g->locks; return g->bytes.data();
}
BOOL GlobalUnlock(HGLOBAL memory) {
    Global* g = static_cast<Global*>(memory);
    if (!globals().count(g)) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
    if (g->locks > 0) --g->locks;
    if (g->locks == 0) SetLastError(0);
    return g->locks > 0 ? TRUE : FALSE;
}
HGLOBAL GlobalFree(HGLOBAL memory) {
    Global* g = static_cast<Global*>(memory);
    if (!memory) return nullptr;
    if (!globals().erase(g)) { SetLastError(ERROR_INVALID_HANDLE); return memory; }
    delete g; return nullptr;
}
SIZE_T GlobalSize(HGLOBAL memory) { Global* g = static_cast<Global*>(memory); return globals().count(g) ? g->bytes.size() : 0; }
BOOL OpenClipboard(HWND) { if (g_clipboard_open) { SetLastError(5); return FALSE; } g_clipboard_open = true; return TRUE; }
BOOL CloseClipboard() { if (!g_clipboard_open) { SetLastError(1418); return FALSE; } g_clipboard_open = false; return TRUE; }
BOOL EmptyClipboard() {
    if (!g_clipboard_open) { SetLastError(1418); return FALSE; } // ERROR_CLIPBOARD_NOT_OPEN
    return state().backend && state().backend->set_clipboard("") ? TRUE : FALSE;
}
HANDLE SetClipboardData(UINT format, HANDLE memory) {
    State& s = state();
    Global* g = static_cast<Global*>(memory);
    if (!g_clipboard_open) { SetLastError(1418); return nullptr; }
    if (format != CF_TEXT || !globals().count(g)) { SetLastError(ERROR_NOT_SUPPORTED); return nullptr; }
    std::string text(g->bytes.data(), strnlen(g->bytes.data(), g->bytes.size()));
    if (!s.backend || !s.backend->set_clipboard(fourstory::platform::acp_to_utf8(text.c_str()).c_str())) { SetLastError(ERROR_NOT_SUPPORTED); return nullptr; }
    GlobalFree(memory); // the system owns the data after a successful call
    return memory;
}
HANDLE GetClipboardData(UINT format) {
    State& s = state();
    if (!g_clipboard_open) { SetLastError(1418); return nullptr; }
    std::string utf8;
    if (format != CF_TEXT || !s.backend || !s.backend->clipboard(&utf8)) { SetLastError(ERROR_NOT_SUPPORTED); return nullptr; }
    const std::string text = fourstory::platform::utf8_to_acp(utf8.c_str());
    static HGLOBAL last = nullptr; // owned by the clipboard until the next call
    if (last) GlobalFree(last);
    last = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
    if (!last) return nullptr;
    std::memcpy(static_cast<Global*>(last)->bytes.data(), text.c_str(), text.size() + 1);
    return last;
}
BOOL IsClipboardFormatAvailable(UINT format) {
    std::string utf8;
    return format == CF_TEXT && state().backend && state().backend->clipboard(&utf8) && !utf8.empty() ? TRUE : FALSE;
}

// ---- Code pages -----------------------------------------------------------------------
UINT GetACP() { return active_code_page(); }
int MultiByteToWideChar(UINT code_page, DWORD, LPCSTR text, int bytes, WCHAR* wide, int wide_size) {
    if (!text || wide_size < 0 || (wide_size && !wide)) { SetLastError(ERROR_INVALID_PARAMETER); return 0; }
    const UINT page = resolve_page(code_page);
    std::u16string out;
    const size_t n = bytes < 0 ? std::strlen(text) + 1 : static_cast<size_t>(bytes);
    if (page == CP_UTF8) {
        std::string copy(text, n);
        size_t start = 0;
        for (;;) {
            const size_t zero = copy.find('\0', start);
            for (char32_t c : decode_utf8(copy.substr(start, zero == std::string::npos ? std::string::npos : zero - start).c_str())) {
                if (c >= 0x10000) { c -= 0x10000; out += static_cast<char16_t>(0xD800 + (c >> 10)); out += static_cast<char16_t>(0xDC00 + (c & 0x3FF)); }
                else out += static_cast<char16_t>(c);
            }
            if (zero == std::string::npos) break;
            out += u'\0'; start = zero + 1;
        }
    } else {
        const char16_t* table = code_page_table(page);
        for (size_t i = 0; i < n; ++i) {
            const BYTE b = static_cast<BYTE>(text[i]);
            // An undefined byte maps to the same code point, as Windows does for code page 1252.
            out += table[b] || b == 0 ? table[b] : static_cast<char16_t>(b);
        }
    }
    if (wide_size == 0) return static_cast<int>(out.size());
    if (static_cast<int>(out.size()) > wide_size) { SetLastError(ERROR_INSUFFICIENT_BUFFER); return 0; }
    std::memcpy(wide, out.data(), out.size() * sizeof(WCHAR));
    return static_cast<int>(out.size());
}
int WideCharToMultiByte(UINT code_page, DWORD, const WCHAR* wide, int wide_size, LPSTR text, int bytes, LPCSTR default_char, BOOL* used_default) {
    if (!wide || bytes < 0 || (bytes && !text)) { SetLastError(ERROR_INVALID_PARAMETER); return 0; }
    const UINT page = resolve_page(code_page);
    size_t n = 0;
    if (wide_size < 0) { while (wide[n]) ++n; ++n; } else n = static_cast<size_t>(wide_size);
    std::string out;
    if (used_default) *used_default = FALSE;
    if (page == CP_UTF8) {
        for (size_t i = 0; i < n; ++i) {
            char32_t c = wide[i];
            if (c >= 0xD800 && c < 0xDC00 && i + 1 < n && wide[i + 1] >= 0xDC00 && wide[i + 1] < 0xE000) { c = 0x10000 + ((c - 0xD800) << 10) + (wide[i + 1] - 0xDC00); ++i; }
            append_utf8(&out, c);
        }
    } else {
        const char16_t* table = code_page_table(page);
        const char fallback = default_char ? default_char[0] : '?';
        for (size_t i = 0; i < n; ++i) {
            const char16_t c = wide[i];
            int found = -1;
            if (c < 0x80) found = c;
            else for (int b = 0x80; b < 0x100; ++b) if (table[b] == c) { found = b; break; }
            if (found < 0 && page == 1252 && c >= 0x80 && c < 0x100) for (int b = 0x80; b < 0xA0; ++b) if (!cp1252_high[b - 0x80] && b == c) found = b;
            if (found < 0) { out += fallback; if (used_default) *used_default = TRUE; } else out += static_cast<char>(found);
        }
    }
    if (bytes == 0) return static_cast<int>(out.size());
    if (static_cast<int>(out.size()) > bytes) { SetLastError(ERROR_INSUFFICIENT_BUFFER); return 0; }
    std::memcpy(text, out.data(), out.size());
    return static_cast<int>(out.size());
}
BOOL IsDBCSLeadByte(BYTE) { return FALSE; }
BOOL IsDBCSLeadByteEx(UINT, BYTE) { return FALSE; }
// The thread locale is stored and returned. No native function depends on it.
namespace { thread_local LCID g_thread_locale = 0x0409; }
BOOL SetThreadLocale(LCID locale) { g_thread_locale = locale; return TRUE; }
LCID GetThreadLocale() { return g_thread_locale; }
LCID GetUserDefaultLCID() { return 0x0409; }
int GetLocaleInfo(LCID, DWORD type, LPSTR data, int size) {
    std::string value;
    if ((type & 0xFFFF) == LOCALE_IDEFAULTANSICODEPAGE) value = std::to_string(active_code_page());
    else { SetLastError(ERROR_NOT_SUPPORTED); return 0; }
    if (size == 0) return static_cast<int>(value.size() + 1);
    if (!data || size < static_cast<int>(value.size() + 1)) { SetLastError(ERROR_INSUFFICIENT_BUFFER); return 0; }
    std::memcpy(data, value.c_str(), value.size() + 1);
    return static_cast<int>(value.size() + 1);
}

// ---- Native window system interface -----------------------------------------------------
namespace fourstory { namespace platform {
std::string acp_to_utf8(const char* text) {
    std::string out;
    if (!text) return out;
    const char16_t* table = code_page_table(active_code_page());
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(text); *p; ++p) append_utf8(&out, table[*p] ? table[*p] : *p);
    return out;
}
std::string utf8_to_acp(const char* text) {
    std::u16string wide;
    for (char32_t c : decode_utf8(text)) wide += c < 0x10000 ? static_cast<char16_t>(c) : u'?';
    if (wide.empty()) return std::string();
    std::string out(wide.size(), '\0');
    const int n = WideCharToMultiByte(CP_ACP, 0, reinterpret_cast<const WCHAR*>(wide.data()), static_cast<int>(wide.size()), &out[0], static_cast<int>(out.size()), nullptr, nullptr);
    out.resize(static_cast<size_t>(std::max(0, n)));
    return out;
}
void set_backend(Backend* b) { std::lock_guard<std::recursive_mutex> guard(state().lock); state().backend = b; }
Backend* backend() { return state().backend; }
void* native_window(HWND window) { Window* w = get(window); return w ? w->native : nullptr; }
HWND window_from_native(void* native) {
    if (!native) return nullptr;
    for (Window* w : state().windows) if (w->native == native) return w;
    return nullptr;
}
void set_create_hook(CreateHook hook) { state().create_hook = hook; }

void input_key(HWND top, UINT vk, UINT scan_code, bool extended, bool down, bool repeat) {
    State& s = state();
    if (s.input_suspended || !get(top) || vk == 0 || vk > 0xFE) return;
    set_async(static_cast<int>(vk), down);
    UINT generic = vk;
    if (vk == VK_LSHIFT || vk == VK_RSHIFT) generic = VK_SHIFT;
    else if (vk == VK_LCONTROL || vk == VK_RCONTROL) generic = VK_CONTROL;
    else if (vk == VK_LMENU || vk == VK_RMENU) generic = VK_MENU;
    if (generic != vk) {
        const UINT other = vk == VK_LSHIFT ? VK_RSHIFT : vk == VK_RSHIFT ? VK_LSHIFT : vk == VK_LCONTROL ? VK_RCONTROL : vk == VK_RCONTROL ? VK_LCONTROL : vk == VK_LMENU ? VK_RMENU : VK_LMENU;
        // Each physical key sends its message; the generic key stays down while the other side is down.
        set_async(static_cast<int>(generic), down || (s.key_async[other] & 0x80));
    }
    if (scan_code == 0) scan_code = MapVirtualKey(vk, MAPVK_VK_TO_VSC);
    if (vk == VK_RCONTROL || vk == VK_RMENU) extended = true;
    const bool alt = (s.key_async[VK_MENU] & 0x80) != 0, ctrl = (s.key_async[VK_CONTROL] & 0x80) != 0;
    // Alt, F10 and Alt combinations without Ctrl are system keys.
    const bool system = generic == VK_MENU || vk == VK_F10 || (alt && !ctrl) || (!down && generic != VK_MENU && alt && !ctrl);
    LPARAM lp = 1 | (static_cast<LPARAM>(scan_code & 0xFF) << 16) | (extended ? (1LL << 24) : 0);
    if (alt && !ctrl) lp |= 1LL << 29;
    if (!down || repeat) lp |= 1LL << 30;
    if (!down) lp |= 1LL << 31;
    const UINT message = down ? (system ? WM_SYSKEYDOWN : WM_KEYDOWN) : (system ? WM_SYSKEYUP : WM_KEYUP);
    HWND target = get(s.focus) && top_level(s.focus) == top ? s.focus : (s.active == top ? top : top);
    { std::lock_guard<std::recursive_mutex> guard(s.lock); s.last_key_lparam = lp; }
    post_input(target, message, generic, lp);
    wake_gui();
}
void input_text(HWND top, const char* utf8) {
    State& s = state();
    if (s.input_suspended || !get(top) || !utf8) return;
    const std::string bytes = utf8_to_acp(utf8);
    HWND target = get(s.focus) && top_level(s.focus) == top ? s.focus : top;
    for (unsigned char c : bytes) {
        if (c < 0x20) continue;
        std::lock_guard<std::recursive_mutex> guard(s.lock);
        MSG m; make_msg(&m, target, WM_CHAR, c, (s.last_key_lparam & 0x00FFFFFF) | 1);
        s.input_queue.push_back(m);
    }
    wake_gui();
}
void input_mouse_move(HWND top, int x, int y) {
    if (state().input_suspended || !get(top)) return;
    POINT c; HWND target = mouse_target(top, x, y, &c);
    if (target) post_input(target, WM_MOUSEMOVE, mouse_key_flags(), point_lparam(c.x, c.y));
    wake_gui();
}
void input_mouse_button(HWND top, int button, bool down, int x, int y) {
    State& s = state();
    if (s.input_suspended || !get(top) || button < 0 || button > 2) return;
    static const int vks[3] = {VK_LBUTTON, VK_RBUTTON, VK_MBUTTON};
    static const UINT downs[3] = {WM_LBUTTONDOWN, WM_RBUTTONDOWN, WM_MBUTTONDOWN};
    static const UINT ups[3] = {WM_LBUTTONUP, WM_RBUTTONUP, WM_MBUTTONUP};
    static const UINT doubles[3] = {WM_LBUTTONDBLCLK, WM_RBUTTONDBLCLK, WM_MBUTTONDBLCLK};
    POINT c; HWND target = mouse_target(top, x, y, &c);
    set_async(vks[button], down);
    if (!target) return;
    UINT message = down ? downs[button] : ups[button];
    if (down) {
        // A second press of the same button on the same window within the
        // double-click time and SM_CXDOUBLECLK x SM_CYDOUBLECLK is a double click
        // when the window class has CS_DBLCLKS.
        const DWORD t = now_ticks();
        Window* w = get(target);
        const bool near = std::abs(s.cursor.x - s.last_click_point.x) <= 2 && std::abs(s.cursor.y - s.last_click_point.y) <= 2;
        if (w && (w->class_style & CS_DBLCLKS) && s.last_click_message == message && s.last_click_window == target && t - s.last_click_time <= GetDoubleClickTime() && near) {
            message = doubles[button];
            s.last_click_message = 0;
        } else {
            s.last_click_message = message; s.last_click_window = target; s.last_click_time = t; s.last_click_point = s.cursor;
        }
    }
    post_input(target, message, mouse_key_flags(), point_lparam(c.x, c.y));
    wake_gui();
}
void input_mouse_wheel(HWND top, float delta_y, float delta_x, int x, int y) {
    State& s = state();
    if (s.input_suspended || !get(top)) return;
    POINT c; HWND target = mouse_target(top, x, y, &c);
    HWND focus = get(s.focus) ? s.focus : top; // Windows sends wheel messages to the focus window
    (void)target;
    s.wheel_rest_y += delta_y * WHEEL_DELTA; s.wheel_rest_x += delta_x * WHEEL_DELTA;
    const int dy = static_cast<int>(s.wheel_rest_y), dx = static_cast<int>(s.wheel_rest_x);
    s.wheel_rest_y -= static_cast<float>(dy); s.wheel_rest_x -= static_cast<float>(dx);
    const LPARAM screen = point_lparam(s.cursor.x, s.cursor.y);
    if (dy) post_input(focus, WM_MOUSEWHEEL, MAKEWPARAM(static_cast<WORD>(mouse_key_flags()), static_cast<WORD>(static_cast<SHORT>(dy))), screen);
    if (dx) post_input(focus, WM_MOUSEHWHEEL, MAKEWPARAM(static_cast<WORD>(mouse_key_flags()), static_cast<WORD>(static_cast<SHORT>(dx))), screen);
    wake_gui();
}
void input_focus(HWND top, bool gained) {
    if (!get(top)) return;
    g_application_focused = gained;
    State& s = state();
    s.input_suspended = !gained;
    if (!gained) {
        // SDL pumps a batch before Win32 dispatch. Never replay a pre-switch
        // down/motion after activation has already released the game keys.
        std::lock_guard<std::recursive_mutex> guard(s.lock);
        s.input_queue.clear();
        std::memset(s.key_sync, 0, sizeof(s.key_sync));
        std::memset(s.key_async, 0, sizeof(s.key_async));
        s.wheel_rest_x = s.wheel_rest_y = 0;
        s.last_click_message = 0;
    }
    // Windows sends WM_ACTIVATEAPP before WM_ACTIVATE.
    SendMessage(top, WM_ACTIVATEAPP, gained ? TRUE : FALSE, 0);
    if (!get(top)) return;
    if (gained) {
        State& s = state();
        if (s.active != top) SetActiveWindow(top);
        else SendMessage(top, WM_ACTIVATE, MAKEWPARAM(WA_ACTIVE, IsIconic(top)), 0);
        if (!s.focus) SetFocus(top);
    } else {
        State& s = state();
        // Keys held while the focus leaves are released, as Windows reports them up.
        std::memset(s.key_async, 0, sizeof(s.key_async));
        // Another application takes the mouse capture.
        if (s.capture) ReleaseCapture();
        SendMessage(top, WM_ACTIVATE, MAKEWPARAM(WA_INACTIVE, IsIconic(top)), 0);
    }
}
void input_moved(HWND top, int x, int y) {
    Window* w = get(top);
    if (!w) return;
    const int width = w->rect.right - w->rect.left, height = w->rect.bottom - w->rect.top;
    w->rect = {x - w->client.left, y - w->client.top, x - w->client.left + width, y - w->client.top + height};
    SendMessage(top, WM_MOVE, 0, point_lparam(x, y));
}
void input_resized(HWND top, int width, int height) {
    Window* w = get(top);
    if (!w) return;
    RECT insets; frame_insets(w->style, &insets);
    w->rect.right = w->rect.left + width + insets.left + insets.right;
    w->rect.bottom = w->rect.top + height + insets.top + insets.bottom;
    w->client = {insets.left, insets.top, insets.left + width, insets.top + height};
    w->needs_paint = true;
    SendMessage(top, WM_SIZE, w->minimized ? SIZE_MINIMIZED : (w->maximized ? SIZE_MAXIMIZED : SIZE_RESTORED), MAKELPARAM(width, height));
}
void input_shown(HWND top, int command) {
    Window* w = get(top);
    if (!w) return;
    const int width = w->client.right - w->client.left, height = w->client.bottom - w->client.top;
    if (command == SW_MINIMIZE) { w->minimized = true; SendMessage(top, WM_SIZE, SIZE_MINIMIZED, 0); }
    else if (command == SW_MAXIMIZE) { w->minimized = false; w->maximized = true; SendMessage(top, WM_SIZE, SIZE_MAXIMIZED, MAKELPARAM(width, height)); }
    else { w->minimized = false; w->maximized = false; SendMessage(top, WM_SIZE, SIZE_RESTORED, MAKELPARAM(width, height)); }
}
void input_close(HWND top) { if (get(top)) PostMessage(top, WM_SYSCOMMAND, SC_CLOSE, 0); }
void input_quit() {
    State& s = state();
    // The application menu Quit of macOS closes every top-level window.
    std::vector<HWND> tops;
    for (Window* w : s.windows) if (!(w->style & WS_CHILD)) tops.push_back(w);
    for (HWND h : tops) PostMessage(h, WM_SYSCOMMAND, SC_CLOSE, 0);
}
}}
