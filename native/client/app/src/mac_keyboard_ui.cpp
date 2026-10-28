// Native-only addition to the original component tree and D3D9 renderer.
#include "StdAfx.h"
#include <fourstory_mac_keyboard_ui.h>
#include "../../mac-keyboard/manager.h"
#include "../assets/keyboard/keyboard_pixels.h"
#include <cstring>
namespace {
const char* keyboardTooltip() {
    // A change usually completes within a frame or two; keep the previous text
    // meanwhile instead of flashing a progress message.
    static const char* shown="Turn on function keys system override";
    switch(fourstory_keyboard_state()) {
    case FSKeyboardPending: break;
    case FSKeyboardUnverified: case FSKeyboardEnabled: shown="Turn off function keys system override"; break;
    case FSKeyboardUnknown: shown="Check your keyboard settings"; break;
    // Unavailable: the next click shows the confirmation dialog again.
    default: shown="Turn on function keys system override"; break;
    }
    return shown;
}
void deleteCopiedTree(TComponent* component){
    for(auto* child:component->m_kids)deleteCopiedTree(child);
    component->m_kids.clear();delete component;
}
class KeyboardButton : public TButton {
    bool hover_=false,down_=false;
    IDirect3DTexture9* textures_[2]={nullptr,nullptr};
    IDirect3DTexture9* artwork(IDirect3DDevice9* device,int index) {
        if(textures_[index])return textures_[index];
        IDirect3DTexture9* texture=nullptr;
        if(FAILED(device->CreateTexture(keyboard_art::width,keyboard_art::height,1,0,
            D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&texture,nullptr)))return nullptr;
        D3DLOCKED_RECT lock;
        if(FAILED(texture->LockRect(0,&lock,nullptr,0))){texture->Release();return nullptr;}
        auto* pixels=index?keyboard_art::active:keyboard_art::inactive;
        for(int y=0;y<keyboard_art::height;++y)
            std::memcpy(static_cast<unsigned char*>(lock.pBits)+y*lock.Pitch,pixels+y*keyboard_art::width,keyboard_art::width*4);
        texture->UnlockRect(0);textures_[index]=texture;return texture;
    }
public:
    KeyboardButton(TComponent* parent,const TButton& gear):TButton(parent,gear) {
        for(auto* child:m_kids)deleteCopiedTree(child);
        ClearKids();m_pUp=m_pDown=m_pHover=nullptr;
        m_id=0xf5010001;m_bType=TCML_TYPE_BUTTON;m_bHitTestType=HITTEST_TYPE_RECT;
        m_pEnable=m_pDisable=nullptr;m_strText.Empty();
        for(int i=0;i<TCML_MENU_COUNT;++i)m_menu[i]=0;
        m_rc=CRect(159,4,203,47);m_strTooltip=keyboardTooltip();
    }
    ~KeyboardButton() override {fourstory_keyboard_focus(0);for(auto* texture:textures_)if(texture)texture->Release();}
    void OnLButtonDown(UINT,CPoint pt) override {down_=CanProcess() && HitTest(pt);}
    void OnLButtonUp(UINT,CPoint pt) override {
        bool clicked=down_ && CanProcess() && HitTest(pt);down_=false;
        if(clicked)fourstory_keyboard_toggle();
    }
    void OnMouseMove(UINT,CPoint pt) override {hover_=CanProcess() && HitTest(pt);if(!hover_)down_=false;}
    HRESULT Render(DWORD) override {
        if(!IsVisible())return S_OK;
        m_strTooltip=keyboardTooltip();
        auto* device=CTachyonRes::m_pDEVICE?CTachyonRes::m_pDEVICE->m_pDevice:nullptr;
        if(!device)return S_OK;
        IDirect3DStateBlock9* saved=nullptr;
        if(FAILED(device->CreateStateBlock(D3DSBT_ALL,&saved)))return S_OK;
        if(FAILED(saved->Capture())){saved->Release();return S_OK;}
        int mode=fourstory_keyboard_state();
        // The override is already active while physical F-key delivery is being checked.
        bool active=mode==FSKeyboardUnverified || mode==FSKeyboardEnabled;
        auto* texture=artwork(device,active?1:0);
        if(!texture){saved->Apply();saved->Release();return S_OK;}
        device->SetTexture(0,texture);device->SetVertexShader(nullptr);device->SetPixelShader(nullptr);
        device->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1);
        device->SetRenderState(D3DRS_ZENABLE,FALSE);device->SetRenderState(D3DRS_ZWRITEENABLE,FALSE);
        device->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE);device->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE);
        device->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_ONE);device->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);
        device->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);device->SetRenderState(D3DRS_FILLMODE,D3DFILL_SOLID);
        device->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_MODULATE);device->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE);device->SetTextureStageState(0,D3DTSS_COLORARG2,D3DTA_DIFFUSE);
        device->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_MODULATE);device->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_TEXTURE);device->SetTextureStageState(0,D3DTSS_ALPHAARG2,D3DTA_DIFFUSE);
        device->SetTextureStageState(0,D3DTSS_TEXCOORDINDEX,0);device->SetTextureStageState(0,D3DTSS_TEXTURETRANSFORMFLAGS,D3DTTFF_DISABLE);
        device->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE);
        device->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_LINEAR);device->SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR);
        device->SetSamplerState(0,D3DSAMP_MIPFILTER,D3DTEXF_NONE);
        device->SetSamplerState(0,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP);device->SetSamplerState(0,D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP);
        CPoint origin(m_rc.left,m_rc.top);ComponentToScreen(&origin);
        struct Vertex {float x,y,z,w;DWORD color;float u,v;};
        // Preserve the supplied artwork's aspect ratio inside the existing hit box.
        float inset=down_?2.f:hover_?0.f:1.f;
        float width=44.f-2*inset,height=width*keyboard_art::height/keyboard_art::width;
        float l=origin.x+inset*m_fScaleX-.5f,t=origin.y+((43-height)/2+(down_?1:0))*m_fScaleY-.5f;
        float r=l+width*m_fScaleX,b=t+height*m_fScaleY;
        DWORD tint=down_?0xffb0b0b0:0xffffffff;
        Vertex vertices[]={{l,t,0,1,tint,0,0},{r,t,0,1,tint,1,0},{l,b,0,1,tint,0,1},
                           {l,b,0,1,tint,0,1},{r,t,0,1,tint,1,0},{r,b,0,1,tint,1,1}};
        device->DrawPrimitiveUP(D3DPT_TRIANGLELIST,2,vertices,sizeof(Vertex));
        saved->Apply();saved->Release();return S_OK;
    }
};
}
void fourstory_keyboard_attach(TComponent* parent,TComponent** buttons,CPoint* defaults,int count) {
    if(!parent || count<1 || !buttons[0])return;
    // User-approved insertion: keep the gear fixed; shift preceding icons and
    // the defaults used by special-map restoration together.
    for(int i=1;i<count;++i){defaults[i].x-=52;buttons[i]->MoveComponent(defaults[i]);}
    parent->AddKid(new KeyboardButton(parent,*static_cast<TButton*>(buttons[0])));
}

bool fourstory_keyboard_tooltip(TComponent* parent,const CPoint& point,CString& text,CRect& rect) {
    auto* button=parent?parent->FindKid(0xf5010001):nullptr;
    if(!button || !button->CanProcess() || !button->HitTest(point))return false;
    text=keyboardTooltip();
    button->GetComponentRect(&rect);
    button->ComponentToScreen(&rect);
    return true;
}
