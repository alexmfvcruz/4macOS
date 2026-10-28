// Native backend preflight for the engine's texture formats. No asset loaders.
struct FormatCase {D3DFORMAT format;const char* name;bool alpha;bool compressed;};
const FormatCase formatCases[]={
    {D3DFMT_A8R8G8B8,"A8R8G8B8",true,false},{D3DFMT_X8R8G8B8,"X8R8G8B8",false,false},
    {D3DFMT_A4R4G4B4,"A4R4G4B4",true,false},{D3DFMT_R5G6B5,"R5G6B5",false,false},{D3DFMT_A8,"A8",true,false},
    {D3DFMT_DXT1,"DXT1",true,true},{D3DFMT_DXT2,"DXT2",true,true},{D3DFMT_DXT3,"DXT3",true,true},
    {D3DFMT_DXT4,"DXT4",true,true},{D3DFMT_DXT5,"DXT5",true,true}};
struct FormatTextures {Com<IDirect3DTexture9> texture[10];};
void createFormats(IDirect3D9* api,IDirect3DDevice9* d,FormatTextures& textures){
    const DWORD rgb[]={0xff0000,0x00ff00,0x0000ff,0xffffff};
    const WORD packed[]={0xf800,0x07e0,0x001f,0xffff};
    const BYTE alpha[]={255,136,0,68};
    for(UINT item=0;item<10;++item){
        const auto& f=formatCases[item];
        check(api->CheckDeviceFormat(0,D3DDEVTYPE_HAL,D3DFMT_X8R8G8B8,0,D3DRTYPE_TEXTURE,f.format),f.name);
        auto& t=textures.texture[item];check(d->CreateTexture(8,8,4,0,f.format,D3DPOOL_MANAGED,t.put(),nullptr),f.name);
        if(t->GetLevelCount()!=4)throw std::runtime_error("texture mip count mismatch");
        for(UINT level=0;level<4;++level){
            UINT side=8>>level;D3DSURFACE_DESC desc;check(t->GetLevelDesc(level,&desc),"mip description");
            if(desc.Width!=side||desc.Height!=side||desc.Format!=f.format)throw std::runtime_error("mip layout mismatch");
            D3DLOCKED_RECT lock;check(t->LockRect(level,&lock,nullptr,0),"mip lock");
            UINT rowBytes=f.compressed?((side+3)/4)*(f.format==D3DFMT_DXT1?8:16):side*(f.format==D3DFMT_A8?1:(f.format==D3DFMT_A4R4G4B4||f.format==D3DFMT_R5G6B5?2:4));
            if(lock.Pitch<int(rowBytes)){t->UnlockRect(level);throw std::runtime_error("mip pitch too small");}
            if(f.compressed){
                BYTE block[16]={};UINT colorOffset=f.format==D3DFMT_DXT1?0:8;
                // BC1 uses the transparent index only for the zero-alpha level.
                WORD color=packed[level];std::memcpy(block+colorOffset,&color,2);std::memcpy(block+colorOffset+2,&color,2);
                if(f.format==D3DFMT_DXT1&&alpha[level]==0)std::memset(block+4,255,4);
                else if(f.format==D3DFMT_DXT2||f.format==D3DFMT_DXT3)std::memset(block,(alpha[level]/17)*17,8);
                else if(f.format==D3DFMT_DXT4||f.format==D3DFMT_DXT5){block[0]=alpha[level];block[1]=alpha[level];}
                for(UINT y=0;y<(side+3)/4;++y)for(UINT x=0;x<(side+3)/4;++x)
                    std::memcpy(static_cast<char*>(lock.pBits)+y*lock.Pitch+x*(colorOffset+8),block,colorOffset+8);
            }else for(UINT y=0;y<side;++y)for(UINT x=0;x<side;++x){
                char* row=static_cast<char*>(lock.pBits)+y*lock.Pitch;
                if(f.format==D3DFMT_A8)std::memcpy(row+x,&alpha[level],1);
                else if(f.format==D3DFMT_R5G6B5)std::memcpy(row+2*x,&packed[level],2);
                else if(f.format==D3DFMT_A4R4G4B4){
                    WORD pixel=WORD((alpha[level]/17)<<12)|WORD(((rgb[level]>>20)&15)<<8)|WORD(((rgb[level]>>12)&15)<<4)|WORD((rgb[level]>>4)&15);
                    std::memcpy(row+2*x,&pixel,2);
                }else{DWORD pixel=(DWORD(alpha[level])<<24)|rgb[level];std::memcpy(row+4*x,&pixel,4);}
            }
            check(t->UnlockRect(level),"mip unlock");
        }
        std::cout<<"FORMAT "<<f.name<<" 8/4/2/1 mip upload verified\n";
    }
}
void drawFormats(IDirect3DDevice9* d,FormatTextures& textures){
    check(d->Clear(0,nullptr,D3DCLEAR_TARGET,0xff203040,1,0),"format Clear");check(d->BeginScene(),"format BeginScene");state(d);
    check(d->SetRenderState(D3DRS_ALPHATESTENABLE,TRUE),"format alpha test");
    check(d->SetRenderState(D3DRS_ALPHAFUNC,D3DCMP_GREATEREQUAL),"format alpha func");check(d->SetRenderState(D3DRS_ALPHAREF,128),"format alpha ref");
    check(d->SetSamplerState(0,D3DSAMP_MIPFILTER,D3DTEXF_POINT),"format mip filter");
    for(UINT item=0;item<10;++item){
        check(d->SetTexture(0,textures.texture[item].p),"format texture");
        // A8 carries only alpha. Test its mask with diffuse white, not texture RGB.
        check(d->SetTextureStageState(0,D3DTSS_COLOROP,item==4?D3DTOP_SELECTARG2:D3DTOP_MODULATE),"format color op");
        for(UINT level=0;level<4;++level){
            check(d->SetSamplerState(0,D3DSAMP_MAXMIPLEVEL,level),"format mip selection");
            quad(d,float(32+112*(item%5)+24*level),float(48+160*(item/5)),24,64,0xffffffff);
        }
    }
    check(d->SetTexture(0,nullptr),"format unbind");check(d->SetSamplerState(0,D3DSAMP_MAXMIPLEVEL,0),"format mip restore");
    check(d->EndScene(),"format EndScene");
}
void stateBlockRoundtrip(IDirect3DDevice9* d,IDirect3DTexture9* texture){
    state(d);check(d->SetTexture(0,texture),"stateblock texture");
    check(d->SetRenderState(D3DRS_ALPHAREF,173),"stateblock alpha ref");
    Com<IDirect3DStateBlock9> block;check(d->CreateStateBlock(D3DSBT_ALL,block.put()),"CreateStateBlock ALL");
    check(d->SetTexture(0,nullptr),"stateblock mutate texture");check(d->SetRenderState(D3DRS_ALPHAREF,9),"stateblock mutate ref");
    check(d->SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR),"stateblock mutate filter");check(block->Apply(),"stateblock Apply");
    DWORD value;check(d->GetRenderState(D3DRS_ALPHAREF,&value),"stateblock read ref");if(value!=173)throw std::runtime_error("stateblock ref mismatch");
    check(d->GetSamplerState(0,D3DSAMP_MAGFILTER,&value),"stateblock read filter");if(value!=D3DTEXF_POINT)throw std::runtime_error("stateblock filter mismatch");
    Com<IDirect3DBaseTexture9> restored;check(d->GetTexture(0,restored.put()),"stateblock read texture");
    if(restored.p!=static_cast<IDirect3DBaseTexture9*>(texture))throw std::runtime_error("stateblock texture mismatch");
    check(d->SetTexture(0,nullptr),"stateblock unbind");
}
void createRTT(IDirect3DDevice9* d,Com<IDirect3DSurface9>& back,Com<IDirect3DTexture9>& target,Com<IDirect3DSurface9>& surface){
    check(d->GetRenderTarget(0,back.put()),"GetRenderTarget");
    check(d->CreateTexture(128,128,1,D3DUSAGE_RENDERTARGET,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,target.put(),nullptr),"CreateTexture RTT");
    check(target->GetSurfaceLevel(0,surface.put()),"RTT surface");
}
void drawLifecycle(IDirect3DDevice9* d,IDirect3DSurface9* back,IDirect3DTexture9* target,IDirect3DSurface9* surface,IDirect3DTexture9* managed){
    stateBlockRoundtrip(d,managed);drawRTT(d,back,target,surface);
    check(d->BeginScene(),"managed footer BeginScene");state(d);check(d->SetTexture(0,managed),"managed footer texture");
    check(d->SetRenderState(D3DRS_ALPHATESTENABLE,TRUE),"managed footer alpha test");
    check(d->SetRenderState(D3DRS_ALPHAFUNC,D3DCMP_GREATEREQUAL),"managed footer alpha func");check(d->SetRenderState(D3DRS_ALPHAREF,128),"managed footer alpha ref");
    quad(d,32,440,64,32,0xffffffff);check(d->SetTexture(0,nullptr),"managed footer unbind");check(d->EndScene(),"managed footer EndScene");
}
