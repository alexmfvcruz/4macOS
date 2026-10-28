// C7: original game-facing API backed by native audio, with no Windows device.
#include <fourstory_engine_ui.h>
#include "../../audio/backend.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <cstdlib>
#include <cstdio>
#include <cstring>
namespace audio = fourstory_audio;
namespace {
void reap();
float gain(BYTE category, BYTE fade, BYTE item) {
    if (CTachyonMedia::m_bBACK) return 0;
    return float(CTachyonMedia::m_bMasterVolume) * category * fade * item / 100000000.f;
}
// The client only calls SetPosition. Keep the interface for its existing callers;
// unsupported DirectSound features return E_NOTIMPL rather than fake success.
class Spatial final : public IDirectSound3DBuffer {
    ULONG refs = 1;
public:
    audio::VoicePtr voice;
    DS3DBUFFER params{};
    explicit Spatial(audio::VoicePtr v) : voice(v) { params.dwSize=sizeof(params); params.flMinDistance=1; params.flMaxDistance=1e9f; params.dwMode=1; }
    void apply() { audio::spatial(voice,params.vPosition.x,params.vPosition.y,params.vPosition.z,params.flMinDistance,params.flMaxDistance,params.dwMode!=2); }
    HRESULT QueryInterface(REFIID, void** p) override { if(p) *p=nullptr; return E_NOINTERFACE; }
    ULONG AddRef() override { return ++refs; }
    ULONG Release() override { auto n=--refs; if(!n) delete this; return n; }
    HRESULT GetAllParameters(LPDS3DBUFFER p) override { if(!p) return E_POINTER; *p=params; return S_OK; }
    HRESULT GetConeAngles(LPDWORD,LPDWORD) override { return E_NOTIMPL; }
    HRESULT GetConeOrientation(D3DVECTOR*) override { return E_NOTIMPL; }
    HRESULT GetConeOutsideVolume(LPLONG) override { return E_NOTIMPL; }
    HRESULT GetMaxDistance(float* p) override { if(!p) return E_POINTER; *p=params.flMaxDistance; return S_OK; }
    HRESULT GetMinDistance(float* p) override { if(!p) return E_POINTER; *p=params.flMinDistance; return S_OK; }
    HRESULT GetMode(LPDWORD p) override { if(!p) return E_POINTER; *p=params.dwMode; return S_OK; }
    HRESULT GetPosition(D3DVECTOR* p) override { if(!p) return E_POINTER; p->x=params.vPosition.x; p->y=params.vPosition.y; p->z=params.vPosition.z; return S_OK; }
    HRESULT GetVelocity(D3DVECTOR*) override { return E_NOTIMPL; }
    HRESULT SetAllParameters(const DS3DBUFFER*,DWORD) override { return E_NOTIMPL; }
    HRESULT SetConeAngles(DWORD,DWORD,DWORD) override { return E_NOTIMPL; }
    HRESULT SetConeOrientation(float,float,float,DWORD) override { return E_NOTIMPL; }
    HRESULT SetConeOutsideVolume(LONG,DWORD) override { return E_NOTIMPL; }
    HRESULT SetMaxDistance(float d,DWORD mode) override { if(mode!=DS3D_IMMEDIATE || !std::isfinite(d) || d<params.flMinDistance) return E_INVALIDARG; params.flMaxDistance=d; apply(); return S_OK; }
    HRESULT SetMinDistance(float d,DWORD mode) override { if(mode!=DS3D_IMMEDIATE || !std::isfinite(d) || d<=0 || d>params.flMaxDistance) return E_INVALIDARG; params.flMinDistance=d; apply(); return S_OK; }
    HRESULT SetMode(DWORD mode,DWORD applyMode) override { if(applyMode!=DS3D_IMMEDIATE || (mode!=1 && mode!=2)) return E_NOTIMPL; params.dwMode=mode; apply(); return S_OK; }
    HRESULT SetPosition(float x,float y,float z,DWORD mode) override { if(mode!=DS3D_IMMEDIATE || !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) return E_INVALIDARG; params.vPosition={x,y,z}; apply(); return S_OK; }
    HRESULT SetVelocity(float,float,float,DWORD) override { return E_NOTIMPL; }
};
struct Slot {
    audio::VoicePtr voice;
    std::shared_ptr<Spatial> spatial;
    bool locked=false;
};
struct Sound {
    audio::ClipPtr clip;
    std::vector<Slot> slots;
    bool singleInstance=false;
    audio::EffectProfile profile=audio::EffectProfile::Original;
};
std::map<CD3DSound*,Sound>& sounds() { static auto* s=new std::map<CD3DSound*,Sound>(); return *s; }
struct Orphan { audio::VoicePtr voice; BYTE item,fade; };
std::vector<Orphan>& orphans() { static auto* o=new std::vector<Orphan>(); return *o; }
Slot* slot(CD3DSound* s,int i) {
    auto f=sounds().find(s); if(f==sounds().end() || i<0 || size_t(i)>=f->second.slots.size()) return nullptr;
    return &f->second.slots[i];
}
}
BYTE CD3DSound::m_bMasterVolume=VOLUME_MAX;
BYTE CD3DSound::m_bON=TRUE;
VDIRECTSOUND3DBUFFER CD3DSound::m_v3DGARBAGE;
VDIRECTSOUNDBUFFER CD3DSound::m_vGARBAGE;
LPDIRECTSOUND3DLISTENER8 CD3DSound::m_pLISTENER=nullptr;
D3DXMATRIX CD3DSound::m_vLISTENER;
CD3DSound::CD3DSound() : m_pDS(nullptr),m_dwSize(0),m_bFadeVolume(VOLUME_MAX),m_bVolume(VOLUME_MAX) {}
CD3DSound::~CD3DSound() { Release(); }
void CD3DSound::Initialize(LPDIRECTSOUND8 ds,CString file) { Release(); m_pDS=ds; m_strFile=file; }
void CD3DSound::Release() {
    auto f=sounds().find(this);
    if(f!=sounds().end()) {
        for(auto& s:f->second.slots) if(audio::playing(s.voice)) orphans().push_back({s.voice,m_bVolume,m_bFadeVolume});
        sounds().erase(f);
    }
    m_dwSize=0;
}
void fourstory_sound_ui_cue(CD3DSound* sound, bool opening) {
    auto& s=sounds()[sound];
    s.singleInstance=true;
    s.profile=opening ? audio::EffectProfile::WindowOpen : audio::EffectProfile::Original;
}
BYTE CD3DSound::LoadData() {
    auto& s=sounds()[this];
    if(!s.clip) s.clip=audio::load(fourstory::compat::native_path((LPCSTR)m_strFile).c_str(),0,s.profile);
    m_dwSize=audio::bytes(s.clip); return bool(s.clip);
}
BYTE CD3DSound::LoadData(LPDIRECTSOUNDBUFFER,CWaveFile*) { return FALSE; }
void CD3DSound::Restore() {}
int CD3DSound::Lock() {
    reap();
    if(!CTachyonMedia::m_bON || !m_bON || !LoadData()) return -1;
    auto& s=sounds()[this];
    size_t i=0;
    for(;i<s.slots.size();++i) if(!s.slots[i].locked && !audio::playing(s.slots[i].voice)) break;
    auto v=audio::create(s.clip); if(!v) return -1;
    Slot fresh; fresh.voice=v; fresh.locked=true;
    // Stereo WAVs use the original DirectSound fallback without a 3D buffer.
    // Backend spatial() leaves their channel layout intact until SetPosition.
    if(audio::channels(s.clip)==1) fresh.spatial=std::shared_ptr<Spatial>(new Spatial(v),[](Spatial* p){p->Release();});
    if(i==s.slots.size()) s.slots.push_back(fresh); else s.slots[i]=fresh;
    ResetVolume(int(i)); return int(i);
}
void CD3DSound::Unlock(int i) { auto p=slot(this,i); if(p) p->locked=false; }
BYTE CD3DSound::Play(int i) {
    auto p=slot(this,i);
    if(!p || !CTachyonMedia::m_bON || !m_bON || !ResetVolume(i) || !audio::play(p->voice)) return FALSE;
    const char* trace=std::getenv("FOURSTORY_AUDIO_TRACE");
    auto f=sounds().find(this);
    if(trace && std::strcmp(trace,"1")==0 && f!=sounds().end() && f->second.singleInstance) {
        size_t active=0;
        for(auto& instance:f->second.slots) if(audio::playing(instance.voice)) ++active;
        std::fprintf(stderr,"AUDIO_UI_%s tick=%u slot=%d same_asset_voices=%zu gain=%.4f\n",
            f->second.profile==audio::EffectProfile::WindowOpen ? "OPEN" : "CLOSE",
            unsigned(GetTickCount()),i,active,gain(m_bMasterVolume,m_bFadeVolume,m_bVolume));
    }
    return TRUE;
}
int CD3DSound::Play() {
    // Window visibility cascades can request the same UI cue repeatedly.
    // Reuse its active playback without rewinding; positional effects keep polyphony.
    auto f=sounds().find(this);
    if(CTachyonMedia::m_bON && m_bON && f!=sounds().end() && f->second.singleInstance) {
        for(size_t n=0;n<f->second.slots.size();++n) {
            if(!audio::playing(f->second.slots[n].voice)) continue;
            const char* trace=std::getenv("FOURSTORY_AUDIO_TRACE");
            if(trace && std::strcmp(trace,"1")==0)
                std::fprintf(stderr,"AUDIO_UI_COALESCED tick=%u slot=%zu\n",unsigned(GetTickCount()),n);
            return int(n);
        }
    }
    int i=Lock(); if(i>=0) { if(!Play(i)) { Unlock(i); return -1; } Unlock(i); } return i; }
