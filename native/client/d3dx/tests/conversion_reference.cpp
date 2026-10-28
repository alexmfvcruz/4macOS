// Portable D3DX conversion readbacks: original Windows DLL or native CPU device.
// Captures are test evidence, never a runtime lookup table.
#define NOMINMAX
#include <windows.h>
#include <d3dx9.h>
#ifdef FOURSTORY_CLIENT_NATIVE
#include "../../device/empty_device.h"
#endif
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
int main(int argc,char** argv){
    if(argc!=4)return 2;
    IDirect3DDevice9* device=nullptr;
#ifdef FOURSTORY_CLIENT_NATIVE
    if(FAILED(fourstory::device::CreateEmptyDevice(&device)))return 1;
#else
    IDirect3D9* api=Direct3DCreate9(D3D_SDK_VERSION);if(!api)return 1;
    D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=GetDesktopWindow();
    pp.BackBufferWidth=640;pp.BackBufferHeight=480;
    HRESULT hr=api->CreateDevice(0,D3DDEVTYPE_HAL,pp.hDeviceWindow,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,&device);
    if(FAILED(hr)){api->Release();return 1;}
#endif
    std::ifstream manifest(argv[1]);std::string name,line;unsigned format;bool failed=false;unsigned cases=0;
    while(std::getline(manifest,line)){
        std::istringstream fields(line);std::string extra;
        if(!(fields>>name>>format)||(fields>>extra)||(format!=D3DFMT_DXT1&&format!=D3DFMT_DXT3&&format!=D3DFMT_A8R8G8B8)){failed=true;break;}
        ++cases;
        std::ifstream input(std::string(argv[2])+"/"+name,std::ios::binary);
        std::vector<char> bytes((std::istreambuf_iterator<char>(input)),std::istreambuf_iterator<char>());
        D3DXIMAGE_INFO info{};IDirect3DTexture9* texture=nullptr;
        HRESULT hr=D3DXGetImageInfoFromFileInMemory(bytes.data(),static_cast<UINT>(bytes.size()),&info);
        if(SUCCEEDED(hr))hr=D3DXCreateTextureFromFileInMemoryEx(device,bytes.data(),static_cast<UINT>(bytes.size()),D3DX_DEFAULT,D3DX_DEFAULT,info.MipLevels,0,static_cast<D3DFORMAT>(format),D3DPOOL_MANAGED,D3DX_FILTER_NONE,D3DX_DEFAULT,0,nullptr,nullptr,&texture);
        std::printf("conversion\t%s\thresult\t%08x\n",name.c_str(),static_cast<unsigned>(hr));
        if(FAILED(hr)||!texture){failed=true;continue;}
        for(UINT level=0;level<texture->GetLevelCount();++level){D3DSURFACE_DESC desc{};D3DLOCKED_RECT lock{};
            if(FAILED(texture->GetLevelDesc(level,&desc))||FAILED(texture->LockRect(level,&lock,nullptr,D3DLOCK_READONLY))){failed=true;break;}
            const bool compressed=format==D3DFMT_DXT1||format==D3DFMT_DXT3;
            const UINT rows=compressed?(desc.Height+3)/4:desc.Height;
            const UINT active=compressed?((desc.Width+3)/4)*(format==D3DFMT_DXT1?8:16):desc.Width*4;
            if(lock.Pitch<0||static_cast<UINT>(lock.Pitch)<active){texture->UnlockRect(level);failed=true;break;}
            std::ofstream out(std::string(argv[3])+"/"+name+".level"+std::to_string(level)+".bin",std::ios::binary);
            for(UINT y=0;y<rows;++y)out.write(static_cast<const char*>(lock.pBits)+size_t(y)*lock.Pitch,active);
            out.close();if(!out)failed=true;
            std::printf("conversion\t%s\tlevel%u\t%u,%u,%u,%d\n",name.c_str(),level,unsigned(desc.Format),desc.Width,desc.Height,lock.Pitch);
            if(FAILED(texture->UnlockRect(level)))failed=true;
        }texture->Release();
    }
    device->Release();
#ifdef FOURSTORY_CLIENT_NATIVE
    const auto& stats=fourstory::device::statistics();
    failed=failed||stats.live_objects||stats.live_bytes||stats.failed_creations||!stats.not_executed.empty();
#else
    api->Release();
#endif
    return failed||!manifest.eof()||!cases?1:0;
}
