// C7 native media dispatch, adapted from the original TachyonMedia.cpp.
#include <fourstory_engine_ui.h>
#include "../../audio/backend.h"
#include <cstdlib>
#include <cstdio>
#include <cstring>
void fourstory_sound_ui_cue(CD3DSound*, bool opening);
namespace {
constexpr DWORD uiOpen=0x49, uiClose=0x4a;
bool uiTrace() {
    const char* trace=std::getenv("FOURSTORY_AUDIO_TRACE");
    return trace && std::strcmp(trace,"1")==0;
}
}

BYTE CTachyonMedia::m_bMasterVolume = VOLUME_MAX;
BYTE CTachyonMedia::m_bBACK = FALSE;
BYTE CTachyonMedia::m_bON = TRUE;


CTachyonMedia::CTachyonMedia()
{
	m_mapDSOUND.clear();
	m_mapDMUSIC.clear();
	m_mapDSHOW.clear();

	m_pDS = NULL;
}

CTachyonMedia::~CTachyonMedia()
{
	ReleaseAll();
}

BYTE CTachyonMedia::InitMEDIA(HWND, DWORD, DWORD, DWORD, DWORD)
{
    D3DXMatrixIdentity(&CD3DSound::m_vLISTENER);
    if(!fourstory_audio::initialize()) return FALSE;
    if(AfxGetApp()) fourstory_audio::set_spatial(AfxGetApp()->GetProfileInt("Settings","SpatialAudio",0)!=0);
    return TRUE;
}

void CTachyonMedia::ReleaseAll()
{
	m_bPendingUIOpen = m_bPendingUIClose = false;
	MAPMEDIA::iterator it;

	for( it = m_mapDMUSIC.begin(); it != m_mapDMUSIC.end(); it++)
		if((*it).second)
			delete ((CT3DMusic *) (*it).second);

	for( it = m_mapDSOUND.begin(); it != m_mapDSOUND.end(); it++)
		if((*it).second)
			delete ((CD3DSound *) (*it).second);

	for( it = m_mapDSHOW.begin(); it != m_mapDSHOW.end(); it++)
		if((*it).second)
			delete ((CTShow *) (*it).second);

	CD3DSound::ClearGARBAGE();
	m_mapDMUSIC.clear();
	m_mapDSOUND.clear();
	m_mapDSHOW.clear();

	fourstory_audio::shutdown();
}

LPVOID CTachyonMedia::GetData( BYTE bType, DWORD dwID)
{
	switch(bType)
	{
	case MEDIA_TMUSIC	:
		{
			MAPMEDIA::iterator finder = m_mapDMUSIC.find(dwID);

			if( finder != m_mapDMUSIC.end() )
				return (*finder).second;
		}

		break;

	case MEDIA_TSOUND	:
		{
			MAPMEDIA::iterator finder = m_mapDSOUND.find(dwID);

			if( finder != m_mapDSOUND.end() )
				return (*finder).second;
		}

		break;

	case MEDIA_TSHOW	:
		{
			MAPMEDIA::iterator finder = m_mapDSHOW.find(dwID);

			if( finder != m_mapDSHOW.end() )
				return (*finder).second;
		}

		break;
	}

	return NULL;
}

void CTachyonMedia::Unregister( BYTE bType, DWORD dwID)
{
    if(bType==MEDIA_TSOUND) {
        if(dwID==uiOpen) m_bPendingUIOpen=false;
        if(dwID==uiClose) m_bPendingUIClose=false;
    }
	switch(bType)
	{
	case MEDIA_TMUSIC	:
		{
			MAPMEDIA::iterator finder = m_mapDMUSIC.find(dwID);

			if( finder != m_mapDMUSIC.end() )
			{
				delete ((CT3DMusic *) (*finder).second);
				m_mapDMUSIC.erase(finder);
			}
		}

		break;

	case MEDIA_TSOUND	:
		{
			MAPMEDIA::iterator finder = m_mapDSOUND.find(dwID);

			if( finder != m_mapDSOUND.end() )
			{
				delete ((CD3DSound *) (*finder).second);
				m_mapDSOUND.erase(finder);
			}
		}

		break;

	case MEDIA_TSHOW	:
		{
			MAPMEDIA::iterator finder = m_mapDSHOW.find(dwID);

			if( finder != m_mapDSHOW.end() )
			{
				delete ((CTShow *) (*finder).second);
				m_mapDSHOW.erase(finder);
			}
		}

		break;
	}
}

