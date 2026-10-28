// Native MFC window classes (C4-P). Declarations: include/afxwin.h.
#include <afxwin.h>
#include <cerrno>
#include <map>
#include <mutex>
#include <string>
#include <sys/stat.h>
#include <vector>

// ---- Run-time classes ---------------------------------------------------------
IMPLEMENT_DYNAMIC(CCmdTarget, CObject)
IMPLEMENT_DYNAMIC(CWnd, CCmdTarget)
IMPLEMENT_DYNAMIC(CWinThread, CCmdTarget)
IMPLEMENT_DYNAMIC(CWinApp, CWinThread)
IMPLEMENT_DYNAMIC(CGdiObject, CObject)
IMPLEMENT_DYNAMIC(CFont, CGdiObject)
IMPLEMENT_DYNAMIC(CBitmap, CGdiObject)
IMPLEMENT_DYNAMIC(CBrush, CGdiObject)
IMPLEMENT_DYNAMIC(CDC, CObject)

namespace {
CWinApp* g_app = nullptr;
thread_local CWnd* g_creating = nullptr;
thread_local MSG g_current = {};       // the message of the innermost WindowProc (CWnd::Default)
std::map<HWND, CWnd*>& permanent() { static std::map<HWND, CWnd*> m; return m; }
std::map<HWND, CWnd*>& temporary() { static std::map<HWND, CWnd*> m; return m; }
std::map<HGDIOBJ, CFont*>& temporary_fonts() { static std::map<HGDIOBJ, CFont*> m; return m; }
std::map<HDC, CDC*>& temporary_dcs() { static std::map<HDC, CDC*> m; return m; }
std::map<UINT, std::string>& strings() { static std::map<UINT, std::string> m; return m; }

void attach_creating(HWND window) {
    CWnd* w = g_creating;
    if (!w) return;
    g_creating = nullptr;
    w->m_hWnd = window;
    permanent()[window] = w;
}
}  // namespace

// ---- CCmdTarget -------------------------------------------------------------------
const AFX_MSGMAP* CCmdTarget::GetMessageMap() const { return GetThisMessageMap(); }
const AFX_MSGMAP* CCmdTarget::GetThisMessageMap() {
    static const AFX_MSGMAP_ENTRY entries[] = { { 0, 0, 0, 0, AfxSig_end, nullptr } };
    static const AFX_MSGMAP map = { nullptr, &entries[0] };
    return &map;
}
// WM_COMMAND and WM_NOTIFY entries of the message maps.
BOOL CCmdTarget::OnCmdMsg(UINT id, int code, void* extra, void*) {
    for (const AFX_MSGMAP* map = GetMessageMap(); map; map = map->pfnGetBaseMap ? map->pfnGetBaseMap() : nullptr) {
        for (const AFX_MSGMAP_ENTRY* e = map->lpEntries; e->nSig != AfxSig_end; ++e) {
            if (e->nID > id || e->nLastID < id) continue;
            if (e->nSig == AfxSig_cmd_v && e->nMessage == WM_COMMAND && static_cast<int>(e->nCode) == code) {
                (this->*e->pfn)();
                return TRUE;
            }
            if (e->nSig == AfxSig_notify_v && e->nMessage == WM_NOTIFY && static_cast<int>(e->nCode) == code && extra) {
                auto* n = static_cast<std::pair<NMHDR*, LRESULT*>*>(extra);
                (this->*reinterpret_cast<void (CCmdTarget::*)(NMHDR*, LRESULT*)>(e->pfn))(n->first, n->second);
                return TRUE;
            }
        }
    }
    return FALSE;
}

