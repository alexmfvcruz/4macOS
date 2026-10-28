#pragma once


class CTClientUIBase : public TFrame
{
public:
	static BYTE m_bDragLock;
	static void ResetBasis( INT nWidth, INT nHeight);
	static CPoint m_vBasis[9];
	static CPoint m_vDefaultBASIS[9];
#ifdef FOURSTORY_CLIENT_NATIVE
	// The constructor sets m_bBasisPoint to TBASISPOINT_COUNT, and the first
	// MoveComponent reads m_vBasis[TBASISPOINT_COUNT], one element past the
	// array. MSVC places m_vDefaultBASIS directly after m_vBasis
	// (TClientUIBase.cpp:5 and :20, both CPoint[9]), so that read returns
	// m_vDefaultBASIS[0]. This returns the same value without the overrun.
	static CPoint BasisOrNext( BYTE bBasisPoint)
	{
		return bBasisPoint < TBASISPOINT_COUNT ? m_vBasis[bBasisPoint] : m_vDefaultBASIS[bBasisPoint - TBASISPOINT_COUNT];
	}
#endif
	static CRect m_rtDEFAULT_SCREEN;
	static CRect GetScreenRect();
	static CPoint GetUIBasisPos( BYTE bBasisPoint, CPoint vOffset );
	static void GetUIOffset( CRect rtScreen, CRect rtComp, TFRAMEOFFSET* pFRAMEOFFSET );

public:
	BYTE m_bDropTarget;
	BYTE m_bCanDrag;
	CPoint m_ptDrag;
	CRect m_rcDrag;
	BYTE m_bDraging;
	BYTE m_bBasisPoint;
	BOOL m_bUseBasis;
	CPoint m_vCompOffset;
	BYTE m_bDragRaid;

public:
	virtual void OnLButtonDown(UINT nFlags, CPoint pt);
	virtual void OnLButtonUp(UINT nFlags, CPoint pt);
	virtual void OnMouseMove(UINT nFlags, CPoint pt);

	virtual ITDetailInfoPtr GetTInfoKey( const CPoint& point );
	virtual BOOL GetTChatInfo( const CPoint& point, TCHATINFO& outInfo );

	virtual TDROPINFO OnDrop( CPoint point);
	virtual BYTE OnBeginDrag(
		LPTDRAG pDRAG,
		CPoint point);

	virtual UINT GetUniqueID();
	virtual BOOL CanWithItemUI();
	virtual BOOL CanDisable();
	virtual	void DefaultPosition( CPoint* vBASIS, BOOL bRestore );
	virtual void ResetPosition();
	virtual void MoveComponent( CPoint pt );

	virtual void SmoothCallback(std::vector<SmoothData> vComponent, std::vector<SmoothData> vImage);
public:
	CTClientUIBase( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc);
	virtual ~CTClientUIBase();
};