void CTachyonMedia::Register( BYTE bType,
							  DWORD dwID,
							  LPVOID pItem)
{
	switch(bType)
	{
	case MEDIA_TMUSIC	:
		{
			m_mapDMUSIC.insert( MAPMEDIA::value_type(
				dwID,
				pItem));
		}

		break;

	case MEDIA_TSOUND	:
		{
			m_mapDSOUND.insert( MAPMEDIA::value_type(
				dwID,
				pItem));
		}

		break;

	case MEDIA_TSHOW	:
		{
			m_mapDSHOW.insert( MAPMEDIA::value_type(
				dwID,
				pItem));
		}

		break;
	}
}

void CTachyonMedia::ResetVolume()
{
	MAPMEDIA::iterator it;

	for( it = m_mapDMUSIC.begin(); it != m_mapDMUSIC.end(); it++)
		((CT3DMusic *) (*it).second)->ResetVolume();

	for( it = m_mapDSOUND.begin(); it != m_mapDSOUND.end(); it++)
		((CD3DSound *) (*it).second)->ResetVolume();

	for( it = m_mapDSHOW.begin(); it != m_mapDSHOW.end(); it++)
		((CTShow *) (*it).second)->ResetVolume();
}

void CTachyonMedia::ResetVolume( BYTE bType)
{
	switch(bType)
	{
	case MEDIA_TMUSIC	:
		{
			MAPMEDIA::iterator it;

			for( it = m_mapDMUSIC.begin(); it != m_mapDMUSIC.end(); it++)
				((CT3DMusic *) (*it).second)->ResetVolume();
		}

		break;

	case MEDIA_TSOUND	:
		{
			MAPMEDIA::iterator it;

			for( it = m_mapDSOUND.begin(); it != m_mapDSOUND.end(); it++)
				((CD3DSound *) (*it).second)->ResetVolume();
		}

		break;

	case MEDIA_TSHOW	:
		{
			MAPMEDIA::iterator it;

			for( it = m_mapDSHOW.begin(); it != m_mapDSHOW.end(); it++)
				((CTShow *) (*it).second)->ResetVolume();
		}

		break;
	}
}

void CTachyonMedia::ResetVolume( BYTE bType, DWORD dwID)
{
	switch(bType)
	{
	case MEDIA_TMUSIC	:
		{
			MAPMEDIA::iterator finder = m_mapDMUSIC.find(dwID);

			if( finder != m_mapDMUSIC.end() )
				((CT3DMusic *) (*finder).second)->ResetVolume();
		}

		break;

	case MEDIA_TSOUND	:
		{
			MAPMEDIA::iterator finder = m_mapDSOUND.find(dwID);

			if( finder != m_mapDSOUND.end() )
				((CD3DSound *) (*finder).second)->ResetVolume();
		}

		break;

	case MEDIA_TSHOW	:
		{
			MAPMEDIA::iterator finder = m_mapDSHOW.find(dwID);

			if( finder != m_mapDSHOW.end() )
				((CTShow *) (*finder).second)->ResetVolume();
		}

		break;
	}
}

void CTachyonMedia::ResetVolume( BYTE bType, DWORD dwID, int nIndex)
{
	switch(bType)
	{
	case MEDIA_TMUSIC	:
		{
			MAPMEDIA::iterator finder = m_mapDMUSIC.find(dwID);

			if( finder != m_mapDMUSIC.end() )
				((CT3DMusic *) (*finder).second)->ResetVolume();
		}

		break;

	case MEDIA_TSOUND	:
		{
			MAPMEDIA::iterator finder = m_mapDSOUND.find(dwID);

			if( finder != m_mapDSOUND.end() )
				((CD3DSound *) (*finder).second)->ResetVolume(nIndex);
		}

		break;

	case MEDIA_TSHOW	:
		{
			MAPMEDIA::iterator finder = m_mapDSHOW.find(dwID);

			if( finder != m_mapDSHOW.end() )
				((CTShow *) (*finder).second)->ResetVolume();
		}

		break;
	}
}