// ---- CWnd ---------------------------------------------------------------------------
const CWnd CWnd::wndTop(HWND_TOP), CWnd::wndBottom(HWND_BOTTOM), CWnd::wndTopMost(HWND_TOPMOST), CWnd::wndNoTopMost(HWND_NOTOPMOST);
CWnd::CWnd() : m_hWnd(nullptr), m_pfnSuper(nullptr) {}
CWnd::CWnd(HWND special) : m_hWnd(special), m_pfnSuper(nullptr) {}
CWnd::~CWnd() {
    // wndTop, wndTopMost and the other special objects hold no window; they are
    // destroyed at exit, possibly after the handle maps.
    if (m_hWnd == HWND_TOP || m_hWnd == HWND_BOTTOM || m_hWnd == HWND_TOPMOST || m_hWnd == HWND_NOTOPMOST) return;
    if (m_hWnd && permanent().count(m_hWnd) && permanent()[m_hWnd] == this) {
        // MFC destroys the window of a CWnd that is deleted while it exists.
        HWND window = m_hWnd;
        if (::IsWindow(window)) ::DestroyWindow(window);
        permanent().erase(window);
    }
}
const AFX_MSGMAP* CWnd::GetMessageMap() const { return GetThisMessageMap(); }
const AFX_MSGMAP* CWnd::GetThisMessageMap() {
    typedef CWnd ThisClass; typedef CCmdTarget TheBaseClass;
    static const AFX_MSGMAP_ENTRY entries[] = {
        { WM_NCDESTROY, 0, 0, 0, FOURSTORY_MSG(AfxSig_v_v, void (CWnd::*)(), OnNcDestroy) },
        { 0, 0, 0, 0, AfxSig_end, nullptr } };
    static const AFX_MSGMAP map = { &TheBaseClass::GetThisMessageMap, &entries[0] };
    return &map;
}
CWnd* CWnd::FromHandlePermanent(HWND window) {
    auto it = permanent().find(window);
    return it == permanent().end() ? nullptr : it->second;
}
// A window that no CWnd owns gets a temporary CWnd, kept for the process (MFC
// frees temporary objects in idle time).
CWnd* CWnd::FromHandle(HWND window) {
    if (!window) return nullptr;
    if (CWnd* w = FromHandlePermanent(window)) return w;
    CWnd*& t = temporary()[window];
    if (!t) t = new CWnd(window);
    return t;
}
BOOL CWnd::Attach(HWND window) {
    if (!window || m_hWnd || permanent().count(window)) return FALSE;
    m_hWnd = window; permanent()[window] = this; return TRUE;
}
HWND CWnd::Detach() {
    HWND window = m_hWnd;
    if (window) permanent().erase(window);
    m_hWnd = nullptr;
    return window;
}
BOOL CWnd::PreCreateWindow(CREATESTRUCT&) { return TRUE; }
BOOL CWnd::Create(LPCTSTR class_name, LPCTSTR window_name, DWORD style, const RECT& rect, CWnd* parent, UINT id, void*) {
    return CreateEx(0, class_name, window_name, style | WS_CHILD, rect, parent, id);
}
BOOL CWnd::CreateEx(DWORD ex_style, LPCTSTR class_name, LPCTSTR window_name, DWORD style, const RECT& rect, CWnd* parent, UINT id, LPVOID param) {
    return CreateEx(ex_style, class_name, window_name, style, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
                    parent ? parent->m_hWnd : nullptr, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(id)), param);
}
BOOL CWnd::CreateEx(DWORD ex_style, LPCTSTR class_name, LPCTSTR window_name, DWORD style, int x, int y, int width, int height,
                    HWND parent, HMENU menu, LPVOID param) {
    CREATESTRUCT cs = {param, AfxGetInstanceHandle(), menu, parent, height, width, y, x, static_cast<LONG>(style), window_name, class_name, ex_style};
    if (!PreCreateWindow(cs)) { PostNcDestroy(); return FALSE; }
    if (!cs.lpszClass) cs.lpszClass = AfxRegisterWndClass(CS_DBLCLKS);
    fourstory::platform::set_create_hook(attach_creating);
    g_creating = this;
    HWND window = ::CreateWindowEx(cs.dwExStyle, cs.lpszClass, cs.lpszName, static_cast<DWORD>(cs.style), cs.x, cs.y, cs.cx, cs.cy,
                                   cs.hwndParent, cs.hMenu, cs.hInstance, cs.lpCreateParams);
    // The hook clears g_creating when it attached this object. After a failed
    // creation that reached the window, OnNcDestroy has detached the object and
    // PostNcDestroy may have deleted it: do not touch it.
    g_creating = nullptr;
    if (!window) return FALSE;
    return TRUE;
}
BOOL CWnd::DestroyWindow() {
    if (!m_hWnd) return FALSE;
    return ::DestroyWindow(m_hWnd);
}
BOOL CWnd::PreTranslateMessage(MSG*) { return FALSE; }
LRESULT CWnd::WindowProc(UINT message, WPARAM wparam, LPARAM lparam) {
    LRESULT result = 0;
    if (!OnWndMsg(message, wparam, lparam, &result)) result = DefWindowProc(message, wparam, lparam);
    return result;
}
LRESULT CWnd::DefWindowProc(UINT message, WPARAM wparam, LPARAM lparam) {
    if (m_pfnSuper) return ::CallWindowProc(m_pfnSuper, m_hWnd, message, wparam, lparam);
    return ::DefWindowProc(m_hWnd, message, wparam, lparam);
}
LRESULT CWnd::Default() { return DefWindowProc(g_current.message, g_current.wParam, g_current.lParam); }
void CWnd::PostNcDestroy() {}
BOOL CWnd::OnCommand(WPARAM wparam, LPARAM) { return OnCmdMsg(LOWORD(wparam), HIWORD(wparam), nullptr, nullptr); }
BOOL CWnd::OnNotify(WPARAM, LPARAM lparam, LRESULT* result) {
    NMHDR* header = reinterpret_cast<NMHDR*>(lparam);
    if (!header) return FALSE;
    std::pair<NMHDR*, LRESULT*> extra(header, result);
    return OnCmdMsg(static_cast<UINT>(header->idFrom), static_cast<int>(header->code), &extra, nullptr);
}

