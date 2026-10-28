// Native MFC window classes (C4-P): CCmdTarget message maps, CWnd, CWinThread,
// CWinApp with the profile store, CGdiObject/CFont/CBitmap/CBrush, CDC and
// AfxWinMain. Implementation: native/client/compat/src/mfc.cpp. Message maps
// follow MFC: the entry holds a member pointer checked against the handler
// signature at compile time; CWnd::OnWndMsg walks the maps from the most
// derived class to CWnd and calls DefWindowProc for an unhandled message.
// Differences from MFC: compat/README.md, section "Window system (C4-P)".
#pragma once
#define __AFXWIN_H__
#include "afx.h"
// MFC calls GetSafeHwnd, GetSafeHandle and GetSafeHdc through null pointers
// and tests "this". That is undefined in C++; the native build compiles with
// -fno-delete-null-pointer-checks so that the test is kept (README).
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wundefined-bool-conversion"
#pragma clang diagnostic ignored "-Wtautological-undefined-compare"
#endif

class CWnd;
class CDC;
class CWinApp;
class CCmdTarget;

// ---- Message maps -------------------------------------------------------------
typedef void (CCmdTarget::*AFX_PMSG)();
enum AfxSig {
    AfxSig_end = 0, AfxSig_v_v,         // void ()
    AfxSig_i_CREATE,                    // int (LPCREATESTRUCT)
    AfxSig_v_u_W_b,                     // void (UINT, CWnd*, BOOL)       WM_ACTIVATE
    AfxSig_v_u_u_u,                     // void (UINT, UINT, UINT)        key messages
    AfxSig_l_p,                         // LRESULT (CPoint)               WM_NCHITTEST
    AfxSig_v_u_p,                       // void (UINT, CPoint)            mouse, NC mouse
    AfxSig_b_u_s_p,                     // BOOL (UINT, short, CPoint)     WM_MOUSEWHEEL
    AfxSig_v_u_s_p,                     // void (UINT, short, CPoint)     WM_MOUSEHWHEEL
    AfxSig_b_W_u_u,                     // BOOL (CWnd*, UINT, UINT)       WM_SETCURSOR
    AfxSig_v_up,                        // void (UINT_PTR)                WM_TIMER
    AfxSig_v_W_p,                       // void (CWnd*, CPoint)           WM_CONTEXTMENU
    AfxSig_v_u_i_i,                     // void (UINT, int, int)          WM_SIZE
    AfxSig_v_i_i,                       // void (int, int)                WM_MOVE
    AfxSig_v_W,                         // void (CWnd*)                   WM_SETFOCUS, WM_KILLFOCUS
    AfxSig_v_b_dw,                      // void (BOOL, DWORD)             WM_ACTIVATEAPP
    AfxSig_l_w_l,                       // LRESULT (WPARAM, LPARAM)       ON_MESSAGE
    AfxSig_cmd_v,                       // void ()                        WM_COMMAND notification
    AfxSig_notify_v,                    // void (NMHDR*, LRESULT*)        WM_NOTIFY
};
struct AFX_MSGMAP_ENTRY { UINT nMessage, nCode, nID, nLastID; UINT_PTR nSig; AFX_PMSG pfn; };
struct AFX_MSGMAP { const AFX_MSGMAP* (*pfnGetBaseMap)(); const AFX_MSGMAP_ENTRY* lpEntries; };

#define DECLARE_MESSAGE_MAP() \
protected: static const AFX_MSGMAP* GetThisMessageMap(); virtual const AFX_MSGMAP* GetMessageMap() const;
#define BEGIN_MESSAGE_MAP(theClass, baseClass) \
    const AFX_MSGMAP* theClass::GetMessageMap() const { return GetThisMessageMap(); } \
    const AFX_MSGMAP* theClass::GetThisMessageMap() { \
        typedef theClass ThisClass; typedef baseClass TheBaseClass; (void)sizeof(ThisClass); \
        static const AFX_MSGMAP_ENTRY _messageEntries[] = {
#define END_MESSAGE_MAP() \
        { 0, 0, 0, 0, AfxSig_end, (AFX_PMSG)0 } }; \
        static const AFX_MSGMAP messageMap = { &TheBaseClass::GetThisMessageMap, &_messageEntries[0] }; \
        return &messageMap; }

