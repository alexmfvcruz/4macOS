#include <d3dx9.h>
#include "../baseline/dds_upload_hashes.h"
#include "unused_device.h"
#include "unused_texture.h"
#include <cassert>
#include <fstream>
#include <iostream>
struct CpuTexture : UnusedTextureMethods {
    static int live;UINT refs=1;D3DFORMAT format;UINT width,height;int failLock=-1,failUnlock=-1;bool badPitch=false;
    std::vector<std::vector<BYTE>> storage;std::vector<UINT> pitches;
    CpuTexture(UINT w,UINT h,UINT levels,D3DFORMAT f):format(f),width(w),height(h){
        ++live;auto p=fourstory_dds::layout(f);for(UINT i=0;i<levels;++i){UINT pitch=(p.block?((w+3)/4)*p.block:w*p.bytes)+16,rows=p.block?(h+3)/4:h;
        pitches.push_back(pitch);storage.emplace_back(pitch*rows,0xcd);w=std::max(1U,w/2);h=std::max(1U,h/2);}}
    ~CpuTexture(){--live;}
    ULONG AddRef() override {return ++refs;}
    ULONG Release() override {UINT r=--refs;if(!r)delete this;return r;}
    UINT GetLevelCount() override {return UINT(storage.size());}
    HRESULT GetLevelDesc(UINT level,D3DSURFACE_DESC* desc) override {
        if(level>=storage.size())return D3DERR_INVALIDCALL;
        *desc={};desc->Format=format;desc->Width=std::max(1U,width>>level);desc->Height=std::max(1U,height>>level);return D3D_OK;}
    HRESULT LockRect(UINT level,D3DLOCKED_RECT* rect,const RECT*,DWORD) override {
        if(level>=storage.size()||int(level)==failLock)return D3DERR_NOTAVAILABLE;
        rect->pBits=storage[level].data();rect->Pitch=badPitch?1:pitches[level];return D3D_OK;}
    HRESULT UnlockRect(UINT level) override {return int(level)==failUnlock?D3DERR_NOTAVAILABLE:D3D_OK;}
};
int CpuTexture::live=0;
struct CpuDevice : UnusedDeviceMethods {
    UINT creates=0;int failLock=-1,failUnlock=-1;bool badPitch=false,failCreate=false;D3DPOOL lastPool;DWORD lastUsage;
    HRESULT CreateTexture(UINT w,UINT h,UINT m,DWORD usage,D3DFORMAT f,D3DPOOL pool,IDirect3DTexture9** out,HANDLE*) override {
        ++creates;lastPool=pool;lastUsage=usage;if(failCreate)return D3DERR_NOTAVAILABLE;
        auto t=new CpuTexture(w,h,m,f);t->failLock=failLock;t->failUnlock=failUnlock;t->badPitch=badPitch;*out=t;return D3D_OK;}
};
void setword(std::vector<BYTE>& bytes,UINT offset,UINT value){for(UINT i=0;i<4;++i)bytes[offset+i]=BYTE(value>>(i*8));}
std::vector<BYTE> fixture(D3DFORMAT f,UINT width=8,UINT height=8,UINT mips=4,UINT padding=0,bool alignLower=false){
    auto p=fourstory_dds::layout(f);std::vector<BYTE> data(128);setword(data,0,0x20534444);setword(data,4,124);setword(data,8,p.block?0x21007:0x2100f);
    setword(data,12,height);setword(data,16,width);setword(data,20,width*p.bytes+padding);setword(data,28,mips);setword(data,76,32);setword(data,108,0x401008);
    if(p.block){setword(data,80,4);setword(data,84,UINT(f));}
    else{setword(data,80,f==D3DFMT_A8?2:((f==D3DFMT_L8||f==D3DFMT_A8L8)?0x20000:0x40)|(p.mask[3]?1:0));setword(data,88,p.bytes*8);
        for(UINT i=0;i<4;++i)setword(data,92+i*4,((f==D3DFMT_L8||f==D3DFMT_A8L8)&&(i==1||i==2))?0:p.mask[i]);}
    for(UINT i=0;i<mips;++i){UINT rows=p.block?(height+3)/4:height,row=p.block?((width+3)/4)*p.block:(i==0?width*p.bytes+padding:(alignLower?((width*p.bytes+3)&~3U):width*p.bytes));
        for(UINT byte=0;byte<rows*row;++byte)data.push_back(BYTE((byte*13+i*37)&255));width=std::max(1U,width/2);height=std::max(1U,height/2);}
    return data;
}
HRESULT create(CpuDevice& device,const std::vector<BYTE>& data,D3DFORMAT f,IDirect3DTexture9** texture){
    D3DXIMAGE_INFO info;HRESULT hr=D3DXGetImageInfoFromFileInMemory(data.data(),UINT(data.size()),&info);if(FAILED(hr))return hr;
    return D3DXCreateTextureFromFileInMemoryEx(&device,data.data(),UINT(data.size()),D3DX_DEFAULT,D3DX_DEFAULT,info.MipLevels,0,f,D3DPOOL_MANAGED,D3DX_FILTER_NONE,D3DX_DEFAULT,0,nullptr,nullptr,texture);
}
void verify(const std::vector<BYTE>& data){
    fourstory_dds::Image image;assert(fourstory_dds::parse(data.data(),UINT(data.size()),image)==D3D_OK);CpuDevice device;IDirect3DTexture9* result=nullptr;
    assert(create(device,data,image.info.Format,&result)==D3D_OK);assert(device.creates==1&&device.lastPool==D3DPOOL_MANAGED&&device.lastUsage==0);
    auto t=static_cast<CpuTexture*>(result);
    auto sourceHash=dds_test_reference::sha256(data.data(),data.size());
    for(UINT i=0;i<image.info.MipLevels;++i){
        const auto& level=image.levels[i];D3DSURFACE_DESC desc{};assert(t->GetLevelDesc(i,&desc)==D3D_OK);
        auto pixels=fourstory_dds::layout(desc.Format);UINT rows=pixels.block?(desc.Height+3)/4:desc.Height;
        UINT rowBytes=pixels.block?((desc.Width+3)/4)*pixels.block:desc.Width*pixels.bytes;
        std::vector<BYTE> active;
        for(UINT row=0;row<rows;++row){
            const BYTE* bytes=t->storage[i].data()+row*t->pitches[i];
            active.insert(active.end(),bytes,bytes+rowBytes);
            for(UINT pad=rowBytes;pad<t->pitches[i];++pad)assert(bytes[pad]==0xcd);
        }
        const char* expected=dds_test_reference::find(sourceHash,i);
        if(expected)assert(dds_test_reference::sha256(active.data(),active.size())==expected);
        else {
            // Full compressed blocks retain bytes; RGB default NPOT expansion
            // preserves source pixels and explicitly zero-fills the new area.
            assert(!(pixels.block&&(level.width<4||level.height<4)&&(desc.Format==D3DFMT_DXT1||desc.Format==D3DFMT_DXT3)));
            for(UINT row=0;row<rows;++row){const BYTE* bytes=t->storage[i].data()+row*t->pitches[i];
                if(row<level.rows){assert(!std::memcmp(bytes,data.data()+level.offset+row*level.pitch,level.rowBytes));
                    for(UINT x=level.rowBytes;x<rowBytes;++x)assert(bytes[x]==0);}
                else for(UINT x=0;x<rowBytes;++x)assert(bytes[x]==0);
            }
        }
    }
    result->Release();assert(CpuTexture::live==0);
}
int main(int argc,char** argv){
    if(argc>1){for(int i=1;i<argc;++i){std::ifstream in(argv[i],std::ios::binary);assert(in);std::vector<BYTE> data((std::istreambuf_iterator<char>(in)),{});verify(data);std::cout<<"PASS DDS byte upload "<<argv[i]<<'\n';}return 0;}
    for(auto f:{D3DFMT_DXT1,D3DFMT_DXT2,D3DFMT_DXT3,D3DFMT_DXT4,D3DFMT_DXT5,D3DFMT_A8R8G8B8,D3DFMT_X8R8G8B8,D3DFMT_R5G6B5,D3DFMT_A4R4G4B4,D3DFMT_A8,D3DFMT_L8,D3DFMT_A8L8,D3DFMT_A8B8G8R8,D3DFMT_R8G8B8})verify(fixture(f));
    verify(fixture(D3DFMT_R8G8B8,7,5,1,3));
    verify(fixture(D3DFMT_R8G8B8,7,5,3,3,true));
    verify(fixture(D3DFMT_R8G8B8,7,5,3,3,false));
    fourstory_dds::Image paddedImage;auto padded=fixture(D3DFMT_R8G8B8,7,5,3,3,true);
    assert(fourstory_dds::parse(padded.data(),UINT(padded.size()),paddedImage)==D3D_OK);
    assert(paddedImage.levels[1].pitch==12&&paddedImage.levels[2].offset==272);
    auto tight=fixture(D3DFMT_R8G8B8,7,5,3,3,false);
    assert(fourstory_dds::parse(tight.data(),UINT(tight.size()),paddedImage)==D3D_OK);
    assert(paddedImage.levels[1].pitch==9&&paddedImage.levels[2].offset==266);
    // Default NPOT expansion needs a longer full chain than the source has.
    for(UINT requested:{0U,D3DX_DEFAULT}){CpuDevice device;IDirect3DTexture9* texture=nullptr;
        assert(D3DXCreateTextureFromFileInMemoryEx(&device,tight.data(),UINT(tight.size()),D3DX_DEFAULT,D3DX_DEFAULT,requested,0,D3DFMT_R8G8B8,D3DPOOL_MANAGED,D3DX_FILTER_NONE,D3DX_DEFAULT,0,nullptr,nullptr,&texture)==D3DXERR_INVALIDDATA);
        assert(!texture&&!device.creates);}
    // Explicit non-power-of-two BC input is rejected before allocation too.
    auto compressedNpot=fixture(D3DFMT_DXT1,6,2,1);
    for(UINT dimension:{D3DX_FROM_FILE,D3DX_DEFAULT_NONPOW2}){CpuDevice device;IDirect3DTexture9* texture=nullptr;
        assert(D3DXCreateTextureFromFileInMemoryEx(&device,compressedNpot.data(),UINT(compressedNpot.size()),dimension,dimension,1,0,D3DFMT_DXT1,D3DPOOL_MANAGED,D3DX_FILTER_NONE,D3DX_DEFAULT,0,nullptr,nullptr,&texture)==D3DXERR_INVALIDDATA);
        assert(!texture&&!device.creates);}
    auto valid=fixture(D3DFMT_DXT1);D3DXIMAGE_INFO info;
    for(UINT n=1;n<valid.size();++n)assert(D3DXGetImageInfoFromFileInMemory(valid.data(),n,&info)==D3DXERR_INVALIDDATA);
    assert(D3DXGetImageInfoFromFileInMemory(valid.data(),UINT(valid.size()),nullptr)==D3D_OK);
    for(auto change:{std::make_pair(4U,123U),{76U,31U},{16U,0U},{12U,0U},{28U,99U},{112U,0x200U},{84U,0x30315844U}}){auto data=valid;setword(data,change.first,change.second);CpuDevice d;IDirect3DTexture9* t=nullptr;assert(create(d,data,D3DFMT_DXT1,&t)==D3DXERR_INVALIDDATA);assert(!t&&!d.creates);}
    for(int mode=0;mode<4;++mode){CpuDevice d;d.failCreate=mode==0;d.failLock=mode==1?1:-1;d.failUnlock=mode==2?1:-1;d.badPitch=mode==3;IDirect3DTexture9* t=nullptr;
        assert(FAILED(create(d,valid,D3DFMT_DXT1,&t)));assert(!t&&CpuTexture::live==0);}
    auto xrgb=fixture(D3DFMT_X8R8G8B8,1,1,1);xrgb[128]=3;xrgb[129]=2;xrgb[130]=1;xrgb[131]=0;
    CpuDevice d;IDirect3DTexture9* t=nullptr;assert(create(d,xrgb,D3DFMT_A8R8G8B8,&t)==D3D_OK);auto cpu=static_cast<CpuTexture*>(t);
    assert(cpu->storage[0][0]==3&&cpu->storage[0][1]==2&&cpu->storage[0][2]==1&&cpu->storage[0][3]==255);t->Release();
    assert(create(d,xrgb,D3DFMT_DXT3,&t)==D3D_OK&&t);t->Release();
    assert(create(d,xrgb,D3DFMT_DXT5,&t)==D3DXERR_INVALIDDATA&&!t);
    auto bc=fixture(D3DFMT_DXT3,4,4,1);std::fill(bc.begin()+128,bc.end(),0);std::fill(bc.begin()+128,bc.begin()+136,0x88);bc[137]=0xf8;
    assert(create(d,bc,D3DFMT_A8R8G8B8,&t)==D3D_OK);cpu=static_cast<CpuTexture*>(t);
    for(UINT y=0;y<4;++y)for(UINT x=0;x<4;++x){auto p=cpu->storage[0].data()+y*cpu->pitches[0]+4*x;assert(p[0]==0&&p[1]==0&&p[2]==255&&p[3]==136);}t->Release();
    bc=fixture(D3DFMT_DXT1,4,4,1);std::fill(bc.begin()+128,bc.end(),0);std::fill(bc.begin()+132,bc.end(),255);
    assert(create(d,bc,D3DFMT_A8R8G8B8,&t)==D3D_OK);cpu=static_cast<CpuTexture*>(t);for(UINT y=0;y<4;++y)for(UINT x=0;x<16;++x)assert(cpu->storage[0][y*cpu->pitches[0]+x]==0);t->Release();
    std::cout<<"PASS DDS formats/mips, pitch padding, truncation, headers, conversion and device failure cleanup\n";
}