void CTachyonMedia::LoadDShow( CWnd *pHost,
							   CWnd *pWnd,
							   DWORD dwID,
							   DWORD dwMessage,
							   LPTSTR szFileName,
							   BYTE bVolume)
{
	CTShow *pItem = new CTShow();

	pItem->InitDShow(
		pHost,
		pWnd,
		szFileName,
		dwMessage);
	pItem->m_bVolume = bVolume;

	Register( MEDIA_TSHOW, dwID, (LPVOID) pItem);
}

void CTachyonMedia::LoadDMusic( DWORD dwID,
							    LPTSTR szFileName,
								BYTE bVolume)
{
	CT3DMusic *pItem = new CT3DMusic();

	pItem->InitMusic(szFileName);
	pItem->m_bVolume = bVolume;
	Register( MEDIA_TMUSIC, dwID, pItem);
}

void CTachyonMedia::LoadDSound( DWORD dwID,
								LPTSTR szFileName,
								BYTE bVolume)
{
	CD3DSound *pItem = new CD3DSound();

	pItem->Initialize(
		m_pDS,
		szFileName);
	pItem->m_bVolume = bVolume;
    // Both window cues are monophonic; only the open asset is treated.
    if(dwID==uiOpen || dwID==uiClose) fourstory_sound_ui_cue(pItem,dwID==uiOpen);

	Register( MEDIA_TSOUND, dwID, pItem);
}

void CTachyonMedia::SetPos( BYTE bType,
						    DWORD dwID,
							int nIndex,
							LONGLONG nPos)
{
	switch(bType)
	{
	case MEDIA_TMUSIC	:
		{
			MAPMEDIA::iterator finder = m_mapDMUSIC.find(dwID);

			if( finder != m_mapDMUSIC.end() )
				((CT3DMusic *) (*finder).second)->SetPos((DWORD) nPos);
		}

		break;

	case MEDIA_TSOUND	:
		{
			MAPMEDIA::iterator finder = m_mapDSOUND.find(dwID);

			if( finder != m_mapDSOUND.end() )
				((CD3DSound *) (*finder).second)->SetPos( nIndex, (DWORD) nPos);
		}

		break;

	case MEDIA_TSHOW	:
		{
			MAPMEDIA::iterator finder = m_mapDSHOW.find(dwID);

			if( finder != m_mapDSHOW.end() )
				((CTShow *) (*finder).second)->SetPos(nPos);
		}

		break;
	}
}

LONGLONG CTachyonMedia::GetLength( BYTE bType, DWORD dwID)
{
	switch(bType)
	{
	case MEDIA_TMUSIC	:
		{
			MAPMEDIA::iterator finder = m_mapDMUSIC.find(dwID);

			if( finder != m_mapDMUSIC.end() )
				return ((CT3DMusic *) (*finder).second)->GetLength();
		}

		break;

	case MEDIA_TSOUND	:
		{
			MAPMEDIA::iterator finder = m_mapDSOUND.find(dwID);

			if( finder != m_mapDSOUND.end() )
				return ((CD3DSound *) (*finder).second)->GetLength();
		}

		break;

	case MEDIA_TSHOW	:
		{
			MAPMEDIA::iterator finder = m_mapDSHOW.find(dwID);

			if( finder != m_mapDSHOW.end() )
				return ((CTShow *) (*finder).second)->GetLength();
		}

		break;
	}

	return 0;
}

LONGLONG CTachyonMedia::GetPos( BYTE bType, DWORD dwID, int nIndex)
{
	switch(bType)
	{
	case MEDIA_TMUSIC	:
		{
			MAPMEDIA::iterator finder = m_mapDMUSIC.find(dwID);

			if( finder != m_mapDMUSIC.end() )
				return ((CT3DMusic *) (*finder).second)->GetPos();
		}

		break;

	case MEDIA_TSOUND	:
		{
			MAPMEDIA::iterator finder = m_mapDSOUND.find(dwID);

			if( finder != m_mapDSOUND.end() )
				return ((CD3DSound *) (*finder).second)->GetPos(nIndex);
		}

		break;

	case MEDIA_TSHOW	:
		{
			MAPMEDIA::iterator finder = m_mapDSHOW.find(dwID);

			if( finder != m_mapDSHOW.end() )
				return ((CTShow *) (*finder).second)->GetPos();
		}

		break;
	}

	return 0;
}