namespace {
CPoint lparam_point(LPARAM l) { return CPoint(GET_X_LPARAM(l), GET_Y_LPARAM(l)); }
}
// The message map walk and the argument decoding of each handler signature.
BOOL CWnd::OnWndMsg(UINT message, WPARAM w, LPARAM l, LRESULT* out) {
    LRESULT result = 0;
    if (message == WM_COMMAND) { if (OnCommand(w, l)) { if (out) *out = 0; return TRUE; } return FALSE; }
    if (message == WM_NOTIFY) { if (OnNotify(w, l, &result)) { if (out) *out = result; return TRUE; } return FALSE; }
    for (const AFX_MSGMAP* map = GetMessageMap(); map; map = map->pfnGetBaseMap ? map->pfnGetBaseMap() : nullptr) {
        for (const AFX_MSGMAP_ENTRY* e = map->lpEntries; e->nSig != AfxSig_end; ++e) {
            if (e->nMessage != message || e->nSig == AfxSig_cmd_v || e->nSig == AfxSig_notify_v) continue;
            CWnd* self = this;
            switch (e->nSig) {
            case AfxSig_v_v: (self->*reinterpret_cast<void (CWnd::*)()>(e->pfn))(); break;
            case AfxSig_i_CREATE: result = (self->*reinterpret_cast<int (CWnd::*)(LPCREATESTRUCT)>(e->pfn))(reinterpret_cast<LPCREATESTRUCT>(l)); break;
            case AfxSig_v_u_W_b: (self->*reinterpret_cast<void (CWnd::*)(UINT, CWnd*, BOOL)>(e->pfn))(LOWORD(w), CWnd::FromHandle(reinterpret_cast<HWND>(l)), HIWORD(w)); break;
            case AfxSig_v_u_u_u: (self->*reinterpret_cast<void (CWnd::*)(UINT, UINT, UINT)>(e->pfn))(static_cast<UINT>(w), LOWORD(l), HIWORD(l)); break;
            case AfxSig_l_p: result = (self->*reinterpret_cast<LRESULT (CWnd::*)(CPoint)>(e->pfn))(lparam_point(l)); break;
            case AfxSig_v_u_p: (self->*reinterpret_cast<void (CWnd::*)(UINT, CPoint)>(e->pfn))(static_cast<UINT>(w), lparam_point(l)); break;
            case AfxSig_b_u_s_p: {
                // WM_MOUSEWHEEL carries screen coordinates, as the handler receives them in MFC.
                result = (self->*reinterpret_cast<BOOL (CWnd::*)(UINT, short, CPoint)>(e->pfn))(LOWORD(w), static_cast<short>(HIWORD(w)), lparam_point(l));
                break; }
            case AfxSig_v_u_s_p: (self->*reinterpret_cast<void (CWnd::*)(UINT, short, CPoint)>(e->pfn))(LOWORD(w), static_cast<short>(HIWORD(w)), lparam_point(l)); break;
            case AfxSig_b_W_u_u: result = (self->*reinterpret_cast<BOOL (CWnd::*)(CWnd*, UINT, UINT)>(e->pfn))(CWnd::FromHandle(reinterpret_cast<HWND>(w)), LOWORD(l), HIWORD(l)); break;
            case AfxSig_v_up: (self->*reinterpret_cast<void (CWnd::*)(UINT_PTR)>(e->pfn))(static_cast<UINT_PTR>(w)); break;
            case AfxSig_v_W_p: (self->*reinterpret_cast<void (CWnd::*)(CWnd*, CPoint)>(e->pfn))(CWnd::FromHandle(reinterpret_cast<HWND>(w)), lparam_point(l)); break;
            case AfxSig_v_u_i_i: (self->*reinterpret_cast<void (CWnd::*)(UINT, int, int)>(e->pfn))(static_cast<UINT>(w), static_cast<short>(LOWORD(l)), static_cast<short>(HIWORD(l))); break;
            case AfxSig_v_i_i: (self->*reinterpret_cast<void (CWnd::*)(int, int)>(e->pfn))(static_cast<short>(LOWORD(l)), static_cast<short>(HIWORD(l))); break;
            case AfxSig_v_W: (self->*reinterpret_cast<void (CWnd::*)(CWnd*)>(e->pfn))(CWnd::FromHandle(reinterpret_cast<HWND>(w))); break;
            case AfxSig_v_b_dw: (self->*reinterpret_cast<void (CWnd::*)(BOOL, DWORD)>(e->pfn))(static_cast<BOOL>(w), static_cast<DWORD>(l)); break;
            case AfxSig_l_w_l: result = (self->*reinterpret_cast<LRESULT (CWnd::*)(WPARAM, LPARAM)>(e->pfn))(w, l); break;
            default:
                // An entry with a signature that this dispatcher does not decode.
                std::fprintf(stderr, "NOT SUPPORTED (native MFC): message map signature %llu for message 0x%04x\n",
                             static_cast<unsigned long long>(e->nSig), message);
                continue;
            }
            if (out) *out = result;
            return TRUE;
        }
    }
    return FALSE;
}