#define FOURSTORY_MSG(sig, type, fn) static_cast<UINT_PTR>(sig), reinterpret_cast<AFX_PMSG>(static_cast<type>(&ThisClass::fn))
#define ON_WM_CREATE() { WM_CREATE, 0, 0, 0, FOURSTORY_MSG(AfxSig_i_CREATE, int (CWnd::*)(LPCREATESTRUCT), OnCreate) },
#define ON_WM_DESTROY() { WM_DESTROY, 0, 0, 0, FOURSTORY_MSG(AfxSig_v_v, void (CWnd::*)(), OnDestroy) },
#define ON_WM_CLOSE() { WM_CLOSE, 0, 0, 0, FOURSTORY_MSG(AfxSig_v_v, void (CWnd::*)(), OnClose) },
#define ON_WM_PAINT() { WM_PAINT, 0, 0, 0, FOURSTORY_MSG(AfxSig_v_v, void (CWnd::*)(), OnPaint) },
#define ON_WM_ACTIVATE() { WM_ACTIVATE, 0, 0, 0, FOURSTORY_MSG(AfxSig_v_u_W_b, void (CWnd::*)(UINT, CWnd*, BOOL), OnActivate) },
#define ON_WM_ACTIVATEAPP() { WM_ACTIVATEAPP, 0, 0, 0, FOURSTORY_MSG(AfxSig_v_b_dw, void (CWnd::*)(BOOL, DWORD), OnActivateApp) },
#define ON_WM_KEYDOWN() { WM_KEYDOWN, 0, 0, 0, FOURSTORY_MSG(AfxSig_v_u_u_u, void (CWnd::*)(UINT, UINT, UINT), OnKeyDown) },
#define ON_WM_KEYUP() { WM_KEYUP, 0, 0, 0, FOURSTORY_MSG(AfxSig_v_u_u_u, void (CWnd::*)(UINT, UINT, UINT), OnKeyUp) },
#define ON_WM_SYSKEYDOWN() { WM_SYSKEYDOWN, 0, 0, 0, FOURSTORY_MSG(AfxSig_v_u_u_u, void (CWnd::*)(UINT, UINT, UINT), OnSysKeyDown) },
#define ON_WM_SYSKEYUP() { WM_SYSKEYUP, 0, 0, 0, FOURSTORY_MSG(AfxSig_v_u_u_u, void (CWnd::*)(UINT, UINT, UINT), OnSysKeyUp) },
#define ON_WM_CHAR() { WM_CHAR, 0, 0, 0, FOURSTORY_MSG(AfxSig_v_u_u_u, void (CWnd::*)(UINT, UINT, UINT), OnChar) },
#define ON_WM_NCHITTEST() { WM_NCHITTEST, 0, 0, 0, FOURSTORY_MSG(AfxSig_l_p, LRESULT (CWnd::*)(CPoint), OnNcHitTest) },
#define ON_WM_NCLBUTTONDOWN() { WM_NCLBUTTONDOWN, 0, 0, 0, FOURSTORY_MSG(AfxSig_v_u_p, void (CWnd::*)(UINT, CPoint), OnNcLButtonDown) },
#define ON_WM_NCRBUTTONDOWN() { WM_NCRBUTTONDOWN, 0, 0, 0, FOURSTORY_MSG(AfxSig_v_u_p, void (CWnd::*)(UINT, CPoint), OnNcRButtonDown) },
#define ON_WM_MOUSEMOVE() { WM_MOUSEMOVE, 0, 0, 0, FOURSTORY_MSG(AfxSig_v_u_p, void (CWnd::*)(UINT, CPoint), OnMouseMove) },
#define ON_WM_LBUTTONDOWN() { WM_LBUTTONDOWN, 0, 0, 0, FOURSTORY_MSG(AfxSig_v_u_p, void (CWnd::*)(UINT, CPoint), OnLButtonDown) },
#define ON_WM_LBUTTONUP() { WM_LBUTTONUP, 0, 0, 0, FOURSTORY_MSG(AfxSig_v_u_p, void (CWnd::*)(UINT, CPoint), OnLButtonUp) },
#define ON_WM_LBUTTONDBLCLK() { WM_LBUTTONDBLCLK, 0, 0, 0, FOURSTORY_MSG(AfxSig_v_u_p, void (CWnd::*)(UINT, CPoint), OnLButtonDblClk) },
#define ON_WM_RBUTTONDOWN() { WM_RBUTTONDOWN, 0, 0, 0, FOURSTORY_MSG(AfxSig_v_u_p, void (CWnd::*)(UINT, CPoint), OnRButtonDown) },
#define ON_WM_RBUTTONUP() { WM_RBUTTONUP, 0, 0, 0, FOURSTORY_MSG(AfxSig_v_u_p, void (CWnd::*)(UINT, CPoint), OnRButtonUp) },
#define ON_WM_RBUTTONDBLCLK() { WM_RBUTTONDBLCLK, 0, 0, 0, FOURSTORY_MSG(AfxSig_v_u_p, void (CWnd::*)(UINT, CPoint), OnRButtonDblClk) },
#define ON_WM_MBUTTONDOWN() { WM_MBUTTONDOWN, 0, 0, 0, FOURSTORY_MSG(AfxSig_v_u_p, void (CWnd::*)(UINT, CPoint), OnMButtonDown) },
#define ON_WM_MBUTTONUP() { WM_MBUTTONUP, 0, 0, 0, FOURSTORY_MSG(AfxSig_v_u_p, void (CWnd::*)(UINT, CPoint), OnMButtonUp) },
#define ON_WM_MBUTTONDBLCLK() { WM_MBUTTONDBLCLK, 0, 0, 0, FOURSTORY_MSG(AfxSig_v_u_p, void (CWnd::*)(UINT, CPoint), OnMButtonDblClk) },
#define ON_WM_MOUSEWHEEL() { WM_MOUSEWHEEL, 0, 0, 0, FOURSTORY_MSG(AfxSig_b_u_s_p, BOOL (CWnd::*)(UINT, short, CPoint), OnMouseWheel) },
#define ON_WM_MOUSEHWHEEL() { WM_MOUSEHWHEEL, 0, 0, 0, FOURSTORY_MSG(AfxSig_v_u_s_p, void (CWnd::*)(UINT, short, CPoint), OnMouseHWheel) },
#define ON_WM_SETCURSOR() { WM_SETCURSOR, 0, 0, 0, FOURSTORY_MSG(AfxSig_b_W_u_u, BOOL (CWnd::*)(CWnd*, UINT, UINT), OnSetCursor) },
#define ON_WM_TIMER() { WM_TIMER, 0, 0, 0, FOURSTORY_MSG(AfxSig_v_up, void (CWnd::*)(UINT_PTR), OnTimer) },
#define ON_WM_CONTEXTMENU() { WM_CONTEXTMENU, 0, 0, 0, FOURSTORY_MSG(AfxSig_v_W_p, void (CWnd::*)(CWnd*, CPoint), OnContextMenu) },
#define ON_WM_SIZE() { WM_SIZE, 0, 0, 0, FOURSTORY_MSG(AfxSig_v_u_i_i, void (CWnd::*)(UINT, int, int), OnSize) },
#define ON_WM_MOVE() { WM_MOVE, 0, 0, 0, FOURSTORY_MSG(AfxSig_v_i_i, void (CWnd::*)(int, int), OnMove) },
#define ON_WM_SETFOCUS() { WM_SETFOCUS, 0, 0, 0, FOURSTORY_MSG(AfxSig_v_W, void (CWnd::*)(CWnd*), OnSetFocus) },
#define ON_WM_KILLFOCUS() { WM_KILLFOCUS, 0, 0, 0, FOURSTORY_MSG(AfxSig_v_W, void (CWnd::*)(CWnd*), OnKillFocus) },
#define ON_MESSAGE(message, memberFxn) { message, 0, 0, 0, FOURSTORY_MSG(AfxSig_l_w_l, LRESULT (CWnd::*)(WPARAM, LPARAM), memberFxn) },
#define ON_REGISTERED_MESSAGE(message, memberFxn) ON_MESSAGE(message, memberFxn)
#define ON_COMMAND(id, memberFxn) { WM_COMMAND, 0, (WORD)(id), (WORD)(id), FOURSTORY_MSG(AfxSig_cmd_v, void (CCmdTarget::*)(), memberFxn) },
#define ON_CONTROL(code, id, memberFxn) { WM_COMMAND, (WORD)(code), (WORD)(id), (WORD)(id), FOURSTORY_MSG(AfxSig_cmd_v, void (CCmdTarget::*)(), memberFxn) },
#define ON_BN_CLICKED(id, memberFxn) ON_CONTROL(BN_CLICKED, id, memberFxn)
#define ON_NOTIFY(code, id, memberFxn) { WM_NOTIFY, (UINT)(code), (WORD)(id), (WORD)(id), FOURSTORY_MSG(AfxSig_notify_v, void (CCmdTarget::*)(NMHDR*, LRESULT*), memberFxn) },

