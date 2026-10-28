#include <d3dx9.h>
#include "unused_surface.h"
#include "unused_device.h"
#include <cassert>
#include <iostream>
struct CpuSurface : UnusedSurfaceMethods {
    D3DSURFACE_DESC desc={};std::vector<BYTE> bytes;UINT pitch;bool locked=false;int locks=0;bool failLock=false,failUnlock=false;
    CpuSurface(UINT w,UINT h,D3DFORMAT f){auto p=fourstory_dds::layout(f);pitch=(p.block?((w+3)/4)*p.block:w*p.bytes)+16;desc.Width=w;desc.Height=h;desc.Format=f;bytes.resize(pitch*(p.block?(h+3)/4:h),0xcd);}
    HRESULT GetDesc(D3DSURFACE_DESC* out) override {*out=desc;return D3D_OK;}
    HRESULT LockRect(D3DLOCKED_RECT* out,const RECT*,DWORD) override {++locks;if(locked||failLock)return D3DERR_INVALIDCALL;locked=true;out->pBits=bytes.data();out->Pitch=pitch;return D3D_OK;}
    HRESULT UnlockRect() override {assert(locked);locked=false;return failUnlock?D3DERR_NOTAVAILABLE:D3D_OK;}
    void pixel(UINT x,UINT y,DWORD color){std::memcpy(bytes.data()+y*pitch+x*4,&color,4);}
    DWORD pixel(UINT x,UINT y){DWORD color;std::memcpy(&color,bytes.data()+y*pitch+x*4,4);return color;}
};
struct ArgumentDevice : UnusedDeviceMethods {
    UINT width=0,height=0,mips=0;D3DFORMAT format;DWORD usage;D3DPOOL pool;
    HRESULT CreateTexture(UINT w,UINT h,UINT m,DWORD u,D3DFORMAT f,D3DPOOL p,IDirect3DTexture9**,HANDLE*) override {width=w;height=h;mips=m;format=f;usage=u;pool=p;return D3DERR_NOTAVAILABLE;}
};
void word(std::vector<BYTE>& data,UINT offset,UINT value){for(UINT i=0;i<4;++i)data[offset+i]=BYTE(value>>(i*8));}
std::vector<BYTE> dds(){std::vector<BYTE> data(144);word(data,0,0x20534444);word(data,4,124);word(data,8,0x100f);word(data,12,2);word(data,16,2);word(data,20,8);word(data,76,32);word(data,80,0x41);word(data,88,32);word(data,92,0xff0000);word(data,96,0xff00);word(data,100,0xff);word(data,104,0xff000000);word(data,108,0x1000);for(UINT i=0;i<4;++i)word(data,128+i*4,0xff010203+i);return data;}
int main(){
    CpuSurface source(4,4,D3DFMT_A8R8G8B8),destination(6,6,D3DFMT_A8R8G8B8);
    for(UINT y=0;y<4;++y)for(UINT x=0;x<4;++x)source.pixel(x,y,0xff000000|(x<<16)|(y<<8)|17);
    RECT from={1,2,3,4},to={2,1,4,3};
    assert(D3DXLoadSurfaceFromSurface(&destination,nullptr,&to,&source,nullptr,&from,D3DX_FILTER_NONE,0)==D3D_OK);
    for(UINT y=0;y<6;++y)for(UINT x=0;x<6;++x)assert(destination.pixel(x,y)==(x>=2&&x<4&&y>=1&&y<3?source.pixel(x-1,y+1):0xcdcdcdcd));
    // Overlapping same-surface copy must use the old source, not cascade pixels.
    from={0,0,3,1};to={1,0,4,1};DWORD old[]={source.pixel(0,0),source.pixel(1,0),source.pixel(2,0)};
    assert(D3DXLoadSurfaceFromSurface(&source,nullptr,&to,&source,nullptr,&from,D3DX_FILTER_NONE,0)==D3D_OK);
    for(UINT x=0;x<3;++x)assert(source.pixel(x+1,0)==old[x]);
    auto data=dds();to={2,2,4,4};assert(D3DXLoadSurfaceFromFileInMemory(&destination,nullptr,&to,data.data(),UINT(data.size()),nullptr,D3DX_FILTER_NONE,0,nullptr)==D3D_OK);
    assert(destination.pixel(2,2)==0xff010203&&destination.pixel(3,3)==0xff010206);
    // Independently colored BC1 blocks: arbitrary crop crosses both block axes.
    std::vector<BYTE> blocks(160);std::copy(data.begin(),data.begin()+128,blocks.begin());word(blocks,12,8);word(blocks,16,8);word(blocks,8,0x1007);word(blocks,80,4);word(blocks,84,UINT(D3DFMT_DXT1));
    WORD colors[]={0xf800,0x07e0,0x001f,0xffff};for(UINT i=0;i<4;++i){std::memcpy(blocks.data()+128+i*8,&colors[i],2);std::memcpy(blocks.data()+130+i*8,&colors[i],2);}
    CpuSurface crop(3,3,D3DFMT_A8R8G8B8);from={3,3,6,6};
    assert(D3DXLoadSurfaceFromFileInMemory(&crop,nullptr,nullptr,blocks.data(),UINT(blocks.size()),&from,D3DX_FILTER_NONE,0,nullptr)==D3D_OK);
    DWORD rgb[]={0xffff0000,0xff00ff00,0xff0000ff,0xffffffff};for(UINT y=0;y<3;++y)for(UINT x=0;x<3;++x)assert(crop.pixel(x,y)==rgb[(y>0)*2+(x>0)]);
    CpuSurface smallBc(2,2,D3DFMT_DXT1);auto small=blocks;small.resize(136);word(small,12,2);word(small,16,2);
    assert(D3DXLoadSurfaceFromFileInMemory(&smallBc,nullptr,nullptr,small.data(),UINT(small.size()),nullptr,D3DX_FILTER_NONE,0,nullptr)==D3D_OK);
    assert(!std::memcmp(smallBc.bytes.data(),small.data()+128,8));
    CpuSurface edgeBc(6,6,D3DFMT_DXT1);for(UINT y=0;y<2;++y)std::memcpy(edgeBc.bytes.data()+y*edgeBc.pitch,blocks.data()+128+y*16,16);
    from={4,4,6,6};assert(D3DXLoadSurfaceFromSurface(&smallBc,nullptr,nullptr,&edgeBc,nullptr,&from,D3DX_FILTER_NONE,0)==D3D_OK);
    assert(!std::memcmp(smallBc.bytes.data(),blocks.data()+152,8));
    CpuSurface rgb565(2,2,D3DFMT_R5G6B5),alpha(2,2,D3DFMT_A8);
    for(UINT i=0;i<4;++i)word(data,128+i*4,0x88ff0000);
    assert(D3DXLoadSurfaceFromFileInMemory(&rgb565,nullptr,nullptr,data.data(),UINT(data.size()),nullptr,D3DX_FILTER_NONE,0,nullptr)==D3D_OK);
    WORD red;std::memcpy(&red,rgb565.bytes.data(),2);assert(red==0xf800);
    assert(D3DXLoadSurfaceFromFileInMemory(&alpha,nullptr,nullptr,data.data(),UINT(data.size()),nullptr,D3DX_FILTER_NONE,0,nullptr)==D3D_OK);assert(alpha.bytes[0]==0x88);
    to={-1,0,1,2};assert(D3DXLoadSurfaceFromFileInMemory(&destination,nullptr,&to,data.data(),UINT(data.size()),nullptr,D3DX_FILTER_NONE,0,nullptr)==D3DERR_INVALIDCALL);
    to={0,0,3,2};assert(D3DXLoadSurfaceFromFileInMemory(&destination,nullptr,&to,data.data(),UINT(data.size()),nullptr,D3DX_FILTER_NONE,0,nullptr)==D3DXERR_INVALIDDATA);
    source.failUnlock=true;assert(D3DXLoadSurfaceFromSurface(&destination,nullptr,nullptr,&source,nullptr,nullptr,D3DX_FILTER_NONE,0)==D3DERR_NOTAVAILABLE);assert(!source.locked);source.failUnlock=false;
    destination.failLock=true;to={0,0,2,2};assert(FAILED(D3DXLoadSurfaceFromFileInMemory(&destination,nullptr,&to,data.data(),UINT(data.size()),nullptr,D3DX_FILTER_NONE,0,nullptr)));assert(!destination.locked);
    ArgumentDevice device;IDirect3DTexture9* texture=nullptr;
    assert(D3DXCreateTexture(&device,D3DX_DEFAULT,D3DX_DEFAULT,0,0,D3DFMT_UNKNOWN,D3DPOOL_MANAGED,&texture)==D3DERR_NOTAVAILABLE);
    assert(device.width==256&&device.height==256&&device.mips==9&&device.format==D3DFMT_A8R8G8B8&&!texture);
    D3DXCreateTexture(&device,D3DX_DEFAULT,8,99,0,D3DFMT_A8,D3DPOOL_MANAGED,&texture);assert(device.width==8&&device.height==8&&device.mips==4);
    D3DXCreateTexture(&device,0,0,1,0,D3DFMT_A8,D3DPOOL_MANAGED,&texture);assert(device.width==1&&device.height==1&&device.mips==1);
    std::cout<<"PASS DDS surface crop/conversion, padded rows, overlap, invalid/failure cases and texture arguments\n";
}
