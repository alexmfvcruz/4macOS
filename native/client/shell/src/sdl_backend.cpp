// SDL3 platform backend (C4-P). See include/fourstory_sdl_backend.h.
#include <SDL3/SDL.h>
#define FOURSTORY_KEYBOARD_OPTIONAL
#include "../../mac-keyboard/manager.h"
#include <fourstory_sdl_backend.h>
#include <fourstory_build_info.h>
#include <atomic>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

extern "C" void fourstory_display_mode_changed(HWND, int) __attribute__((weak_import));
extern "C" void fourstory_window_mode_saved(int) __attribute__((weak_import));
extern "C" void fourstory_display_focus_resumed(HWND) __attribute__((weak_import));
extern "C" void fourstory_display_focus_lost(HWND) __attribute__((weak_import));

namespace {
using fourstory::platform::WindowDesc;
Uint32 g_wake_event = 0;

bool reconcile_origin(HWND window, SDL_Window* native) {
    int x = 0, y = 0;
    if (!SDL_GetWindowPosition(native, &x, &y)) return false;
    fourstory::platform::input_moved(window, x, y);
    return true;
}


class SdlBackend : public fourstory::platform::Backend {
public:
    explicit SdlBackend(const fourstory::shell::SdlOptions& o) : options_(o) { load_script(); }
    ~SdlBackend() override { for (auto& c : cursors_) SDL_DestroyCursor(c.second); }
    const char* name() const override { return "sdl3"; }
    void* create_window(HWND hwnd, const WindowDesc& d) override {
        // Resizable is also Cocoa's prerequisite for the green fullscreen/Spaces control.
        SDL_WindowFlags flags = SDL_WINDOW_HIDDEN;
        if (options_.virtual_game_size) flags |= SDL_WINDOW_RESIZABLE;
        if (!d.decorated) flags |= SDL_WINDOW_BORDERLESS;
        if (d.topmost) flags |= SDL_WINDOW_ALWAYS_ON_TOP;
        if (options_.vulkan) flags |= SDL_WINDOW_VULKAN;
        if (options_.high_pixel_density) flags |= SDL_WINDOW_HIGH_PIXEL_DENSITY;
        SDL_Window* w = SDL_CreateWindow(d.title_utf8 ? d.title_utf8 : "", d.width > 0 ? d.width : 1, d.height > 0 ? d.height : 1, flags);
        if (!w) return nullptr;
        SDL_SetWindowPosition(w, d.x, d.y);
        SDL_StartTextInput(w);
        windows_[SDL_GetWindowID(w)] = hwnd;
        decorated_[SDL_GetWindowID(w)] = d.decorated;
        logical_[SDL_GetWindowID(w)] = {d.width > 0 ? d.width : 1, d.height > 0 ? d.height : 1};
        return w;
    }
    void destroy_window(void* native) override {
        SDL_Window* w = static_cast<SDL_Window*>(native);
        fourstory_build_overlay_detach(SDL_GetWindowID(w));
        windows_.erase(SDL_GetWindowID(w));
        decorated_.erase(SDL_GetWindowID(w));
        logical_.erase(SDL_GetWindowID(w));
        SDL_StopTextInput(w);
        SDL_DestroyWindow(w);
    }
    void set_title(void* native, const char* utf8) override { SDL_SetWindowTitle(static_cast<SDL_Window*>(native), utf8); }
    bool set_decorated(void* native, bool decorated) override {
        SDL_Window* w = static_cast<SDL_Window*>(native);
        // SDL ignores border changes in fullscreen. Remember the desired frame
        // and apply it once DXVK's asynchronous fullscreen exit has completed.
        if (!(SDL_GetWindowFlags(w) & SDL_WINDOW_FULLSCREEN) &&
            !SDL_SetWindowBordered(w, decorated)) return false;
        decorated_[SDL_GetWindowID(w)] = decorated;
        return true;
    }
    void set_client_rect(void* native, int x, int y, int width, int height) override {
        SDL_Window* w = static_cast<SDL_Window*>(native);
        SDL_SetWindowPosition(w, x, y);
        auto& size = logical_[SDL_GetWindowID(w)];
        const bool changed = size.first != width || size.second != height;
        size = {width > 0 ? width : 1, height > 0 ? height : 1};
        // A move-only compatibility call must not undo a native resize/Space.
        if (!options_.virtual_game_size || changed)
            SDL_SetWindowSize(w, size.first, size.second);
    }
    void show(void* native, int command) override {
        SDL_Window* w = static_cast<SDL_Window*>(native);
        switch (command) {
        case SW_HIDE: SDL_HideWindow(w); break;
        case SW_MINIMIZE: case SW_SHOWMINIMIZED: case SW_SHOWMINNOACTIVE: SDL_MinimizeWindow(w); break;
        case SW_MAXIMIZE: SDL_ShowWindow(w); SDL_MaximizeWindow(w); break;
        case SW_RESTORE: SDL_RestoreWindow(w); SDL_ShowWindow(w); break;
        default: SDL_ShowWindow(w); break;
        }
    }
    void raise(void* native) override { SDL_RaiseWindow(static_cast<SDL_Window*>(native)); }
    bool set_topmost(void* native, bool topmost) override { return SDL_SetWindowAlwaysOnTop(static_cast<SDL_Window*>(native), topmost); }
    void pump() override {
        run_script();
        SDL_Event e;
        while (SDL_PollEvent(&e)) translate(e);
        restore_decorations();
        update_build_labels();
    }
    void wait(DWORD milliseconds) override {
        SDL_Event e;
        const Sint32 ms = milliseconds > 0x7FFFFFFF ? -1 : static_cast<Sint32>(milliseconds);
        if (SDL_WaitEventTimeout(&e, ms)) translate(e);
        restore_decorations();
    }
    void wake() override {
        SDL_Event e;
        SDL_zero(e);
        e.type = g_wake_event;
        SDL_PushEvent(&e);
    }
    void capture_mouse(bool on) override { SDL_CaptureMouse(on); }
    void show_cursor(bool show) override { if (show) SDL_ShowCursor(); else SDL_HideCursor(); }
    void set_system_cursor(int idc) override {
        SDL_SystemCursor id = SDL_SYSTEM_CURSOR_DEFAULT;
        switch (idc) {
        case 32513: id = SDL_SYSTEM_CURSOR_TEXT; break;        // IDC_IBEAM
        case 32514: id = SDL_SYSTEM_CURSOR_WAIT; break;        // IDC_WAIT
        case 32515: id = SDL_SYSTEM_CURSOR_CROSSHAIR; break;   // IDC_CROSS
        case 32646: id = SDL_SYSTEM_CURSOR_MOVE; break;        // IDC_SIZEALL
        case 32648: id = SDL_SYSTEM_CURSOR_NOT_ALLOWED; break; // IDC_NO
        case 32649: id = SDL_SYSTEM_CURSOR_POINTER; break;     // IDC_HAND
        }
        SDL_Cursor*& c = cursors_[id];
        if (!c) c = SDL_CreateSystemCursor(id);
        if (c) SDL_SetCursor(c);
    }
    // Program cursors: one SDL color cursor, or an animated cursor (SDL 3.4) for .ani frames.
    void* create_cursor(const fourstory::platform::CursorImage& image) override {
        std::vector<SDL_Surface*> surfaces;
        for (const auto& frame : image.frames) {
            SDL_Surface* s = SDL_CreateSurface(image.width, image.height, SDL_PIXELFORMAT_RGBA32);
            if (!s) break;
            for (int y = 0; y < image.height; ++y)
                std::memcpy(static_cast<unsigned char*>(s->pixels) + y * s->pitch, frame.data() + static_cast<std::size_t>(y) * image.width * 4, static_cast<std::size_t>(image.width) * 4);
            surfaces.push_back(s);
        }
        SDL_Cursor* c = nullptr;
        if (!surfaces.empty() && surfaces.size() == image.frames.size()) {
            if (surfaces.size() == 1) c = SDL_CreateColorCursor(surfaces[0], image.hot_x, image.hot_y);
            else {
                std::vector<SDL_CursorFrameInfo> frames;
                for (std::size_t i = 0; i < surfaces.size(); ++i) frames.push_back({surfaces[i], i < image.durations_ms.size() ? image.durations_ms[i] : 100u});
                c = SDL_CreateAnimatedCursor(frames.data(), static_cast<int>(frames.size()), image.hot_x, image.hot_y);
            }
        }
        for (SDL_Surface* s : surfaces) SDL_DestroySurface(s);
        return c;
    }
    void set_cursor(void* native) override { SDL_SetCursor(static_cast<SDL_Cursor*>(native)); }
    void destroy_cursor(void* native) override { SDL_DestroyCursor(static_cast<SDL_Cursor*>(native)); }
    bool warp_mouse(void* native, int x, int y) override {
        SDL_Window* w = static_cast<SDL_Window*>(native);
        float fx = float(x), fy = float(y);
        map_point(w, fx, fy, true);
        SDL_WarpMouseInWindow(w, fx, fy);
        return true;
    }
    bool cursor_position(int* x, int* y) override {
        float fx = 0, fy = 0;
        SDL_GetGlobalMouseState(&fx, &fy);
        if (options_.virtual_game_size) {
            SDL_Window* w = SDL_GetKeyboardFocus();
            if (w && windows_.count(SDL_GetWindowID(w))) {
                int ox = 0, oy = 0; SDL_GetWindowPosition(w, &ox, &oy);
                fx -= ox; fy -= oy; map_point(w, fx, fy, false); fx += ox; fy += oy;
            }
        }
        *x = static_cast<int>(fx); *y = static_cast<int>(fy);
        return true;
    }
    void screen_size(int* width, int* height) override {
        SDL_Rect r = {0, 0, 0, 0};
        if (!SDL_GetDisplayBounds(SDL_GetPrimaryDisplay(), &r)) { r.w = 0; r.h = 0; }
        *width = r.w; *height = r.h;
    }
    int frame_caption_height() const override { return options_.caption_height; }
    bool set_clipboard(const char* utf8) override { return SDL_SetClipboardText(utf8 ? utf8 : ""); }
    bool clipboard(std::string* utf8) override {
        char* text = SDL_GetClipboardText();
        if (!text) return false;
        *utf8 = text;
        SDL_free(text);
        return true;
    }
    // Message boxes: SDL requires the main thread for its message box on macOS;
    // another thread gets 0 (failure), as Windows reports a box it cannot show.
    int message_box(const char* text, const char* caption, UINT type) override {
        if (!SDL_IsMainThread()) return 0;
        const UINT kind = type & MB_TYPEMASK;
        SDL_MessageBoxButtonData buttons[3];
        int count = 0, cancel = 0;
        auto add = [&](int id, const char* label, Uint32 flags) { buttons[count].flags = flags; buttons[count].buttonID = id; buttons[count].text = label; ++count; };
        switch (kind) {
        case MB_OK: add(IDOK, "OK", SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT | SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT); cancel = IDOK; break;
        case MB_OKCANCEL: add(IDOK, "OK", SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT); add(IDCANCEL, "Cancel", SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT); cancel = IDCANCEL; break;
        case MB_YESNO: add(IDYES, "Yes", SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT); add(IDNO, "No", SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT); cancel = IDNO; break;
        case MB_YESNOCANCEL: add(IDYES, "Yes", SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT); add(IDNO, "No", 0); add(IDCANCEL, "Cancel", SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT); cancel = IDCANCEL; break;
        case MB_RETRYCANCEL: add(IDRETRY, "Retry", SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT); add(IDCANCEL, "Cancel", SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT); cancel = IDCANCEL; break;
        case MB_ABORTRETRYIGNORE: add(IDABORT, "Abort", 0); add(IDRETRY, "Retry", SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT); add(IDIGNORE, "Ignore", 0); cancel = 0; break;
        default: SetLastError(ERROR_NOT_SUPPORTED); return 0;
        }
        SDL_MessageBoxData data;
        SDL_zero(data);
        data.flags = (type & MB_ICONMASK) == MB_ICONERROR ? SDL_MESSAGEBOX_ERROR : ((type & MB_ICONMASK) == MB_ICONWARNING ? SDL_MESSAGEBOX_WARNING : SDL_MESSAGEBOX_INFORMATION);
        data.title = caption; data.message = text; data.numbuttons = count; data.buttons = buttons;
        int pressed = -1;
        if (!SDL_ShowMessageBox(&data, &pressed)) return 0;
        // A box closed without a button counts as the Escape answer (0 when the type has none).
        return pressed < 0 ? cancel : pressed;
    }

private:
    void map_point(SDL_Window* w, float& x, float& y, bool inverse) const {
        if (!options_.virtual_game_size || !w) return;
        auto it = logical_.find(SDL_GetWindowID(w));
        int width = 0, height = 0;
        if (it == logical_.end() || !SDL_GetWindowSize(w, &width, &height) || width <= 0 || height <= 0) return;
        const double sx = double(it->second.first) / width, sy = double(it->second.second) / height;
        // Deliberately do not clamp: captured dragging can leave the window.
        x = inverse ? x / sx : x * sx; y = inverse ? y / sy : y * sy;
    }
    std::pair<int,int> game_point(SDL_WindowID id, float x, float y) const {
        map_point(SDL_GetWindowFromID(id), x, y, false);
        return {static_cast<int>(x), static_cast<int>(y)};
    }
    // Development build label (fourstory_build_info.h) in the bottom-right
    // corner of each game-sized window; dialogs and popups stay unlabelled.
    // pump() runs on every PeekMessage, so this checks at most 4 times a second.
    void update_build_labels() {
        const Uint64 now = SDL_GetTicks();
        if (now - labels_checked_ < 250) return;
        labels_checked_ = now;
        for (const auto& entry : windows_) {
            SDL_Window* w = SDL_GetWindowFromID(entry.first);
            int width = 0, height = 0;
            if (!w || !SDL_GetWindowSize(w, &width, &height) || width < 640 || height < 400) continue;
            void* ns = SDL_GetPointerProperty(SDL_GetWindowProperties(w), SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
            fourstory_build_overlay_update(entry.first, ns);
        }
    }
    Uint64 labels_checked_ = 0;
    void restore_decorations() {
        for (const auto& entry : decorated_) {
            SDL_Window* w = SDL_GetWindowFromID(entry.first);
            if (!w) continue;
            const SDL_WindowFlags flags = SDL_GetWindowFlags(w);
            if (!(flags & SDL_WINDOW_FULLSCREEN) &&
                bool(flags & SDL_WINDOW_BORDERLESS) == entry.second)
                SDL_SetWindowBordered(w, entry.second);
        }
    }
    // Test input script (C4): FOURSTORY_C4_INPUT_SCRIPT names a file with lines
    // "<ms> <action> [arguments]"; times count from the first pump. Actions on
    // the first top-level window: click X Y, move X Y, text UTF8, text-env
    // VARIABLE (text from the environment, logged as its length only), key VK,
    // focus 0|1 and resize W H (Win32-level input, as the backend reports it),
    // window-size W H, minimize, restore (real SDL window operations whose
    // events return through the event queue) and quit. Each action
    // is logged on stderr as "C4_INPUT <ms> <line>".
    struct Action { Uint64 at; std::string line; };
    std::vector<Action> script_;
    std::size_t next_ = 0;
    Uint64 start_ = 0;
    void load_script() {
        const char* path = std::getenv("FOURSTORY_C4_INPUT_SCRIPT");
        if (!path) return;
        std::ifstream in(path);
        std::string line;
        while (std::getline(in, line)) {
            if (line.empty() || line[0] == '#') continue;
            std::istringstream fields(line);
            Uint64 at = 0; fields >> at;
            std::string rest; std::getline(fields, rest);
            script_.push_back({at, rest.substr(rest.find_first_not_of(' ') == std::string::npos ? rest.size() : rest.find_first_not_of(' '))});
        }
        if (script_.empty()) std::fprintf(stderr, "C4_INPUT no actions in %s\n", path);
    }
    // Live commands (C5): FOURSTORY_C4_INPUT_LIVE names a file; lines appended to
    // it while the program runs ("<action> [arguments]", no time) run in order
    // at the next pump, logged as "C4_LIVE <line>". Same actions as the script;
    // use text-env for secrets (text logs its argument); wheel <delta> <x> <y> scrolls.
    // The first look starts at the end of the file (old lines are not
    // replayed); a file that became shorter is read again from its start.
    std::string live_path_;
    long long live_offset_ = -1;
    void run_live() {
        if (live_path_.empty()) { const char* p = std::getenv("FOURSTORY_C4_INPUT_LIVE"); if (!p) return; live_path_ = p; }
        std::ifstream in(live_path_);
        if (!in) return;
        in.seekg(0, std::ios::end);
        const long long size = static_cast<long long>(in.tellg());
        if (live_offset_ < 0) { live_offset_ = size; return; }
        if (size < live_offset_) live_offset_ = 0;
        if (size == live_offset_) return;
        in.seekg(live_offset_);
        std::string line;
        std::vector<std::string> lines;
        while (std::getline(in, line)) {
            if (in.eof()) break;  // incomplete last line: wait for its newline
            live_offset_ = static_cast<long long>(in.tellg());
            if (!line.empty() && line[0] != '#') lines.push_back(line);
        }
        std::size_t at = next_;  // keep the order of a batch
        for (const std::string& l : lines) {
            script_.insert(script_.begin() + static_cast<std::ptrdiff_t>(at++), Action{0, l});
            std::fprintf(stderr, "C4_LIVE %s\n", l.c_str());
        }
    }
    void run_script() {
        using namespace fourstory::platform;
        if (windows_.empty()) return;
        run_live();
        if (next_ >= script_.size()) return;
        const Uint64 now = SDL_GetTicks();
        if (!start_) start_ = now;
        const HWND w = windows_.begin()->second;
        SDL_Window* sdl = SDL_GetWindowFromID(windows_.begin()->first);
        while (next_ < script_.size() && now - start_ >= script_[next_].at) {
            const std::string& line = script_[next_].line;
            std::istringstream f(line);
            std::string action; f >> action;
            int a = 0, b = 0;
            if (action == "click") { f >> a >> b; input_mouse_move(w, a, b); input_mouse_button(w, 0, true, a, b); input_mouse_button(w, 0, false, a, b); }
            else if (action == "move") { f >> a >> b; input_mouse_move(w, a, b); }
            else if (action == "wheel") { float delta = 0; f >> delta >> a >> b; input_mouse_wheel(w, delta, 0, a, b); }
            else if (action == "text") { std::string text; std::getline(f, text); if (!text.empty() && text[0] == ' ') text.erase(0, 1); input_text(w, text.c_str()); }
            else if (action == "text-env") {
                // The text comes from an environment variable (a password); the log shows only its length.
                std::string name; f >> name; const char* value = std::getenv(name.c_str());
                if (value) input_text(w, value);
                std::fprintf(stderr, "C4_INPUT %llu text-env %s (%zu characters%s)\n", static_cast<unsigned long long>(now - start_), name.c_str(), value ? std::strlen(value) : 0, value ? "" : ", variable not set");
                ++next_; continue; }
            else if (action == "key") { f >> a; input_key(w, static_cast<UINT>(a), MapVirtualKey(static_cast<UINT>(a), MAPVK_VK_TO_VSC), false, true, false); input_key(w, static_cast<UINT>(a), MapVirtualKey(static_cast<UINT>(a), MAPVK_VK_TO_VSC), false, false, false); }
            else if (action == "focus") { f >> a; input_focus(w, a != 0); }
            else if (action == "resize") { f >> a >> b; input_resized(w, a, b); }
            else if (action == "window-size") { f >> a >> b; if (sdl) SDL_SetWindowSize(sdl, a, b); }
            else if (action == "minimize") { if (sdl) SDL_MinimizeWindow(sdl); }
            else if (action == "restore") { if (sdl) SDL_RestoreWindow(sdl); }
            else if (action == "quit") { input_quit(); }
            else { std::fprintf(stderr, "C4_INPUT unknown action: %s\n", line.c_str()); }
            std::fprintf(stderr, "C4_INPUT %llu %s\n", static_cast<unsigned long long>(now - start_), line.c_str());
            ++next_;
        }
    }
    HWND window_of(SDL_WindowID id) const { auto it = windows_.find(id); return it == windows_.end() ? nullptr : it->second; }
    void translate(const SDL_Event& e) {
        using namespace fourstory::platform;
        if (!script_.empty()) {  // test diagnostics with an input script only
            if (e.type == SDL_EVENT_KEY_DOWN) std::fprintf(stderr, "C4_SDL key down window %u\n", static_cast<unsigned>(e.key.windowID));
            if (e.type == SDL_EVENT_TEXT_INPUT) std::fprintf(stderr, "C4_SDL text (%zu bytes) window %u\n", std::strlen(e.text.text), static_cast<unsigned>(e.text.windowID));
            if (e.type == SDL_EVENT_WINDOW_FOCUS_GAINED || e.type == SDL_EVENT_WINDOW_FOCUS_LOST) std::fprintf(stderr, "C4_SDL focus %s\n", e.type == SDL_EVENT_WINDOW_FOCUS_GAINED ? "gained" : "lost");
            if (e.type == SDL_EVENT_WINDOW_RESIZED) std::fprintf(stderr, "C4_SDL resized %d %d\n", static_cast<int>(e.window.data1), static_cast<int>(e.window.data2));
            if (e.type == SDL_EVENT_WINDOW_MINIMIZED) std::fprintf(stderr, "C4_SDL minimized\n");
            if (e.type == SDL_EVENT_WINDOW_RESTORED) std::fprintf(stderr, "C4_SDL restored\n");
        }
        switch (e.type) {
        case SDL_EVENT_KEY_DOWN: case SDL_EVENT_KEY_UP: {
            HWND w = window_of(e.key.windowID);
            bool extended = false;
            const UINT vk = fourstory::shell::sdl_to_vk(e.key.scancode, e.key.key, &extended);
            if (w && e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat && vk>=VK_F1 && vk<=VK_F12 && fourstory_keyboard_observe) fourstory_keyboard_observe(int(vk-VK_F1+1));
            if (w && vk) input_key(w, vk, MapVirtualKey(vk, MAPVK_VK_TO_VSC), extended, e.type == SDL_EVENT_KEY_DOWN, e.key.repeat);
            break; }
        case SDL_EVENT_TEXT_INPUT: if (HWND w = window_of(e.text.windowID)) input_text(w, e.text.text); break;
        case SDL_EVENT_MOUSE_MOTION: if (HWND w = window_of(e.motion.windowID)) {
            auto p = game_point(e.motion.windowID, e.motion.x, e.motion.y); input_mouse_move(w, p.first, p.second);
        } break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN: case SDL_EVENT_MOUSE_BUTTON_UP: {
            HWND w = window_of(e.button.windowID);
            const int button = e.button.button == SDL_BUTTON_LEFT ? 0 : e.button.button == SDL_BUTTON_RIGHT ? 1 : e.button.button == SDL_BUTTON_MIDDLE ? 2 : -1;
            if (w && button >= 0) {
                auto p = game_point(e.button.windowID, e.button.x, e.button.y);
                input_mouse_button(w, button, e.type == SDL_EVENT_MOUSE_BUTTON_DOWN, p.first, p.second);
            }
            break; }
        case SDL_EVENT_MOUSE_WHEEL: {
            HWND w = window_of(e.wheel.windowID);
            const float sign = e.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -1.0f : 1.0f;
            // Windows: positive delta = wheel rotated away from the user, as SDL y > 0.
            if (w) {
                auto p = game_point(e.wheel.windowID, e.wheel.mouse_x, e.wheel.mouse_y);
                input_mouse_wheel(w, e.wheel.y * sign, e.wheel.x * sign, p.first, p.second);
            }
            break; }
        case SDL_EVENT_WINDOW_FOCUS_GAINED: if (HWND w = window_of(e.window.windowID)) {
            if (options_.virtual_game_size) {
                // Focus can return through a different Cocoa Space/window origin
                // without a MOVED event. Repair the same invariant as a mode
                // refresh before any activation/camera cursor queries resume.
                POINT before = {0,0}; ClientToScreen(w,&before);
                reconcile_origin(w, SDL_GetWindowFromID(e.window.windowID));
                POINT after = {0,0}; ClientToScreen(w,&after);
                std::fprintf(stderr,"DISPLAY_FOCUS origin=%d,%d -> %d,%d\n",
                    int(before.x),int(before.y),int(after.x),int(after.y));
            }
            if(fourstory_keyboard_focus)fourstory_keyboard_focus(1);
            input_focus(w, true);
            if (options_.virtual_game_size && fourstory_display_focus_resumed)
                fourstory_display_focus_resumed(w);
        } break;
        case SDL_EVENT_WINDOW_FOCUS_LOST: if (HWND w = window_of(e.window.windowID)) {
            if(fourstory_keyboard_focus)fourstory_keyboard_focus(0);
            input_focus(w, false);
            if (options_.virtual_game_size && fourstory_display_focus_lost)
                fourstory_display_focus_lost(w);
        } break;
        case SDL_EVENT_WINDOW_MOVED: if (HWND w = window_of(e.window.windowID)) input_moved(w, e.window.data1, e.window.data2); break;
        case SDL_EVENT_WINDOW_RESIZED: if (HWND w = window_of(e.window.windowID)) {
            auto size = logical_.at(e.window.windowID);
            input_resized(w, options_.virtual_game_size ? size.first : e.window.data1,
                          options_.virtual_game_size ? size.second : e.window.data2);
        } break;
        case SDL_EVENT_WINDOW_ENTER_FULLSCREEN: case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
            if (HWND w = window_of(e.window.windowID))
                if (fourstory_display_mode_changed)
                    fourstory_display_mode_changed(w, e.type == SDL_EVENT_WINDOW_ENTER_FULLSCREEN);
            // Release Settings.plist WindowedMode follows every change (C8).
            if (fourstory_window_mode_saved)
                fourstory_window_mode_saved(e.type == SDL_EVENT_WINDOW_ENTER_FULLSCREEN);
            break;
        case SDL_EVENT_WINDOW_MINIMIZED: if (HWND w = window_of(e.window.windowID)) input_shown(w, SW_MINIMIZE); break;
        case SDL_EVENT_WINDOW_MAXIMIZED: if (HWND w = window_of(e.window.windowID)) input_shown(w, SW_MAXIMIZE); break;
        case SDL_EVENT_WINDOW_RESTORED: if (HWND w = window_of(e.window.windowID)) input_shown(w, SW_RESTORE); break;
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED: if (HWND w = window_of(e.window.windowID)) input_close(w); break;
        case SDL_EVENT_QUIT: input_quit(); break;
        default: break;
        }
    }
    fourstory::shell::SdlOptions options_;
    std::map<SDL_WindowID, HWND> windows_;
    std::map<SDL_WindowID, bool> decorated_;
    std::map<SDL_WindowID, std::pair<int,int>> logical_;
    std::map<SDL_SystemCursor, SDL_Cursor*> cursors_;
};
SdlBackend* g_backend = nullptr;
}  // namespace

namespace fourstory { namespace shell {
bool game_fullscreen(HWND window) {
    auto* w = static_cast<SDL_Window*>(fourstory::platform::native_window(window));
    return w && (SDL_GetWindowFlags(w) & SDL_WINDOW_FULLSCREEN);
}
bool set_game_fullscreen(HWND window, bool fullscreen) {
    auto* w = static_cast<SDL_Window*>(fourstory::platform::native_window(window));
    if (!w) return false;
    // Start in fullscreen (C8: Settings.plist WindowedMode=false or --fullscreen;
    // FOURSTORY_START_FULLSCREEN=1): the first
    // mode request after device creation is turned into fullscreen. Both
    // callers (D3DDevice.cpp, TNetGM.cpp) read the real state back into the
    // game's option when the request "was not applied", so the game then
    // keeps fullscreen across later device resets.
    static bool start_fullscreen_checked = false;
    bool forced = false;
    if (!start_fullscreen_checked) {
        start_fullscreen_checked = true;
        const char* v = std::getenv("FOURSTORY_START_FULLSCREEN");
        if (v && std::strcmp(v, "1") == 0 && !fullscreen) { fullscreen = true; forced = true; }
    }
    // Never request a physical display mode. The renderer stretches the game's
    // selected resolution to the native Space and input maps back to it.
    if (!SDL_SetWindowFullscreenMode(w, nullptr)) return false;
    if (!SDL_SetWindowFullscreen(w, fullscreen)) return false;
    // A settings preview starts only after the native transition has settled.
    const bool synced = SDL_SyncWindow(w);
    // MoveWindow updates Win32's frame model even when Cocoa ignores a move
    // inside a fullscreen Space. No native MOVED event is generated in that
    // case, so explicitly reconcile the cursor's screen-coordinate origin.
    reconcile_origin(window, w);
    if (!synced) return false;
    if (game_fullscreen(window) != fullscreen)
        return SDL_SetError("The fullscreen request was not applied");
    if (forced) {
        std::fprintf(stderr, "4Story: starting in fullscreen (WindowedMode is false)\n");
        return false;
    }
    return true;
}
bool install_sdl_backend(const SdlOptions& options) {
    if (g_backend) return true;
    // Closing the last window must reach the program as WM_CLOSE only (the client
    // may refuse it); SDL_EVENT_QUIT then means the application Quit command.
    SDL_SetHint(SDL_HINT_QUIT_ON_LAST_WINDOW_CLOSE, "0");
    // The game uses absolute motion + cursor recentering. Automatic relative
    // warp emulation changes that contract and freezes global cursor queries.
    if (options.virtual_game_size)
        SDL_SetHintWithPriority(SDL_HINT_MOUSE_EMULATE_WARP_WITH_RELATIVE, "0", SDL_HINT_OVERRIDE);
    // Movement keys must repeat instead of opening the macOS accent picker.
    // This affects this process only; SDL text input/IME remains enabled.
    SDL_SetHintWithPriority(SDL_HINT_MAC_PRESS_AND_HOLD, "0", SDL_HINT_OVERRIDE);
    if (!SDL_Init(SDL_INIT_VIDEO)) return false;
    g_wake_event = SDL_RegisterEvents(1);
    if (g_wake_event == 0) { SDL_Quit(); return false; }
    g_backend = new SdlBackend(options);
    fourstory::platform::set_backend(g_backend);
    fourstory_build_label();  // hash the executable once, at startup on the main thread
    return true;
}
void uninstall_sdl_backend() {
    if (!g_backend) return;
    fourstory::platform::set_backend(nullptr);
    delete g_backend;
    g_backend = nullptr;
    SDL_Quit();
}

// Letters follow the key code (the layout); digits, function, editing,
// navigation and numeric-pad keys follow the scan code; punctuation keys use
// the US positions (VK_OEM_*), as Windows does for a US layout.
UINT sdl_to_vk(int scancode, unsigned int keycode, bool* extended) {
    bool ext = false;
    UINT vk = 0;
    if (keycode >= 'a' && keycode <= 'z') vk = keycode - 'a' + 'A';
    else if (scancode >= SDL_SCANCODE_A && scancode <= SDL_SCANCODE_Z) vk = 'A' + (scancode - SDL_SCANCODE_A);
    else if (scancode >= SDL_SCANCODE_1 && scancode <= SDL_SCANCODE_9) vk = '1' + (scancode - SDL_SCANCODE_1);
    else if (scancode == SDL_SCANCODE_0) vk = '0';
    else if (scancode >= SDL_SCANCODE_F1 && scancode <= SDL_SCANCODE_F12) vk = VK_F1 + (scancode - SDL_SCANCODE_F1);
    else if (scancode >= SDL_SCANCODE_F13 && scancode <= SDL_SCANCODE_F24) vk = VK_F13 + (scancode - SDL_SCANCODE_F13);
    else if (scancode >= SDL_SCANCODE_KP_1 && scancode <= SDL_SCANCODE_KP_9) vk = VK_NUMPAD1 + (scancode - SDL_SCANCODE_KP_1);
    else switch (scancode) {
    case SDL_SCANCODE_RETURN: vk = VK_RETURN; break;
    case SDL_SCANCODE_ESCAPE: vk = VK_ESCAPE; break;
    case SDL_SCANCODE_BACKSPACE: vk = VK_BACK; break;
    case SDL_SCANCODE_TAB: vk = VK_TAB; break;
    case SDL_SCANCODE_SPACE: vk = VK_SPACE; break;
    case SDL_SCANCODE_MINUS: vk = VK_OEM_MINUS; break;
    case SDL_SCANCODE_EQUALS: vk = VK_OEM_PLUS; break;
    case SDL_SCANCODE_LEFTBRACKET: vk = VK_OEM_4; break;
    case SDL_SCANCODE_RIGHTBRACKET: vk = VK_OEM_6; break;
    case SDL_SCANCODE_BACKSLASH: vk = VK_OEM_5; break;
    case SDL_SCANCODE_SEMICOLON: vk = VK_OEM_1; break;
    case SDL_SCANCODE_APOSTROPHE: vk = VK_OEM_7; break;
    case SDL_SCANCODE_GRAVE: vk = VK_OEM_3; break;
    case SDL_SCANCODE_COMMA: vk = VK_OEM_COMMA; break;
    case SDL_SCANCODE_PERIOD: vk = VK_OEM_PERIOD; break;
    case SDL_SCANCODE_SLASH: vk = VK_OEM_2; break;
    case SDL_SCANCODE_NONUSBACKSLASH: vk = VK_OEM_102; break;
    case SDL_SCANCODE_CAPSLOCK: vk = VK_CAPITAL; break;
    case SDL_SCANCODE_PRINTSCREEN: vk = VK_SNAPSHOT; ext = true; break;
    case SDL_SCANCODE_SCROLLLOCK: vk = VK_SCROLL; break;
    case SDL_SCANCODE_PAUSE: vk = VK_PAUSE; break;
    case SDL_SCANCODE_INSERT: vk = VK_INSERT; ext = true; break;
    case SDL_SCANCODE_HOME: vk = VK_HOME; ext = true; break;
    case SDL_SCANCODE_PAGEUP: vk = VK_PRIOR; ext = true; break;
    case SDL_SCANCODE_DELETE: vk = VK_DELETE; ext = true; break;
    case SDL_SCANCODE_END: vk = VK_END; ext = true; break;
    case SDL_SCANCODE_PAGEDOWN: vk = VK_NEXT; ext = true; break;
    case SDL_SCANCODE_RIGHT: vk = VK_RIGHT; ext = true; break;
    case SDL_SCANCODE_LEFT: vk = VK_LEFT; ext = true; break;
    case SDL_SCANCODE_DOWN: vk = VK_DOWN; ext = true; break;
    case SDL_SCANCODE_UP: vk = VK_UP; ext = true; break;
    case SDL_SCANCODE_NUMLOCKCLEAR: vk = VK_NUMLOCK; ext = true; break;
    case SDL_SCANCODE_KP_DIVIDE: vk = VK_DIVIDE; ext = true; break;
    case SDL_SCANCODE_KP_MULTIPLY: vk = VK_MULTIPLY; break;
    case SDL_SCANCODE_KP_MINUS: vk = VK_SUBTRACT; break;
    case SDL_SCANCODE_KP_PLUS: vk = VK_ADD; break;
    case SDL_SCANCODE_KP_ENTER: vk = VK_RETURN; ext = true; break;
    case SDL_SCANCODE_KP_0: vk = VK_NUMPAD0; break;
    case SDL_SCANCODE_KP_PERIOD: vk = VK_DECIMAL; break;
    case SDL_SCANCODE_APPLICATION: vk = VK_APPS; ext = true; break;
    case SDL_SCANCODE_LCTRL: vk = VK_LCONTROL; break;
    case SDL_SCANCODE_LSHIFT: vk = VK_LSHIFT; break;
    case SDL_SCANCODE_LALT: vk = VK_LMENU; break;          // Option
    case SDL_SCANCODE_LGUI: vk = VK_LWIN; ext = true; break; // Command
    case SDL_SCANCODE_RCTRL: vk = VK_RCONTROL; ext = true; break;
    case SDL_SCANCODE_RSHIFT: vk = VK_RSHIFT; break;
    case SDL_SCANCODE_RALT: vk = VK_RMENU; ext = true; break;
    case SDL_SCANCODE_RGUI: vk = VK_RWIN; ext = true; break;
    default: break;
    }
    if (extended) *extended = ext;
    return vk;
}
}}
