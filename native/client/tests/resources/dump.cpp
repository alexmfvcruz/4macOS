// One resource serialization implementation for native and original Windows libs.
#ifdef FOURSTORY_CLIENT_NATIVE
#include <fourstory_engine_resources.h>
#include "../../device/empty_device.h"
#else
#include "stdafx.h"
#include <direct.h>
#endif
#include "canonical.h"
#include <iostream>
#include <memory>
#include <set>
using namespace c2dump;
namespace {
std::ofstream output;
unsigned failedTextures=0;
int textureDetail=0;
bool textureOnly=false;
Row row(const char* dataset,DWORD id){return {output,dataset,std::to_string(id),""};}
Ids texIds,imgIds,aniIds,meshIds,objIds,sfxIds,texSourceIds,imgSourceIds,aniSourceIds,imgBufferIds;
#define F(member) r.field(#member,p.member)
void uv(const Row& r,const UVKEY& p){F(m_dwTick);F(m_fKeySU);F(m_fKeySV);F(m_fKeyU);F(m_fKeyV);F(m_fKeyR);}
void color(const Row& r,const CLRKEY& p){F(m_dwTick);F(m_dwColor);}
void key(const Row& r,const ANIKEY& p){F(m_dwEventID);F(m_fTime);F(m_fPosX);F(m_fPosY);F(m_fPosZ);F(m_fRotX);F(m_fRotY);F(m_fRotZ);F(m_fScaleX);F(m_fScaleY);F(m_fScaleZ);}
template<class V,class Function> void sequence(const Row& r,const char* name,const V& v,Function function){
    r.field(std::string(name)+".count",v.size());
    for(size_t i=0;i<v.size();++i)function(r.child(std::string(name)+"["+std::to_string(i)+"]"),*v[i]);
}
void index(const char* name,const CString& path){
    CFile file(path,CFile::modeRead|CFile::typeBinary);
    auto read=[&](void* p,UINT n){if(file.Read(p,n)!=n)throw std::runtime_error("short index read");};
    int files=0,entries=0;read(&files,4);read(&entries,4);
    if(files<0||files>10000||entries<0||entries>1000000)throw std::runtime_error("invalid index counts");
    Row r{output,std::string("index.")+name,"header",""};r.field("files",files);r.field("entries",entries);
    std::vector<CString> filenames;
    struct Entry{DWORD id,file,offset;};std::vector<Entry> records;
    for(int i=0;i<files;++i){int n=0;read(&n,4);if(n<0||n>4096)throw std::runtime_error("invalid index filename");std::vector<char>b(n);if(n)read(b.data(),n);filenames.emplace_back(b.data(),n);r.field("file["+std::to_string(i)+"]",filenames.back());}
    const bool listOnly=std::string(name)=="imgbuf";
    for(int i=0;i<(listOnly?0:entries);++i){DWORD id,f,pos;read(&id,4);read(&f,4);read(&pos,4);records.push_back({id,f,pos});Row e{output,std::string("index.")+name,std::to_string(i),""};e.field("id",id);e.field("file",f);e.field("offset",pos);}
    if(file.GetPosition()!=file.GetLength())throw std::runtime_error("index has trailing bytes");
    // Hash the indexed disk spans as well as parsed fields. These spans retain
    // consumed-but-discarded option/material bytes and original compressed TAF.
    for(size_t f=0;f<filenames.size();++f){
        CFile data(CString(".\\Data\\")+filenames[f],CFile::modeRead|CFile::typeBinary);
        const auto size=data.GetLength();if(size>UINT32_MAX)throw std::runtime_error("resource file too large");
        std::vector<BYTE> bytes(static_cast<size_t>(size));if(size&&data.Read(bytes.data(),static_cast<UINT>(size))!=size)throw std::runtime_error("short resource file");
        if(listOnly){Row raw{output,"disk.imgbuf_file",std::to_string(f),""};raw.bytes("data",bytes.data(),bytes.size());continue;}
        std::set<DWORD> boundaries;boundaries.insert(static_cast<DWORD>(size));
        for(const auto& e:records)if(e.file==f)boundaries.insert(e.offset);
        for(const auto& e:records)if(e.file==f){auto next=boundaries.upper_bound(e.offset);if(next==boundaries.end()||e.offset>*next)throw std::runtime_error("invalid resource span");
            Row raw{output,std::string("disk.")+name,std::to_string(e.id),""};raw.bytes("indexed_span",bytes.data()+e.offset,*next-e.offset);
        }
    }

}
void texture(const Row& r,CT3DTexture& p){
    r.field("has_payload",p.m_pTDATA!=nullptr);
    if(!p.m_pTDATA)return;
    if(!p.m_bEnabled){auto& b=*reinterpret_cast<LPTTEXLOADBUF>(p.m_pTDATA);r.field("format_tag",b.m_bFormat);r.field("decoded_size",b.m_dwSIZE);r.bytes("source",b.m_pDATA,b.m_dwDATA);}
    auto* texture=p.GetTEX();r.field("created",texture!=nullptr);if(!texture){++failedTextures;return;}
    r.field("levels",texture->GetLevelCount());
    for(UINT i=0;i<texture->GetLevelCount();++i){auto level=r.child("level["+std::to_string(i)+"]");D3DSURFACE_DESC d{};D3DLOCKED_RECT lock{};
        if(FAILED(texture->GetLevelDesc(i,&d))||FAILED(texture->LockRect(i,&lock,nullptr,D3DLOCK_READONLY)))throw std::runtime_error("texture readback failed");
        level.field("format",d.Format);level.field("width",d.Width);level.field("height",d.Height);level.field("pitch",lock.Pitch);
        bool bc=d.Format==D3DFMT_DXT1||d.Format==D3DFMT_DXT2||d.Format==D3DFMT_DXT3||d.Format==D3DFMT_DXT4||d.Format==D3DFMT_DXT5;
        UINT rows=bc?(d.Height+3)/4:d.Height;UINT bytes=bc?((d.Width+3)/4)*(d.Format==D3DFMT_DXT1?8:16):d.Width*4;
        if(lock.Pitch<0||static_cast<UINT>(lock.Pitch)<bytes){texture->UnlockRect(i);throw std::runtime_error("invalid texture pitch");}
        std::vector<BYTE> active(static_cast<size_t>(rows)*bytes);for(UINT y=0;y<rows;++y)memcpy(active.data()+size_t(y)*bytes,static_cast<BYTE*>(lock.pBits)+size_t(y)*lock.Pitch,bytes);
        level.bytes("active_rows",active.data(),active.size());if(FAILED(texture->UnlockRect(i)))throw std::runtime_error("texture unlock failed");
    }
    // Keep the Win32 reference below its address-space limit. Later phases
    // inspect texture identities, and do not sample this released resource.
    p.ReleaseDATA();
}
void textureSet(const Row& r,TEXTURESET& p){
    F(m_dwTotalTick);F(m_dwMipFilter);F(m_fMipBias);sequence(r,"keys",p.m_vKey,uv);
    r.field("sources.count",p.GetTextureCount());for(int i=0;i<p.GetTextureCount();++i)r.field("sources["+std::to_string(i)+"]",texSourceIds(p.GetTexturePtr(i)));
}
void animationSet(const Row& r,const ANISET& p){F(m_fTimeScale);F(m_dwLoopID);F(m_bLoop);sequence(r,"keys",p.m_vKey,key);
    r.field("sources.count",p.m_vAni.size());for(size_t i=0;i<p.m_vAni.size();++i){auto s=r.child("sources["+std::to_string(i)+"]");s.field("id",aniSourceIds(p.m_vAni[i]->m_pAni));s.field("reverse",p.m_vAni[i]->m_bReverse);}}
void animation(const Row& r,CTachyonAnimation& p,CTachyonRes& resources){
    p.Lock(&resources.m_vANIFILE);F(m_dwStartTick);F(m_dwEndTick);r.field("node_count",p.GetNodeCount());
    for(DWORD n=0;n<p.GetNodeCount();++n){auto a=r.child("node["+std::to_string(n)+"]");a.field("index",p.m_pTREE[n].m_bINDEX);a.field("parent_rot",p.m_pTREE[n].m_bParentROT);a.field("parent_pos",p.m_pTREE[n].m_bParentPOS);
        for(int i=0;i<16;++i)a.field("bone["+std::to_string(i)+"]",reinterpret_cast<const FLOAT*>(&p.m_pBones[n])[i]);
        const auto& k=p.m_pNode[n];a.field("position.count",k.m_nPositionKeyCount);a.field("rotation.count",k.m_nRotationKeyCount);a.field("scale.count",k.m_nScaleKeyCount);
        for(int i=0;i<k.m_nPositionKeyCount;++i){const auto& v=static_cast<LPTPOINTKEY>(k.m_pPositionKey)[i];auto t=a.child("position["+std::to_string(i)+"]");t.field("time",v.m_dwTime);t.field("point",v.m_vKeyPoint);}
        for(int i=0;i<k.m_nRotationKeyCount;++i){const auto& v=static_cast<LPTROTKEY>(k.m_pRotationKey)[i];auto t=a.child("rotation["+std::to_string(i)+"]");t.field("time",v.m_dwTime);t.field("quat",v.m_vKeyQuat);}
        for(int i=0;i<k.m_nScaleKeyCount;++i){const auto& v=static_cast<LPTSCALEKEY>(k.m_pScaleKey)[i];auto t=a.child("scale["+std::to_string(i)+"]");t.field("time",v.m_dwTime);t.field("scale",v.m_vKeyScale);}
    }p.Unlock();
}
void vertexBuffer(const Row& r,IDirect3DVertexBuffer9* buffer){r.field("present",buffer!=nullptr);if(!buffer)return;D3DVERTEXBUFFER_DESC desc{};void* bytes=nullptr;if(FAILED(buffer->GetDesc(&desc))||FAILED(buffer->Lock(0,0,&bytes,D3DLOCK_READONLY)))throw std::runtime_error("vertex buffer readback failed");r.field("fvf",desc.FVF);r.field("usage",desc.Usage);r.bytes("data",bytes,desc.Size);if(FAILED(buffer->Unlock()))throw std::runtime_error("vertex buffer unlock failed");}
void indexBuffer(const Row& r,IDirect3DIndexBuffer9* buffer){r.field("present",buffer!=nullptr);if(!buffer)return;D3DINDEXBUFFER_DESC desc{};void* bytes=nullptr;if(FAILED(buffer->GetDesc(&desc))||FAILED(buffer->Lock(0,0,&bytes,D3DLOCK_READONLY)))throw std::runtime_error("index buffer readback failed");r.field("format",desc.Format);r.field("usage",desc.Usage);r.bytes("data",bytes,desc.Size);if(FAILED(buffer->Unlock()))throw std::runtime_error("index buffer unlock failed");}
void mesh(const Row& r,CTachyonMesh& p,CTachyonRes& resources,IDirect3DDevice9* device){
    F(m_vCenterPoint);F(m_fRadius);F(m_dwNodeCount);F(m_dwMeshCount);F(m_bMESHType);F(m_bVBType);F(m_bUseVB);r.field("lod.count",p.m_vDist.size());for(size_t i=0;i<p.m_vDist.size();++i)r.field("lod["+std::to_string(i)+"]",p.m_vDist[i]);
    p.Lock(device,&resources.m_vMESHFILE);p.Restore(device);
    for(DWORD i=0;i<p.m_dwNodeCount;++i)for(int j=0;j<16;++j)r.field("bone["+std::to_string(i)+"]["+std::to_string(j)+"]",reinterpret_cast<const FLOAT*>(&p.m_pBones[i])[j]);
    for(DWORD i=0;i<p.m_dwMeshCount;++i)for(size_t j=0;j<p.m_pMESH[i].size();++j){const auto& segments=p.m_pMESH[i][j]->m_vIB;auto a=r.child("segments["+std::to_string(i)+"]["+std::to_string(j)+"]");a.field("count",segments.size());for(size_t k=0;k<segments.size();++k){auto b=a.child(std::to_string(k));b.field("indices",segments[k]->m_dwCount);b.field("matrix",segments[k]->m_wMatrix);}}
    vertexBuffer(r.child("vertex_buffer"),p.m_pVB);
    for(DWORD i=0;i<p.m_dwMeshCount;++i)for(size_t j=0;j<p.m_pIB[i].size();++j)indexBuffer(r.child("index_buffer["+std::to_string(i)+"]["+std::to_string(j)+"]"),p.m_pIB[i][j]);
    p.Unlock();
}
template<class M,class Function> void mapping(const Row& r,const char* name,const M& map,Function function){
    r.field(std::string(name)+".count",map.size());for(const auto& item:map)function(r.child(std::string(name)+"["+std::to_string(item.first)+"]"),*item.second);
}
void sound(const Row& r,const SNDINST& p){F(m_dwMediaID);F(m_dwFuncID);F(m_dwTick);F(m_bDeleteOnStop);}
void effectInstance(const Row& r,const SFXINST& p){F(m_dwSFX);F(m_vPOS);F(m_vROT);F(m_dwFuncID);F(m_dwTick);F(m_nPivotID);F(m_bDeleteOnStop);F(m_bBias);r.field("sfx_id",sfxIds(p.m_pSFX));r.field("hide_actions.count",p.m_mapHideByAction.size());for(const auto& v:p.m_mapHideByAction)r.field("hide_actions["+std::to_string(v.first)+"]",v.second);}
void attribute(const Row& r,const ATTRIBUTE& p){F(m_bValueType);F(m_nSize);r.text("value",hexBytes(p.m_pValue,p.m_nSize));}
void clockInstance(const Row& r,const CLKINST& p){F(m_dwCurMESH);F(m_dwCurCL);}
void material(const Row& r,const OBJTEX& p){
    for(int i=0;i<2;++i){r.field("type["+std::to_string(i)+"]",p.m_bType[i]);r.field("texture["+std::to_string(i)+"]",texIds(p.m_pTEX[i]));}
    F(m_fIntensity);F(m_dwCOLORID);F(m_dwCOLOR);F(m_dwOrder);F(m_dwOP);F(m_dwDESTBlend);F(m_dwSRCBlend);F(m_dwBlendOP);F(m_bMinAlpha);F(m_bZEnable);F(m_bZWrite);F(m_b2side);F(m_bUseDirLight);F(m_dwAmbientCOLOR);
}
void objectMesh(const Row& r,const MESH& p){r.field("mesh_id",meshIds(p.m_pMESH));sequence(r,"materials",p.m_vTEX,material);}
void cloth(const Row& r,const CLOTH& p){mapping(r,"meshes",p.m_mapMESH,objectMesh);}
void clothKind(const Row& r,const CLKIND& p){mapping(r,"cloths",p.m_mapCLOTH,cloth);}
void objectAnimation(const Row& r,const ANI& p){r.field("animation_id",aniIds(p.m_pANI));mapping(r,"sounds",p.m_mapSND,sound);mapping(r,"effects",p.m_mapSFX,effectInstance);}
void action(const Row& r,const ACTION& p){mapping(r,"sounds",p.m_mapSND,sound);mapping(r,"effects",p.m_mapSFX,effectInstance);mapping(r,"animations",p.m_mapANI,objectAnimation);}
void object(const Row& r,const OBJECT& p){F(m_bPivotCount);r.field("pivots.count",p.m_mapPIVOT.size());for(const auto& v:p.m_mapPIVOT)r.field("pivots["+std::to_string(v.first)+"]",v.second);mapping(r,"sounds",p.m_mapSND,sound);mapping(r,"effects",p.m_mapSFX,effectInstance);mapping(r,"attributes",p.m_mapATTR,attribute);mapping(r,"actions",p.m_mapACTION,action);mapping(r,"cloth_kinds",p.m_mapCLKIND,clothKind);}
void effect(const Row& r,const SFX& p){
    sequence(r,"keys",p.m_vKey,color);r.field("combined.count",p.m_vSFX.size());for(size_t i=0;i<p.m_vSFX.size();++i)r.field("combined["+std::to_string(i)+"]",sfxIds(p.m_vSFX[i]));
    F(m_bSFXType);F(m_bSRCType);
    if(p.m_bSRCType==SFXSRC_TEX)r.field("source_id",texIds(p.m_SRC.m_pTEX));
    else if(p.m_bSRCType==SFXSRC_OBJ)r.field("source_id",objIds(p.m_SRC.m_pOBJ));
    else if(p.m_bSRCType==SFXSRC_SFX)r.field("source_id",sfxIds(p.m_SRC.m_pSFX));
    else throw std::runtime_error("unknown effect source type");
    r.field("animation_id",aniIds(p.m_pANISRC));r.field("spray_animation_id",aniIds(p.m_pSprayANI));
    F(m_dwLifeTime);F(m_fFollow);F(m_fSizeX);F(m_fSizeY);mapping(r,"clocks",p.m_mapCLK,clockInstance);
    F(m_dwOBJFuncID);F(m_dwACTFuncID);F(m_dwANIFuncID);F(m_dwACT);F(m_dwANI);F(m_bSprayType);F(m_bZEnable);F(m_bZWrite);F(m_bStrict);F(m_bFollow);F(m_dwDESTBlend);F(m_dwSRCBlend);F(m_dwBlendOP);F(m_dwGTime);F(m_dwGCnt);F(m_vOutBound);F(m_vInBound);F(m_dwPLifeTime);F(m_dwERRTime);F(m_vERRScale);F(m_vERRPos);F(m_vERRRot);F(m_dwLodLevel);
}
void image(const Row& r,CD3DImage& p){r.field("width",p.GetWidth());r.field("height",p.GetHeight());r.field("parts.count",p.GetPartCount());
    for(int i=0;i<p.GetPartCount();++i){auto part=r.child("parts["+std::to_string(i)+"]");part.field("image_buffer_id",imgBufferIds(p.GetT3DTEX()[i]));vertexBuffer(part.child("vertex_buffer"),p.GetT3DVB()[i]->GetVB());}
    // Original format stores ceil(width/8) bytes per row, as GetMask addresses it.
    r.bytes("mask",p.GetMask(),size_t((p.GetWidth()+7)/8)*p.GetHeight());
}
void imageSet(const Row& r,const IMAGESET& p){F(m_dwTotalTick);sequence(r,"keys",p.m_vKey,color);r.field("sources.count",p.m_vImage.size());for(size_t i=0;i<p.m_vImage.size();++i)r.field("sources["+std::to_string(i)+"]",imgSourceIds(p.m_vImage[i]));}
void oneMap(CTachyonRes& resources,CD3DDevice& context){
    if(resources.m_mapMAP.empty())throw std::runtime_error("no indexed map");
    const DWORD id=resources.m_mapMAP.begin()->first;
    // TClientWnd.cpp:1524-1528, TClientType.h:484-488 startup settings.
    CTachyonHUGEMAP::m_nLODBound=96;CTachyonHUGEMAP::m_nDiffuseCount=10;
    CTachyonHUGEMAP::m_nMaxLength=4;CTachyonHUGEMAP::m_nMaxLevel=1;CTachyonHUGEMAP::m_nIndexCount=36000;
    auto* base=resources.LockMAP(id);if(!base||base->m_bMAPType!=MT_EXT)throw std::runtime_error("expected first indexed terrain map");
    std::unique_ptr<CTachyonHUGEMAP> holder(static_cast<CTachyonHUGEMAP*>(base));auto& p=*holder;auto r=row("map",id);
    F(m_bMAPType);F(m_nUnitCountX);F(m_nUnitCountZ);F(m_nUnitLength);F(m_nTileLength);F(m_nCellCount);F(m_nTextureSize);F(m_nShadowPixel);F(m_vLIGHT);F(m_bUseSHADER);F(m_fIntensity);F(m_fSpecRange);F(m_fSpecMax);
    r.bytes("unit_enable",p.m_pENABLE,size_t(p.m_nUnitCountX)*p.m_nUnitCountZ);
    r.field("details.count",p.m_vDETAILDATA.size());for(size_t i=0;i<p.m_vDETAILDATA.size();++i){r.field("details["+std::to_string(i)+"].texture_id",texIds(p.m_vDETAILDATA[i]));r.field("details["+std::to_string(i)+"].scale",p.m_vDETAILSDATA[i]);}
    // LockMAP initializes only diffuse[0]. Other geometry buffers are allocated
    // for later terrain-unit loading, and their unspecified bytes are not read.
    vertexBuffer(r.child("null_tile_buffer"),p.m_vDIFFUSE.at(0));
    auto descriptor=[&](const Row& v,IDirect3DVertexBuffer9* buffer){v.field("present",buffer!=nullptr);v.field("initialized",0);if(buffer){D3DVERTEXBUFFER_DESC d{};if(FAILED(buffer->GetDesc(&d)))throw std::runtime_error("map buffer descriptor failed");v.field("size",d.Size);v.field("fvf",d.FVF);}};
    descriptor(r.child("position_buffer"),p.m_pPOSVB);descriptor(r.child("uv_buffer"),p.m_pUVVB);
    r.field("index_buffer.present",p.m_pINDEX!=nullptr);r.field("index_buffer.initialized",0);
    r.field("shadow_texture.present",p.m_pSHADOWTEX!=nullptr);r.field("tile_texture.present",p.m_pTILETEX!=nullptr);r.field("geo_texture.present",p.m_pGEOTEX!=nullptr);
}
void load(CTachyonRes& resources){
#ifdef FOURSTORY_CLIENT_NATIVE
    resources.Init("TClient",0);
#else
    // Same original loading sequence minus media, which is explicitly outside
    // C2. Windows Init always invokes media; no original library is patched.
    resources.m_bGroupID=0;resources.LoadTEX("TClient");resources.LoadIMGBUF("TClient");resources.LoadIMG("TClient");resources.LoadANI("TClient");resources.LoadMESH("TClient");resources.LoadOBJ("TClient");resources.LoadSFX("TClient");resources.LoadMAP("TClient");resources.Complate();
#endif
}
void dump(CTachyonRes& resources,CD3DDevice& context){
    texIds.add(resources.m_mapTEX);imgIds.add(resources.m_mapIMG);aniIds.add(resources.m_mapANI);meshIds.add(resources.m_mapMESH);objIds.add(resources.m_mapOBJ);sfxIds.add(resources.m_mapSFX);texSourceIds.add(resources.m_mapTEXSRC);imgSourceIds.add(resources.m_mapIMGSRC);aniSourceIds.add(resources.m_mapANISRC);imgBufferIds.add(resources.m_mapIMGBUF);
    CString textureIndex;textureIndex.Format(".\\Index\\%u_TClientS.IDX",unsigned(textureDetail));
    if(textureOnly){
        index("tex",textureIndex);
        for(const auto& v:resources.m_mapTEX)textureSet(row("tex",v.first),*static_cast<LPTEXTURESET>(v.second));
        for(const auto& v:resources.m_mapTEXSRC)texture(row("tex.created",v.first),*static_cast<CT3DTexture*>(v.second));
        return;
    }
    index("tex",textureIndex);index("imgbuf",".\\Index\\TClientList.LST");index("img",".\\Index\\TClientI.IDX");index("ani",".\\Index\\TClientA.IDX");index("mesh",".\\Index\\TClientM.IDX");index("obj",".\\Index\\TClientO.IDX");index("sfx",".\\Index\\TClientX.IDX");index("map",".\\Index\\TClientP.IDX");
    for(const auto& v:resources.m_mapTEX)textureSet(row("tex",v.first),*static_cast<LPTEXTURESET>(v.second));
    for(const auto& v:resources.m_mapIMG)imageSet(row("img",v.first),*static_cast<LPIMAGESET>(v.second));
    for(const auto& v:resources.m_mapANI)animationSet(row("ani",v.first),*static_cast<LPANISET>(v.second));
    for(const auto& v:resources.m_mapOBJ)object(row("obj",v.first),*static_cast<LPOBJECT>(v.second));
    for(const auto& v:resources.m_mapSFX)effect(row("sfx",v.first),*static_cast<LPSFX>(v.second));
    std::cerr<<"DUMP forcing textures\n";
    for(const auto& v:resources.m_mapTEXSRC)texture(row("tex.created",v.first),*static_cast<CT3DTexture*>(v.second));
    for(const auto& v:resources.m_mapIMGBUF)texture(row("imgbuf",v.first),*v.second);
    for(const auto& v:resources.m_mapIMGSRC)image(row("img.source",v.first),*static_cast<CD3DImage*>(v.second));
    std::cerr<<"DUMP forcing animations\n";
    for(const auto& v:resources.m_mapANISRC)animation(row("ani.source",v.first),*static_cast<CTachyonAnimation*>(v.second),resources);
    std::cerr<<"DUMP forcing meshes\n";
    for(const auto& v:resources.m_mapMESH)mesh(row("mesh",v.first),*static_cast<CTachyonMesh*>(v.second),resources,context.m_pDevice);
    std::cerr<<"DUMP forcing map\n";oneMap(resources,context);
}
}
int main(int argc,char** argv){
    if(argc!=3&&argc!=4){std::cerr<<"usage: c2-resource-dump ASSET_DIRECTORY OUTPUT.tsv [TEXTURES_ONLY_DETAIL_0_TO_2]\n";return 2;}
    try{
        if(argc==4){textureOnly=true;if(std::strlen(argv[3])!=1||argv[3][0]<'0'||argv[3][0]>'2')throw std::runtime_error("texture detail must be 0, 1 or 2");textureDetail=argv[3][0]-'0';}
        if(sha256("abc",3)!="BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD")throw std::runtime_error("SHA256 self-check failed");
        output.open(argv[2],std::ios::binary);if(!output)throw std::runtime_error("output unavailable");
#ifdef _WIN32
        if(_chdir(argv[1]))throw std::runtime_error("asset directory unavailable");
#else
        if(chdir(argv[1]))throw std::runtime_error("asset directory unavailable");
#endif
        {
            CD3DDevice context;context.m_option.m_nTextureDetail=static_cast<TEXTURE_DETAIL_LEVEL>(textureDetail);context.m_option.m_dwScreenX=640;context.m_option.m_dwScreenY=480;
#ifdef FOURSTORY_CLIENT_NATIVE
            if(FAILED(fourstory::device::CreateEmptyDevice(&context.m_pDevice)))throw std::runtime_error("empty device failed");
#else
            context.m_pD3D=Direct3DCreate9(D3D_SDK_VERSION);if(!context.m_pD3D)throw std::runtime_error("Direct3DCreate9 failed");
            D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=GetDesktopWindow();pp.BackBufferWidth=640;pp.BackBufferHeight=480;pp.BackBufferFormat=D3DFMT_UNKNOWN;
            if(FAILED(context.m_pD3D->CreateDevice(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,pp.hDeviceWindow,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,&context.m_pDevice)))throw std::runtime_error("Windows HAL device failed");
#endif
            if(FAILED(context.m_pDevice->GetDeviceCaps(&context.m_vCAPS)))throw std::runtime_error("device caps unavailable");
            CD3DDevice::m_bAnisotropic=FALSE;
            CTachyonRes::m_pDEVICE=&context;CTachyonMesh::InitGlobalVB();
            {CTachyonRes resources;load(resources);dump(resources,context);}
            CTachyonMesh::ReleaseGlobalVB();CTachyonRes::m_pDEVICE=nullptr;
        }
#ifdef FOURSTORY_CLIENT_NATIVE
        const auto& s=fourstory::device::statistics();auto r=row("device",0);r.text("backend","cpu_empty_device");r.field("textures",s.textures);r.field("surfaces",s.surfaces);r.field("vertex_buffers",s.vertex_buffers);r.field("index_buffers",s.index_buffers);r.field("bytes_created",s.bytes_created);r.field("live_objects",s.live_objects);r.field("live_bytes",s.live_bytes);r.field("failed_creations",s.failed_creations);for(const auto& call:s.not_executed)r.field("not_executed."+call.first,call.second);
        if(s.live_objects||s.live_bytes||s.failed_creations||!s.not_executed.empty())throw std::runtime_error("unexpected device failures or leaked resources");
#endif
        auto result=row("result",0);result.text("scope",textureOnly?"textures":"full");result.field("detail",textureDetail);result.field("texture_failures",failedTextures);result.text("status",failedTextures?"incomplete":"complete");
        output.close();if(!output)throw std::runtime_error("dump close failed");if(failedTextures){std::cerr<<"INCOMPLETE texture creation failures="<<failedTextures<<"\n";return 3;}
        std::cerr<<"PASS resource dump\n";
    }catch(CFileException* e){std::cerr<<"FAIL file cause="<<e->m_cause<<" errno="<<e->m_lOsError<<"\n";e->Delete();return 1;}
    catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}
}
