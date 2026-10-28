#include "stdafx.h"
#ifdef FOURSTORY_CLIENT_64
// TCML uses native long in memory; LOGFONT retains its Windows field widths.
static void CopyFontPayload(const TCML_LOGFONT& font, LOGFONT& lf)
{
	lf.lfHeight = static_cast<LONG>(font.tlfHeight);
	lf.lfWidth = static_cast<LONG>(font.tlfWidth);
	lf.lfEscapement = static_cast<LONG>(font.tlfEscapement);
	lf.lfOrientation = static_cast<LONG>(font.tlfOrientation);
	lf.lfWeight = static_cast<LONG>(font.tlfWeight);
	lf.lfItalic = static_cast<BYTE>(font.tlfItalic);
	lf.lfUnderline = static_cast<BYTE>(font.tlfUnderline);
	lf.lfStrikeOut = static_cast<BYTE>(font.tlfStrikeOut);
	lf.lfCharSet = static_cast<BYTE>(font.tlfCharSet);
	lf.lfOutPrecision = static_cast<BYTE>(font.tlfOutPrecision);
	lf.lfClipPrecision = static_cast<BYTE>(font.tlfClipPrecision);
	lf.lfQuality = static_cast<BYTE>(font.tlfQuality);
	lf.lfPitchAndFamily = static_cast<BYTE>(font.tlfPitchAndFamily);
	static_assert(sizeof(lf.lfFaceName) == sizeof(font.tlfFaceName), "TCML font names require ANSI LOGFONT");
	memcpy(lf.lfFaceName, font.tlfFaceName, sizeof(font.tlfFaceName));
}
#endif
#include "THtmlDocWnd.h"


CTachyonRes*	g_pImagePool = NULL;
CTachyonRes*	g_pMenuPool = NULL;

#define COMPAREMEMBERLESS(x)	if (_Left.x < _Right.x) return true; if (_Left.x > _Right.x) return false;

struct LessLOGFONT : public binary_function<LOGFONT, LOGFONT, bool>
{
	bool operator()(const LOGFONT& _Left, const LOGFONT& _Right) const
	{
		COMPAREMEMBERLESS(lfHeight)
		COMPAREMEMBERLESS(lfWidth)
		COMPAREMEMBERLESS(lfEscapement)
		COMPAREMEMBERLESS(lfOrientation)
		COMPAREMEMBERLESS(lfWeight)
		COMPAREMEMBERLESS(lfItalic)
		COMPAREMEMBERLESS(lfUnderline)
		COMPAREMEMBERLESS(lfStrikeOut)
		COMPAREMEMBERLESS(lfOutPrecision)
		COMPAREMEMBERLESS(lfClipPrecision)
		COMPAREMEMBERLESS(lfQuality)
		COMPAREMEMBERLESS(lfPitchAndFamily)

		if (CString(_Left.lfFaceName) < CString(_Right.lfFaceName))
			return true;

		return false;
	}
};
typedef std::map<LOGFONT, CFont*, LessLOGFONT> CFONT_MAP;

GDIFONT_MAP		g_Fonts;
CFONT_MAP		g_CFonts;

CString			g_strForceFont;
INT				g_nFontRate = 100;
BYTE			g_typeQuality = ANTIALIASED_QUALITY;

void SetForceTachyonFont(LPCSTR strForceFont)
{
	g_strForceFont = strForceFont;
}

void SetTachyonFontSizeRate(INT nRate)
{
	g_nFontRate = nRate;
}

void SetTachyonFontQuality( LPCSTR strQuality )
{

	typedef std::map< CString, BYTE >	_TMAP_QUALITY;
	typedef _TMAP_QUALITY::iterator	_TMAP_QUALITY_ITOR;

	_TMAP_QUALITY mapQuality;
	mapQuality.insert( make_pair( _T("DEFAULT_QUALITY"), 0 ) );
	mapQuality.insert( make_pair( _T("DRAFT_QUALITY"), 1 ) );
	mapQuality.insert( make_pair( _T("PROOF_QUALITY"), 2 ) );
	mapQuality.insert( make_pair( _T("NONANTIALIASED_QUALITY"), 3 ) );
	mapQuality.insert( make_pair( _T("ANTIALIASED_QUALITY"), 4 ) );

	_TMAP_QUALITY_ITOR finder = mapQuality.find( strQuality );
	if( finder != mapQuality.end() )
		g_typeQuality = (BYTE)(*finder).second;

	// default value of 'g_typeQuality' is ANTIALIASED_QUALITY.
}