// ---- CCmdTarget, CWnd -----------------------------------------------------------
class CCmdTarget : public CObject {
    DECLARE_DYNAMIC(CCmdTarget)
public:
    CCmdTarget() {}
    virtual BOOL OnCmdMsg(UINT id, int code, void* extra, void* handler_info);
    DECLARE_MESSAGE_MAP()
};
struct AFX_CMDHANDLERINFO;

class CWnd : public CCmdTarget {
    DECLARE_DYNAMIC(CWnd)
public:
    HWND m_hWnd;
    static const CWnd wndTop, wndBottom, wndTopMost, wndNoTopMost;
    CWnd();
    explicit CWnd(HWND special);
    virtual ~CWnd();
    operator HWND() const { return this ? m_hWnd : nullptr; }
    HWND GetSafeHwnd() const { return this ? m_hWnd : nullptr; }
    static CWnd* FromHandle(HWND window);
    static CWnd* FromHandlePermanent(HWND window);
    BOOL Attach(HWND window);
    HWND Detach();

    virtual BOOL Create(LPCTSTR class_name, LPCTSTR window_name, DWORD style, const RECT& rect, CWnd* parent, UINT id, void* context = nullptr);
    virtual BOOL CreateEx(DWORD ex_style, LPCTSTR class_name, LPCTSTR window_name, DWORD style, const RECT& rect, CWnd* parent, UINT id, LPVOID param = nullptr);
    BOOL CreateEx(DWORD ex_style, LPCTSTR class_name, LPCTSTR window_name, DWORD style, int x, int y, int width, int height, HWND parent, HMENU menu, LPVOID param = nullptr);
    virtual BOOL DestroyWindow();
    virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
    virtual BOOL PreTranslateMessage(MSG* msg);
    virtual LRESULT WindowProc(UINT message, WPARAM wparam, LPARAM lparam);
    virtual BOOL OnWndMsg(UINT message, WPARAM wparam, LPARAM lparam, LRESULT* result);
    virtual LRESULT DefWindowProc(UINT message, WPARAM wparam, LPARAM lparam);
    virtual void PostNcDestroy();
    virtual BOOL OnCommand(WPARAM wparam, LPARAM lparam);
    virtual BOOL OnNotify(WPARAM wparam, LPARAM lparam, LRESULT* result);
    LRESULT Default();

