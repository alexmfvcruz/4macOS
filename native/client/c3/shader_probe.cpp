// Verify original shader creation independently of the object integration build.
#define NOMINMAX
#ifdef FOURSTORY_CLIENT_NATIVE
#include <fourstory_engine_resources.h>
#else
#include "stdafx.h"
#endif
#define FOURSTORY_C3_SCENE_LIBRARY
#include "probe.cpp"
#include "asset_shaders.h"
int main(int argc,char** argv){
    if(argc!=2)return 2;
    SDL_Window* window=nullptr;int result=0;
    SDL_SetMainReady();if(!SDL_Init(SDL_INIT_VIDEO)){std::cerr<<SDL_GetError()<<'\n';return 1;}
    try{
#ifdef _WIN32
        window=SDL_CreateWindow("C3 original shader creation",640,480,0);
#else
        window=SDL_CreateWindow("C3 original shader creation",640,480,SDL_WINDOW_VULKAN);
#endif
        if(!window)throw std::runtime_error(SDL_GetError());
#ifdef _WIN32
        HWND handle=static_cast<HWND>(SDL_GetPointerProperty(SDL_GetWindowProperties(window),SDL_PROP_WINDOW_WIN32_HWND_POINTER,nullptr));
#else
        HWND handle=reinterpret_cast<HWND>(window);
#endif
        Com<IDirect3D9> api;api.p=Direct3DCreate9(D3D_SDK_VERSION);if(!api.p)throw std::runtime_error("Direct3DCreate9 failed");
        D3DADAPTER_IDENTIFIER9 adapter{};check(api->GetAdapterIdentifier(0,0,&adapter),"adapter");std::cout<<"Adapter: "<<adapter.Description<<'\n';
        D3DPRESENT_PARAMETERS pp{};pp.BackBufferWidth=640;pp.BackBufferHeight=480;pp.BackBufferFormat=D3DFMT_X8R8G8B8;pp.BackBufferCount=1;pp.Windowed=TRUE;pp.hDeviceWindow=handle;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.PresentationInterval=D3DPRESENT_INTERVAL_IMMEDIATE;
        CD3DDevice context;check(api->CreateDevice(0,D3DDEVTYPE_HAL,handle,D3DCREATE_MIXED_VERTEXPROCESSING,&pp,&context.m_pDevice),"shader device");check(context.m_pDevice->SetSoftwareVertexProcessing(TRUE),"original shader software constant bank");
        initializeShaders(context,std::filesystem::absolute(argv[1]));presentInfo(context.m_pDevice);
        unsigned vertex=0,pixel=0;for(auto* p:context.m_pVertexShader)vertex+=p!=nullptr;for(auto* p:context.m_pPixelShader)pixel+=p!=nullptr;
        if(vertex!=2||pixel!=24)throw std::runtime_error("incomplete shader creation");
        std::cout<<"PASS original shader creation: "<<vertex<<" vertex, "<<pixel<<" pixel, 2 declarations; original object drawing NOT TESTED\n";
    }catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';result=1;}
    if(window)SDL_DestroyWindow(window);SDL_Quit();return result;
}
