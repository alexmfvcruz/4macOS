// D3DFont.h: interface for the CD3DFont class.
//
//////////////////////////////////////////////////////////////////////

#if !defined ___D3DFONT_H
#define ___D3DFONT_H


#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000


class CD3DFont
{
public:
	static int m_nRenderingHint;

protected:
	LPDIRECT3DVERTEXBUFFER9 *m_pVB3D;
	LPDIRECT3DVERTEXBUFFER9 *m_pVB;
	LPDIRECT3DTEXTURE9 *m_pTEX[2];

	CString m_strTEXT;
	CFont *m_pFont;
	UINT m_nFormat;

	int m_nPartCount;
	int m_nHeight;
	int m_nWidth;
	int m_nPosX;
	int m_nPosY;

	float** m_ppPosBufX;
	float** m_ppPosBufY;

	float m_fScaleX;
	float m_fScaleY;
	float m_fPrvScaleX;
	float m_fPrvScaleY;

public:
	DWORD m_dwLineColor;
	DWORD m_dwColor;

	BYTE m_bOutLine;
	BYTE m_bShadow;

protected:
	WORD GetColorByLinePixel(
		LPDWORD pBits,
		int nPosX, 
		int nPosY);

public:
	void SetFont( CFont *pFont);
	void Release();

	void MakeText(
		LPDIRECT3DDEVICE9 pDevice,
		CString strText,
		CRect& rect,
		UINT nFormat = 0,
		CPoint Hide = CPoint(0,0));
#ifdef FOURSTORY_CLIENT_NATIVE
	// MSVC binds a temporary CRect to the non-const reference; MakeText may
	// write it (DT_CALCRECT), so a copy takes the result that the caller drops.
	void MakeText(
		LPDIRECT3DDEVICE9 pDevice,
		CString strText,
		const CRect& rect,
		UINT nFormat = 0,
		CPoint Hide = CPoint(0,0))
	{
		CRect vRECT(rect);
		MakeText( pDevice, strText, vRECT, nFormat, Hide);
	}
#endif

	void TextOut(
		LPDIRECT3DDEVICE9 pDevice,
		CString strText,
		CRect& rect,
		UINT nFormat = 0,
		BOOL bOpaque = FALSE,
		CPoint Hide = CPoint(0,0));

	void TextOut(
		LPDIRECT3DDEVICE9 pDevice,
		CString strText,
		int nPosX,
		int nPosY,
		BOOL bOpaque = FALSE,
		CPoint Hide = CPoint(0,0));

	void TextOut(
		LPDIRECT3DDEVICE9 pDevice,
		D3DXMATRIX& vWorld,
		CString strText,
		FLOAT fTextHeight = 0.3f,
		UINT nFormat = 0);
#ifdef FOURSTORY_CLIENT_NATIVE
	// MSVC binds temporaries to the non-const reference; TextOut only reads vWorld.
	void TextOut(
		LPDIRECT3DDEVICE9 pDevice,
		const D3DXMATRIX& vWorld,
		CString strText,
		FLOAT fTextHeight = 0.3f,
		UINT nFormat = 0)
	{
		TextOut( pDevice, const_cast<D3DXMATRIX&>(vWorld), strText, fTextHeight, nFormat);
	}
#endif

	int GetWidth() { return m_nWidth; };
	int GetHeight() { return m_nHeight; };

	float GetScaleX() { return m_fScaleX; }
	float GetScaleY() { return m_fScaleY; }
	void SetScaleX(float fScale);
	void SetScaleY(float fScale);

#ifdef FOURSTORY_CLIENT_MOD
public:
	// Incremented when the fonts are recreated; cached text made with an older
	// value is rendered again (see MakeText).
	static DWORD m_dwFontGeneration;
	DWORD m_dwMadeGeneration;
	// Text is drawn with GDI+, which only knows fonts installed in Windows;
	// font files registered here are used for faces it cannot find.
	static BOOL AddPrivateFontFile( LPCSTR szFile);
	// New GDI+ font for a GDI font whose face is a registered file, or NULL.
	static Gdiplus::Font *NewPrivateFont( HFONT hFONT);
#endif
public:
	CD3DFont();
	virtual ~CD3DFont();
};


#ifdef FOURSTORY_CLIENT_MOD
// The GDI+ font for the GDI font selected into a device context. GDI+ only
// knows fonts installed in Windows; for other faces the registered font file is
// used (CD3DFont::AddPrivateFontFile). Replaces "Font vFONT(hDC)".
struct TGDIPLUSFONT
{
	Gdiplus::Font m_vHDC;
	Gdiplus::Font *m_pPRIVATE;

	TGDIPLUSFONT( HDC hDC)
		: m_vHDC(hDC), m_pPRIVATE(NULL)
	{
		if( m_vHDC.GetLastStatus() != Gdiplus::Ok )
			m_pPRIVATE = CD3DFont::NewPrivateFont((HFONT) GetCurrentObject( hDC, OBJ_FONT));
	}

	~TGDIPLUSFONT()
	{
		delete m_pPRIVATE;
	}

	Gdiplus::Font& Get()
	{
		return m_pPRIVATE ? *m_pPRIVATE : m_vHDC;
	}
};

#endif
#endif // !defined ___D3DFONT_H
