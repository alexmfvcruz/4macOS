#pragma once

class CTDetailNameDlg : public CTClientUIBase
{
public:
	CTDetailNameDlg( TComponent*pParent, FRAMEDESC_SHAREDPTR pDesc);
	virtual ~CTDetailNameDlg();

	void ResetText( CString& );
#ifdef FOURSTORY_CLIENT_NATIVE
	// MSVC binds temporaries to the non-const reference; ResetText only reads it.
	void ResetText( const CString& str) { ResetText( const_cast<CString&>(str)); }
#endif
	void ClearText();
	void SetPos( POINT );

	virtual BOOL CanWithItemUI();

protected:
	INT					m_nDefaultWidth;
	INT					m_nHeight;

	TComponent*		m_pMID;
	TComponent*		m_pHEAD;
	TComponent*		m_pTAIL;
	TComponent*		m_pTEXT;

	TCOMP_LIST			m_vExtBLOCK;
	INT					m_nBlockWidth;

	INT					m_nTextWidth;

	void AddBlock( INT nCount );
	void RemoveBlock( INT nCount );
	void ReAlignBlock();
};