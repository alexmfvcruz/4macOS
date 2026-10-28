// RetDlg.cpp : 구현 파일입니다.
//

#include "stdafx.h"
#include "Happy.h"
#include "RetDlg.h"


// CRetDlg 대화 상자입니다.

IMPLEMENT_DYNAMIC(CRetDlg, CDialog)
CRetDlg::CRetDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CRetDlg::IDD, pParent)
{
}

CRetDlg::~CRetDlg()
{
}

void CRetDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LB_RET, m_lbRet);
}


BEGIN_MESSAGE_MAP(CRetDlg, CDialog)
END_MESSAGE_MAP()


// CRetDlg 메시지 처리기입니다.


void CRetDlg::SetMsg(CString str)
{
	/*
	CString strFont = _T("2002L");
	CFont Font;
	LOGFONT lf;	
	ZeroMemory(&lf, sizeof(lf));
	lf.lfHeight = 100;
	strcpy(lf.lfFaceName, strFont);
	Font.CreateFontIndirect(&lf);
*/
	//CFont Font;
	//Font.CreateFont(16,0,0,0,100,FALSE,FALSE,
	//	0,ANSI_CHARSET,OUT_DEFAULT_PRECIS,
	//	CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,
	//	NULL,"2002L");

	//m_lbRet.SetFont(&Font);

#ifdef FOURSTORY_CLIENT_MOD
	CRect rect;
	GetClientRect(&rect);
	CRect spacing(0, 0, 6, 6);
	MapDialogRect(&spacing);
	CRect top(0, 0, 0, 12);
	MapDialogRect(&top);

	const int textWidth = max(1, rect.Width() - 2 * spacing.Width());
	CClientDC dc(&m_lbRet);
	CFont* pFont = m_lbRet.GetFont();
	CFont* pOldFont = pFont ? dc.SelectObject(pFont) : NULL;
	TEXTMETRIC metrics;
	dc.GetTextMetrics(&metrics);
	CRect textRect(0, 0, textWidth, 0);
	dc.DrawText(str, &textRect, DT_CALCRECT | DT_WORDBREAK | DT_CENTER | DT_NOPREFIX);
	if(pOldFont)
		dc.SelectObject(pOldFont);
	const int textHeight = max(metrics.tmHeight, textRect.Height());

	CWnd* pOK = GetDlgItem(IDOK);
	CRect button;
	pOK->GetWindowRect(&button);
	const int buttonTop = top.Height() + textHeight + spacing.Height();
	const int requiredHeight = buttonTop + button.Height() + spacing.Height();
	if(requiredHeight > rect.Height())
	{
		CRect window;
		GetWindowRect(&window);
		SetWindowPos(NULL, 0, 0, window.Width(),
			window.Height() + requiredHeight - rect.Height(),
			SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
	}

	m_lbRet.ModifyStyle(SS_TYPEMASK, SS_CENTER | SS_NOPREFIX);
	m_lbRet.MoveWindow(spacing.Width(), top.Height(), textWidth, textHeight);
	pOK->SetWindowPos(NULL, (rect.Width() - button.Width()) / 2, buttonTop,
		0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
#else
	CRect rect;
	GetClientRect(&rect);
	
	//CRect rRect;
	//GetDlgItem(IDOK)->GetWindowRect(&rRect);
	DWORD dwCharWidth = 8;	
	DWORD dwLen = str.GetLength();
	DWORD width = dwLen * dwCharWidth;
	m_lbRet.MoveWindow( (rect.Width() - width)/2 + 5, rect.top + 25,width + 5, 20);
#endif
	
	m_lbRet.SetWindowText(str);
}