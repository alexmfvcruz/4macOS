// Analytic colored-mip oracle; no game geometry or sharpening post-process.
#define FOURSTORY_C3_SCENE_LIBRARY
#include "probe.cpp"
#include <filesystem>
#include <array>
std::vector<DWORD> loadCode(const std::filesystem::path& dir,const std::string& name){
    std::ifstream in(dir/name,std::ios::binary);std::vector<char> bytes((std::istreambuf_iterator<char>(in)),{});
    if(bytes.empty()||bytes.size()%4)throw std::runtime_error("invalid fixture shader "+name);
    std::vector<DWORD> code(bytes.size()/4);std::memcpy(code.data(),bytes.data(),bytes.size());if(code.back()!=D3DSIO_END)throw std::runtime_error("missing shader end");return code;
}
constexpr DWORD mipColors[]={0xffff0000,0xff00ff00,0xff0000ff,0xffffff00,0xffff00ff,0xff00ffff,0xffffffff,0xff808080,0xff000000};
struct LODVertex{float pos[4],uv[4];};
void shaderQuad(IDirect3DDevice9* d,float x,float y,float w){
    LODVertex v[]={{{2*x/640-1,1-2*y/480,.5f,1},{0,0,0,w}},{{2*(x+64)/640-1,1-2*y/480,.5f,1},{1,0,0,w}},{{2*x/640-1,1-2*(y+64)/480,.5f,1},{0,1,0,w}},{{2*(x+64)/640-1,1-2*(y+64)/480,.5f,1},{1,1,0,w}}};
    check(d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,v,sizeof(LODVertex)),"LOD shader quad");
}
void validateLOD(const std::string& path){
    std::ifstream in(path,std::ios::binary);std::string line;for(int i=0;i<3;++i)std::getline(in,line);std::vector<unsigned char> rgb((std::istreambuf_iterator<char>(in)),{});if(rgb.size()!=640*480*3)throw std::runtime_error("LOD capture size");
    size_t checked=0;
    for(unsigned row=0;row<6;++row)for(unsigned col=0;col<6;++col){
        const unsigned mip=(row==2?3:2)+(col%3)-1;const DWORD color=mipColors[mip];
        for(unsigned y=36+72*row;y<92+72*row;++y)for(unsigned x=36+96*col;x<92+96*col;++x){
            const auto i=3*(y*640+x);if(rgb[i]!=((color>>16)&255)||rgb[i+1]!=((color>>8)&255)||rgb[i+2]!=(color&255)){
                std::ostringstream s;s<<"LOD oracle row="<<row<<" col="<<col<<" pixel="<<x<<','<<y<<" expected="<<std::hex<<color<<" actual="<<unsigned(rgb[i])<<','<<unsigned(rgb[i+1])<<','<<unsigned(rgb[i+2]);throw std::runtime_error(s.str());}++checked;
        }
    }
    std::cout<<"PASS LOD oracle "<<checked<<" interior pixels; FFP stages0/1, PS implicit/texldb/texldl/texldd samplers0/15, VS texldl samplers0/3; API bias -1/0/+1\n";
}
int main(int argc,char** argv){
 if(argc!=3){std::cerr<<"usage: c3-lod OUTPUT_PREFIX SHADER_DIR\n";return 2;}
 SDL_Window* window=nullptr;
 try{
    SDL_SetMainReady();if(!SDL_Init(SDL_INIT_VIDEO))throw std::runtime_error(SDL_GetError());
#ifdef _WIN32
    window=SDL_CreateWindow("4Story LOD bias oracle",640,480,0);
#else
    window=SDL_CreateWindow("4Story LOD bias oracle",640,480,SDL_WINDOW_VULKAN);
#endif
    if(!window)throw std::runtime_error(SDL_GetError());
#ifdef _WIN32
    HWND handle=static_cast<HWND>(SDL_GetPointerProperty(SDL_GetWindowProperties(window),SDL_PROP_WINDOW_WIN32_HWND_POINTER,nullptr));
#else
    HWND handle=reinterpret_cast<HWND>(window);
#endif
    Com<IDirect3D9> api;api.p=Direct3DCreate9(D3D_SDK_VERSION);if(!api.p)throw std::runtime_error("D3D9 unavailable");
    D3DPRESENT_PARAMETERS pp{};pp.BackBufferWidth=640;pp.BackBufferHeight=480;pp.BackBufferFormat=D3DFMT_X8R8G8B8;pp.BackBufferCount=1;pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=handle;pp.PresentationInterval=D3DPRESENT_INTERVAL_IMMEDIATE;
    Com<IDirect3DDevice9> d;check(api->CreateDevice(0,D3DDEVTYPE_HAL,handle,D3DCREATE_HARDWARE_VERTEXPROCESSING,&pp,d.put()),"LOD real device");presentInfo(d.p);
    Com<IDirect3DTexture9> texture;check(d->CreateTexture(256,256,9,0,D3DFMT_A32B32G32R32F,D3DPOOL_MANAGED,texture.put(),nullptr),"LOD float texture");
    for(unsigned mip=0;mip<9;++mip){D3DLOCKED_RECT rect{};check(texture->LockRect(mip,&rect,nullptr,0),"LOD lock");float c[]={float((mipColors[mip]>>16)&255)/255,float((mipColors[mip]>>8)&255)/255,float(mipColors[mip]&255)/255,1};for(unsigned y=0;y<(256u>>mip);++y)for(unsigned x=0;x<(256u>>mip);++x)std::memcpy(static_cast<char*>(rect.pBits)+y*rect.Pitch+16*x,c,16);check(texture->UnlockRect(mip),"LOD unlock");}
    std::filesystem::path dir=argv[2];Com<IDirect3DVertexShader9> pass;auto code=loadCode(dir,"pass.vsh.bin");check(d->CreateVertexShader(code.data(),pass.put()),"LOD VS pass");
    std::array<Com<IDirect3DVertexShader9>,2> vertex;std::array<Com<IDirect3DPixelShader9>,8> pixel;Com<IDirect3DPixelShader9> color;
    for(unsigned i=0;i<2;++i){code=loadCode(dir,"vertex-"+std::to_string(i?3:0)+".vsh.bin");check(d->CreateVertexShader(code.data(),vertex[i].put()),"LOD vertex texture shader");}
    const char* modes[]={"implicit","bias","explicit","grad"};for(unsigned mode=0;mode<4;++mode)for(unsigned i=0;i<2;++i){code=loadCode(dir,std::string(modes[mode])+"-"+std::to_string(i?15:0)+".psh.bin");check(d->CreatePixelShader(code.data(),pixel[2*mode+i].put()),"LOD pixel shader");}
    code=loadCode(dir,"color.psh.bin");check(d->CreatePixelShader(code.data(),color.put()),"LOD color shader");
    D3DVERTEXELEMENT9 decl[]={{0,0,D3DDECLTYPE_FLOAT4,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0},{0,16,D3DDECLTYPE_FLOAT4,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,0},D3DDECL_END()};Com<IDirect3DVertexDeclaration9> declaration;check(d->CreateVertexDeclaration(decl,declaration.put()),"LOD declaration");
    Com<IDirect3DSurface9> back;check(d->GetRenderTarget(0,back.put()),"LOD backbuffer");
    for(unsigned frame=0;frame<4;++frame){check(d->Clear(0,nullptr,D3DCLEAR_TARGET,0xff203040,1,0),"LOD clear");check(d->BeginScene(),"LOD begin");
      for(unsigned row=0;row<6;++row)for(unsigned col=0;col<6;++col){state(d.p);unsigned high=col>=3;DWORD sampler=row==5?D3DVERTEXTEXTURESAMPLER0+(high?3:0):row?high?15:0:high?1:0;float bias=float(int(col%3)-1);DWORD bits;std::memcpy(&bits,&bias,4);
        check(d->SetTexture(sampler,texture.p),"LOD bind texture");check(d->SetSamplerState(sampler,D3DSAMP_MINFILTER,D3DTEXF_POINT),"LOD min");check(d->SetSamplerState(sampler,D3DSAMP_MAGFILTER,D3DTEXF_POINT),"LOD mag");check(d->SetSamplerState(sampler,D3DSAMP_MIPFILTER,D3DTEXF_POINT),"LOD mip");check(d->SetSamplerState(sampler,D3DSAMP_MIPMAPLODBIAS,bits),"LOD bias");check(d->SetSamplerState(sampler,D3DSAMP_MAXMIPLEVEL,0),"LOD min level");
        if(row==0){check(d->SetVertexShader(nullptr),"LOD FFP VS");check(d->SetPixelShader(nullptr),"LOD FFP PS");if(high){check(d->SetTexture(0,nullptr),"LOD stage0 unbind");check(d->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG2),"LOD stage0 diffuse");check(d->SetTextureStageState(0,D3DTSS_COLORARG2,D3DTA_DIFFUSE),"LOD diffuse");}check(d->SetTextureStageState(sampler,D3DTSS_COLOROP,D3DTOP_SELECTARG1),"LOD select texture");check(d->SetTextureStageState(sampler,D3DTSS_COLORARG1,D3DTA_TEXTURE),"LOD texture arg");check(d->SetTextureStageState(sampler,D3DTSS_TEXCOORDINDEX,0),"LOD coordinate");quad(d.p,float(32+96*col),float(32+72*row),64,64,0xffffffff);}
        else{check(d->SetVertexShader(row==5?vertex[high].p:pass.p),"LOD VS");check(d->SetPixelShader(row==5?color.p:pixel[2*(row-1)+high].p),"LOD PS");check(d->SetVertexDeclaration(declaration.p),"LOD vertex layout");float gradients[]={1.f/64,0,0,0,0,1.f/64,0,0};check(d->SetPixelShaderConstantF(0,gradients,2),"LOD gradients");shaderQuad(d.p,float(32+96*col),float(32+72*row),row==2?1.f:2.f);}
      }
      check(d->EndScene(),"LOD end");if(frame==3)capture(d.p,back.p,std::string(argv[1])+".ppm");check(d->Present(nullptr,nullptr,nullptr,nullptr),"LOD present");
    }
    validateLOD(std::string(argv[1])+".ppm");
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';if(window)SDL_DestroyWindow(window);SDL_Quit();return 1;}
 if(window)SDL_DestroyWindow(window);SDL_Quit();return 0;
}