void CWnd::GetWindowText(CString& text) const {
    const int n = ::GetWindowTextLength(m_hWnd);
    std::vector<char> buffer(static_cast<size_t>(n) + 1, 0);
    ::GetWindowText(m_hWnd, buffer.data(), n + 1);
    text = buffer.data();
}
UINT CWnd::GetWindowedChildCount() {
    UINT count = 0;
    for (HWND c = ::GetWindow(m_hWnd, GW_CHILD); c; c = ::GetWindow(c, GW_HWNDNEXT)) ++count;
    return count;
}
CDC* CWnd::GetDC() { return CDC::FromHandle(::GetDC(m_hWnd)); }
int CWnd::ReleaseDC(CDC* dc) {
    if (!dc) return 0;
    HDC h = dc->m_hDC;
    auto it = temporary_dcs().find(h);
    if (it != temporary_dcs().end()) { it->second->Detach(); delete it->second; temporary_dcs().erase(it); }
    return ::ReleaseDC(m_hWnd, h);
}
HICON CWnd::SetIcon(HICON icon, BOOL) { ::SetWindowIcon(m_hWnd, icon); return nullptr; }
CWnd* CWnd::GetDlgItem(int id) const {
    for (HWND c = ::GetWindow(m_hWnd, GW_CHILD); c; c = ::GetWindow(c, GW_HWNDNEXT))
        if (::GetWindowLongPtr(c, GWLP_ID) == id) return FromHandle(c);
    return nullptr;
}

int CWnd::OnCreate(LPCREATESTRUCT) { return static_cast<int>(Default()); }
void CWnd::OnDestroy() { Default(); }
// MFC: the main window of the application ends the message loop when it is destroyed.
void CWnd::OnNcDestroy() {
    CWinThread* thread = AfxGetThread();
    if (thread && thread->m_pMainWnd == this) {
        thread->m_pMainWnd = nullptr;
        AfxPostQuitMessage(0);
    }
    Default();
    Detach();
    PostNcDestroy();
}
void CWnd::OnClose() { Default(); }
void CWnd::OnPaint() { Default(); }
void CWnd::OnActivate(UINT, CWnd*, BOOL) { Default(); }
void CWnd::OnActivateApp(BOOL, DWORD) { Default(); }
void CWnd::OnKeyDown(UINT, UINT, UINT) { Default(); }
void CWnd::OnKeyUp(UINT, UINT, UINT) { Default(); }
void CWnd::OnSysKeyDown(UINT, UINT, UINT) { Default(); }
void CWnd::OnSysKeyUp(UINT, UINT, UINT) { Default(); }
void CWnd::OnChar(UINT, UINT, UINT) { Default(); }
LRESULT CWnd::OnNcHitTest(CPoint) { return Default(); }
void CWnd::OnNcLButtonDown(UINT, CPoint) { Default(); }
void CWnd::OnNcRButtonDown(UINT, CPoint) { Default(); }
void CWnd::OnMouseMove(UINT, CPoint) { Default(); }
void CWnd::OnLButtonDown(UINT, CPoint) { Default(); }
void CWnd::OnLButtonUp(UINT, CPoint) { Default(); }
void CWnd::OnLButtonDblClk(UINT, CPoint) { Default(); }
void CWnd::OnRButtonDown(UINT, CPoint) { Default(); }
void CWnd::OnRButtonUp(UINT, CPoint) { Default(); }
void CWnd::OnRButtonDblClk(UINT, CPoint) { Default(); }
void CWnd::OnMButtonDown(UINT, CPoint) { Default(); }
void CWnd::OnMButtonUp(UINT, CPoint) { Default(); }
void CWnd::OnMButtonDblClk(UINT, CPoint) { Default(); }
BOOL CWnd::OnMouseWheel(UINT, short, CPoint) { return static_cast<BOOL>(Default()); }
void CWnd::OnMouseHWheel(UINT, short, CPoint) { Default(); }
BOOL CWnd::OnSetCursor(CWnd*, UINT, UINT) { return static_cast<BOOL>(Default()); }
void CWnd::OnTimer(UINT_PTR) { Default(); }
void CWnd::OnContextMenu(CWnd*, CPoint) { Default(); }
void CWnd::OnSize(UINT, int, int) { Default(); }
void CWnd::OnMove(int, int) { Default(); }
void CWnd::OnSetFocus(CWnd*) { Default(); }
void CWnd::OnKillFocus(CWnd*) { Default(); }