    BOOL ShowWindow(int command) { return ::ShowWindow(m_hWnd, command); }
    void UpdateWindow() { ::UpdateWindow(m_hWnd); }
    void MoveWindow(int x, int y, int width, int height, BOOL repaint = TRUE) { ::MoveWindow(m_hWnd, x, y, width, height, repaint); }
    void MoveWindow(LPCRECT rect, BOOL repaint = TRUE) { ::MoveWindow(m_hWnd, rect->left, rect->top, rect->right - rect->left, rect->bottom - rect->top, repaint); }
    BOOL SetWindowPos(const CWnd* after, int x, int y, int cx, int cy, UINT flags) { return ::SetWindowPos(m_hWnd, after ? after->m_hWnd : nullptr, x, y, cx, cy, flags); }
    void GetWindowRect(LPRECT rect) const { ::GetWindowRect(m_hWnd, rect); }
    void GetClientRect(LPRECT rect) const { ::GetClientRect(m_hWnd, rect); }
    void ClientToScreen(LPPOINT point) const { ::ClientToScreen(m_hWnd, point); }
    void ClientToScreen(LPRECT rect) const { ::ClientToScreen(m_hWnd, reinterpret_cast<LPPOINT>(rect)); ::ClientToScreen(m_hWnd, reinterpret_cast<LPPOINT>(rect) + 1); }
    void ScreenToClient(LPPOINT point) const { ::ScreenToClient(m_hWnd, point); }
    void ScreenToClient(LPRECT rect) const { ::ScreenToClient(m_hWnd, reinterpret_cast<LPPOINT>(rect)); ::ScreenToClient(m_hWnd, reinterpret_cast<LPPOINT>(rect) + 1); }
    void SetWindowText(LPCTSTR text) { ::SetWindowText(m_hWnd, text); }
    int GetWindowText(LPTSTR text, int size) const { return ::GetWindowText(m_hWnd, text, size); }
    void GetWindowText(CString& text) const;
    int GetWindowTextLength() const { return ::GetWindowTextLength(m_hWnd); }
    CWnd* SetFocus() { return FromHandle(::SetFocus(m_hWnd)); }
    static CWnd* GetFocus() { return FromHandle(::GetFocus()); }
    CWnd* SetCapture() { return FromHandle(::SetCapture(m_hWnd)); }
    static CWnd* GetCapture() { return FromHandle(::GetCapture()); }
    static CWnd* GetForegroundWindow() { return FromHandle(::GetForegroundWindow()); }
    static CWnd* GetActiveWindow() { return FromHandle(::GetActiveWindow()); }
    BOOL SetForegroundWindow() { return ::SetForegroundWindow(m_hWnd); }
    CWnd* SetActiveWindow() { return FromHandle(::SetActiveWindow(m_hWnd)); }
    BOOL IsWindowVisible() const { return ::IsWindowVisible(m_hWnd); }
    BOOL IsWindowEnabled() const { return ::IsWindowEnabled(m_hWnd); }
    BOOL EnableWindow(BOOL enable = TRUE) { return ::EnableWindow(m_hWnd, enable); }
    BOOL IsIconic() const { return ::IsIconic(m_hWnd); }
    BOOL IsZoomed() const { return ::IsZoomed(m_hWnd); }
    BOOL IsChild(const CWnd* window) const { return window && ::IsChild(m_hWnd, window->m_hWnd); }
    CWnd* GetParent() const { return FromHandle(::GetParent(m_hWnd)); }
    CWnd* SetParent(CWnd* parent) { return FromHandle(::SetParent(m_hWnd, parent ? parent->m_hWnd : nullptr)); }
    CWnd* ChildWindowFromPoint(POINT point, UINT flags = 0) const { return FromHandle(::ChildWindowFromPointEx(m_hWnd, point, flags)); }
    UINT GetWindowedChildCount();
    CDC* GetDC();
    int ReleaseDC(CDC* dc);
    void Invalidate(BOOL erase = TRUE) { ::InvalidateRect(m_hWnd, nullptr, erase); }
    void InvalidateRect(LPCRECT rect, BOOL erase = TRUE) { ::InvalidateRect(m_hWnd, rect, erase); }
    void ValidateRect(LPCRECT rect) { ::ValidateRect(m_hWnd, rect); }
    UINT_PTR SetTimer(UINT_PTR id, UINT elapse, TIMERPROC procedure) { return ::SetTimer(m_hWnd, id, elapse, procedure); }
    BOOL KillTimer(UINT_PTR id) { return ::KillTimer(m_hWnd, id); }
    LRESULT SendMessage(UINT message, WPARAM wparam = 0, LPARAM lparam = 0) const { return ::SendMessage(m_hWnd, message, wparam, lparam); }
    BOOL PostMessage(UINT message, WPARAM wparam = 0, LPARAM lparam = 0) const { return ::PostMessage(m_hWnd, message, wparam, lparam); }
    HICON SetIcon(HICON icon, BOOL big);
    DWORD GetStyle() const { return static_cast<DWORD>(::GetWindowLong(m_hWnd, GWL_STYLE)); }
    DWORD GetExStyle() const { return static_cast<DWORD>(::GetWindowLong(m_hWnd, GWL_EXSTYLE)); }
    int GetDlgCtrlID() const { return static_cast<int>(::GetWindowLongPtr(m_hWnd, GWLP_ID)); }
    CWnd* GetDlgItem(int id) const;
    int MessageBox(LPCTSTR text, LPCTSTR caption = nullptr, UINT type = MB_OK) { return ::MessageBox(m_hWnd, text, caption, type); }

