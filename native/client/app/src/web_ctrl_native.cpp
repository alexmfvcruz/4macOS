// Native replacement of Client/TClient/TClientWebCtrl.cpp (C4).
// The Windows unit opens Internet Explorer as a COM local server
// (CLSID_InternetExplorer). macOS has no such server: Navigate takes the
// original failure path of CoCreateInstance (destroy the window, keep the
// host, return FALSE) and Create fails because no browser window exists.
#include "StdAfx.h"

IMPLEMENT_DYNAMIC(CTClientWebCtrl, CWnd)
CTClientWebCtrl::CTClientWebCtrl()
{
	m_pActHost = NULL;
	m_pWebCtrl = NULL;
}

CTClientWebCtrl::~CTClientWebCtrl()
{
	if(m_pActHost)
	{
		m_pActHost->SetForegroundWindow();
		m_pActHost->ShowWindow(SW_NORMAL);
	}
}

BEGIN_MESSAGE_MAP(CTClientWebCtrl, CWnd)
	ON_WM_DESTROY()
END_MESSAGE_MAP()

BYTE CTClientWebCtrl::Navigate(CString, int, int, int, int, BYTE, BYTE, BYTE, BYTE, BYTE, HWND)
{
	CWnd *pActHost = m_pActHost;

	if(GetSafeHwnd())
		DestroyWindow();
	m_pActHost = pActHost;

	return FALSE;
}

BOOL CTClientWebCtrl::Create()
{
	return FALSE;
}

void CTClientWebCtrl::OnDestroy()
{
	if(m_pActHost)
	{
		m_pActHost->SetForegroundWindow();
		m_pActHost->ShowWindow(SW_NORMAL);
	}

	CWnd::OnDestroy();
}