// The window procedure of every MFC window class.
LRESULT CALLBACK AfxWndProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    CWnd* w = CWnd::FromHandlePermanent(window);
    if (!w) return ::DefWindowProc(window, message, wparam, lparam);
    const MSG saved = g_current;
    g_current.hwnd = window; g_current.message = message; g_current.wParam = wparam; g_current.lParam = lparam;
    const LRESULT result = w->WindowProc(message, wparam, lparam);
    g_current = saved;
    return result;
}
LPCTSTR AfxRegisterWndClass(UINT class_style, HCURSOR cursor, HBRUSH background, HICON icon) {
    static std::map<std::string, std::string> names;
    char key[128];
    std::snprintf(key, sizeof(key), "Afx:%x:%llx:%llx:%llx", class_style, static_cast<unsigned long long>(reinterpret_cast<UINT_PTR>(cursor)),
                  static_cast<unsigned long long>(reinterpret_cast<UINT_PTR>(background)), static_cast<unsigned long long>(reinterpret_cast<UINT_PTR>(icon)));
    auto it = names.find(key);
    if (it != names.end()) return it->second.c_str();
    std::string& name = names[key] = key;
    WNDCLASS wc = {};
    wc.style = class_style; wc.lpfnWndProc = AfxWndProc; wc.hInstance = AfxGetInstanceHandle();
    wc.hCursor = cursor; wc.hbrBackground = background; wc.hIcon = icon; wc.lpszClassName = name.c_str();
    WNDCLASS existing;
    if (!::GetClassInfo(wc.hInstance, wc.lpszClassName, &existing)) ::RegisterClass(&wc);
    return name.c_str();
}


// ---- CWinThread, CWinApp ------------------------------------------------------------
const AFX_MSGMAP* CWinThread::GetMessageMap() const { return GetThisMessageMap(); }
const AFX_MSGMAP* CWinThread::GetThisMessageMap() {
    static const AFX_MSGMAP_ENTRY entries[] = { { 0, 0, 0, 0, AfxSig_end, nullptr } };
    static const AFX_MSGMAP map = { &CCmdTarget::GetThisMessageMap, &entries[0] };
    return &map;
}
const AFX_MSGMAP* CWinApp::GetMessageMap() const { return GetThisMessageMap(); }
const AFX_MSGMAP* CWinApp::GetThisMessageMap() {
    static const AFX_MSGMAP_ENTRY entries[] = { { 0, 0, 0, 0, AfxSig_end, nullptr } };
    static const AFX_MSGMAP map = { &CWinThread::GetThisMessageMap, &entries[0] };
    return &map;
}
BOOL CWinThread::PreTranslateMessage(MSG* msg) {
    for (HWND h = msg->hwnd; h; h = (::GetWindowLong(h, GWL_STYLE) & WS_CHILD) ? ::GetParent(h) : nullptr)
        if (CWnd* w = CWnd::FromHandlePermanent(h)) if (w->PreTranslateMessage(msg)) return TRUE;
    return FALSE;
}
BOOL CWinThread::PumpMessage() {
    if (!::GetMessage(&m_msgCur, nullptr, 0, 0)) return FALSE;
    if (!PreTranslateMessage(&m_msgCur)) { ::TranslateMessage(&m_msgCur); ::DispatchMessage(&m_msgCur); }
    return TRUE;
}
int CWinThread::Run() {
    LONG idle = 0;
    for (;;) {
        MSG peek;
        while (!::PeekMessage(&peek, nullptr, 0, 0, PM_NOREMOVE)) { if (!OnIdle(idle++)) { ::WaitMessage(); break; } }
        if (!PumpMessage()) return ExitInstance();
        idle = 0;
    }
}

CWinApp::CWinApp(LPCTSTR app_name) {
    if (app_name) { app_name_storage_ = app_name; m_pszAppName = app_name_storage_.c_str(); }
    g_app = this;
}
CWinApp::~CWinApp() { if (g_app == this) g_app = nullptr; }
void CWinApp::LoadStdProfileSettings(UINT) {}
void CWinApp::SetRegistryKey(LPCTSTR key) {
    registry_storage_ = key ? key : "";
    m_pszRegistryKey = registry_storage_.c_str();
    profile_storage_ = m_pszAppName ? m_pszAppName : "";
    m_pszProfileName = profile_storage_.c_str();
}
void CWinApp::SetRegistryKey(UINT id) { std::string s; fourstory::mfc::string_resource(id, &s); SetRegistryKey(s.c_str()); }
HICON CWinApp::LoadIcon(UINT id) const { return ::LoadIcon(m_hInstance, MAKEINTRESOURCE(id)); }
HICON CWinApp::LoadIcon(LPCTSTR name) const { return ::LoadIcon(m_hInstance, name); }
HCURSOR CWinApp::LoadCursor(UINT id) const { return ::LoadCursor(m_hInstance, MAKEINTRESOURCE(id)); }