    // Default handlers (call Default(), as MFC does).
    afx_msg int OnCreate(LPCREATESTRUCT cs);
    afx_msg void OnDestroy();
    afx_msg void OnNcDestroy();
    afx_msg void OnClose();
    afx_msg void OnPaint();
    afx_msg void OnActivate(UINT state, CWnd* other, BOOL minimized);
    afx_msg void OnActivateApp(BOOL active, DWORD thread);
    afx_msg void OnKeyDown(UINT ch, UINT repeat, UINT flags);
    afx_msg void OnKeyUp(UINT ch, UINT repeat, UINT flags);
    afx_msg void OnSysKeyDown(UINT ch, UINT repeat, UINT flags);
    afx_msg void OnSysKeyUp(UINT ch, UINT repeat, UINT flags);
    afx_msg void OnChar(UINT ch, UINT repeat, UINT flags);
    afx_msg LRESULT OnNcHitTest(CPoint point);
    afx_msg void OnNcLButtonDown(UINT hit_test, CPoint point);
    afx_msg void OnNcRButtonDown(UINT hit_test, CPoint point);
    afx_msg void OnMouseMove(UINT flags, CPoint point);
    afx_msg void OnLButtonDown(UINT flags, CPoint point);
    afx_msg void OnLButtonUp(UINT flags, CPoint point);
    afx_msg void OnLButtonDblClk(UINT flags, CPoint point);
    afx_msg void OnRButtonDown(UINT flags, CPoint point);
    afx_msg void OnRButtonUp(UINT flags, CPoint point);
    afx_msg void OnRButtonDblClk(UINT flags, CPoint point);
    afx_msg void OnMButtonDown(UINT flags, CPoint point);
    afx_msg void OnMButtonUp(UINT flags, CPoint point);
    afx_msg void OnMButtonDblClk(UINT flags, CPoint point);
    afx_msg BOOL OnMouseWheel(UINT flags, short delta, CPoint point);
    afx_msg void OnMouseHWheel(UINT flags, short delta, CPoint point);
    afx_msg BOOL OnSetCursor(CWnd* window, UINT hit_test, UINT message);
    afx_msg void OnTimer(UINT_PTR id);
    afx_msg void OnContextMenu(CWnd* window, CPoint point);
    afx_msg void OnSize(UINT type, int cx, int cy);
    afx_msg void OnMove(int x, int y);
    afx_msg void OnSetFocus(CWnd* old);
    afx_msg void OnKillFocus(CWnd* next);
protected:
    WNDPROC m_pfnSuper;
    DECLARE_MESSAGE_MAP()
private:
    CWnd(const CWnd&);
    CWnd& operator=(const CWnd&);
};

