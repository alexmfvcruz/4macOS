// One portable source: original Direct3D 9 on Windows, DXVK Native on macOS.
#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <d3d9.h>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

template<class T> struct Com {
    T* p=nullptr;
    ~Com(){if(p)p->Release();}
    Com()=default;Com(const Com&)=delete;Com& operator=(const Com&)=delete;
    T** put(){if(p){p->Release();p=nullptr;}return &p;}
    T* operator->()const{return p;}
};
void check(HRESULT hr,const char* operation){
    if(FAILED(hr)){std::ostringstream s;s<<operation<<" failed: 0x"<<std::hex<<unsigned(hr);throw std::runtime_error(s.str());}
}
void presentInfo(IDirect3DDevice9* device){
    Com<IDirect3DSwapChain9> swap;check(device->GetSwapChain(0,swap.put()),"GetSwapChain");
    D3DPRESENT_PARAMETERS actual{};check(swap->GetPresentParameters(&actual),"GetPresentParameters");
    std::cout<<"Present parameters: interval="<<actual.PresentationInterval<<" windowed="<<actual.Windowed<<" swap_effect="<<actual.SwapEffect<<" backbuffers="<<actual.BackBufferCount<<'\n';
}
struct Vertex {float x,y,z,rhw;DWORD color;float u,v;};
static_assert(sizeof(Vertex)==28,"fixed-function vertex layout");
constexpr DWORD fvf=D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1;
void quad(IDirect3DDevice9* d,float x,float y,float width,float height,DWORD color){
    // D3D9 pixel centers are integer coordinates; align rectangle edges at -.5.
    Vertex v[]={{x-.5f,y-.5f,0,1,color,0,0},{x+width-.5f,y-.5f,0,1,color,1,0},
                {x-.5f,y+height-.5f,0,1,color,0,1},{x+width-.5f,y+height-.5f,0,1,color,1,1}};
    check(d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,v,sizeof(Vertex)),"DrawPrimitiveUP");
}
void state(IDirect3DDevice9* d){
    check(d->SetFVF(fvf),"SetFVF");
    check(d->SetRenderState(D3DRS_ZENABLE,FALSE),"ZENABLE");
    check(d->SetRenderState(D3DRS_ZWRITEENABLE,FALSE),"ZWRITEENABLE");
    check(d->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE),"CULLMODE");
    check(d->SetRenderState(D3DRS_LIGHTING,FALSE),"LIGHTING");
    check(d->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE),"ALPHATESTENABLE");
    check(d->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE),"ALPHABLENDENABLE");
    check(d->SetRenderState(D3DRS_SRGBWRITEENABLE,FALSE),"SRGBWRITEENABLE");
    check(d->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_MODULATE),"COLOROP");
    check(d->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE),"COLORARG1");
    check(d->SetTextureStageState(0,D3DTSS_COLORARG2,D3DTA_DIFFUSE),"COLORARG2");
    check(d->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_MODULATE),"ALPHAOP");
    check(d->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_TEXTURE),"ALPHAARG1");
    check(d->SetTextureStageState(0,D3DTSS_ALPHAARG2,D3DTA_DIFFUSE),"ALPHAARG2");
    check(d->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE),"stage1 disable");
    for(auto item:{D3DSAMP_MINFILTER,D3DSAMP_MAGFILTER})check(d->SetSamplerState(0,item,D3DTEXF_POINT),"point sampler");
    check(d->SetSamplerState(0,D3DSAMP_MIPFILTER,D3DTEXF_NONE),"mip sampler");
    check(d->SetSamplerState(0,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP),"ADDRESSU");
    check(d->SetSamplerState(0,D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP),"ADDRESSV");
    check(d->SetSamplerState(0,D3DSAMP_SRGBTEXTURE,FALSE),"SRGBTEXTURE");
}
void texture(IDirect3DDevice9* d,Com<IDirect3DTexture9>& t){
    check(d->CreateTexture(8,8,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,t.put(),nullptr),"CreateTexture");
    D3DLOCKED_RECT lock;check(t->LockRect(0,&lock,nullptr,0),"LockRect texture");
    for(int y=0;y<8;++y)for(int x=0;x<8;++x){
        DWORD alpha=((x/2+y/2)&1)?64:255;
        DWORD pixel=(alpha<<24)|0x00e04020;
        std::memcpy(static_cast<char*>(lock.pBits)+y*lock.Pitch+4*x,&pixel,4);
    }
    check(t->UnlockRect(0),"UnlockRect texture");
}
void drawQuads(IDirect3DDevice9* d,IDirect3DTexture9* t){
    check(d->Clear(0,nullptr,D3DCLEAR_TARGET,0xff203040,1,0),"Clear");
    check(d->BeginScene(),"BeginScene");state(d);
    check(d->SetTexture(0,nullptr),"untextured");
    check(d->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG2),"solid background");
    quad(d,32,48,256,256,0xff4080c0);quad(d,352,48,256,256,0xff4080c0);
    check(d->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_MODULATE),"textured");
    check(d->SetTexture(0,t),"SetTexture");
    check(d->SetRenderState(D3DRS_ALPHATESTENABLE,TRUE),"alpha test");
    check(d->SetRenderState(D3DRS_ALPHAFUNC,D3DCMP_GREATEREQUAL),"alpha func");
    check(d->SetRenderState(D3DRS_ALPHAREF,128),"alpha ref");
    quad(d,32,48,256,256,0xffffffff);
    check(d->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE),"alpha test off");
    check(d->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE),"alpha blend");
    check(d->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA),"src blend");
    check(d->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA),"dest blend");
    check(d->SetRenderState(D3DRS_BLENDOP,D3DBLENDOP_ADD),"blend op");
    quad(d,352,48,256,256,0xffffffff);
    check(d->EndScene(),"EndScene");
}
void drawRTT(IDirect3DDevice9* d,IDirect3DSurface9* back,IDirect3DTexture9* target,IDirect3DSurface9* surface){
    // Explicit unbinding prevents the previous frame's texture/surface feedback.
    check(d->SetTexture(0,nullptr),"unbind RTT");
    check(d->SetRenderTarget(0,surface),"SetRenderTarget texture");
    D3DVIEWPORT9 rttViewport={0,0,128,128,0,1};check(d->SetViewport(&rttViewport),"RTT viewport");
    check(d->Clear(0,nullptr,D3DCLEAR_TARGET,0xff102030,1,0),"RTT Clear");
    check(d->BeginScene(),"RTT BeginScene");state(d);
    check(d->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG2),"RTT solid");
    quad(d,0,0,64,64,0xffff2020);quad(d,64,0,64,64,0xff20ff20);
    quad(d,0,64,64,64,0xff2020ff);quad(d,64,64,64,64,0xffffe020);
    check(d->EndScene(),"RTT EndScene");
    check(d->SetRenderTarget(0,back),"restore backbuffer");
    D3DVIEWPORT9 full={0,0,640,480,0,1};check(d->SetViewport(&full),"main viewport");
    check(d->Clear(0,nullptr,D3DCLEAR_TARGET,0xff203040,1,0),"main Clear");
    check(d->BeginScene(),"main BeginScene");state(d);
    check(d->SetTexture(0,target),"sample RTT");quad(d,64,48,512,384,0xffffffff);
    check(d->EndScene(),"main EndScene");
}
void capture(IDirect3DDevice9* d,IDirect3DSurface9* back,const std::string& path){
    D3DSURFACE_DESC desc;check(back->GetDesc(&desc),"backbuffer description");
    if(desc.Width!=640||desc.Height!=480||desc.Format!=D3DFMT_X8R8G8B8||desc.MultiSampleType!=D3DMULTISAMPLE_NONE)throw std::runtime_error("unexpected backbuffer readback layout");
    Com<IDirect3DSurface9> copy;
    check(d->CreateOffscreenPlainSurface(desc.Width,desc.Height,desc.Format,D3DPOOL_SYSTEMMEM,copy.put(),nullptr),"readback surface");
    check(d->GetRenderTargetData(back,copy.p),"GetRenderTargetData");
    D3DLOCKED_RECT lock;check(copy->LockRect(&lock,nullptr,D3DLOCK_READONLY),"readback LockRect");
    if(lock.Pitch<int(desc.Width*4)){copy->UnlockRect();throw std::runtime_error("unexpected readback pitch");}
    std::vector<unsigned char> rgb(desc.Width*desc.Height*3);
    for(UINT y=0;y<desc.Height;++y)for(UINT x=0;x<desc.Width;++x){
        DWORD pixel;std::memcpy(&pixel,static_cast<char*>(lock.pBits)+y*lock.Pitch+x*4,4);
        auto i=(y*desc.Width+x)*3;rgb[i]=(pixel>>16)&255;rgb[i+1]=(pixel>>8)&255;rgb[i+2]=pixel&255;
    }
    check(copy->UnlockRect(),"readback UnlockRect");
    std::ofstream out(path,std::ios::binary);out<<"P6\n"<<desc.Width<<' '<<desc.Height<<"\n255\n";
    out.write(reinterpret_cast<const char*>(rgb.data()),rgb.size());out.close();if(!out)throw std::runtime_error("image write failed");
}
#include "features.h"
#include "formats.h"
#ifndef FOURSTORY_C3_SCENE_LIBRARY
int main(int argc,char** argv){
    if(argc!=3&&argc!=5){std::cerr<<"usage: c3-probe OUTPUT_PREFIX quads|rtt|skinning|constants|relative|formats|lifecycle [FRAMES WARMUP]\n";return 2;}
    SDL_Window* window=nullptr;
    try{
        std::string scene=argv[2];if(scene!="quads"&&scene!="rtt"&&scene!="skinning"&&scene!="constants"&&scene!="relative"&&scene!="formats"&&scene!="lifecycle")throw std::runtime_error("unknown scene");
        int frames=argc==5?std::stoi(argv[3]):120,warmup=argc==5?std::stoi(argv[4]):30;
        if(frames<1||frames>10000||warmup<0||warmup>10000)throw std::runtime_error("invalid frame count");
        SDL_SetMainReady();
        if(!SDL_Init(SDL_INIT_VIDEO))throw std::runtime_error(SDL_GetError());
#ifdef _WIN32
        window=SDL_CreateWindow("4Story C3 D3D9 reference",640,480,0);
#else
        window=SDL_CreateWindow("4Story C3 DXVK / MoltenVK",640,480,SDL_WINDOW_VULKAN);
#endif
        if(!window)throw std::runtime_error(SDL_GetError());
#ifdef _WIN32
        HWND handle=static_cast<HWND>(SDL_GetPointerProperty(SDL_GetWindowProperties(window),SDL_PROP_WINDOW_WIN32_HWND_POINTER,nullptr));
#else
        HWND handle=reinterpret_cast<HWND>(window);
#endif
        if(!handle)throw std::runtime_error("native window handle unavailable");
        Com<IDirect3D9> api;api.p=Direct3DCreate9(D3D_SDK_VERSION);if(!api.p)throw std::runtime_error("Direct3DCreate9 returned null");
        D3DADAPTER_IDENTIFIER9 id={};check(api->GetAdapterIdentifier(0,0,&id),"GetAdapterIdentifier");std::cout<<"Adapter: "<<id.Description<<'\n';
        D3DPRESENT_PARAMETERS params={};params.BackBufferWidth=640;params.BackBufferHeight=480;params.BackBufferFormat=D3DFMT_X8R8G8B8;params.BackBufferCount=1;
        params.SwapEffect=D3DSWAPEFFECT_DISCARD;params.hDeviceWindow=handle;params.Windowed=TRUE;params.PresentationInterval=D3DPRESENT_INTERVAL_IMMEDIATE;
        Com<IDirect3DDevice9> device;check(api->CreateDevice(0,D3DDEVTYPE_HAL,handle,(scene=="skinning"||scene=="constants"||scene=="relative")?D3DCREATE_MIXED_VERTEXPROCESSING:D3DCREATE_HARDWARE_VERTEXPROCESSING,&params,device.put()),"CreateDevice");
#ifdef FOURSTORY_C3_CHECK_DEVICE
        Com<CheckedDevice> observed;observed.p=new CheckedDevice(device.p);
        device.p->Release();device.p=observed.p;device.p->AddRef();
#endif
        presentInfo(device.p);
        Com<IDirect3DSurface9> back;check(device->GetRenderTarget(0,back.put()),"GetRenderTarget");
        Com<IDirect3DTexture9> tex,target;Com<IDirect3DSurface9> surface;
        FormatTextures formatTextures;
        if(scene=="formats")createFormats(api.p,device.p,formatTextures);
        if(scene=="quads"||scene=="lifecycle")texture(device.p,tex);
        if(scene=="rtt"||scene=="lifecycle")createRTT(device.p,back,target,surface);
        std::vector<double> timing;bool cancelled=false;
        for(int i=0;i<frames+warmup;++i){
            SDL_Event e;while(SDL_PollEvent(&e))if(e.type==SDL_EVENT_QUIT)cancelled=true;
            if(cancelled)throw std::runtime_error("window closed before completion");
            if(scene=="lifecycle"&&i>0&&i%30==0){
                check(device->SetTexture(0,nullptr),"reset unbind");
                surface.put();target.put();back.put();
                check(device->Reset(&params),"Reset");check(device->TestCooperativeLevel(),"post-reset cooperative level");
                createRTT(device.p,back,target,surface);
                std::cout<<"RESET "<<i/30<<" default resources recreated; managed texture retained\n";
            }
            auto start=std::chrono::steady_clock::now();
            if(scene=="quads")drawQuads(device.p,tex.p);else if(scene=="rtt")drawRTT(device.p,back.p,target.p,surface.p);else if(scene=="formats")drawFormats(device.p,formatTextures);else if(scene=="lifecycle")drawLifecycle(device.p,back.p,target.p,surface.p,tex.p);else if(scene=="skinning")drawSkinning(device.p);else drawConstants(device.p,scene=="relative");
            if(scene=="lifecycle"&&i%30==0)capture(device.p,back.p,std::string(argv[1])+"-reset"+std::to_string(i/30)+".ppm");
            if(i==frames+warmup-1)capture(device.p,back.p,std::string(argv[1])+".ppm");
            check(device->Present(nullptr,nullptr,nullptr,nullptr),"Present");
#ifdef FOURSTORY_C3_CHECK_DEVICE
            observed->ensure();
#endif
            auto end=std::chrono::steady_clock::now();
            // Exclude the readback frame; Present wall time is not a GPU timestamp.
            if(i>=warmup&&i!=frames+warmup-1&&!(scene=="lifecycle"&&i%30==0))timing.push_back(std::chrono::duration<double,std::milli>(end-start).count());
        }
        std::ofstream out(std::string(argv[1])+"-timing.tsv");out<<"frame\tcpu_draw_present_ms\n";for(size_t i=0;i<timing.size();++i)out<<i<<'\t'<<std::fixed<<std::setprecision(6)<<timing[i]<<'\n';out.close();if(!out)throw std::runtime_error("timing write failed");
        std::cout<<"PASS "<<scene<<" draw, readback and Present; "<<timing.size()<<" timed frames (CPU wall time, readback excluded)\n";
        // COM objects must be destroyed before their SDL window, so leave the scope.
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';if(window)SDL_DestroyWindow(window);SDL_Quit();return 1;}
    if(window)SDL_DestroyWindow(window);SDL_Quit();return 0;
}

#endif // FOURSTORY_C3_SCENE_LIBRARY