// ---- Profile store --------------------------------------------------------------------
// The registry tree HKEY_CURRENT_USER\Software\<registry key>\<profile name>
// as a REGEDIT4 text file; every write rewrites the file (MFC writes the
// registry at once). Values are REG_SZ (bytes of the ANSI code page) or REG_DWORD.
// Location: $FOURSTORY_SETTINGS_DIR, else ~/Library/Application Support/4Story,
// then <registry key>/<profile name>/settings.reg.
namespace {
struct Value { bool dword = false; DWORD number = 0; std::string text; };
// The registry compares key and value names without regard to ASCII case.
struct NoCase { bool operator()(const std::string& a, const std::string& b) const { return strcasecmp(a.c_str(), b.c_str()) < 0; } };
typedef std::map<std::string, std::map<std::string, Value, NoCase>, NoCase> Store;   // section -> name -> value
struct Profile { std::string path; Store store; bool loaded = false; };
Profile& profile() { static Profile p; return p; }
std::recursive_mutex& profile_lock() { static std::recursive_mutex m; return m; }

std::string settings_dir() {
    if (const char* d = std::getenv("FOURSTORY_SETTINGS_DIR")) return d;
    const char* home = std::getenv("HOME");
    return std::string(home ? home : ".") + "/Library/Application Support/4Story";
}
std::string key_root() {
    CWinApp* app = AfxGetApp();
    std::string key = app && app->m_pszRegistryKey ? app->m_pszRegistryKey : "";
    std::string name = app && app->m_pszProfileName ? app->m_pszProfileName : (app && app->m_pszAppName ? app->m_pszAppName : "");
    return "HKEY_CURRENT_USER\\Software\\" + key + "\\" + name;
}
std::string store_path() {
    CWinApp* app = AfxGetApp();
    std::string key = app && app->m_pszRegistryKey ? app->m_pszRegistryKey : "";
    std::string name = app && app->m_pszProfileName ? app->m_pszProfileName : (app && app->m_pszAppName ? app->m_pszAppName : "");
    return settings_dir() + "/" + key + "/" + name + "/settings.reg";
}
bool make_dirs(const std::string& path) {
    for (size_t i = 1; i < path.size(); ++i)
        if (path[i] == '/') { std::string p = path.substr(0, i); if (mkdir(p.c_str(), 0755) != 0 && errno != EEXIST) return false; }
    return true;
}
std::string unescape(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) { out += s[++i]; continue; }
        out += s[i];
    }
    return out;
}
std::string escape(const std::string& s) {
    std::string out;
    for (char c : s) { if (c == '\\' || c == '"') out += '\\'; out += c; }
    return out;
}
// Reads REGEDIT4 and "Windows Registry Editor Version 5.00" (UTF-16 with BOM,
// as reg export writes; code units above 0xFF become '?').
void load_store(Profile& p) {
    p.store.clear();
    FILE* f = std::fopen(p.path.c_str(), "rb");
    if (!f) return;
    std::string bytes;
    char buffer[4096];
    size_t n;
    while ((n = std::fread(buffer, 1, sizeof(buffer), f)) > 0) bytes.append(buffer, n);
    std::fclose(f);
    if (bytes.size() >= 2 && static_cast<BYTE>(bytes[0]) == 0xFF && static_cast<BYTE>(bytes[1]) == 0xFE) {
        std::string narrow;
        for (size_t i = 2; i + 1 < bytes.size(); i += 2) {
            const unsigned unit = static_cast<BYTE>(bytes[i]) | (static_cast<BYTE>(bytes[i + 1]) << 8);
            narrow += unit <= 0xFF ? static_cast<char>(unit) : '?';
        }
        bytes.swap(narrow);
    }
    const std::string root = key_root();
    std::string section;
    bool in_root = false;
    size_t start = 0;
    while (start < bytes.size()) {
        size_t end = bytes.find('\n', start);
        std::string line = bytes.substr(start, end == std::string::npos ? std::string::npos : end - start);
        start = end == std::string::npos ? bytes.size() : end + 1;
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        if (line.size() > 2 && line.front() == '[' && line.back() == ']') {
            const std::string k = line.substr(1, line.size() - 2);
            in_root = k.size() > root.size() && k.compare(0, root.size() + 1, root + "\\") == 0;
            section = in_root ? k.substr(root.size() + 1) : "";
            continue;
        }
        if (!in_root || line.size() < 4 || line[0] != '"') continue;
        size_t q = 1;
        while (q < line.size() && line[q] != '"') q += line[q] == '\\' ? 2 : 1;
        if (q + 1 >= line.size() || line[q + 1] != '=') continue;
        const std::string name = unescape(line.substr(1, q - 1));
        const std::string rhs = line.substr(q + 2);
        Value v;
        if (rhs.compare(0, 6, "dword:") == 0) { v.dword = true; v.number = static_cast<DWORD>(std::strtoul(rhs.c_str() + 6, nullptr, 16)); }
        else if (rhs.size() >= 2 && rhs.front() == '"' && rhs.back() == '"') v.text = unescape(rhs.substr(1, rhs.size() - 2));
        else continue;
        p.store[section][name] = v;
    }
}
Profile& open_profile() {
    Profile& p = profile();
    const std::string path = store_path();
    if (!p.loaded || p.path != path) { p.path = path; load_store(p); p.loaded = true; }
    return p;
}
bool save_store(const Profile& p) {
    if (!make_dirs(p.path)) return false;
    const std::string temp = p.path + ".tmp";
    FILE* f = std::fopen(temp.c_str(), "wb");
    if (!f) return false;
    const std::string root = key_root();
    std::fprintf(f, "REGEDIT4\r\n\r\n[%s]\r\n\r\n", root.c_str());
    for (const auto& section : p.store) {
        std::fprintf(f, "[%s\\%s]\r\n", root.c_str(), section.first.c_str());
        for (const auto& entry : section.second) {
            if (entry.second.dword) std::fprintf(f, "\"%s\"=dword:%08x\r\n", escape(entry.first).c_str(), entry.second.number);
            else std::fprintf(f, "\"%s\"=\"%s\"\r\n", escape(entry.first).c_str(), escape(entry.second.text).c_str());
        }
        std::fprintf(f, "\r\n");
    }
    const bool ok = std::fflush(f) == 0 && std::fclose(f) == 0;
    return ok && std::rename(temp.c_str(), p.path.c_str()) == 0;
}
}  // namespace