// ---- CWinThread, CWinApp --------------------------------------------------------
class CWinThread : public CCmdTarget {
    DECLARE_DYNAMIC(CWinThread)
public:
    CWnd* m_pMainWnd = nullptr;
    MSG m_msgCur = {};
    virtual BOOL InitInstance() { return TRUE; }
    virtual int ExitInstance() { return static_cast<int>(m_msgCur.wParam); }
    virtual int Run();
    virtual BOOL PreTranslateMessage(MSG* msg);
    virtual BOOL PumpMessage();
    virtual BOOL OnIdle(LONG count) { (void)count; return FALSE; }
    virtual CWnd* GetMainWnd() { return m_pMainWnd; }
    DECLARE_MESSAGE_MAP()
};
class CWinApp : public CWinThread {
    DECLARE_DYNAMIC(CWinApp)
public:
    explicit CWinApp(LPCTSTR app_name = nullptr);
    virtual ~CWinApp();
    HINSTANCE m_hInstance = nullptr;
    LPTSTR m_lpCmdLine = nullptr;
    int m_nCmdShow = SW_SHOWNORMAL;
    LPCTSTR m_pszAppName = nullptr;
    LPCTSTR m_pszRegistryKey = nullptr;
    LPCTSTR m_pszProfileName = nullptr;
    LPCTSTR m_pszExeName = nullptr;
    virtual BOOL InitApplication() { return TRUE; }
    virtual void LoadStdProfileSettings(UINT max_mru = 4);
    void SetRegistryKey(LPCTSTR key);
    void SetRegistryKey(UINT id);
    CString GetProfileString(LPCTSTR section, LPCTSTR entry, LPCTSTR default_value = nullptr);
    UINT GetProfileInt(LPCTSTR section, LPCTSTR entry, int default_value);
    BOOL WriteProfileString(LPCTSTR section, LPCTSTR entry, LPCTSTR value);
    BOOL WriteProfileInt(LPCTSTR section, LPCTSTR entry, int value);
    HICON LoadIcon(UINT id) const;
    HICON LoadIcon(LPCTSTR name) const;
    HCURSOR LoadCursor(UINT id) const;
    HCURSOR LoadStandardCursor(LPCTSTR name) const { return ::LoadCursor(nullptr, name); }
    DECLARE_MESSAGE_MAP()
private:
    std::string app_name_storage_, profile_storage_, registry_storage_, exe_storage_;
};

