#include "Stdafx.h"
#ifdef FOURSTORY_CLIENT_NATIVE
#include <fourstory_display_preview.h>
#include <fourstory_audio_settings.h>
#endif
#include "TGameSettingFrame.h"
#include "TClientGame.h"
#include "TClientWnd.h"
#include "TClient.h"


#ifdef FOURSTORY_CLIENT_MOD
// The list rows are checkboxes bound to a byte; the two rows that open a list
// (interface scale, font) use these as their click detectors.
#define TMOD_POPUP_ROWS 2
static BYTE s_bModPopupRow[TMOD_POPUP_ROWS] = { FALSE, FALSE};
static CTMiniPopupDlg *s_pModPopup[TMOD_POPUP_ROWS] = { NULL, NULL};
#endif
LPBYTE CTGameSettingFrame::TOPTIONPTR[OPTION_COUNT] =
{
	&CTClientGame::m_vTOPTION.m_bNpcNAME,
	&CTClientGame::m_vTOPTION.m_bPcNAME,
	&CTClientGame::m_vTOPTION.m_bMonNAME,
	&CTClientGame::m_vTOPTION.m_bAutoHelp,
	&CTClientGame::m_vTOPTION.m_bTalkBox,
	&CTClientGame::m_vTOPTION.m_bDenyWhisper,
	&CTClientGame::m_vTOPTION.m_bDenyCommunity,
	&CTClientGame::m_vTOPTION.m_bUIBar,
	&CTClientGame::m_vTOPTION.m_bContinualChat,
	&CTClientGame::m_vTOPTION.m_bMouseClickMove,
	&CTClientGame::m_vTOPTION.m_bAutoTargeting,
	&CTClientGame::m_vTOPTION.m_bDisableDoubleSkill,
	&CTClientGame::m_vTOPTION.m_bDontShowOtherCompanions,
	&CTClientGame::m_vTOPTION.m_bHideTournament,
	&CTClientGame::m_vTOPTION.m_bHideArena,
	&CTClientGame::m_vTOPTION.m_bHideDuel,
	&CTClientGame::m_vTOPTION.m_bDisableAutoSelectSFX,
	&CTClientGame::m_vTOPTION.m_bDisableChatTransparency,
#ifdef FOURSTORY_CLIENT_MOD
	&CTClientWnd::m_bInvertWheel,
	&CD3DDevice::m_bAnisotropic,
#ifdef FOURSTORY_CLIENT_NATIVE
	&fourstory_spatial_audio_option,
#endif
	&s_bModPopupRow[0],
	&s_bModPopupRow[1],
#endif
};

DWORD CTGameSettingFrame::TNAME[OPTION_COUNT] = 
{
	TSTR_GAMEOPTION_SHOW_NPC_NAME, //NPC 이름 표시
	TSTR_GAMEOPTION_SHOW_PLAYER_NAME, //캐릭터 이름 표시
	TSTR_GAMEOPTION_SHOW_MONSTER_NAME, //몬스터 이름 표시
	TSTR_GAMEOPTION_AUTO_HELP, //자동 도움말
	TSTR_GAMEOPTION_SHOW_TALK_BOX, //말풍선 보이기
	TSTR_GAMEOPTION_DENY_WHISPER, //귓말 거부
	TSTR_GAMEOPTION_DENY_COMMUNITY, //커뮤니티 거부
	TSTR_GAMEOPTION_QUICKBAR, //HUD 보이기
	TSTR_GAMEOPTION_CONTINUOUS_CHAT, //연속 채팅
	TSTR_GAMEOPTION_MOUSE_CLICK_MOVE, //마우스 클릭 이동
	TSTR_GAMEOPTION_AUTO_TARGETING, //자동 타겟팅 사용
	3000,
	3001,
	3002,
	3003,
	3004,
	3005,
	3008
#ifdef FOURSTORY_CLIENT_MOD
	,0 // our own options have no chart string; see TMODNAME
#ifdef FOURSTORY_CLIENT_NATIVE
	,0
#endif
	,0
	,0
	,0
#endif
};
#ifdef FOURSTORY_CLIENT_MOD
// Labels of the options added by this project, in list order after the originals.
static LPCTSTR TMODNAME[] =
{
	_T("Invert mouse wheel"),
	_T("Anisotropic filtering"),
#ifdef FOURSTORY_CLIENT_NATIVE
	_T("Spatial audio (headphones)"),
#endif
	_T("Interface scale"),
	_T("Font"),
};
#endif

