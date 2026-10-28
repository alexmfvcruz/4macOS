#pragma once

class CTTextPopup : public TFrame
{
public:
	CTTextPopup( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc);
	virtual ~CTTextPopup();
	void ClearBlock();
	void SetText( CString& strText );
#ifdef FOURSTORY_CLIENT_NATIVE
	// MSVC binds temporaries to the non-const reference; SetText only reads it.
	void SetText( const CString& strText ) { SetText( const_cast<CString&>(strText)); }
#endif
	void RealignPos();
	HRESULT Render(DWORD dwTickCount);

protected:
	TComponent* m_pTOP;
	TComponent* m_pBOTTOM;
	TComponent* m_pMIDDLE;
	TComponent* m_pTEXT;
	CPoint m_ptTEXTOffset;
	INT m_nHeightBLOCK;
	CSize m_sizeTEXT;
	TCOMP_ARRAY m_vBLOCK;
	TCOMP_ARRAY m_vTEXT;
};