// ---- GDI objects --------------------------------------------------------------------
class CGdiObject : public CObject {
    DECLARE_DYNAMIC(CGdiObject)
public:
    HGDIOBJ m_hObject = nullptr;
    CGdiObject() {}
    virtual ~CGdiObject() { DeleteObject(); }
    HGDIOBJ GetSafeHandle() const { return this ? m_hObject : nullptr; }
    operator HGDIOBJ() const { return m_hObject; }
    BOOL Attach(HGDIOBJ object) { if (m_hObject) return FALSE; m_hObject = object; return object != nullptr; }
    HGDIOBJ Detach() { HGDIOBJ o = m_hObject; m_hObject = nullptr; return o; }
    BOOL DeleteObject() { if (!m_hObject) return FALSE; HGDIOBJ o = Detach(); return ::DeleteObject(o); }
    int GetObject(int size, LPVOID out) const { return ::GetObject(m_hObject, size, out); }
    BOOL CreateStockObject(int index) { return Attach(::GetStockObject(index)); }
};
class CFont : public CGdiObject {
    DECLARE_DYNAMIC(CFont)
public:
    operator HFONT() const { return static_cast<HFONT>(m_hObject); }
    static CFont* FromHandle(HFONT font);
    BOOL CreateFontIndirect(const LOGFONT* font) { return Attach(::CreateFontIndirect(font)); }
    BOOL CreateFont(int height, int width, int escapement, int orientation, int weight, BYTE italic, BYTE underline, BYTE strike_out,
                    BYTE char_set, BYTE out_precision, BYTE clip_precision, BYTE quality, BYTE pitch_and_family, LPCTSTR face) {
        return Attach(::CreateFont(height, width, escapement, orientation, weight, italic, underline, strike_out, char_set,
                                   out_precision, clip_precision, quality, pitch_and_family, face));
    }
    BOOL CreatePointFont(int point_tenths, LPCTSTR face, CDC* dc = nullptr);
    int GetLogFont(LOGFONT* font) const { return ::GetObject(m_hObject, sizeof(LOGFONT), font); }
};
class CBitmap : public CGdiObject {
    DECLARE_DYNAMIC(CBitmap)
public:
    operator HBITMAP() const { return static_cast<HBITMAP>(m_hObject); }
    BOOL CreateCompatibleBitmap(CDC* dc, int width, int height);
    BOOL CreateBitmap(int width, int height, UINT planes, UINT bits, const void* data) { return Attach(::CreateBitmap(width, height, planes, bits, data)); }
    int GetBitmap(BITMAP* out) const { return ::GetObject(m_hObject, sizeof(BITMAP), out); }
};
class CBrush : public CGdiObject {
    DECLARE_DYNAMIC(CBrush)
public:
    CBrush() {}
    explicit CBrush(COLORREF color) { Attach(::CreateSolidBrush(color)); }
    operator HBRUSH() const { return static_cast<HBRUSH>(m_hObject); }
    BOOL CreateSolidBrush(COLORREF color) { return Attach(::CreateSolidBrush(color)); }
};
class CDC : public CObject {
    DECLARE_DYNAMIC(CDC)
public:
    HDC m_hDC = nullptr;
    HDC m_hAttribDC = nullptr;
    CDC() {}
    virtual ~CDC() { if (m_hDC && owned_) ::DeleteDC(m_hDC); }
    operator HDC() const { return this ? m_hDC : nullptr; }
    HDC GetSafeHdc() const { return this ? m_hDC : nullptr; }
    static CDC* FromHandle(HDC dc);
    BOOL Attach(HDC dc) { if (m_hDC) return FALSE; m_hDC = m_hAttribDC = dc; return dc != nullptr; }
    HDC Detach() { HDC dc = m_hDC; m_hDC = m_hAttribDC = nullptr; owned_ = false; return dc; }
    BOOL CreateCompatibleDC(CDC* dc) { if (!Attach(::CreateCompatibleDC(dc ? dc->m_hDC : nullptr))) return FALSE; owned_ = true; return TRUE; }
    BOOL DeleteDC() { if (!m_hDC) return FALSE; HDC dc = Detach(); return ::DeleteDC(dc); }
    CFont* SelectObject(CFont* font) { return CFont::FromHandle(static_cast<HFONT>(::SelectObject(m_hDC, font ? font->m_hObject : nullptr))); }
    HGDIOBJ SelectObject(HGDIOBJ object) { return ::SelectObject(m_hDC, object); }
    CBitmap* SelectObject(CBitmap* bitmap);
    COLORREF SetTextColor(COLORREF color) { return ::SetTextColor(m_hDC, color); }
    COLORREF SetBkColor(COLORREF color) { return ::SetBkColor(m_hDC, color); }
    int SetBkMode(int mode) { return ::SetBkMode(m_hDC, mode); }
    int GetDeviceCaps(int index) const { return ::GetDeviceCaps(m_hDC, index); }
    BOOL BitBlt(int x, int y, int width, int height, CDC* from, int fx, int fy, DWORD rop) { return ::BitBlt(m_hDC, x, y, width, height, from ? from->m_hDC : nullptr, fx, fy, rop); }
private:
    bool owned_ = false;
};
class CClientDC : public CDC {
public:
    explicit CClientDC(CWnd* window) : window_(window ? window->m_hWnd : nullptr) { Attach(::GetDC(window_)); }
    ~CClientDC() { if (m_hDC) ::ReleaseDC(window_, Detach()); }
private:
    HWND window_;
};
class CPaintDC : public CDC {
public:
    explicit CPaintDC(CWnd* window) : window_(window ? window->m_hWnd : nullptr) { Attach(::BeginPaint(window_, &m_ps)); }
    ~CPaintDC() { ::EndPaint(window_, &m_ps); Detach(); }
    PAINTSTRUCT m_ps;
private:
    HWND window_;
};