// ===============================================================================
CTGameSettingFrame::CTGameSettingFrame(TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc)
	: ITInnerFrame(pParent, pDesc, TSETTING_FRAME_GAME)
{
#ifdef FOURSTORY_CLIENT_NATIVE
	fourstory_spatial_audio_refresh();
#endif
	m_pTLIST = (TList*) FindKid( ID_CTRLINST_LIST );

	for( int i=0 ; i < OPTION_COUNT ; ++i )
	{
#ifdef FOURSTORY_CLIENT_MOD
		int nLine = m_pTLIST->AddString( CTGameSettingFrame::TNAME[i] ? CTChart::LoadString( (TSTRING) CTGameSettingFrame::TNAME[i] ) : CString(TMODNAME[ i - (OPTION_COUNT - (int)(sizeof(TMODNAME) / sizeof(TMODNAME[0]))) ]) );
#else
		int nLine = m_pTLIST->AddString( CTChart::LoadString( (TSTRING) CTGameSettingFrame::TNAME[i] ) );
#endif
		m_pTLIST->SetItemData( nLine, 0, i );
	}

	CPoint ptBasisPos;
	m_pTLIST->GetComponentPos(&ptBasisPos);
    
	TButton* pTCHECK = (TButton*) FindKid( ID_CTRLINST_CHECK );
	RemoveKid(pTCHECK);

	CPoint ptOffset;
	pTCHECK->GetComponentPos(&ptOffset);
	ptOffset = ptOffset - ptBasisPos;
	int n = m_pTLIST->GetMaxItemHeight();
	int nTotal = m_pTLIST->GetItemPerPage();
	m_pTCHECK = new TButton*[nTotal];

	for( int i=0 ; i < nTotal ; ++i )
	{
		m_pTCHECK[ i ] = new TButton( this, pTCHECK->m_pDESC );
		m_pTCHECK[ i ]->m_id = GetUniqueID();
		m_pTCHECK[ i ]->SetStateButton();
		m_pTCHECK[ i ]->MoveComponent( ptBasisPos + ptOffset );
		AddKid( m_pTCHECK[ i ] );
		ptOffset.y += n;
	}

	delete pTCHECK;

	UpdateUI();
#ifdef FOURSTORY_CLIENT_MOD
	RefreshModLabels();
#endif
}
// -------------------------------------------------------------------------------
CTGameSettingFrame::~CTGameSettingFrame()
{
	if( m_pTCHECK )
		delete [] m_pTCHECK;
}
// ===============================================================================
void CTGameSettingFrame::UpdateUI()
{
	for( INT i=0 ; i < m_pTLIST->GetItemPerPage() ; ++i )
	{
		BOOL bValid = ( m_pTLIST->GetTop() + i < m_pTLIST->GetItemCount() );
		if( bValid )
		{
			m_pTCHECK[ i ]->Select( *(CTGameSettingFrame::TOPTIONPTR[ m_pTLIST->GetTop()+i ]) );
			RemoveKid(m_pTCHECK[ i ] );
			AddKid(m_pTCHECK[ i ] );
			m_pTCHECK[ i ]->ShowComponent(TRUE);
		}
		else
		{
			m_pTCHECK[ i ]->ShowComponent(FALSE);
		}
	}
}
// ===============================================================================
#ifdef FOURSTORY_CLIENT_MOD
static void ToggleModPopup( int nPopup, CPoint pt);
#endif
void CTGameSettingFrame::OnLButtonDown( UINT nFlags, CPoint pt)
{
#ifdef FOURSTORY_CLIENT_MOD
	for( int nPopup=0; nPopup<TMOD_POPUP_ROWS; nPopup++)
		if( s_pModPopup[nPopup] && s_pModPopup[nPopup]->IsVisible() && s_pModPopup[nPopup]->HitTest(pt) )
		{
			s_pModPopup[nPopup]->OnLButtonDown( nFlags, pt);
			return;
		}

#endif
#ifdef FOURSTORY_CLIENT_NATIVE
	// Checkbox controls are reused for visible rows. Synchronize their mapping
	// before input, and never commit old checkbox states after scrolling.
	UpdateUI();
	const int nPreviousTop = m_pTLIST->GetTop();
#endif
	ITInnerFrame::OnLButtonDown(nFlags, pt);
#ifdef FOURSTORY_CLIENT_NATIVE
	if( nPreviousTop != m_pTLIST->GetTop() )
	{
		UpdateUI();
		return;
	}
#endif

	if( IsVisible() )
	{
		for( int i=0 ; i < m_pTLIST->GetItemPerPage() ; ++i )
		{
			BOOL bChecked = m_pTCHECK[ i ]->IsStateDown();

			BOOL bValid = ( m_pTLIST->GetTop() + i < m_pTLIST->GetItemCount() );
			if( bValid )
			{
				int index = m_pTLIST->GetTop() + i;
				BOOL bChecked = m_pTCHECK[i]->IsStateDown() ? TRUE : FALSE;
				BOOL bCurrent = *(CTGameSettingFrame::TOPTIONPTR[ index ]);
				CTGaugePannel* pTGAUGE = static_cast<CTGaugePannel*>( CTClientGame::GetInstance()->GetFrame( TFRAME_GAUGE ) );

				if( bChecked != bCurrent && index == 0 ) // 등급표시 변경
				{
					CTGaugePannel* pTGAUGE = static_cast<CTGaugePannel*>( CTClientGame::GetInstance()->GetFrame( TFRAME_GAUGE ) );

					if( bChecked  )
						pTGAUGE->AttachCaution15();
					else
						pTGAUGE->DettachCaution15();
				}

				CTClientGame* pGAME = CTClientGame::GetInstance();
				if (index == 12 && bChecked != bCurrent && bChecked)
				{
					for (MAPSPOLECNIK::iterator it = pGAME->m_mapSPOLECNIK.begin(); it != pGAME->m_mapSPOLECNIK.end(); ++it)
					{
						if ((*it).second->m_dwHostID == pGAME->GetMainChar()->m_dwID)
							continue;

						pGAME->DeleteDynimicOBJ((*it).second);
						pGAME->m_mapSPOLECNIK.erase(it);
					}
				}
				else if (index == 12 && bChecked != bCurrent && !bChecked)
					pGAME->GetMainWnd()->MessageBoxOK("You have to teleport for this to be applied.", TSTR_OK, GM_CLOSE_MSGBOX, GM_CLOSE_MSGBOX);

				else if (index == 3 && bChecked != bCurrent)
					pTGAUGE->NotifyFrameStaticMove(bChecked);

#ifdef FOURSTORY_CLIENT_MOD
				if( index >= OPTION_COUNT - TMOD_POPUP_ROWS )
				{
					if( bChecked != bCurrent )
						ToggleModPopup( index - (OPTION_COUNT - TMOD_POPUP_ROWS), pt);

					continue;
				}
#endif
				*(CTGameSettingFrame::TOPTIONPTR[ index ]) = bChecked;
#ifdef FOURSTORY_CLIENT_NATIVE
				if( bChecked != bCurrent && index == OPTION_COUNT - TMOD_POPUP_ROWS - 1 )
					fourstory_spatial_audio_apply();
#endif
				if (bChecked != bCurrent)
					theApp.SaveStdProfileSettings();
			}
		}
	}
}
// ===============================================================================