void CTachyonMedia::Toggle( BYTE bType, DWORD dwID)
{
	switch(bType)
	{
	case MEDIA_TSHOW	:
		{
			MAPMEDIA::iterator finder = m_mapDSHOW.find(dwID);

			if( finder != m_mapDSHOW.end() )
				((CTShow *) (*finder).second)->Toggle();
		}

		break;
	}
}

void CTachyonMedia::Pause( BYTE bType, DWORD dwID, int nIndex)
{
	switch(bType)
	{
	case MEDIA_TMUSIC	:
		{
			MAPMEDIA::iterator finder = m_mapDMUSIC.find(dwID);

			if( finder != m_mapDMUSIC.end() )
				((CT3DMusic *) (*finder).second)->Pause();
		}

		break;

	case MEDIA_TSOUND	:
		{
			MAPMEDIA::iterator finder = m_mapDSOUND.find(dwID);

			if( finder != m_mapDSOUND.end() )
				((CD3DSound *) (*finder).second)->Pause(nIndex);
		}

		break;

	case MEDIA_TSHOW	:
		{
			MAPMEDIA::iterator finder = m_mapDSHOW.find(dwID);

			if( finder != m_mapDSHOW.end() )
				((CTShow *) (*finder).second)->Pause();
		}

		break;
	}
}

void CTachyonMedia::Play( BYTE bType, DWORD dwID, int nIndex)
{
	switch(bType)
	{
	case MEDIA_TMUSIC	:
		{
			MAPMEDIA::iterator finder = m_mapDMUSIC.find(dwID);

			if( finder != m_mapDMUSIC.end() )
				((CT3DMusic *) (*finder).second)->Play();
		}

		break;

	case MEDIA_TSOUND	:
		{
			MAPMEDIA::iterator finder = m_mapDSOUND.find(dwID);

			if( finder != m_mapDSOUND.end() )
				((CD3DSound *) (*finder).second)->Play(nIndex);
		}

		break;

	case MEDIA_TSHOW	:
		{
			MAPMEDIA::iterator finder = m_mapDSHOW.find(dwID);

			if( finder != m_mapDSHOW.end() )
				((CTShow *) (*finder).second)->Play();
		}

		break;
	}
}

int CTachyonMedia::Play( BYTE bType, DWORD dwID)
{
    if(bType==MEDIA_TSOUND && (dwID==uiOpen || dwID==uiClose)) {
        // Visibility propagation and tab selection may show/hide many panels.
        // Queue intent until presentation, rather than audibly playing each
        // intermediate state. No playback slot exists until FlushUISound.
        if(m_bON && CD3DSound::m_bON && !m_bBACK && m_mapDSOUND.count(dwID)) {
            if(dwID==uiOpen) m_bPendingUIOpen=true;
            else m_bPendingUIClose=true;
            if(uiTrace()) std::fprintf(stderr,"AUDIO_UI_REQUEST tick=%u id=%u\n",
                unsigned(GetTickCount()),unsigned(dwID));
        }
        return -1;
    }
	switch(bType)
	{
	case MEDIA_TMUSIC	:
		{
			MAPMEDIA::iterator finder = m_mapDMUSIC.find(dwID);

			if( finder != m_mapDMUSIC.end() )
				((CT3DMusic *) (*finder).second)->Play();
		}

		break;

	case MEDIA_TSOUND	:
		{
			MAPMEDIA::iterator finder = m_mapDSOUND.find(dwID);

			if( finder != m_mapDSOUND.end() )
				return ((CD3DSound *) (*finder).second)->Play();
		}

		break;

	case MEDIA_TSHOW	:
		{
			MAPMEDIA::iterator finder = m_mapDSHOW.find(dwID);

			if( finder != m_mapDSHOW.end() )
				((CTShow *) (*finder).second)->Play();
		}

		break;
	}

	return 0;
}

