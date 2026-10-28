// Native stand-in for THttpCtrl (C4-P). The original
// (Lib/Own/TComp/TComp/THttpCtrl.cpp, with THtmlDocWnd.cpp and TOccManager.cpp)
// hosts the Internet Explorer ActiveX control (IWebBrowser2) through MFC OLE.
// That control does not exist on macOS, so the three units are not in the
// native build. This file implements the same class with the behavior of the
// original while no browser exists (m_pHTTP NULL, no host window): InitHTTP
// and Navigate fail, input reaches only the TComponent handlers, and Render
// draws only the component. A later step can host a WebKit view instead.
#include <fourstory_engine_ui.h>

LPDIRECT3DTEXTURE9 THttpCtrl::m_pTIMG = NULL;
CSize THttpCtrl::m_vTBUFSIZE(0, 0);
BYTE THttpCtrl::m_bRedrawHTTP = FALSE;
CWnd THttpCtrl::m_vHTTP;

THttpCtrl::THttpCtrl(TComponent* pParent, FRAMEDESC_SHAREDPTR pDesc) : TComponent(pParent, pDesc) { Init(); }
THttpCtrl::THttpCtrl(TComponent* pParent, FRAMEDESC_WEAKPTR pDesc) : THttpCtrl(pParent, pDesc.lock()) {}
THttpCtrl::THttpCtrl(TComponent* pParent, const THttpCtrl& rSrcHttpCtrl) : TComponent(pParent, rSrcHttpCtrl) { Init(); }
THttpCtrl::~THttpCtrl() { ReleaseHTTP(); }

// As the original Init.
void THttpCtrl::Init()
{
    m_bHitTestType = HITTEST_TYPE_RECT;
    m_bType = TCML_TYPE_HTTPCTRL;
    m_style |= TS_FOCUSABLE;
    m_strURL.Empty();
    m_pHTTP = NULL;
    m_bLButtonDown = FALSE;
    m_bCaret = FALSE;
    m_dwTick = 0;
}
void THttpCtrl::CheckBUF() {}
void THttpCtrl::UpdateTIMGBUF() {}
void THttpCtrl::ReleaseBUF()
{
    if (m_pTIMG) { m_pTIMG->Release(); m_pTIMG = NULL; }
    m_vTBUFSIZE.cx = 0;
    m_vTBUFSIZE.cy = 0;
}
BYTE THttpCtrl::IsFocusedHTML(HWND) { return FALSE; }
// No browser control: navigation fails, as the original does when InitHTTP fails.
BYTE THttpCtrl::Navigate(LPCTSTR) { return InitHTTP(); }
BYTE THttpCtrl::InitHTTP() { ReleaseHTTP(); return FALSE; }
void THttpCtrl::ReleaseHTTP() { m_pHTTP = NULL; m_bCaret = FALSE; m_dwTick = 0; }
BYTE THttpCtrl::HaveCaret(CRect* pRECT) { if (pRECT) pRECT->SetRectEmpty(); return FALSE; }
CWnd* THttpCtrl::GetChildFromPoint(CPoint) { return NULL; }
BYTE THttpCtrl::OnMouseMSG(UINT, UINT, CPoint) { return FALSE; }
void THttpCtrl::MoveWndHTTP() {}
void THttpCtrl::Redraw() {}

HRESULT THttpCtrl::Render(DWORD dwTickCount)
{
    if (!m_bVisible)
        return S_OK;
    if (!CTachyonRes::m_pDEVICE)
        return E_FAIL;
    return TComponent::Render(dwTickCount);
}
void THttpCtrl::ShowComponent(BOOL bVisible)
{
    TComponent::ShowComponent(bVisible);
    if (bVisible)
        Navigate(LPCTSTR(m_strURL));
    else
    {
        ReleaseHTTP();
        ReleaseBUF();
    }
}
BOOL THttpCtrl::OnSetCursor(CWnd*, UINT, UINT, CPoint) { return FALSE; }
BOOL THttpCtrl::DoMouseWheel(UINT, short, CPoint) { return FALSE; }
void THttpCtrl::OnLButtonDblClk(UINT nFlags, CPoint pt) { TComponent::OnLButtonDblClk(nFlags, pt); }
void THttpCtrl::OnMButtonDblClk(UINT nFlags, CPoint pt) { TComponent::OnMButtonDblClk(nFlags, pt); }
void THttpCtrl::OnRButtonDblClk(UINT nFlags, CPoint pt) { TComponent::OnRButtonDblClk(nFlags, pt); }
void THttpCtrl::OnLButtonDown(UINT nFlags, CPoint pt) { m_bLButtonDown = TRUE; TComponent::OnLButtonDown(nFlags, pt); }
void THttpCtrl::OnMButtonDown(UINT nFlags, CPoint pt) { TComponent::OnMButtonDown(nFlags, pt); }
void THttpCtrl::OnRButtonDown(UINT nFlags, CPoint pt) { TComponent::OnRButtonDown(nFlags, pt); }
void THttpCtrl::OnLButtonUp(UINT nFlags, CPoint pt) { m_bLButtonDown = FALSE; TComponent::OnLButtonUp(nFlags, pt); }
void THttpCtrl::OnMButtonUp(UINT nFlags, CPoint pt) { TComponent::OnMButtonUp(nFlags, pt); }
void THttpCtrl::OnRButtonUp(UINT nFlags, CPoint pt) { TComponent::OnRButtonUp(nFlags, pt); }
void THttpCtrl::OnMouseMove(UINT nFlags, CPoint pt) { TComponent::OnMouseMove(nFlags, pt); }
void THttpCtrl::OnChar(UINT, int, UINT) {}
void THttpCtrl::OnLoseFocus(TComponent*) {}
void THttpCtrl::OnSetFocus(TComponent*) {}
