// C7: retain game music/DirectShow WAV interfaces; Apple decodes the files.
#include <fourstory_engine_ui.h>
#include "../../audio/backend.h"
#include <map>
namespace audio=fourstory_audio;
namespace {
struct Music { audio::TrackPtr track; bool failed=false; };
std::map<CT3DMusic*,Music>& music() { static auto* m=new std::map<CT3DMusic*,Music>(); return *m; }
std::map<CTShow*,Music>& shows() { static auto* m=new std::map<CTShow*,Music>(); return *m; }
float gain(BYTE category,BYTE fade,BYTE item) { return CTachyonMedia::m_bBACK ? 0.f : float(CTachyonMedia::m_bMasterVolume)*category*fade*item/100000000.f; }
audio::TrackPtr ensure(Music& m,const CString& file) { if(!m.track && !m.failed) { m.track=audio::track(fourstory::compat::native_path((LPCSTR)file).c_str()); m.failed=!m.track; } return m.track; }
}
BYTE CT3DMusic::m_bMasterVolume=VOLUME_MAX;
BYTE CT3DMusic::m_bON=TRUE;
BYTE CT3DMusic::m_bEQ=FALSE;
FLOAT CT3DMusic::m_vEQ[32]={};
CT3DMusic::CT3DMusic() {
    m_dwSTART=m_dwEND=m_dwPOS=m_dwSEEK=0; m_wFRAME=0;
    m_bFadeVolume=m_bVolume=VOLUME_MAX; m_bLOOP=FALSE; m_bSTATE=MMS_CLOSED;
}
CT3DMusic::~CT3DMusic() { music().erase(this); }
BYTE CT3DMusic::InitMusic(LPTSTR file) { music().erase(this); m_strFILE=file; return TRUE; }
void CT3DMusic::DecodeINIT(LPDIRECTSOUND8) {}
BYTE CT3DMusic::Lock() { auto t=ensure(music()[this],m_strFILE); if(t) m_dwEND=DWORD(audio::duration(t)*1000); return bool(t); }
void CT3DMusic::Unlock() { Stop(); music().erase(this); }
BYTE CT3DMusic::Play() {
    if(!CTachyonMedia::m_bON || !m_bON || !Lock()) return FALSE;
    ResetVolume(); bool ok=audio::play(music()[this].track,m_bLOOP); m_bSTATE=ok?MMS_PLAY:MMS_OPEN; return ok;
}
BYTE CT3DMusic::Stop() { auto f=music().find(this); if(f!=music().end()) audio::stop(f->second.track); m_dwSEEK=0; m_bSTATE=MMS_OPEN; return TRUE; }
BYTE CT3DMusic::Pause() { auto f=music().find(this); if(f==music().end()) return FALSE; audio::pause(f->second.track); m_bSTATE=MMS_PAUSE; return TRUE; }
BYTE CT3DMusic::IsPlay() { auto f=music().find(this); bool p=f!=music().end() && audio::playing(f->second.track); if(!p && m_bSTATE==MMS_PLAY) m_bSTATE=MMS_OPEN; return p; }
BYTE CT3DMusic::ResetVolume() { auto f=music().find(this); if(f!=music().end()) audio::volume(f->second.track,gain(m_bMasterVolume,m_bFadeVolume,m_bVolume)); return TRUE; }
DWORD CT3DMusic::GetLength() { Lock(); return m_dwEND; }
DWORD CT3DMusic::GetPos() { auto f=music().find(this); return f!=music().end()?DWORD(audio::position(f->second.track)*1000):0; }
BYTE CT3DMusic::SetPos(DWORD pos) { if(!Lock()) return FALSE; if(!audio::seek(music()[this].track,pos/1000.)) return FALSE; m_dwSEEK=pos; return TRUE; }

BYTE CTShow::m_bMasterVolume=VOLUME_MAX;
BYTE CTShow::m_bON=TRUE;
CTShow::CTShow() : m_pGB(nullptr),m_pMS(nullptr),m_pMC(nullptr),m_pME(nullptr),m_pVW(nullptr),m_pHost(nullptr),m_pWnd(nullptr),m_dwMessage(0),m_bVolume(VOLUME_MAX) {}
CTShow::~CTShow() { Release(); }
void CTShow::InitDShow(CWnd* host,CWnd* wnd,LPTSTR file,DWORD message) { Release(); m_pHost=host; m_pWnd=wnd; m_strFILE=file; m_dwMessage=message; }
void CTShow::Release() { shows().erase(this); }
BYTE CTShow::ResetGB() { return bool(ensure(shows()[this],m_strFILE)); }
BYTE CTShow::Play() { if(!CTachyonMedia::m_bON || !m_bON || !ResetGB()) return FALSE; ResetVolume(); return audio::play(shows()[this].track,false); }
BYTE CTShow::Stop() { auto f=shows().find(this); if(f!=shows().end()) audio::stop(f->second.track); return TRUE; }
BYTE CTShow::Pause() { auto f=shows().find(this); if(f==shows().end()) return FALSE; audio::pause(f->second.track); return TRUE; }
BYTE CTShow::IsPlay() { auto f=shows().find(this); return f!=shows().end() && audio::playing(f->second.track); }
BYTE CTShow::Toggle() { return IsPlay()?Pause():Play(); }
BYTE CTShow::ResetVolume() { auto f=shows().find(this); if(f!=shows().end()) audio::volume(f->second.track,gain(m_bMasterVolume,VOLUME_MAX,m_bVolume)); return TRUE; }
// DirectShow uses 100 ns reference-time units; MP3 music uses milliseconds.
LONGLONG CTShow::GetLength() { return ResetGB()?LONGLONG(audio::duration(shows()[this].track)*10000000.):0; }
LONGLONG CTShow::GetPos() { auto f=shows().find(this); return f!=shows().end()?LONGLONG(audio::position(f->second.track)*10000000.):0; }
BYTE CTShow::SetPos(LONGLONG pos) { return pos>=0 && ResetGB() && audio::seek(shows()[this].track,pos/10000000.); }
void CTShow::OnWindowPosition() {} // The release's two DirectShow assets are WAV audio.
