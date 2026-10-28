#pragma once

#ifdef FOURSTORY_CLIENT_MOD
#ifdef FOURSTORY_CLIENT_NATIVE
#define OPTION_COUNT 23
#else
#define OPTION_COUNT 22
#endif
#else
#define OPTION_COUNT 18
#endif

class CTGameSettingFrame : public ITInnerFrame
{
public:
	TList*		m_pTLIST;
	TButton** m_pTCHECK;
	
	static LPBYTE TOPTIONPTR[OPTION_COUNT];
	static DWORD TNAME[OPTION_COUNT];

public:
	virtual void RequestInfo()	{}
	virtual void ResetInfo()	{}

public:
	virtual void OnLButtonDown( UINT nFlags, CPoint pt);
	virtual HRESULT Render(DWORD dwTickCount);

public:
	void UpdateUI();
#ifdef FOURSTORY_CLIENT_MOD
	// Rows added by this project: two checkboxes, then two rows that open a
	// list (interface scale, font).
	static void CreateModPopups( CTGameSettingFrame *pFrame, FRAMEDESC_SHAREDPTR pDesc);
	void RefreshModLabels();
	virtual void ShowComponent( BOOL bVisible = TRUE);
	virtual void OnLButtonUp( UINT nFlags, CPoint pt);
#endif

public:
	CTGameSettingFrame(TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc);
	virtual ~CTGameSettingFrame();
};