void CTachyonMedia::FlushUISound()
{
    // Showing a destination can hide other panels in the same transition.
    // Prefer one open cue; a close-only action retains one close cue.
    DWORD id=m_bPendingUIOpen ? uiOpen : (m_bPendingUIClose ? uiClose : 0);
    m_bPendingUIOpen=m_bPendingUIClose=false;
    if(!id || !m_bON || !CD3DSound::m_bON || m_bBACK) return;
    auto sound=m_mapDSOUND.find(id);
    if(sound==m_mapDSOUND.end()) return;
    // Adjacent frames may switch between opening and closing. These two UI
    // cues share one audible voice; combat and button sounds are unaffected.
    auto other=m_mapDSOUND.find(id==uiOpen ? uiClose : uiOpen);
    if(other!=m_mapDSOUND.end()) static_cast<CD3DSound*>(other->second)->Stop();
    int slot=static_cast<CD3DSound*>(sound->second)->Play();
    if(uiTrace()) std::fprintf(stderr,"AUDIO_UI_COMMIT tick=%u id=%u slot=%d\n",
        unsigned(GetTickCount()),unsigned(id),slot);
}

void CTachyonMedia::Stop( BYTE bType)
{
    if(bType==MEDIA_TSOUND) m_bPendingUIOpen=m_bPendingUIClose=false;
	switch(bType)
	{
	case MEDIA_TMUSIC	:
		{
			MAPMEDIA::iterator it;

			for( it = m_mapDMUSIC.begin(); it != m_mapDMUSIC.end(); it++)
				((CT3DMusic *) (*it).second)->Stop();
		}

		break;

	case MEDIA_TSOUND	:
		{
			MAPMEDIA::iterator it;

			for( it = m_mapDSOUND.begin(); it != m_mapDSOUND.end(); it++)
				((CD3DSound *) (*it).second)->Stop();
		}

		break;

	case MEDIA_TSHOW	:
		{
			MAPMEDIA::iterator it;

			for( it = m_mapDSHOW.begin(); it != m_mapDSHOW.end(); it++)
				((CTShow *) (*it).second)->Stop();
		}

		break;
	}
}

void CTachyonMedia::Stop( BYTE bType, DWORD dwID, int nIndex)
{
    if(bType==MEDIA_TSOUND) {
        if(dwID==uiOpen) m_bPendingUIOpen=false;
        if(dwID==uiClose) m_bPendingUIClose=false;
    }
	switch(bType)
	{
	case MEDIA_TMUSIC	:
		{
			MAPMEDIA::iterator finder = m_mapDMUSIC.find(dwID);

			if( finder != m_mapDMUSIC.end() )
				((CT3DMusic *) (*finder).second)->Stop();
		}

		break;

	case MEDIA_TSOUND	:
		{
			MAPMEDIA::iterator finder = m_mapDSOUND.find(dwID);

			if( finder != m_mapDSOUND.end() )
				((CD3DSound *) (*finder).second)->Stop(nIndex);
		}

		break;

	case MEDIA_TSHOW	:
		{
			MAPMEDIA::iterator finder = m_mapDSHOW.find(dwID);

			if( finder != m_mapDSHOW.end() )
				((CTShow *) (*finder).second)->Stop();
		}

		break;
	}
}

BYTE CTachyonMedia::IsPlay( BYTE bType, DWORD dwID, int nIndex)
{
	switch(bType)
	{
	case MEDIA_TMUSIC	:
		{
			MAPMEDIA::iterator finder = m_mapDMUSIC.find(dwID);

			if( finder != m_mapDMUSIC.end() )
				return ((CT3DMusic *) (*finder).second)->IsPlay();
		}

		break;

	case MEDIA_TSOUND	:
		{
			MAPMEDIA::iterator finder = m_mapDSOUND.find(dwID);

			if( finder != m_mapDSOUND.end() )
				return ((CD3DSound *) (*finder).second)->IsPlay(nIndex);
		}

		break;

	case MEDIA_TSHOW	:
		{
			MAPMEDIA::iterator finder = m_mapDSHOW.find(dwID);

			if( finder != m_mapDSHOW.end() )
				return ((CTShow *) (*finder).second)->IsPlay();
		}

		break;
	}

	return FALSE;
}

void CTachyonMedia::OnWindowPosition( BYTE bType, DWORD dwID)
{
	switch(bType)
	{
	case MEDIA_TSHOW	:
		{
			MAPMEDIA::iterator finder = m_mapDSHOW.find(dwID);

			if( finder != m_mapDSHOW.end() )
				((CTShow *) (*finder).second)->OnWindowPosition();
		}

		break;
	}
}

void CTachyonMedia::OnSetFocus( BYTE bFocus)
{
	if(bFocus) m_bPendingUIOpen=m_bPendingUIClose=false;
	m_bBACK = bFocus;
	ResetVolume();
}