// ---- Global functions -------------------------------------------------------------------
CWinApp* AfxGetApp();
CWinThread* AfxGetThread();
CWnd* AfxGetMainWnd();
HINSTANCE AfxGetInstanceHandle();
HINSTANCE AfxGetResourceHandle();
LPCTSTR AfxGetAppName();
LPCTSTR AfxRegisterWndClass(UINT class_style, HCURSOR cursor = nullptr, HBRUSH background = nullptr, HICON icon = nullptr);
void AfxPostQuitMessage(int exit_code);
int AfxMessageBox(LPCTSTR text, UINT type = MB_OK, UINT help_id = 0);
int AfxMessageBox(UINT id, UINT type = MB_OK, UINT help_id = (UINT)-1);
BOOL AfxSocketInit(void* data = nullptr);
// No COM and no ActiveX container: the call is accepted; control creation fails (THttpCtrl).
void AfxEnableControlContainer(void* manager = nullptr);
LRESULT CALLBACK AfxWndProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
int AfxWinMain(HINSTANCE instance, HINSTANCE previous, LPTSTR command_line, int show);
#define AFX_IDS_APP_TITLE 0xE000
#define AFX_IDS_IDLEMESSAGE 0xE001
#define AFX_WNDCLASS(s) "Afx" s

namespace fourstory { namespace mfc {
// String resources: set_string_resource entries, then the RT_STRING entries of
// the resource table (fourstory_kernel32.h). CString::LoadString finds them.
void set_string_resource(UINT id, const char* text);
bool string_resource(UINT id, std::string* text);
// The profile store file of the application (REGEDIT4 text), for tests.
std::string profile_store_path();
}}
// ---- Dialogs and common controls (C4) ----------------------------------------
// Declarations for headers that hold dialog members (TClient/PacketSpyDlg.h).
// The native build has no dialog templates (no RT_DIALOG resources): Create
// and DoModal fail, as on Windows when the template is missing. The dialog
// units themselves are not built (native/client/app/README.md).
class CDataExchange { public: BOOL m_bSaveAndValidate = FALSE; CWnd* m_pDlgWnd = nullptr; };
class CDialog : public CWnd {
public:
    using CWnd::Create;
    CDialog() {}
    explicit CDialog(UINT, CWnd* = nullptr) {}
    explicit CDialog(LPCTSTR, CWnd* = nullptr) {}
    virtual INT_PTR DoModal() { SetLastError(1813); return -1; }  // ERROR_RESOURCE_TYPE_NOT_FOUND
    BOOL Create(UINT, CWnd* = nullptr) { SetLastError(1813); return FALSE; }
    virtual BOOL OnInitDialog() { return TRUE; }
    void EndDialog(int) {}
protected:
    virtual void DoDataExchange(CDataExchange*) {}
    virtual void OnOK() {}
    virtual void OnCancel() {}
};
class CListBox : public CWnd {};
class CComboBox : public CWnd {};
class CListCtrl : public CWnd {};
class CRichEditCtrl : public CWnd {};
class CIPAddressCtrl : public CWnd {};
inline BOOL AfxInitRichEdit2() { return FALSE; }

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
