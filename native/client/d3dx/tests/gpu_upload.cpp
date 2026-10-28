// Native-only integration: real DXVK device creation and DDS mip lock/readback.
#ifdef FOURSTORY_D3DX_EMPTY_TEST
#include "../../device/empty_device.h"
#else
#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#endif
#include <d3dx9.h>
#include "../baseline/dds_upload_hashes.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <sstream>
template<class T> struct Ref {T* p=nullptr;~Ref(){if(p)p->Release();}};
void require(HRESULT hr,const char* operation){if(FAILED(hr)){std::ostringstream message;message<<operation<<" 0x"<<std::hex<<unsigned(hr);throw std::runtime_error(message.str());}}
void upload(IDirect3DDevice9* device,const char* path){
    std::ifstream in(path,std::ios::binary);if(!in)throw std::runtime_error("DDS file unavailable");std::vector<BYTE> data((std::istreambuf_iterator<char>(in)),{});
    D3DXIMAGE_INFO info;require(D3DXGetImageInfoFromFileInMemory(data.data(),UINT(data.size()),&info),"DDS info");
    Ref<IDirect3DTexture9> texture;
    require(D3DXCreateTextureFromFileInMemoryEx(device,data.data(),UINT(data.size()),D3DX_DEFAULT,D3DX_DEFAULT,info.MipLevels,0,info.Format,D3DPOOL_MANAGED,D3DX_FILTER_NONE,D3DX_DEFAULT,0,nullptr,nullptr,&texture.p),"DDS texture");
    fourstory_dds::Image image;require(fourstory_dds::parse(data.data(),UINT(data.size()),image),"DDS layout");
    auto sourceHash=dds_test_reference::sha256(data.data(),data.size());
    for(UINT i=0;i<info.MipLevels;++i){D3DLOCKED_RECT lock;require(texture.p->LockRect(i,&lock,nullptr,D3DLOCK_READONLY),"readback lock");
        D3DSURFACE_DESC desc{};require(texture.p->GetLevelDesc(i,&desc),"readback descriptor");
        auto pixels=fourstory_dds::layout(desc.Format);UINT rows=pixels.block?(desc.Height+3)/4:desc.Height;
        UINT rowBytes=pixels.block?((desc.Width+3)/4)*pixels.block:desc.Width*pixels.bytes;
        bool equal=lock.pBits&&lock.Pitch>=int(rowBytes);
        std::vector<BYTE> active;
        if(equal)for(UINT row=0;row<rows;++row){auto* bytes=static_cast<BYTE*>(lock.pBits)+std::size_t(row)*lock.Pitch;active.insert(active.end(),bytes,bytes+rowBytes);}
        require(texture.p->UnlockRect(i),"readback unlock");
        const char* expected=dds_test_reference::find(sourceHash,i);
        if(!expected)throw std::runtime_error("missing original Windows DDS baseline");
        if(!equal||dds_test_reference::sha256(active.data(),active.size())!=expected)throw std::runtime_error("Windows DDS mip-byte mismatch");
    }
    Ref<IDirect3DSurface9> scratch,target;
    require(device->CreateOffscreenPlainSurface(info.Width,info.Height,D3DFMT_A8R8G8B8,D3DPOOL_SCRATCH,&scratch.p,nullptr),"scratch surface");
    require(D3DXLoadSurfaceFromFileInMemory(scratch.p,nullptr,nullptr,data.data(),UINT(data.size()),nullptr,D3DX_FILTER_NONE,0,nullptr),"DDS to scratch");
    Ref<IDirect3DTexture9> output;require(D3DXCreateTexture(device,info.Width,info.Height,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&output.p),"D3DX empty texture");
    require(output.p->GetSurfaceLevel(0,&target.p),"managed surface");
    require(D3DXLoadSurfaceFromSurface(target.p,nullptr,nullptr,scratch.p,nullptr,nullptr,D3DX_FILTER_NONE,0),"scratch to managed");
    D3DLOCKED_RECT left,right;require(scratch.p->LockRect(&left,nullptr,D3DLOCK_READONLY),"scratch read");require(target.p->LockRect(&right,nullptr,D3DLOCK_READONLY),"managed read");
    bool same=true;for(UINT y=0;y<info.Height;++y)if(std::memcmp(static_cast<BYTE*>(left.pBits)+std::size_t(y)*left.Pitch,static_cast<BYTE*>(right.pBits)+std::size_t(y)*right.Pitch,info.Width*4))same=false;
    require(target.p->UnlockRect(),"managed unlock");require(scratch.p->UnlockRect(),"scratch unlock");if(!same)throw std::runtime_error("surface copy mismatch");
    // Exercise the known-tag BC3->ARGB conversion on the actual device too.
    if(std::string(path).find("conversion-from-dxt3")!=std::string::npos){
        Ref<IDirect3DTexture9> converted;require(D3DXCreateTextureFromFileInMemoryEx(device,data.data(),UINT(data.size()),D3DX_DEFAULT,D3DX_DEFAULT,info.MipLevels,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,D3DX_FILTER_NONE,D3DX_DEFAULT,0,nullptr,nullptr,&converted.p),"BC3 to ARGB");
        D3DLOCKED_RECT lock;require(converted.p->LockRect(0,&lock,nullptr,D3DLOCK_READONLY),"conversion lock");
        std::ofstream out(std::string(path)+".bgra",std::ios::binary);for(UINT y=0;y<info.Height;++y)out.write(static_cast<char*>(lock.pBits)+std::size_t(y)*lock.Pitch,info.Width*4);
        require(converted.p->UnlockRect(0),"conversion unlock");out.close();if(!out)throw std::runtime_error("conversion dump failed");}
    std::cout<<
#ifdef FOURSTORY_D3DX_EMPTY_TEST
    "PASS CPU empty-device DDS texture/mip bytes "
#else
    "PASS real DXVK DDS texture/mip bytes "
#endif
    <<path<<'\n';
}
#ifdef FOURSTORY_D3DX_EMPTY_TEST
int main(int argc,char** argv){
    try{
        {Ref<IDirect3DDevice9> device;require(fourstory::device::CreateEmptyDevice(&device.p),"empty device");for(int i=1;i<argc;++i)upload(device.p,argv[i]);}
        const auto& stats=fourstory::device::statistics();
        if(stats.live_objects||stats.live_bytes||!stats.not_executed.empty())throw std::runtime_error("resource leak or unsupported empty-device call");
        std::cout<<"PASS actual C2 empty device DDS/surface integration; no draws/state/Present executed\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
#else
int main(int argc,char** argv){
    SDL_Window* window=nullptr;
    try{SDL_SetMainReady();if(!SDL_Init(SDL_INIT_VIDEO))throw std::runtime_error(SDL_GetError());
        window=SDL_CreateWindow("D3DX DDS device integration",640,480,SDL_WINDOW_VULKAN|SDL_WINDOW_HIDDEN);if(!window)throw std::runtime_error(SDL_GetError());
        Ref<IDirect3D9> api;api.p=Direct3DCreate9(D3D_SDK_VERSION);if(!api.p)throw std::runtime_error("no D3D9 backend");
        D3DPRESENT_PARAMETERS params={};params.BackBufferWidth=640;params.BackBufferHeight=480;params.BackBufferFormat=D3DFMT_X8R8G8B8;params.BackBufferCount=1;
        params.SwapEffect=D3DSWAPEFFECT_DISCARD;params.hDeviceWindow=reinterpret_cast<HWND>(window);params.Windowed=TRUE;
        Ref<IDirect3DDevice9> device;require(api.p->CreateDevice(0,D3DDEVTYPE_HAL,params.hDeviceWindow,D3DCREATE_HARDWARE_VERTEXPROCESSING,&params,&device.p),"device");
        for(int i=1;i<argc;++i)upload(device.p,argv[i]);
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';if(window)SDL_DestroyWindow(window);SDL_Quit();return 1;}
    if(window)SDL_DestroyWindow(window);SDL_Quit();return 0;
}

#endif