BYTE CD3DSound::Pause(int i) { auto p=slot(this,i); if(!p) return FALSE; audio::pause(p->voice); return TRUE; }
BYTE CD3DSound::Stop(int i) { auto p=slot(this,i); if(!p) return FALSE; audio::stop(p->voice); return TRUE; }
void CD3DSound::Stop() { auto f=sounds().find(this); if(f!=sounds().end()) for(auto& s:f->second.slots) audio::stop(s.voice); }
BYTE CD3DSound::IsPlay(int i) { auto p=slot(this,i); return p && audio::playing(p->voice); }
BYTE CD3DSound::IsPlay(LPDIRECTSOUNDBUFFER) { return FALSE; }
LPDIRECTSOUNDBUFFER CD3DSound::GetDSB(int) { return nullptr; }
LPDIRECTSOUND3DBUFFER CD3DSound::GetDS3D(int i) { auto p=slot(this,i); return p ? p->spatial.get() : nullptr; }
BYTE CD3DSound::ResetVolume(int i) { auto p=slot(this,i); if(!p) return FALSE; audio::volume(p->voice,gain(m_bMasterVolume,m_bFadeVolume,m_bVolume)); return TRUE; }
BYTE CD3DSound::ResetVolume() {
    auto f=sounds().find(this); if(f!=sounds().end()) for(size_t i=0;i<f->second.slots.size();++i) ResetVolume(int(i));
    for(auto& o:orphans()) audio::volume(o.voice,gain(m_bMasterVolume,o.fade,o.item));
    return TRUE;
}
DWORD CD3DSound::GetLength() { if(!m_dwSize) LoadData(); return m_dwSize; }
DWORD CD3DSound::GetPos(int i) { auto p=slot(this,i); auto f=sounds().find(this); if(!p || f==sounds().end()) return 0; auto d=audio::duration(f->second.clip); return d>0 ? DWORD(audio::position(p->voice)/d*m_dwSize) : 0; }
BYTE CD3DSound::SetPos(int i,DWORD pos) { auto p=slot(this,i); auto f=sounds().find(this); return p && f!=sounds().end() && m_dwSize && pos<=m_dwSize && audio::seek(p->voice,double(pos)/m_dwSize*audio::duration(f->second.clip)); }
namespace {
void reap() {
    auto& o=orphans(); o.erase(std::remove_if(o.begin(),o.end(),[](const Orphan& v){ return !audio::playing(v.voice); }),o.end());
    for(auto& pair:sounds()) {
        bool retained=false;
        for(auto& s:pair.second.slots) {
            if(!s.locked && !audio::playing(s.voice)) { s.spatial.reset(); s.voice.reset(); }
            if(s.voice) retained=true;
        }
        if(!retained) pair.second.clip.reset();
    }
}
}
void CD3DSound::ClearGARBAGE() { for(auto& o:orphans()) audio::stop(o.voice); orphans().clear(); }
void CD3DSound::InitGARBAGE() { ClearGARBAGE(); }
void CD3DSound::ResetLISTENER(LPD3DXVECTOR3 position,LPD3DXVECTOR3 z,LPD3DXVECTOR3 y) {
    if(!position || !z || !y) return;
    D3DXMATRIX scale; D3DXMatrixScaling(&scale,.25f,.25f,.25f);
    D3DXVECTOR3 target=*position+*z;
    D3DXMatrixLookAtLH(&m_vLISTENER,position,&target,y); m_vLISTENER*=scale;
    reap();
}
D3DXVECTOR3 CD3DSound::ConvertPOS(FLOAT x,FLOAT y,FLOAT z) { D3DXVECTOR3 v(x,y,z); CTMath::Transform(&m_vLISTENER,&v); return v; }
void CD3DSound::GetListener(LPDS3DLISTENER p) { if(p) { *p={}; p->dwSize=sizeof(*p); p->vOrientFront={0,0,1}; p->vOrientTop={0,1,0}; p->flDistanceFactor=p->flRolloffFactor=p->flDopplerFactor=1; } }
void CD3DSound::SetListener(LPDS3DLISTENER,DWORD) {}