HRESULT CTGameSettingFrame::Render(DWORD dwTickCount)
{
	if( IsVisible() )
	{
		UpdateUI();
	}

#ifdef FOURSTORY_CLIENT_MOD
	HRESULT hRESULT = ITInnerFrame::Render(dwTickCount);

	// The rows are brought to the front every frame (UpdateUI); an open list is
	// drawn once more so that it stays above them.
	for( int nPopup=0; nPopup<TMOD_POPUP_ROWS; nPopup++)
		if( s_pModPopup[nPopup] && s_pModPopup[nPopup]->IsVisible() )
			s_pModPopup[nPopup]->Render(dwTickCount);

	return hRESULT;
#else
	return ITInnerFrame::Render(dwTickCount);
#endif
}
#ifdef FOURSTORY_CLIENT_MOD

// ===============================================================================
// Interface scale and font lists (rows added by this project).
static CTGameSettingFrame *s_pModFrame = NULL;
static std::vector<CString> s_vModFont;

static void OnModScaleBtn( CTMiniPopupDlg *pPopupDlg, CTMiniPopupDlg::CTButtonMP *pButton)
{
	FLOAT fScale = FLOAT(pButton->GetUserData()) / 100.0f;

	pPopupDlg->ShowComponent(FALSE);
	CTClientGame::GetInstance()->ApplyUIScale(fScale);

	if(s_pModFrame)
		s_pModFrame->RefreshModLabels();

	theApp.SaveStdProfileSettings();
}

