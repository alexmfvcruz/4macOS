// Original resource/object/mesh/animation path with a real D3D9 device.
#ifdef FOURSTORY_CLIENT_NATIVE
#define NOMINMAX
#include <fourstory_engine_resources.h>
#else
#include "stdafx.h"
#undef min
#undef max
#endif
#define FOURSTORY_C3_SCENE_LIBRARY
#include "probe.cpp"
#include "checked_device.h"
#include "asset_scenes.h"
#include "asset_shaders.h"
#include <filesystem>
#include <cmath>
#include <set>
#include <memory>
#include <cstdlib>
#include "asset_diagnostics.h"

namespace {
struct Video {
    SDL_Window* window=nullptr;
    Video(){SDL_SetMainReady();if(!SDL_Init(SDL_INIT_VIDEO))throw std::runtime_error(SDL_GetError());}
    ~Video(){if(window)SDL_DestroyWindow(window);SDL_Quit();}
};
struct ResourceGlobals {
    ResourceGlobals(CD3DDevice& context){CTachyonRes::m_pDEVICE=&context;CTachyonMesh::InitGlobalVB();CTachyonMesh::m_bSoftwareVP=TRUE;}
    ~ResourceGlobals(){CTachyonMesh::ReleaseGlobalVB();CTachyonRes::m_pDEVICE=nullptr;}
};

void load(CTachyonRes& r){
#ifdef FOURSTORY_CLIENT_NATIVE
    r.Init("TClient",0);
#else
    r.m_bGroupID=0;r.LoadTEX("TClient");r.LoadIMGBUF("TClient");r.LoadIMG("TClient");r.LoadANI("TClient");r.LoadMESH("TClient");r.LoadOBJ("TClient");r.LoadSFX("TClient");r.LoadMAP("TClient");r.Complate();
#endif
}
void validateMesh(CTachyonMesh& mesh){
    if(!mesh.m_pDATA||!mesh.m_pDATA->m_pVB||!mesh.m_pDATA->m_dwCount)throw std::runtime_error("selected mesh is empty");
    unsigned stride=mesh.m_dwNodeCount?sizeof(WMESHVERTEX):sizeof(MESHVERTEX);
    for(DWORD i=0;i<mesh.m_pDATA->m_dwCount;++i){
        const auto* p=mesh.m_pDATA->m_pVB+size_t(i)*stride;
        for(unsigned at=0;at<stride;at+=4){
            if(mesh.m_dwNodeCount&&at==24){
                for(unsigned b=0;b<4;++b)if(p[at+b]>mesh.m_dwNodeCount)throw std::runtime_error("bone index out of bounds");
                continue; // packed byte indices, not float.
            }
            float value;std::memcpy(&value,p+at,4);if(!std::isfinite(value))throw std::runtime_error("selected mesh has nonfinite geometry");
        }
    }
    for(DWORD group=0;group<mesh.m_dwMeshCount;++group)for(auto* segment:mesh.m_pMESH[group][0]->m_vIB){
        if(segment->m_dwCount%3)throw std::runtime_error("nontriangle mesh segment");
        for(DWORD i=0;i<segment->m_dwCount;++i)if(segment->m_pIB[i]>=mesh.m_pDATA->m_dwCount)throw std::runtime_error("mesh index out of bounds");
    }
}
template<size_t N> void selectCloths(CTachyonObject& o,CTachyonRes& r,CD3DDevice& c,const C3Cloth (&parts)[N],bool skinned){
    for(const auto& part:parts){
        auto found=r.m_mapMESH.find(part.mesh);if(found==r.m_mapMESH.end())throw std::runtime_error("selected mesh missing");
        auto* mesh=static_cast<CTachyonMesh*>(found->second);validateMesh(*mesh);
        if(bool(mesh->m_dwNodeCount)!=skinned)throw std::runtime_error("unexpected mesh skinning category");
        if(!mesh->m_bUseVB)throw std::runtime_error("selected mesh does not use original global buffers");
        o.SetCloth(&c,part.kind,part.cloth,part.selection);
        auto entry=o.m_OBJ.m_mapCLK.find(part.kind);
        if(entry==o.m_OBJ.m_mapCLK.end()||!entry->second->m_pMESH||entry->second->m_pMESH->m_pMESH!=mesh)throw std::runtime_error("original cloth selection differs");
        std::cout<<"MESH "<<part.mesh<<" nodes="<<mesh->m_dwNodeCount<<" vertices="<<mesh->m_pDATA->m_dwCount<<" finite=1\n";
    }
    o.ResetOBJPart(&c);o.m_bUseSHADER=TRUE;o.m_nVS=skinned?VS_WMESHVERTEX:VS_MESHVERTEX;
    if(o.m_OBJ.m_mapDRAW.empty())throw std::runtime_error("no original draw parts");
}
size_t validateCapture(const std::string& prefix){
    const std::string header="P6\n640 480\n255\n";
    auto read=[&](const std::string& path){std::ifstream in(path,std::ios::binary|std::ios::ate);if(!in||in.tellg()!=std::streamoff(header.size()+640*480*3))throw std::runtime_error("invalid saved readback size");in.seekg(0);std::vector<char> bytes(header.size()+640*480*3);in.read(bytes.data(),bytes.size());if(!in||std::string(bytes.data(),header.size())!=header)throw std::runtime_error("invalid saved readback header");return bytes;};
    auto first=read(prefix+"-first.ppm"),last=read(prefix+".ppm");if(first!=last)throw std::runtime_error("frozen original scene changed between frames");
    size_t foreground=0;for(size_t i=header.size();i<last.size();i+=3)if(static_cast<unsigned char>(last[i])!=32||static_cast<unsigned char>(last[i+1])!=48||static_cast<unsigned char>(last[i+2])!=64)++foreground;
    if(foreground<1000)throw std::runtime_error("original scene is blank or too small");return foreground;
}
void frameConstants(CD3DDevice& c,CD3DCamera& camera,bool shader){
    auto* d=c.m_pDevice;check(d->SetSoftwareVertexProcessing(TRUE),"software constant bank");
    camera.Activate(TRUE);
    if(!shader){
        D3DMATERIAL9 material{};material.Diffuse={1,1,1,1};material.Ambient={1,1,1,1};
        D3DLIGHT9 light{};light.Type=D3DLIGHT_DIRECTIONAL;light.Direction={0,-.8f,.6f};light.Diffuse={.8f,.8f,.8f,1};light.Ambient={.1f,.1f,.1f,1};
        check(d->SetVertexShader(nullptr),"fixed-function vertex path");check(d->SetPixelShader(nullptr),"fixed-function pixel path");
        check(d->SetMaterial(&material),"fixed material");check(d->SetLight(0,&light),"fixed light");check(d->LightEnable(0,TRUE),"fixed light enable");
        check(d->SetRenderState(D3DRS_LIGHTING,TRUE),"fixed lighting");check(d->SetRenderState(D3DRS_AMBIENT,0xff404040),"fixed ambient");check(d->SetRenderState(D3DRS_COLORVERTEX,FALSE),"material colors");
        check(d->SetRenderState(D3DRS_SPECULARENABLE,FALSE),"specular off");return;
    }

    D3DXMATRIX viewProj=camera.m_matView*camera.m_matProjection,transposed;D3DXMatrixTranspose(&transposed,&viewProj);
    check(d->SetVertexShaderConstantF(7,&transposed._11,4),"view projection constants");
    float common[]={1,0,.5f,765.005859f},pixel[]={.5f,0,256,257,0,1,2,4};
    check(d->SetVertexShaderConstantF(0,common,1),"vertex common");check(d->SetPixelShaderConstantF(2,pixel,2),"pixel common");
    float eye[]={camera.m_vPosition.x,camera.m_vPosition.y,camera.m_vPosition.z,1};
    float ambient[]={.25f,.25f,.25f,1},white[]={1,1,1,1},direction[]={0,-.8f,.6f,0},lightAmbient[]={.1f,.1f,.1f,1},diffuse[]={.8f,.8f,.8f,1};int lights[]={1,0,1,0};
    check(d->SetVertexShaderConstantF(1,eye,1),"camera constants");check(d->SetVertexShaderConstantF(2,ambient,1),"ambient constants");
    check(d->SetVertexShaderConstantF(139,white,1),"material ambient");check(d->SetVertexShaderConstantF(140,white,1),"material diffuse");
    check(d->SetVertexShaderConstantF(11,direction,1),"direction constants");check(d->SetVertexShaderConstantF(141,lightAmbient,1),"light ambient");check(d->SetVertexShaderConstantF(269,diffuse,1),"light diffuse");check(d->SetVertexShaderConstantI(0,lights,1),"light count");
    check(d->SetRenderState(D3DRS_SRGBWRITEENABLE,FALSE),"sRGB write off");check(d->SetRenderState(D3DRS_ALPHAFUNC,D3DCMP_GREATEREQUAL),"alpha comparison");
    for(DWORD stage=0;stage<2;++stage){check(d->SetSamplerState(stage,D3DSAMP_MINFILTER,D3DTEXF_LINEAR),"linear min");check(d->SetSamplerState(stage,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR),"linear mag");check(d->SetSamplerState(stage,D3DSAMP_SRGBTEXTURE,FALSE),"linear textures");check(d->SetSamplerState(stage,D3DSAMP_ADDRESSU,D3DTADDRESS_WRAP),"wrap U");check(d->SetSamplerState(stage,D3DSAMP_ADDRESSV,D3DTADDRESS_WRAP),"wrap V");}
}
}
int main(int argc,char** argv){
    if(argc!=5&&argc!=7){std::cerr<<"usage: c3-assets OUTPUT_PREFIX static|character|character-ffp ASSET_DIR SHADER_DIR [FRAMES WARMUP]\n";return 2;}
    try{
        const std::string scene=argv[2];if(scene!="static"&&scene!="character"&&scene!="character-ffp")throw std::runtime_error("unknown asset scene");
        const bool character=scene!="static",shader=scene!="character-ffp";const auto prefix=std::filesystem::absolute(argv[1]).string(),assets=std::filesystem::absolute(argv[3]).string();const auto shaders=std::filesystem::absolute(argv[4]);
        const int frames=argc==7?std::stoi(argv[5]):120,warmup=argc==7?std::stoi(argv[6]):30;if(frames<2||frames>10000||warmup<0||warmup>10000)throw std::runtime_error("invalid frame count");
        if(const char* value=std::getenv("C3_FORCE_LOD_BIAS"))std::cout<<"DIAGNOSTIC forced_sampler_lod_bias="<<value<<'\n';
        if(std::getenv("C3_DIAGNOSTICS"))std::cout<<"DIAGNOSTIC texture_and_sampler_dump=1\n";
        Video video;
#ifdef _WIN32
        video.window=SDL_CreateWindow("4Story C3 original Windows objects",640,480,0);
#else
        video.window=SDL_CreateWindow("4Story C3 original objects / DXVK",640,480,SDL_WINDOW_VULKAN);
#endif
        if(!video.window)throw std::runtime_error(SDL_GetError());
#ifdef _WIN32
        HWND handle=static_cast<HWND>(SDL_GetPointerProperty(SDL_GetWindowProperties(video.window),SDL_PROP_WINDOW_WIN32_HWND_POINTER,nullptr));
#else
        HWND handle=reinterpret_cast<HWND>(video.window);
#endif
        if(!handle)throw std::runtime_error("missing window handle");
        Com<IDirect3D9> api;api.p=Direct3DCreate9(D3D_SDK_VERSION);if(!api.p)throw std::runtime_error("Direct3DCreate9 failed");
        D3DADAPTER_IDENTIFIER9 adapter{};check(api->GetAdapterIdentifier(0,0,&adapter),"adapter");std::cout<<"Adapter: "<<adapter.Description<<'\n';
        D3DPRESENT_PARAMETERS pp{};pp.BackBufferWidth=640;pp.BackBufferHeight=480;pp.BackBufferFormat=D3DFMT_X8R8G8B8;pp.BackBufferCount=1;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=handle;pp.Windowed=TRUE;pp.PresentationInterval=D3DPRESENT_INTERVAL_IMMEDIATE;pp.EnableAutoDepthStencil=TRUE;pp.AutoDepthStencilFormat=D3DFMT_D24S8;
        Com<IDirect3DDevice9> real;check(api->CreateDevice(0,D3DDEVTYPE_HAL,handle,D3DCREATE_MIXED_VERTEXPROCESSING,&pp,real.put()),"real device");
        Com<CheckedDevice> observed;observed.p=new CheckedDevice(real.p);CD3DDevice context;context.m_pDevice=observed.p;context.m_pDevice->AddRef();context.m_option.m_nTextureDetail=TEXTURE_DETAIL_HI;context.m_option.m_dwScreenX=640;context.m_option.m_dwScreenY=480;CD3DDevice::m_bAnisotropic=FALSE;
        check(context.m_pDevice->GetDeviceCaps(&context.m_vCAPS),"device caps");check(context.m_pDevice->SetSoftwareVertexProcessing(TRUE),"mixed software processing");if(shader)initializeShaders(context,shaders);
        presentInfo(context.m_pDevice);
        ResourceGlobals globals(context);
        if(!std::filesystem::is_directory(assets))throw std::runtime_error("asset directory missing");std::filesystem::current_path(assets);
        CTachyonRes resources;load(resources);observed->ensure();
        const DWORD objectId=character?c3CharacterObject:c3StaticObject;LPOBJECT source=resources.GetOBJ(objectId);if(!source)throw std::runtime_error("original object missing");
        CTachyonObject object;object.InitOBJ(source);object.m_pHost=nullptr;object.m_bSFX=FALSE;object.m_bLOD=FALSE;object.m_bBlend=FALSE;
        if(character){selectCloths(object,resources,context,c3CharacterCloths,true);object.SetAction(c3CharacterAction,c3CharacterAnimation,FALSE);if(!object.GetCurANI())throw std::runtime_error("missing chosen animation");object.CalcTick(context.m_pDevice,c3FixedTick);}
        else{selectCloths(object,resources,context,c3StaticCloths,false);object.CalcFrame(FALSE);}
        object.m_bUseSHADER=shader?TRUE:FALSE;object.m_dwTexTick=0;observed->ensure();
        if(character){auto* ani=object.GetCurANI();auto* data=ani&&ani->m_pANI?ani->m_pANI->GetAniData():nullptr;
            if(!data||!data->m_pAni||data->m_pAni->GetNodeCount()+1>source->m_bPivotCount)throw std::runtime_error("animation exceeds original object palette");
            std::cout<<"ANIMATION nodes="<<data->m_pAni->GetNodeCount()<<" fixed_tick="<<c3FixedTick<<" pose_time="<<object.m_fActTime<<'\n';
        }
        CD3DCamera camera;const float height=character?2.6f:4.8f;
        if(!camera.InitOrthoCamera(context.m_pDevice,.1f,300.f,height*640.f/480.f,height))throw std::runtime_error("camera init failed");
        D3DXVECTOR3 eye=character?D3DXVECTOR3(3.f,1.8f,-4.5f):D3DXVECTOR3(5.f,3.7f,-6.f),target=character?D3DXVECTOR3(0,1,0):D3DXVECTOR3(0,1.4f,0),up(0,1,0);camera.SetPosition(eye,target,up,FALSE);
        std::cout<<"SCENE "<<scene<<" object="<<objectId<<" action="<<(character?c3CharacterAction:0)<<" animation="<<(character?c3CharacterAnimation:0)<<" tick="<<(character?c3FixedTick:0)<<" original_shader="<<(shader?(character?"VS_WMESH":"VS_MESH"):"fixed_function_3weights")<<'\n';
        Com<IDirect3DSurface9> back;check(context.m_pDevice->GetRenderTarget(0,back.put()),"backbuffer");std::vector<double> timing;
        AssetDiagnostics diagnostics(prefix);
        for(int i=0;i<frames+warmup;++i){
            observed->before_draw=(std::getenv("C3_DIAGNOSTICS")||std::getenv("C3_FORCE_LOD_BIAS"))?std::function<void(IDirect3DDevice9*)>([&](IDirect3DDevice9* d){diagnostics.draw(d,i==0);}):nullptr;
            SDL_Event event;while(SDL_PollEvent(&event))if(event.type==SDL_EVENT_QUIT)throw std::runtime_error("window closed before completion");
            auto start=std::chrono::steady_clock::now();frameConstants(context,camera,shader);check(context.m_pDevice->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff203040,1,0),"clear color/depth");check(context.m_pDevice->BeginScene(),"begin scene");const auto before=observed->draw_calls;object.Render(&context,&camera);check(context.m_pDevice->EndScene(),"end scene");observed->ensure();if(observed->draw_calls==before)throw std::runtime_error("original renderer issued no draw");
            if(i==0)capture(context.m_pDevice,back.p,prefix+"-startup.ppm");
            if(i==(warmup?warmup-1:0))capture(context.m_pDevice,back.p,prefix+"-first.ppm");
            if(i==frames+warmup-1)capture(context.m_pDevice,back.p,prefix+".ppm");
            check(context.m_pDevice->Present(nullptr,nullptr,nullptr,nullptr),"Present");auto end=std::chrono::steady_clock::now();if(i>=warmup&&i!=frames+warmup-1&&i!=0)timing.push_back(std::chrono::duration<double,std::milli>(end-start).count());
        }
        if(const char* hold=std::getenv("C3_CAPTURE_HOLD_MS")){int ms=std::stoi(hold);if(ms<0||ms>10000)throw std::runtime_error("invalid capture hold");SDL_Delay(ms);}
        const auto foreground=validateCapture(prefix);std::cout<<"IMAGE foreground_pixels="<<foreground<<" frozen_first_last_equal=1\n";
        std::ofstream out(prefix+"-timing.tsv");out<<"frame\tcpu_draw_present_ms\n";for(size_t i=0;i<timing.size();++i)out<<i<<'\t'<<std::fixed<<std::setprecision(6)<<timing[i]<<'\n';out.close();if(!out)throw std::runtime_error("timing write failed");
        observed->ensure();std::cout<<"PASS original "<<scene<<" draw, readback and Present; "<<timing.size()<<" timed frames; checked_draws="<<observed->draw_calls<<" failed_device_calls="<<observed->failures<<'\n';
    }catch(CFileException* e){std::cerr<<"file failure cause="<<e->m_cause<<" errno="<<e->m_lOsError<<'\n';e->Delete();return 1;}
    catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
    return 0;
}
