// Original TComp controls and D3D9 rendering; Rust validates/creates accounts.
#include "StdAfx.h"
#include "TClient.h"
#include "TClientWnd.h"
#include <fourstory_accounts_ui.h>
#include "../../../accounts/include/fourstory_accounts.h"
#include <account_assets.h>
#include "../../../accounts/platform/artwork.h"
#include <algorithm>
#include <array>
#include <memory>
#include <string>

namespace fourstory { namespace accounts {
namespace {
void wipe(CString& value) {
    int count=value.GetLength();
    if(count) { volatile char* p=value.GetBuffer(count); for(int i=0;i<count;++i) p[i]=0; value.ReleaseBuffer(0); }
    value.Empty();
}
void destroy(TComponent* component) {
    if(!component) return;
    auto kids=component->m_kids; component->m_kids.clear();
    for(auto* kid:kids) destroy(kid);
    delete component;
}
struct Vertex { float x,y,z,w; DWORD color; };
void rectangle(LPDIRECT3DDEVICE9 device,CRect r,DWORD color) {
    Vertex v[4]={{float(r.left)-.5f,float(r.top)-.5f,0,1,color},{float(r.right)-.5f,float(r.top)-.5f,0,1,color},
                 {float(r.left)-.5f,float(r.bottom)-.5f,0,1,color},{float(r.right)-.5f,float(r.bottom)-.5f,0,1,color}};
    device->SetTexture(0,nullptr);device->SetVertexShader(nullptr);device->SetPixelShader(nullptr);
    device->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE);
    device->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);
    device->SetRenderState(D3DRS_ZENABLE,FALSE);device->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE);device->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);device->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);
    device->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);device->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_DIFFUSE);
    device->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1);device->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_DIFFUSE);
    device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,v,sizeof(Vertex));
}
struct Artwork {
    IDirect3DTexture9* logo=nullptr;IDirect3DTexture9* panel=nullptr;IDirect3DTexture9* button=nullptr;
    ~Artwork(){if(logo)logo->Release();if(panel)panel->Release();if(button)button->Release();}
    static IDirect3DTexture9* load(const unsigned char* data,size_t count) {
        size_t width=0,height=0;auto* pixels=fourstory_account_image(data,count,&width,&height);
        if(!pixels)return nullptr;
        IDirect3DTexture9* texture=nullptr;auto* device=CTachyonRes::m_pDEVICE->m_pDevice;
        if(SUCCEEDED(device->CreateTexture(UINT(width),UINT(height),1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&texture,nullptr))) {
            D3DLOCKED_RECT r;
            if(SUCCEEDED(texture->LockRect(0,&r,nullptr,0))) {
                for(size_t y=0;y<height;++y)memcpy((char*)r.pBits+y*r.Pitch,pixels+y*width*4,width*4);
                texture->UnlockRect(0);
            } else {texture->Release();texture=nullptr;}
        }
        fourstory_account_image_free(pixels);return texture;
    }
    Artwork(){logo=load(account_logo,sizeof(account_logo));panel=load(account_panel,sizeof(account_panel));button=load(account_button,sizeof(account_button));}
};
std::unique_ptr<Artwork> art;
void picture(LPDIRECT3DDEVICE9 device,CRect r,IDirect3DTexture9* texture,float bottom=1) {
    if(!texture)return;
    struct V {float x,y,z,w;DWORD color;float u,v;};
    V v[4]={{float(r.left)-.5f,float(r.top)-.5f,0,1,0xffffffff,0,0},{float(r.right)-.5f,float(r.top)-.5f,0,1,0xffffffff,1,0},
            {float(r.left)-.5f,float(r.bottom)-.5f,0,1,0xffffffff,0,bottom},{float(r.right)-.5f,float(r.bottom)-.5f,0,1,0xffffffff,1,bottom}};
    device->SetTexture(0,texture);device->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1);
    device->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_ONE);
    device->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_MODULATE);device->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE);device->SetTextureStageState(0,D3DTSS_COLORARG2,D3DTA_DIFFUSE);
    device->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_MODULATE);device->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_TEXTURE);device->SetTextureStageState(0,D3DTSS_ALPHAARG2,D3DTA_DIFFUSE);
    device->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_LINEAR);device->SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR);
    device->SetSamplerState(0,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP);device->SetSamplerState(0,D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP);
    device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,v,sizeof(V));
}
class Button final: public TButton {
public:
    bool hovered=false;
    void OnMouseMove(UINT flags,CPoint point) override {
        TButton::OnMouseMove(flags,point);
        hovered=CanProcess() && HitTest(point);
    }
    Button(TComponent* parent,const TButton& source):TButton(parent,source) {
        m_bHitTestType=HITTEST_TYPE_RECT;m_style|=TS_FOCUSABLE; m_flagAlign=ALIGN_CENTER; m_bVCenter=TRUE;
        m_nHMargine=0;m_nVMargine=0;
        for(auto& command:m_menu) command=TCML_ID_NULL;
    }
    HRESULT Render(DWORD) override {
        if(!IsVisible()) return S_OK;
        CRect r;GetComponentRect(&r);ComponentToScreen(&r);
        auto* device=CTachyonRes::m_pDEVICE->m_pDevice;
        const bool enabled=CanProcess(),hot=enabled && hovered,down=enabled && IsStateDown();
        rectangle(device,r,!enabled?0xff5e6263:hot?0xffffdf91:0xffbda66b);r.DeflateRect(1,1);
        rectangle(device,r,!enabled?0xff192630:down?0xff0b2638:hot?0xff24566d:0xff12334a);
        if(art && m_id==4) {
            picture(device,r,art->button,.5f);
            if(hot || !enabled) rectangle(device,r,!enabled?0x9008121b:down?0x60000000:0x30ffffff);
        }
        if(m_id==1) {
            // Draw the cross geometrically: font bearings cannot offset its center.
            const float cx=(r.left+r.right)*.5f-.5f,cy=(r.top+r.bottom)*.5f-.5f;
            const float arm=min(r.Width(),r.Height())*.20f,half=max(1.0f,arm*.16f);
            for(int direction:{-1,1}) {
                Vertex v[4]={{cx-arm-half,cy-direction*arm+direction*half,0,1,0xffedddba},
                             {cx-arm+half,cy-direction*arm-direction*half,0,1,0xffedddba},
                             {cx+arm-half,cy+direction*arm+direction*half,0,1,0xffedddba},
                             {cx+arm+half,cy+direction*arm-direction*half,0,1,0xffedddba}};
                device->SetTexture(0,nullptr);device->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE);
                device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,v,sizeof(Vertex));
            }
            return S_OK;
        }
        m_clrFg=enabled?hot?0xffffedbd:0xffedddba:0xff899398;
        return DrawText();
    }
};
class Edit final: public TEdit {
public:
    size_t maximum;
    HRESULT Render(DWORD tick) override {
        if(!IsVisible())return S_OK;
        HRESULT result=DrawText();
        // Keep the insertion point steady; the copied caret is an animated sprite.
        if(m_pCaret)m_pCaret->ShowComponent(FALSE);
        DrawKids(tick);
        if(m_pCaret && GetFocus() && CanProcess()) {
            CRect caret;m_pCaret->GetComponentRect(&caret);m_pCaret->ComponentToScreen(&caret);
            CRect bounds;GetComponentRect(&bounds);ComponentToScreen(&bounds);
            caret.left=max(bounds.left+2,min(caret.left,bounds.right-3));caret.right=caret.left+2;
            caret.top=max(bounds.top+4,caret.top);caret.bottom=min(bounds.bottom-4,caret.bottom);
            if(caret.bottom>caret.top)rectangle(CTachyonRes::m_pDEVICE->m_pDevice,caret,0xffedddba);
        }
        return result;
    }
    bool displayMatches(const char* expected) const {return m_strPrvOuputText==expected;}
    void SetPassword(BOOL masked) {
        if(masked)m_style|=TES_PASSWORD;else m_style&=~TES_PASSWORD;
        wipe(m_strDisplayText);wipe(m_strPrvOuputText);wipe(m_strPrvText);
        ClearTextOutputData();m_bNeedUpdateTextSetting=TRUE;
    }
    void clearSecret() {
        wipe(m_strText);wipe(m_strDisplayText);wipe(m_strComposition);wipe(m_strPrvOuputText);wipe(m_strPrvText);
        for(auto& line:m_vTextLine)wipe(line);
        for(const auto& item:m_TextOutputDataSet) { auto& value=const_cast<TComponent::TextOutputData&>(item);wipe(value.strText); }
        ClearText();ClearTextOutputData();
    }
    ~Edit() override {clearSecret();}
    void InsertString(const CString& text) override {
        if(size_t(text.GetLength())+size_t(m_strText.GetLength())>maximum)return;
        for(int i=0;i<text.GetLength();++i) if((unsigned char)text[i]<32 || (unsigned char)text[i]>126)return;
        TEdit::InsertString(text);
    }
    Edit(TComponent* parent,const TEdit& source,size_t max):TEdit(parent,source),maximum(max) { clearSecret();m_bHitTestType=HITTEST_TYPE_RECT; }
    void OnChar(UINT c,int count,UINT flags) override {
        if(c>=32 && (c>126 || size_t(m_strText.GetLength())>=maximum)) return;
        TEdit::OnChar(c,count,flags);
    }
};
bool autoShown=false;
std::array<bool,256> swallowedKeys{};
CTClientWnd* owner=nullptr;
class Panel;
std::unique_ptr<Panel> panel;
void close();

