// Synthetic R1 preflights. No game assets, loader substitution or C2 code.
struct WeightedVertex {float x,y,z,weights[3];DWORD indices;float nx,ny,nz,u,v;};
static_assert(sizeof(WeightedVertex)==48,"WMESHVERTEX layout from T3D.h:815");
constexpr DWORD weightedFVF=D3DFVF_XYZB4|D3DFVF_LASTBETA_UBYTE4|D3DFVF_NORMAL|D3DFVF_TEX1;
D3DMATRIX identity(){D3DMATRIX m={};m._11=m._22=m._33=m._44=1;return m;}
void featureState(IDirect3DDevice9* d){
    state(d);
    check(d->SetSoftwareVertexProcessing(TRUE),"SetSoftwareVertexProcessing TRUE");
    if(!d->GetSoftwareVertexProcessing())throw std::runtime_error("software processing did not enable");
    check(d->SetTexture(0,nullptr),"feature untextured");
    check(d->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG2),"feature solid");
    check(d->SetTextureStageState(0,D3DTSS_COLORARG2,D3DTA_TFACTOR),"feature color factor");
    auto m=identity();check(d->SetTransform(D3DTS_VIEW,&m),"feature view");check(d->SetTransform(D3DTS_PROJECTION,&m),"feature projection");
}
void drawIndexedWeighted(IDirect3DDevice9* d,float top,DWORD indices,DWORD color){
    WeightedVertex v[]={
        {-.75f,top,.5f,{.125f,.25f,.375f},indices,0,0,-1,0,0},
        {-.5f,top,.5f,{.125f,.25f,.375f},indices,0,0,-1,1,0},
        {-.75f,top-.25f,.5f,{.125f,.25f,.375f},indices,0,0,-1,0,1},
        {-.5f,top-.25f,.5f,{.125f,.25f,.375f},indices,0,0,-1,1,1}};
    WORD ix[]={0,1,2,2,1,3};Com<IDirect3DVertexBuffer9> vb;Com<IDirect3DIndexBuffer9> ib;
    DWORD usage=D3DUSAGE_WRITEONLY|D3DUSAGE_SOFTWAREPROCESSING;
    check(d->CreateVertexBuffer(sizeof(v),usage,weightedFVF,D3DPOOL_MANAGED,vb.put(),nullptr),"Create weighted VB");
    check(d->CreateIndexBuffer(sizeof(ix),usage,D3DFMT_INDEX16,D3DPOOL_MANAGED,ib.put(),nullptr),"Create weighted IB");
    void* data;check(vb->Lock(0,sizeof(v),&data,0),"Lock weighted VB");std::memcpy(data,v,sizeof(v));check(vb->Unlock(),"Unlock weighted VB");
    check(ib->Lock(0,sizeof(ix),&data,0),"Lock weighted IB");std::memcpy(data,ix,sizeof(ix));check(ib->Unlock(),"Unlock weighted IB");
    check(d->SetRenderState(D3DRS_TEXTUREFACTOR,color),"weighted color");
    check(d->SetStreamSource(0,vb.p,0,sizeof(WeightedVertex)),"weighted stream");check(d->SetIndices(ib.p),"weighted indices");
    check(d->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,0,0,4,0,2),"DrawIndexedPrimitive weighted");
    check(d->SetStreamSource(0,nullptr,0,0),"unbind weighted stream");check(d->SetIndices(nullptr),"unbind weighted indices");
}
void drawSkinning(IDirect3DDevice9* d){
    check(d->Clear(0,nullptr,D3DCLEAR_TARGET,0xff203040,1,0),"skinning Clear");check(d->BeginScene(),"skinning BeginScene");
    featureState(d);check(d->SetVertexShader(nullptr),"fixed-function shader");check(d->SetFVF(weightedFVF),"weighted FVF");
    check(d->SetRenderState(D3DRS_VERTEXBLEND,D3DVBF_3WEIGHTS),"three weights");check(d->SetRenderState(D3DRS_INDEXEDVERTEXBLENDENABLE,TRUE),"indexed blending");
    for(UINT first:{0u,252u})for(UINT i=0;i<4;++i){auto m=identity();m._41=.25f*i;check(d->SetTransform(D3DTS_WORLDMATRIX(first+i),&m),"bone world transform");}
    // The implicit fourth weight must contribute: unequal weights require translation=.4375.
    drawIndexedWeighted(d,.5f,0x03020100,0xffe04020);
    drawIndexedWeighted(d,0.f,0xfcfdfeff,0xff20e040);
    check(d->SetRenderState(D3DRS_INDEXEDVERTEXBLENDENABLE,FALSE),"indexed blending off");check(d->SetRenderState(D3DRS_VERTEXBLEND,D3DVBF_DISABLE),"blending off");
    check(d->EndScene(),"skinning EndScene");
}
DWORD registerBits(DWORD type){return ((type<<D3DSP_REGTYPE_SHIFT)&D3DSP_REGTYPE_MASK)|((type<<D3DSP_REGTYPE_SHIFT2)&D3DSP_REGTYPE_MASK2);}
void constantShader(IDirect3DDevice9* d,UINT reg,Com<IDirect3DVertexShader9>& shader){
    // Authored SM2 bytecode: dcl_position v0; add oPos,v0,c[reg]; end.
    // High constants require the software-processing path; no shader assembler.
    DWORD code[]={D3DVS_VERSION(2,0),D3DSIO_DCL|(2u<<D3DSI_INSTLENGTH_SHIFT),0x80000000u|D3DDECLUSAGE_POSITION,
        0x80000000u|registerBits(D3DSPR_INPUT)|D3DSP_WRITEMASK_ALL,
        D3DSIO_ADD|(3u<<D3DSI_INSTLENGTH_SHIFT),0x80000000u|registerBits(D3DSPR_RASTOUT)|D3DSP_WRITEMASK_ALL,
        0x80000000u|registerBits(D3DSPR_INPUT)|D3DSP_NOSWIZZLE,
        0x80000000u|registerBits(D3DSPR_CONST)|D3DSP_NOSWIZZLE|reg,D3DSIO_END};
    check(d->CreateVertexShader(code,shader.put()),"Create high-constant shader");
}
void relativeShader(IDirect3DDevice9* d,Com<IDirect3DVertexShader9>& shader){
    // dcl_position v0; dcl_texcoord v1; mova a0.x,v1.x;
    // add oPos,v0,c397[a0.x]; end. Offset is vertex data, not a fixed opcode.
    DWORD code[]={D3DVS_VERSION(2,0),
        D3DSIO_DCL|(2u<<D3DSI_INSTLENGTH_SHIFT),0x80000000u|D3DDECLUSAGE_POSITION,
        0x80000000u|registerBits(D3DSPR_INPUT)|D3DSP_WRITEMASK_ALL,
        D3DSIO_DCL|(2u<<D3DSI_INSTLENGTH_SHIFT),0x80000000u|D3DDECLUSAGE_TEXCOORD,
        0x80000000u|registerBits(D3DSPR_INPUT)|D3DSP_WRITEMASK_ALL|1,
        D3DSIO_MOVA|(2u<<D3DSI_INSTLENGTH_SHIFT),0x80000000u|registerBits(D3DSPR_ADDR)|D3DSP_WRITEMASK_0,
        0x80000000u|registerBits(D3DSPR_INPUT)|1,
        D3DSIO_ADD|(4u<<D3DSI_INSTLENGTH_SHIFT),0x80000000u|registerBits(D3DSPR_RASTOUT)|D3DSP_WRITEMASK_ALL,
        0x80000000u|registerBits(D3DSPR_INPUT)|D3DSP_NOSWIZZLE,
        0x80000000u|registerBits(D3DSPR_CONST)|D3DSP_NOSWIZZLE|397|D3DSHADER_ADDRMODE_RELATIVE,
        0x80000000u|registerBits(D3DSPR_ADDR),D3DSIO_END};
    check(d->CreateVertexShader(code,shader.put()),"Create relative-constant shader");
}
void drawConstants(IDirect3DDevice9* d,bool relative=false){
    check(d->Clear(0,nullptr,D3DCLEAR_TARGET,0xff203040,1,0),"constant Clear");check(d->BeginScene(),"constant BeginScene");featureState(d);
    std::vector<float> constants(512*4,0.f);constants[0]=.375f;constants[511*4]=.625f;
    check(d->SetVertexShaderConstantF(397,constants.data(),512),"upload c397..c908");
    float read[4];for(UINT reg:{397u,908u}){check(d->GetVertexShaderConstantF(reg,read,1),"read high constant");if(std::memcmp(read,constants.data()+(reg-397)*4,sizeof(read)))throw std::runtime_error("high constant roundtrip mismatch");}
    struct Position {float x,y,z;};Position v[]={{-.75f,.5f,.5f},{-.5f,.5f,.5f},{-.75f,.25f,.5f},{-.5f,.25f,.5f}};
    check(d->SetFVF(D3DFVF_XYZ),"constant position FVF");
    for(UINT reg:{397u,908u}){
        Com<IDirect3DVertexShader9> shader;if(relative)relativeShader(d,shader);else constantShader(d,reg,shader);check(d->SetVertexShader(shader.p),"bind high-constant shader");
        check(d->SetRenderState(D3DRS_TEXTUREFACTOR,reg==397?0xffe04020:0xff20e040),"constant color");
        if(relative){
            struct PositionTex {float x,y,z,u,v;};PositionTex input[4];
            for(int i=0;i<4;++i)input[i]={v[i].x,v[i].y,v[i].z,float(reg-397),0};
            check(d->SetFVF(D3DFVF_XYZ|D3DFVF_TEX1),"relative FVF");
            check(d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,input,sizeof(PositionTex)),"draw relative constant");
        }else check(d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,v,sizeof(Position)),"draw high constant");
        check(d->SetVertexShader(nullptr),"unbind constant shader");
    }
    check(d->EndScene(),"constant EndScene");
}