CString CWinApp::GetProfileString(LPCTSTR section, LPCTSTR entry, LPCTSTR default_value) {
    std::lock_guard<std::recursive_mutex> guard(profile_lock());
    if (!section || !entry) return CString(default_value ? default_value : "");
    Profile& p = open_profile();
    auto s = p.store.find(section);
    if (s != p.store.end()) {
        auto e = s->second.find(entry);
        if (e != s->second.end() && !e->second.dword) return CString(e->second.text.c_str());
    }
    return CString(default_value ? default_value : "");
}
// A REG_DWORD value; any other type gives the default, as MFC's registry path does.
UINT CWinApp::GetProfileInt(LPCTSTR section, LPCTSTR entry, int default_value) {
    std::lock_guard<std::recursive_mutex> guard(profile_lock());
    if (!section || !entry) return static_cast<UINT>(default_value);
    Profile& p = open_profile();
    auto s = p.store.find(section);
    if (s != p.store.end()) {
        auto e = s->second.find(entry);
        if (e != s->second.end() && e->second.dword) return e->second.number;
    }
    return static_cast<UINT>(default_value);
}
// REGEDIT4 text has no escape for line breaks: such a value is refused. A
// failed save leaves the store as it was.
BOOL CWinApp::WriteProfileString(LPCTSTR section, LPCTSTR entry, LPCTSTR value) {
    std::lock_guard<std::recursive_mutex> guard(profile_lock());
    if (!section) return FALSE;
    if ((value && std::strpbrk(value, "\r\n")) || (entry && std::strpbrk(entry, "\r\n\"")) || std::strpbrk(section, "\r\n[]")) { SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
    Profile& p = open_profile();
    const Store saved = p.store;
    if (!entry) p.store.erase(section);                 // MFC: a NULL entry deletes the section
    else if (!value) { auto s = p.store.find(section); if (s != p.store.end()) s->second.erase(entry); }
    else { Value v; v.text = value; p.store[section][entry] = v; }
    if (save_store(p)) return TRUE;
    p.store = saved;
    return FALSE;
}
BOOL CWinApp::WriteProfileInt(LPCTSTR section, LPCTSTR entry, int value) {
    std::lock_guard<std::recursive_mutex> guard(profile_lock());
    if (!section || !entry || std::strpbrk(entry, "\r\n\"") || std::strpbrk(section, "\r\n[]")) { SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
    Profile& p = open_profile();
    const Store saved = p.store;
    Value v; v.dword = true; v.number = static_cast<DWORD>(value);
    p.store[section][entry] = v;
    if (save_store(p)) return TRUE;
    p.store = saved;
    return FALSE;
}

// ---- GDI wrapper objects ------------------------------------------------------------------
CFont* CFont::FromHandle(HFONT font) {
    if (!font) return nullptr;
    CFont*& f = temporary_fonts()[font];
    if (!f) { f = new CFont; f->m_hObject = font; }
    return f;
}
BOOL CFont::CreatePointFont(int point_tenths, LPCTSTR face, CDC*) {
    LOGFONT lf = {};
    lf.lfHeight = -MulDiv(point_tenths, 96, 720);
    if (face) strncpy_s(lf.lfFaceName, face, _TRUNCATE);
    return CreateFontIndirect(&lf);
}
BOOL CBitmap::CreateCompatibleBitmap(CDC* dc, int width, int height) { return Attach(::CreateCompatibleBitmap(dc ? dc->m_hDC : nullptr, width, height)); }
CDC* CDC::FromHandle(HDC dc) {
    if (!dc) return nullptr;
    CDC*& d = temporary_dcs()[dc];
    if (!d) { d = new CDC; d->m_hDC = d->m_hAttribDC = dc; }
    return d;
}
CBitmap* CDC::SelectObject(CBitmap* bitmap) {
    HGDIOBJ old = ::SelectObject(m_hDC, bitmap ? bitmap->m_hObject : nullptr);
    if (!old) return nullptr;
    static std::map<HGDIOBJ, CBitmap*> wrappers;
    CBitmap*& b = wrappers[old];
    if (!b) { b = new CBitmap; b->m_hObject = old; }
    return b;
}

// ---- Global functions --------------------------------------------------------------------
CWinApp* AfxGetApp() { return g_app; }
CWinThread* AfxGetThread() { return g_app; }
CWnd* AfxGetMainWnd() { return g_app ? g_app->GetMainWnd() : nullptr; }
HINSTANCE AfxGetInstanceHandle() { return g_app ? g_app->m_hInstance : nullptr; }
HINSTANCE AfxGetResourceHandle() { return AfxGetInstanceHandle(); }
LPCTSTR AfxGetAppName() { return g_app ? g_app->m_pszAppName : nullptr; }
void AfxPostQuitMessage(int exit_code) { ::PostQuitMessage(exit_code); }
int AfxMessageBox(LPCTSTR text, UINT type, UINT) {
    CWnd* main = AfxGetMainWnd();
    return ::MessageBox(main ? main->m_hWnd : nullptr, text, AfxGetAppName(), type);
}
int AfxMessageBox(UINT id, UINT type, UINT help) {
    std::string text;
    fourstory::mfc::string_resource(id, &text);
    return AfxMessageBox(text.c_str(), type, help);
}
// Winsock needs no start-up on macOS.
BOOL AfxSocketInit(void*) { return TRUE; }
void AfxEnableControlContainer(void*) {}

// MFC start-up: the application object exists before main (a global CWinApp).
int AfxWinMain(HINSTANCE instance, HINSTANCE, LPTSTR command_line, int show) {
    CWinApp* app = AfxGetApp();
    if (!app) return -1;
    app->m_hInstance = instance;
    app->m_lpCmdLine = command_line;
    app->m_nCmdShow = show;
    if (!app->m_pszAppName) {
        static std::string name;
        if (!fourstory::mfc::string_resource(AFX_IDS_APP_TITLE, &name)) name = "TClient";
        app->m_pszAppName = name.c_str();
    }
    if (!app->m_pszExeName) app->m_pszExeName = app->m_pszAppName;
    if (!app->InitApplication()) return -1;
    if (!app->InitInstance()) {
        if (app->m_pMainWnd) app->m_pMainWnd->DestroyWindow();
        return app->ExitInstance();
    }
    return app->Run();
}

namespace fourstory { namespace mfc {
void set_string_resource(UINT id, const char* text) { strings()[id] = text ? text : ""; }
bool string_resource(UINT id, std::string* text) {
    auto it = strings().find(id);
    if (it != strings().end()) { if (text) *text = it->second; return true; }
    // STRINGTABLE of the resource table (kernel32 resources, C4).
    if (const fourstory::resources::Resource* r = fourstory::resources::find(MAKEINTRESOURCE(id), RT_STRING)) {
        if (text) text->assign(reinterpret_cast<const char*>(r->data), r->size);
        return true;
    }
    return false;
}
std::string profile_store_path() { return store_path(); }
}}

BOOL CString::LoadString(UINT id) {
    std::string text;
    if (!fourstory::mfc::string_resource(id, &text)) { *this = ""; return FALSE; }
    *this = text.c_str();
    return TRUE;
}