void InitTachyonComponent(CTachyonRes * pImagePool,
						  CTachyonRes * pMenuPool,
						  TCMLParser * pParser)
{
#ifndef FOURSTORY_CLIENT_NATIVE
	THtmlDocWnd::InitHook();
#endif

	g_pImagePool = pImagePool;
	g_pMenuPool = pMenuPool;

	LOGFONT lf;
	LP_TCML_LOGFONT ptr = NULL;

	if(!pParser) return;

	pParser->MoveFirstFont();
	while((ptr = pParser->MoveNextFont())!=NULL)
	{
#ifdef FOURSTORY_CLIENT_64
		CopyFontPayload(*ptr, lf);
#else
		memcpy(&lf, ptr, sizeof(LOGFONT));
#endif

		lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
		lf.lfOutPrecision = OUT_CHARACTER_PRECIS;
		lf.lfPitchAndFamily = FF_DONTCARE;
		lf.lfQuality = g_typeQuality;	// default value is ANTIALIASED_QUALITY.
		lf.lfCharSet = DEFAULT_CHARSET;

#ifndef FOURSTORY_CLIENT_MOD
		if( !g_strForceFont.IsEmpty() )
			strcpy_s(lf.lfFaceName, (LPCSTR)g_strForceFont);
#endif

		if( g_nFontRate != 100 )
			lf.lfHeight = lf.lfHeight * g_nFontRate / 100;

		CGDIFont *pFont = new CGDIFont();
		CFONT_MAP::iterator fit = g_CFonts.find(lf);

		if( fit == g_CFonts.end() )
		{
			pFont->m_pFont = new CFont;
			if( pFont->m_pFont->CreateFontIndirect(&lf) )
				g_CFonts.insert(CFONT_MAP::value_type(lf, pFont->m_pFont));
			else
			{
				delete pFont->m_pFont;
				pFont->m_pFont = NULL;
			}
		}
		else
			pFont->m_pFont = fit->second;

		if( pFont->m_pFont )
		{
			pFont->m_clr = ptr->tlfColor;

			GDIFONT_MAP::iterator it = g_Fonts.find(ptr->tlfId);
			if(it == g_Fonts.end())
				g_Fonts.insert(GDIFONT_MAP::value_type(ptr->tlfId, pFont));
		}
	}
#ifdef FOURSTORY_CLIENT_MOD

	if( !g_strForceFont.IsEmpty() )
		ReloadTachyonFonts(g_strForceFont);
#endif
}

#ifdef FOURSTORY_CLIENT_MOD
// The fonts are created with the faces the interface files name (the forced
// face is applied afterwards by InitTachyonComponent), so the map keys keep the
// original faces and the default can be restored.
void ReloadTachyonFonts(LPCSTR strFace)
{
	g_strForceFont = strFace ? strFace : "";

	for( CFONT_MAP::iterator it = g_CFonts.begin(); it != g_CFonts.end(); ++it)
	{
		LOGFONT lf = (*it).first;

		if( !g_strForceFont.IsEmpty() )
			strcpy_s(lf.lfFaceName, (LPCSTR)g_strForceFont);

		(*it).second->DeleteObject();
		(*it).second->CreateFontIndirect(&lf);
	}

	CD3DFont::m_dwFontGeneration++;
}

#endif
void FinishTachyonComponent()
{
	GDIFONT_MAP::iterator gdit;
	for( gdit = g_Fonts.begin(); gdit != g_Fonts.end(); ++gdit)
		delete (*gdit).second;

	CFONT_MAP::iterator cfit;
	for( cfit = g_CFonts.begin(); cfit != g_CFonts.end(); ++cfit)
		delete (*cfit).second;
}

CGDIFont *FindFont(DWORD id)
{
	if( id > 0 )
	{
		GDIFONT_MAP::iterator it = g_Fonts.find(id);
		if( it != g_Fonts.end())
			return (*it).second;
	}

	return NULL;
}

LPIMAGESET FindSprite(DWORD id)
{
	if( !g_pImagePool )
		return NULL;

	MAPRES::iterator finder = g_pImagePool->m_mapIMG.find(id);
	if( finder != g_pImagePool->m_mapIMG.end() )
		return (LPIMAGESET) (*finder).second;

	return NULL;
}

CString BuildMBCSInfo( CString& strTEXT)
{
	CString strRESULT(_T(""));

	for( int i=0; i<strTEXT.GetLength(); i++)
	{
		BYTE bCODE = strTEXT.GetAt(i);

		if(MBCS_LEAD_TEST(bCODE))
		{
			strRESULT.AppendChar(TCHAR_MBCS_LEAD);
			strRESULT.AppendChar(TCHAR_MBCS_TAIL);
			i++;
		}
		else
			strRESULT.AppendChar(TCHAR_MBCS_SINGLE);
	}

	return strRESULT;
}