class Panel final: public TFrame {
public:
    enum Mode { Register, Success } mode;
    Edit* fields[4]{};
    TComponent* message=nullptr;
    Button* submit=nullptr; Button* show=nullptr;
    bool visiblePasswords=false,waiting=false;
    CString account;
    std::vector<std::pair<TComponent*,CRect>> layout;
    Panel(CTClientWnd* window,Mode m,const CString& created=CString()):TFrame(nullptr,FRAMEDESC_SHAREDPTR{}),mode(m),account(created) {
        owner=window;window->m_MainGame.m_vKEY.ResetSTATE();window->m_MainGame.m_bAutoRun=FALSE;m_bSND=FALSE;m_bHitTestType=HITTEST_TYPE_RECT;m_style &= ~TFS_FLOAT;
        m_rc=CRect(0,0,580,mode==Register?710:460);m_bTabAutoNext=FALSE;
        TFrame* login=window->m_TNet.FindFrame(ID_FRAME_LOGIN_NEW);
        auto* edit=dynamic_cast<TEdit*>(login->FindKid(ID_CTRLINST_ID));
        auto* password=dynamic_cast<TEdit*>(login->FindKid(ID_CTRLINST_PW));
        auto* button=dynamic_cast<TButton*>(login->FindKid(ID_CTRLINST_LOGIN));
        auto* label=login->FindKid(ID_CTRLINST_ID_TEXT);
        if(!edit||!password||!button||!label) throw std::runtime_error("Missing original login controls");
        addLabel(label,mode==Register?"Create account":"Account created",40,135,480,35)->m_flagAlign=ALIGN_CENTER;
        addButton(button,"X",1,524,18,36,32);
        if(mode==Register) {
            const char* labels[]={"Account name","E-mail (optional)","Password","Confirm password"};
            for(int i=0;i<4;++i) {
                int y=190+i*90;addLabel(label,labels[i],40,y,500,24);
                fields[i]=new Edit(this,i>=2?*password:*edit,i==0?50:i==1?254:64);
                fields[i]->m_id=100+i; fields[i]->MoveComponent(CPoint(40,y+28)); fields[i]->SetComponentSize(CSize(500,36));fields[i]->SetTextExtent(484);
                fields[i]->SetPassword(i>=2);AddKid(fields[i]);
            }
            addButton(button,"Generate random account name",2,40,255,320,25);
            show=addButton(button,"Show Password",3,40,550,230,28);
            submit=addButton(button,"Create account",4,140,598,300,40);
            for(int i=0;i<3;++i)fields[i]->m_pNextTab=fields[i+1];
            fields[3]->m_pNextTab=submit;submit->m_pNextTab=fields[0];
            SwitchFocus(fields[0]);
        } else {
            addLabel(label,"Your account was created successfully.",40,195,500,30)->m_flagAlign=ALIGN_CENTER;
            addLabel(label,(LPCSTR)account,40,240,500,30)->m_flagAlign=ALIGN_CENTER;
            submit=addButton(button,"OK",7,190,310,200,40);SwitchFocus(submit);
        }
        message=addLabel(label,"",40,mode==Register?655:370,500,34);
        for(auto* child:m_kids)layout.push_back({child,child->m_rc});
    }
    ~Panel() override {
        for(auto* field:fields) if(field) { field->clearSecret(); }
        auto kids=m_kids;m_kids.clear();for(auto* kid:kids) destroy(kid);
    }
    TComponent* addLabel(TComponent* sample,const char* text,int x,int y,int w,int h) {
        auto* c=new TComponent(this,*sample,false);c->m_strText=text;c->m_pEnable=nullptr;c->m_pDisable=nullptr;
        c->m_id=0;c->m_flagAlign=ALIGN_LEFT;c->m_clrFg=0xffedddba;c->MoveComponent(CPoint(x,y));c->SetComponentSize(CSize(w,h));c->m_bNoHIT=TRUE;AddKid(c);return c;
    }
    Button* addButton(TButton* sample,const char* text,DWORD id,int x,int y,int w,int h) {
        auto* c=new Button(this,*sample);c->m_id=id;c->m_strText=text;c->MoveComponent(CPoint(x,y));c->SetComponentSize(CSize(w,h));AddKid(c);return c;
    }
    void status(const char* text) { message->m_strText=text; }
    void OnNotify(DWORD from,WORD msg,LPVOID) override {
        if(msg!=TNM_LCLICK)return;
        auto* control=FindKid(from);if(!control || !control->CanProcess())return;
        if(from==1 || (from==7 && mode==Success)) { close();return; }
        if(waiting)return;
        if(from==2) { uint8_t value[16];size_t written=0;if(!fourstory_accounts_generate_name(value,sizeof(value),&written)) fields[0]->SetText((const char*)value);return; }
        if(from==3) { visiblePasswords=!visiblePasswords;fields[2]->SetPassword(!visiblePasswords);fields[3]->SetPassword(!visiblePasswords);show->m_strText=visiblePasswords?"Hide Password":"Show Password";return; }
        if(from==4) {
            auto* a=(const uint8_t*)(LPCSTR)fields[0]->m_strText;auto* e=(const uint8_t*)(LPCSTR)fields[1]->m_strText;
            auto* p=(const uint8_t*)(LPCSTR)fields[2]->m_strText;auto* c=(const uint8_t*)(LPCSTR)fields[3]->m_strText;
            int result=fourstory_accounts_register(a,fields[0]->m_strText.GetLength(),e,fields[1]->m_strText.GetLength(),p,fields[2]->m_strText.GetLength(),c,fields[3]->m_strText.GetLength());
            const char* errors[]={"","Use 1-50 letters, numbers or underscores.","Enter a valid e-mail address.","Use 8-64 printable ASCII password characters.","The passwords do not match."};
            if(result==20) { account=fields[0]->m_strText;waiting=true;for(auto* field:fields)field->EnableComponent(FALSE);status("Creating account..."); }
            else if(result>=1&&result<=4) status(errors[result]);
            else status(result==25?"Registration service is not configured.":"Registration is unavailable. Please try later.");
            return;
        }
    }
    HRESULT Render(DWORD tick) override {
        auto* device=CTachyonRes::m_pDEVICE->m_pDevice;
        int w=owner->m_Device.m_option.m_dwScreenX,h=owner->m_Device.m_option.m_dwScreenY;
        const float scale=min(1.0f,min((w-32)/580.0f,(h-32)/float(mode==Register?710:460)));
        CSize size(int(580*scale),int((mode==Register?710:460)*scale));SetComponentSize(size);MoveComponent(CPoint((w-size.cx)/2,(h-size.cy)/2));
        for(auto item:layout) {
            auto r=item.second;item.first->MoveComponent(CPoint(int(r.left*scale),int(r.top*scale)));
            item.first->SetComponentSize(CSize(int(r.Width()*scale),int(r.Height()*scale)));
            if(auto* edit=dynamic_cast<Edit*>(item.first))edit->SetTextExtent(max(16,int(r.Width()*scale)-16));
        }
        rectangle(device,CRect(0,0,w,h),0x99000000);
        CRect r;GetComponentRect(&r);rectangle(device,r,0xffbda66b);r.DeflateRect(2,2);rectangle(device,r,0xff061824);
        if(art) {picture(device,r,art->panel);picture(device,CRect(m_rc.left+int(160*scale),m_rc.top+int(10*scale),m_rc.left+int(420*scale),m_rc.top+int(132*scale)),art->logo,122.0f/260.0f*344.0f/356.0f);}
        if(mode==Register) for(auto* field:fields) { CRect input;field->GetComponentRect(&input);field->ComponentToScreen(&input);rectangle(device,input,0xff8c886b);input.DeflateRect(1,1);rectangle(device,input,0xff08121b); }
        return DrawKids(tick);
    }
};
void selfTest(CTClientWnd* window,unsigned tick) {
    int index=0;auto require=[&](bool value){++index;if(!value){std::fprintf(stderr,"FAIL native account control test %d\n",index);std::abort();}};
    {
        Panel p(window,Panel::Register);
        p.fields[0]->SetText("VisibleAccount");p.fields[1]->SetText("a@example.test");
        p.fields[2]->SetText("Secret123");p.fields[3]->SetText("Secret123");
        p.Render(tick);
        require(p.fields[0]->displayMatches("VisibleAccount"));require(p.fields[1]->displayMatches("a@example.test"));
        require(p.fields[2]->displayMatches("*********"));require(p.fields[3]->displayMatches("*********"));
        p.OnKeyDown(VK_TAB,1,0);require(p.m_pFocus==p.fields[1]);
        p.OnNotify(3,TNM_LCLICK,nullptr);p.Render(tick);
        require(p.show->m_strText=="Hide Password");
        require(p.fields[2]->displayMatches("Secret123"));require(p.fields[3]->displayMatches("Secret123"));
        p.OnNotify(3,TNM_LCLICK,nullptr);p.Render(tick);require(p.fields[2]->displayMatches("*********"));require(p.show->m_strText=="Show Password");
        CRect button;p.submit->GetComponentRect(&button);p.submit->ComponentToScreen(&button);
        p.submit->OnMouseMove(0,CPoint(button.left+2,button.top+2));require(p.submit->hovered);
        p.submit->OnMouseMove(0,CPoint(0,0));require(!p.submit->hovered);
        p.OnNotify(2,TNM_LCLICK,nullptr);require(p.fields[0]->m_strText.GetLength()>0);
    }
    {Panel p(window,Panel::Success,"CreatedAccount");p.Render(tick);require(p.account=="CreatedAccount");require(p.submit->m_id==7);}
    require(window->GetMainFrame()!=&window->m_MainGame);
    panel.reset(new Panel(window,Panel::Register));
    panel->submit->EnableComponent(FALSE);panel->SwitchFocus(panel->submit);
    require(input(0,VK_RETURN,1,0));require(panel->message->m_strText.IsEmpty());require(input(4,VK_RETURN,1,0));
    require(input(0,VK_ESCAPE,1,0));require(input(0,VK_ESCAPE,1,0x4000));require(input(1,VK_ESCAPE,1,0));require(input(4,VK_ESCAPE,1,0));
    require(!input(0,VK_ESCAPE,1,0));panel.reset();
    std::fputs("PASS native account controls: visible name/email, masked/revealed passwords, Tab, random-name action, disabled Enter and Escape autorepeat isolation\n",stderr);
}
void close() {
    fourstory_accounts_cancel();
    // Destruction is deferred until input/Render returns; no delete-this callback.
    if(panel) panel->ShowComponent(FALSE);
}
}
void open(CTClientWnd* window) {
    if(!window)return;
    if(panel && panel->IsVisible()) return;
    panel.reset(new Panel(window,Panel::Register));
}
void logged_out() {
    close();autoShown=false;
}
void render(CTClientWnd* window,unsigned tick) {
    owner=window;
    if(!art)art.reset(new Artwork);
    static bool tested=false;if(!tested && std::getenv("FOURSTORY_ACCOUNTS_SELF_TEST")){tested=true;selfTest(window,tick);}
    if(panel && !panel->IsVisible()) panel.reset();
    auto* login=window->m_TNet.FindFrame(ID_FRAME_LOGIN_NEW);
    if(!autoShown && login && login->IsVisible()) {
        autoShown=true;
        if(std::getenv("FOURSTORY_ACCOUNTS_TEST_REGISTER") && !panel) panel.reset(new Panel(window,Panel::Register));
    }
    if(panel && panel->waiting && panel->mode==Panel::Register) {
        int result=fourstory_accounts_poll();
        if(result!=20) {
            panel->waiting=false;
            for(auto* field:panel->fields)field->EnableComponent(TRUE);
            for(int i=2;i<4;++i) {panel->fields[i]->clearSecret();}
            if(result==21) {
                login->FindKid(ID_CTRLINST_ID)->m_strText=panel->account;
                const CString created=panel->account;
                panel.reset(new Panel(window,Panel::Success,created));
            } else panel->status(result==22?"That account name is already taken.":"No confirmation received. Try logging in before retrying.");
        }
    }
    if(panel && panel->IsVisible()) {
        IDirect3DStateBlock9* state=nullptr;
        if(SUCCEEDED(window->m_Device.m_pDevice->CreateStateBlock(D3DSBT_ALL,&state))) {
            panel->Render(tick);state->Apply();state->Release();
        }
    }
}
bool input(unsigned kind,unsigned key,unsigned repeat,unsigned flags,int x,int y) {
    if(kind==4 && key<swallowedKeys.size() && swallowedKeys[key]){swallowedKeys[key]=false;return true;}
    if(!panel || !panel->IsVisible()) {
        if(kind==0 && key<swallowedKeys.size() && swallowedKeys[key]) {
            if((flags&0x4000) || repeat>1)return true;
            swallowedKeys[key]=false; // a fresh press after focus loss is not a repeat
        }
        if(kind==1 && std::any_of(swallowedKeys.begin(),swallowedKeys.end(),[](bool held){return held;}))return true;
        return false;
    }
    if(kind==0 && key<swallowedKeys.size())swallowedKeys[key]=true;
    if(kind==0 && key==VK_ESCAPE) {close();return true;}
    if(panel->waiting && (kind==0 || kind==1))return true;
    if(kind==5)panel->OnMouseMove(flags,CPoint(x,y));
    if(kind==7)panel->OnLButtonDblClk(flags,CPoint(x,y));
    if(kind==0 && key==VK_RETURN){
        if(auto* button=dynamic_cast<Button*>(panel->m_pFocus)){if(button->CanProcess())panel->OnNotify(button->m_id,TNM_LCLICK,nullptr);}
        else if(panel->mode==Panel::Register && panel->submit->CanProcess())panel->OnNotify(4,TNM_LCLICK,nullptr);
        return true;
    }
    if(kind==0)panel->OnKeyDown(key,repeat,flags);
    if(kind==1)panel->OnChar(key,repeat,flags);
    if(kind==2)panel->OnLButtonDown(flags,CPoint(x,y));
    if(kind==3)panel->OnLButtonUp(flags,CPoint(x,y));
    return true;
}
void shutdown() {
    close();panel.reset();art.reset();owner=nullptr;
}
}}
