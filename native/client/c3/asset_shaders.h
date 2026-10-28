#pragma once
#include <filesystem>
#include <fstream>
#include <vector>
#include <cstring>
#include <stdexcept>
// Original declarations, shader captures and constant-register mapping.
inline std::vector<DWORD> shaderCode(const std::filesystem::path& directory,const char* name){
    std::ifstream in(directory/name,std::ios::binary);if(!in)throw std::runtime_error(std::string("missing original shader ")+name);
    std::vector<char> bytes((std::istreambuf_iterator<char>(in)),{});
    if(bytes.empty()||bytes.size()%4)throw std::runtime_error("invalid shader byte count");
    std::vector<DWORD> code(bytes.size()/4);std::memcpy(code.data(),bytes.data(),bytes.size());
    if(code.back()!=0x0000ffff)throw std::runtime_error("missing shader END token");return code;
}
inline void initializeShaders(CD3DDevice& c,const std::filesystem::path& directory){
    const WORD vs[]={0,397,7,3,5,139,140,2,1,0,141,269,11,0},ps[]={2,1,0};
    static_assert(sizeof(vs)==VC_COUNT*sizeof(WORD)&&sizeof(ps)==PC_COUNT*sizeof(WORD),"original constant mappings");
    std::memcpy(c.m_vConstantVS,vs,sizeof(vs));std::memcpy(c.m_vConstantPS,ps,sizeof(ps));
    D3DVERTEXELEMENT9 weighted[]={
        {0,0,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0},
        {0,12,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_BLENDWEIGHT,0},
        {0,24,D3DDECLTYPE_D3DCOLOR,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_BLENDINDICES,0},
        {0,28,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_NORMAL,0},
        {0,40,D3DDECLTYPE_FLOAT2,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,0},D3DDECL_END()};
    D3DVERTEXELEMENT9 mesh[]={
        {0,0,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0},
        {0,12,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_NORMAL,0},
        {0,24,D3DDECLTYPE_FLOAT2,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,0},
        {0,32,D3DDECLTYPE_FLOAT2,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,1},D3DDECL_END()};
    check(c.m_pDevice->CreateVertexDeclaration(weighted,&c.m_pDECL[VS_WMESHVERTEX]),"original WMESH declaration");
    check(c.m_pDevice->CreateVertexDeclaration(mesh,&c.m_pDECL[VS_MESHVERTEX]),"original MESH declaration");
    for(auto item:{std::make_pair(VS_WMESHVERTEX,"VS_WMESH.vsh.bin"),std::make_pair(VS_MESHVERTEX,"VS_MESH.vsh.bin")}){
        auto code=shaderCode(directory,item.second);check(c.m_pDevice->CreateVertexShader(code.data(),&c.m_pVertexShader[item.first]),item.second);
    }
    const char* names[]={"STAGE1","MODULATE","MODULATE2X","MODULATE4X","ADD","ADDSIGNED","ADDSIGNED2X","SUBTRACT","ADDSMOOTH","BLENDDIFFUSEALPHA","BLENDTEXTUREALPHA","BLENDFACTORALPHA","BLENDTEXTUREALPHAPM","BLENDCURRENTALPHA","MODULATEALPHA_ADDCOLOR","MODULATECOLOR_ADDALPHA","MODULATEINVALPHA_ADDCOLOR","MODULATEINVCOLOR_ADDALPHA","DOTPRODUCT3","MULTIPLYADD","LERP","SHADER","DETAILMAP","MAP"};
    static_assert(sizeof(names)/sizeof(*names)==PS_COUNT,"original pixel shader order");
    for(unsigned i=0;i<PS_COUNT;++i){std::string name=std::string("PS_")+names[i]+".psh.bin";auto code=shaderCode(directory,name.c_str());check(c.m_pDevice->CreatePixelShader(code.data(),&c.m_pPixelShader[i]),name.c_str());}
    c.m_bEnableSHADER=TRUE;
}