static void OnModFontBtn( CTMiniPopupDlg *pPopupDlg, CTMiniPopupDlg::CTButtonMP *pButton)
{
	DWORD dwIndex = pButton->GetUserData();

	pPopupDlg->ShowComponent(FALSE);
	if( dwIndex >= DWORD(s_vModFont.size()) )
		return;

#ifdef FOURSTORY_CLIENT_NATIVE
	fourstory_display_preview_begin(CTClientGame::GetInstance());
#endif
	// "Font" is the setting the original client reads at start (SetForceTachyonFont).
	theApp.WriteProfileString( _T("Settings"), _T("Font"), s_vModFont[dwIndex]);
	ReloadTachyonFonts(s_vModFont[dwIndex]);

	if(s_pModFrame)
		s_pModFrame->RefreshModLabels();
}

static void ToggleModPopup( int nPopup, CPoint pt)
{
	CTMiniPopupDlg *pPopup = s_pModPopup[nPopup];

	if( !pPopup || !s_pModFrame )
		return;

	if(s_pModPopup[!nPopup])
		s_pModPopup[!nPopup]->ShowComponent(FALSE);

	if(pPopup->IsVisible())
	{
		pPopup->ShowComponent(FALSE);
		return;
	}

	// Keep the list inside the settings frame so that it receives the clicks.
	CRect rcFrame;
	CRect rcPopup;

	s_pModFrame->GetComponentRect(&rcFrame);
	pPopup->GetComponentRect(&rcPopup);
	s_pModFrame->ScreenToComponent(&pt);
	pt.x = max( 0, min( (int) pt.x, rcFrame.Width() - rcPopup.Width()));
	pt.y = max( 0, min( (int) pt.y, rcFrame.Height() - rcPopup.Height()));
	pPopup->MoveComponent(pt);
	pPopup->ShowComponent(TRUE);
}

