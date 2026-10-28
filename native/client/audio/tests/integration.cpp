#include "StdAfx.h"
#include "TClientObjBase.h"
#include "../backend.h"
#include <cassert>
#include <cstdio>
#include <algorithm>
#include <string>
#include <vector>
namespace a=fourstory_audio;
static void test_ui_transitions(const char* path) {
    assert(a::initialize(true));
    auto& media=CTachyonRes::m_MEDIA;
    media.LoadDSound(0x49,const_cast<char*>(path),100);
    media.LoadDSound(0x4a,const_cast<char*>(path),100);
    media.LoadDSound(1,const_cast<char*>(path),100);
    auto open=static_cast<CD3DSound*>(media.GetData(MEDIA_TSOUND,0x49));
    auto close=static_cast<CD3DSound*>(media.GetData(MEDIA_TSOUND,0x4a));
    const DWORD oldOpen=TFrame::m_dwOPENSND,oldClose=TFrame::m_dwCLOSESND;
    TFrame::m_dwOPENSND=0x49; TFrame::m_dwCLOSESND=0x4a;
    const auto graph=a::diagnostics();
    {
        // Actual recursive visibility calls: the settings container opens its
        // children, hides temporary popups, then reselects a destination tab.
        TFrame settings(nullptr,FRAMEDESC_SHAREDPTR{});
        TFrame tab(&settings,FRAMEDESC_SHAREDPTR{});
        TFrame popup(&tab,FRAMEDESC_SHAREDPTR{});
        settings.AddKid(&tab); tab.AddKid(&popup);
        settings.ShowComponent(FALSE); media.Stop(MEDIA_TSOUND);
        settings.ShowComponent(TRUE);
        popup.ShowComponent(FALSE);
        tab.ShowComponent(FALSE); tab.ShowComponent(TRUE);
        popup.ShowComponent(FALSE);
        assert(a::diagnostics().leasedNodes==0); // Queued, not fake playback.
        media.FlushUISound();
        assert(open->IsPlay(0) && !close->IsPlay(0));
        assert(a::diagnostics().leasedNodes==1);
        float pcm[2048]; assert(a::render(pcm,1024));
        DWORD position=open->GetPos(0); assert(position>0);
        // A repeated open in another frame shares playback without rewinding.
        assert(media.Play(MEDIA_TSOUND,0x49)==-1);
        media.FlushUISound(); assert(open->GetPos(0)==position);
        // Deliberate close is audible once, and cannot overlap the open cue.
        settings.ShowComponent(FALSE); media.FlushUISound();
        assert(!open->IsPlay(0) && close->IsPlay(0));
        assert(a::diagnostics().leasedNodes==1);
        settings.RemoveKid(&tab); tab.RemoveKid(&popup);
    }
    media.Stop(MEDIA_TSOUND);
    {
        // The real shared login/server/character screen-switching code.
        CFrameGroup lobby;
        TFrame login(&lobby,FRAMEDESC_SHAREDPTR{});
        TFrame server(&lobby,FRAMEDESC_SHAREDPTR{});
        TFrame character(&lobby,FRAMEDESC_SHAREDPTR{});
        lobby.AddKid(&login); lobby.AddKid(&server); lobby.AddKid(&character);
        login.m_id=1; server.m_id=2; character.m_id=3;
        lobby.m_mapFrame[1]=&login; lobby.m_mapFrame[2]=&server; lobby.m_mapFrame[3]=&character;
        lobby.SetLevel(1); media.FlushUISound(); media.Stop(MEDIA_TSOUND);
        for(DWORD level:{2u,3u,1u}) {
            lobby.SetLevel(level);
            assert(a::diagnostics().leasedNodes==0);
            media.FlushUISound();
            assert(open->IsPlay(0) && !close->IsPlay(0));
            assert(a::diagnostics().leasedNodes==1);
            media.Stop(MEDIA_TSOUND);
        }
        lobby.m_mapFrame.clear();
        lobby.RemoveKid(&login); lobby.RemoveKid(&server); lobby.RemoveKid(&character);
    }
    // The System Settings override calls the same dispatcher directly.
    for(int n=0;n<8;++n) media.Play(MEDIA_TSOUND,0x49);
    for(int n=0;n<6;++n) media.Play(MEDIA_TSOUND,0x4a);
    media.FlushUISound();
    assert(open->IsPlay(0) && !close->IsPlay(0));
    int combatA=media.Play(MEDIA_TSOUND,1),combatB=media.Play(MEDIA_TSOUND,1);
    assert(combatA>=0 && combatB>=0 && combatA!=combatB);
    assert(a::diagnostics().leasedNodes==3); // Ordinary effects remain polyphonic.
    media.Stop(MEDIA_TSOUND);
    media.Play(MEDIA_TSOUND,0x4a); media.FlushUISound();
    assert(close->IsPlay(0));
    media.Play(MEDIA_TSOUND,0x49); media.FlushUISound();
    assert(!close->IsPlay(0) && open->IsPlay(0));
    media.Stop(MEDIA_TSOUND);
    // Stop, focus loss, disable-at-flush and unregistration must not leave a
    // deferred cue that surprises the next frame or a reloaded sound ID.
    media.Play(MEDIA_TSOUND,0x49); media.Stop(MEDIA_TSOUND);
    media.FlushUISound(); assert(a::diagnostics().leasedNodes==0);
    media.Play(MEDIA_TSOUND,0x49); media.OnSetFocus(TRUE); media.OnSetFocus(FALSE);
    media.FlushUISound(); assert(a::diagnostics().leasedNodes==0);
    media.Play(MEDIA_TSOUND,0x49); CD3DSound::m_bON=FALSE;
    media.FlushUISound(); CD3DSound::m_bON=TRUE;
    media.FlushUISound(); assert(a::diagnostics().leasedNodes==0);
    media.Play(MEDIA_TSOUND,0x49); media.Unregister(MEDIA_TSOUND,0x49);
    media.LoadDSound(0x49,const_cast<char*>(path),100);
    media.FlushUISound(); assert(a::diagnostics().leasedNodes==0);
    media.Play(MEDIA_TSOUND,0x49); media.Stop(MEDIA_TSOUND,0x49,0);
    media.FlushUISound(); assert(a::diagnostics().leasedNodes==0);
    const auto after=a::diagnostics();
    assert(graph.attachments==after.attachments && graph.connections==after.connections);
    media.Play(MEDIA_TSOUND,0x49); media.ReleaseAll();
    assert(a::initialize(true));
    media.LoadDSound(0x49,const_cast<char*>(path),100);
    media.FlushUISound(); assert(a::diagnostics().leasedNodes==0);
    media.ReleaseAll();
    TFrame::m_dwOPENSND=oldOpen; TFrame::m_dwCLOSESND=oldClose;
    std::puts("PASS UI transitions: recursive settings, lobby screen switches, open/close priority, no overlap, cancellation, ordinary polyphony");
}
int main(int argc,char** argv) {
    assert(argc==3); assert(a::initialize(true));
    CTachyonMedia media;
    std::string windowsPath=argv[1]; std::replace(windowsPath.begin(),windowsPath.end(),'/','\\');
    media.LoadDSound(1,const_cast<char*>(windowsPath.c_str()),100);
    auto s=static_cast<CD3DSound*>(media.GetData(MEDIA_TSOUND,1)); assert(s);
    assert(s->Lock()==0); assert(s->GetDS3D(0)); assert(!s->GetDS3D(-1)); assert(!s->Play(-1));
    assert(s->GetLength()==96000); assert(s->Play(0)); assert(s->IsPlay(0));
    assert(s->SetPos(0,48000)); assert(s->GetPos(0)>=48000); assert(s->Pause(0)); assert(!s->IsPlay(0));
    assert(s->Play(0)); assert(s->Stop(0)); s->Unlock(0);
    // More reserved logical slots than output voices must not starve playback.
    std::vector<int> slots;
    for(int i=0;i<256;++i) { int index=s->Lock(); assert(index>=0); slots.push_back(index); }
    assert(s->Play(slots.back())); s->Stop(slots.back());
    for(auto i:slots) s->Unlock(i);
    CD3DSound::m_bON=FALSE; assert(s->Lock()==-1); CD3DSound::m_bON=TRUE;
    media.LoadDSound(2,argv[2],100);
    auto stereo=static_cast<CD3DSound*>(media.GetData(MEDIA_TSOUND,2)); assert(stereo);
    int si=stereo->Lock(); assert(si>=0 && !stereo->GetDS3D(si)); stereo->Unlock(si);
    // Exercise the restored original object animation trigger path.
    CTachyonObject object; auto previous=CTachyonObject::m_pMedia; CTachyonObject::m_pMedia=&media;
    SNDINST sound{}; sound.m_dwTick=0; sound.m_dwFuncID=0; sound.m_dwMediaID=1;
    MAPSNDINST inst; inst.insert(MAPSNDINST::value_type(7,&sound)); MAPOBJSND objects;
    object.GenerateSND(&inst,&objects,0,0); assert(objects.size()==1);
    object.CalcSND(&inst,&objects,0,0); assert(objects.begin()->second->m_pWAV->IsPlay(objects.begin()->second->m_nIndex));
    object.ClearSND(&objects); assert(objects.empty()); CTachyonObject::m_pMedia=previous;
    // An actor behind the camera must dispatch due sounds without Render.
    {
        CTClientObjBase actor; actor.m_bVisible=FALSE; actor.m_bSFX=FALSE;
        actor.m_vWorld._43=-20;
        CTachyonObject::m_pMedia=&media;
        actor.m_mapOBJSNDINST.insert(MAPSNDINST::value_type(7,&sound));
        actor.m_mapACTSNDINST.insert(MAPSNDINST::value_type(7,&sound));
        actor.m_mapANISNDINST.insert(MAPSNDINST::value_type(7,&sound));
        actor.m_mapTSUBACTSNDINST.insert(MAPSNDINST::value_type(7,&sound));
        actor.m_mapTSUBANISNDINST.insert(MAPSNDINST::value_type(7,&sound));
        actor.GenerateSND(&actor.m_mapTSUBACTSNDINST,&actor.m_mapTSUBACTSND,0,0);
        actor.GenerateSND(&actor.m_mapTSUBANISNDINST,&actor.m_mapTSUBANISND,0,0);
        actor.CalcTick(nullptr,0);
        MAPOBJSND* maps[]={&actor.m_mapOBJSND,&actor.m_mapACTSND,&actor.m_mapANISND,&actor.m_mapTSUBACTSND,&actor.m_mapTSUBANISND};
        for(auto map:maps) {
            assert(map->size()==1); auto item=map->begin()->second;
            assert(item->m_pWAV->IsPlay(item->m_nIndex)); assert(!item->m_bPlay);
        }
        float pcm[2048]; assert(a::render(pcm,1024));
        auto item=actor.m_mapOBJSND.begin()->second;
        DWORD before=item->m_pWAV->GetPos(item->m_nIndex); assert(before>0);
        actor.CalcSND(&actor.m_mapOBJSNDINST,&actor.m_mapOBJSND,0,0);
        assert(item->m_pWAV->GetPos(item->m_nIndex)==before);
    }
    CTachyonObject::m_pMedia=previous;
    // UI visibility bursts must neither stack voices nor rewind the active cue.
    media.LoadDSound(0x49,argv[1],100);
    auto ui=static_cast<CD3DSound*>(media.GetData(MEDIA_TSOUND,0x49)); assert(ui);
    int first=ui->Play(); assert(first>=0);
    float uiPCM[2048]; assert(a::render(uiPCM,1024));
    DWORD uiPosition=ui->GetPos(first); assert(uiPosition>0);
    for(int n=0;n<8;++n) assert(ui->Play()==first);
    assert(ui->GetPos(first)==uiPosition);
    ui->Stop();
    int again=ui->Play(); assert(again>=0 && ui->IsPlay(again)); ui->Stop();
    // Ordinary effects still permit simultaneous instances.
    int combatA=s->Play(),combatB=s->Play();
    assert(combatA>=0 && combatB>=0 && combatA!=combatB);
    assert(s->IsPlay(combatA) && s->IsPlay(combatB)); s->Stop();
    media.LoadDMusic(3,argv[1],100);
    auto music=static_cast<CT3DMusic*>(media.GetData(MEDIA_TMUSIC,3)); assert(music && music->GetLength()==1000);
    assert(music->SetPos(500)); assert(music->GetPos()>=499 && music->GetPos()<=501);
    media.LoadDShow(nullptr,nullptr,4,0,argv[1],100);
    assert(media.GetLength(MEDIA_TSHOW,4)==10000000); media.SetPos(MEDIA_TSHOW,4,0,5000000);
    assert(media.GetPos(MEDIA_TSHOW,4,0)>=4999999);
    media.OnSetFocus(TRUE); assert(CTachyonMedia::m_bBACK); media.OnSetFocus(FALSE);
    media.ReleaseAll(); assert(!media.GetData(MEDIA_TSOUND,1));
    test_ui_transitions(argv[1]);
    std::puts("PASS game audio adapters: paths, slots, stereo fallback, animation triggers, sound/music/show cursor units, cleanup");
}