void CTGameSettingFrame::CreateModPopups( CTGameSettingFrame *pFrame, FRAMEDESC_SHAREDPTR pDesc)
{
	static const DWORD vPERCENT[] = { 100, 110, 120, 125, 130, 140, 150, 175, 200, 225, 250, 300};
	CString strTEXT;

	if( !pFrame || s_pModFrame )
		return;
	s_pModFrame = pFrame;

	for( int i=0; i<TMOD_POPUP_ROWS; i++)
	{
		s_pModPopup[i] = new CTMiniPopupDlg( pFrame, pDesc, FALSE);
		pFrame->AddKid(s_pModPopup[i]);
		s_pModPopup[i]->ShowComponent(FALSE);
	}

	for(auto i=0; i<int(sizeof(vPERCENT) / sizeof(vPERCENT[0])); i++)
	{
		strTEXT.Format( _T("%d%%"), vPERCENT[i]);
		s_pModPopup[0]->AddButton( strTEXT, OnModScaleBtn, vPERCENT[i]);
	}

	// Fonts: the default faces, then Fonts\fonts.txt next to the executable, one
	// "Face name|file" per line. The file part is empty for fonts installed in
	// Windows; other files are registered for this process only.
	s_vModFont.push_back(CString(_T("")));
	s_pModPopup[1]->AddButton( CString(_T("Default")), OnModFontBtn, 0);

	CStdioFile vFILE;
	if(vFILE.Open( _T("Fonts\\fonts.txt"), CFile::modeRead|CFile::typeText|CFile::shareDenyNone))
	{
		CString strLINE;

		while( vFILE.ReadString(strLINE) && s_vModFont.size() < 16 )
		{
			strLINE.Trim();
			if( strLINE.IsEmpty() || strLINE[0] == _T('#') )
				continue;

			int nSPLIT = strLINE.Find(_T('|'));
			CString strFACE = nSPLIT < 0 ? strLINE : strLINE.Left(nSPLIT);
			CString strFONTFILE = nSPLIT < 0 ? CString(_T("")) : strLINE.Mid(nSPLIT + 1);

			strFACE.Trim();
			strFONTFILE.Trim();
			if( strFACE.IsEmpty() || strFACE.GetLength() >= LF_FACESIZE )
				continue;

			// GDI creates the font from the file; the engine draws text with GDI+,
			// which needs the file registered separately.
			if( !strFONTFILE.IsEmpty() && (
				!AddFontResourceEx( CString(_T("Fonts\\")) + strFONTFILE, FR_PRIVATE, NULL) ||
				!CD3DFont::AddPrivateFontFile(CString(_T("Fonts\\")) + strFONTFILE)) )
				continue;

			s_pModPopup[1]->AddButton( strFACE, OnModFontBtn, DWORD(s_vModFont.size()));
			s_vModFont.push_back(strFACE);
		}

		vFILE.Close();
	}

	// A font saved in an earlier session was created before its file was registered.
	strTEXT = theApp.GetProfileString( _T("Settings"), _T("Font"), _T(""));
	if(!strTEXT.IsEmpty())
		ReloadTachyonFonts(strTEXT);

	pFrame->RefreshModLabels();
}

void CTGameSettingFrame::RefreshModLabels()
{
	int nLINE = OPTION_COUNT - TMOD_POPUP_ROWS;
	CString strFONT = theApp.GetProfileString( _T("Settings"), _T("Font"), _T(""));
	CString strTEXT;

	if( !m_pTLIST || m_pTLIST->GetItemCount() < OPTION_COUNT )
		return;

	strTEXT.Format( _T("Interface scale: %d%%"), INT(CD3DDevice::m_fUIScale * 100.0f + 0.5f));
	m_pTLIST->SetItemString( nLINE, 0, strTEXT);
	strTEXT.Format( _T("Font: %s"), strFONT.IsEmpty() ? _T("Default") : LPCTSTR(strFONT));
	m_pTLIST->SetItemString( nLINE + 1, 0, strTEXT);
}

void CTGameSettingFrame::OnLButtonUp( UINT nFlags, CPoint pt)
{
	for( int nPopup=0; nPopup<TMOD_POPUP_ROWS; nPopup++)
		if( s_pModPopup[nPopup] && s_pModPopup[nPopup]->IsVisible() && s_pModPopup[nPopup]->HitTest(pt) )
		{
			s_pModPopup[nPopup]->OnLButtonUp( nFlags, pt);
			return;
		}

	ITInnerFrame::OnLButtonUp( nFlags, pt);
}

void CTGameSettingFrame::ShowComponent( BOOL bVisible)
{
	ITInnerFrame::ShowComponent(bVisible);

	// Showing the frame shows every child; the lists open on request only.
	for( int i=0; i<TMOD_POPUP_ROWS; i++)
		if(s_pModPopup[i])
			s_pModPopup[i]->ShowComponent(FALSE);
}
#endif
