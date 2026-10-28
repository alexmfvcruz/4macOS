// Device-backed DDS subset and uncompressed/RLE TGA (C4). Unsupported input never becomes a placeholder.
#pragma once
#include <d3d9.h>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <vector>
#include <vector>
#include <new>
#include <algorithm>
#include "fourstory_bc.h"
#ifndef D3DX_DEFAULT
#define D3DX_DEFAULT ((UINT)-1)
#define D3DX_DEFAULT_NONPOW2 ((UINT)-2)
#define D3DX_FROM_FILE ((UINT)-3)
#define D3DFMT_FROM_FILE ((D3DFORMAT)-3)
#define D3DX_FILTER_NONE 1U
#define D3DX_FILTER_POINT 2U
#define D3DX_FILTER_LINEAR 3U
#define D3DX_FILTER_TRIANGLE 4U
#define D3DX_FILTER_BOX 5U
#define D3DXERR_INVALIDDATA ((HRESULT)0x88760b59U)
#endif
enum D3DXIMAGE_FILEFORMAT {D3DXIFF_BMP,D3DXIFF_JPG,D3DXIFF_TGA,D3DXIFF_PNG,D3DXIFF_DDS,D3DXIFF_PPM,D3DXIFF_DIB,D3DXIFF_HDR,D3DXIFF_PFM,D3DXIFF_FORCE_DWORD=0x7fffffff};
struct D3DXIMAGE_INFO {UINT Width,Height,Depth,MipLevels;D3DFORMAT Format;D3DRESOURCETYPE ResourceType;D3DXIMAGE_FILEFORMAT ImageFileFormat;};
static_assert(sizeof(D3DXIMAGE_INFO)==28,"D3DX image info ABI");
namespace fourstory_dds {
inline std::uint32_t word(const BYTE* p){return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|(std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);}
constexpr std::uint32_t fourcc(char a,char b,char c,char d){return std::uint32_t(a)|(std::uint32_t(b)<<8)|(std::uint32_t(c)<<16)|(std::uint32_t(d)<<24);}
struct PixelLayout {D3DFORMAT format;UINT bytes,block;std::uint32_t mask[4];};
inline PixelLayout layout(D3DFORMAT format){
    switch(format){
    case D3DFMT_A8R8G8B8:return {format,4,0,{0xff0000,0xff00,0xff,0xff000000}};
    case D3DFMT_X8R8G8B8:return {format,4,0,{0xff0000,0xff00,0xff,0}};
    case D3DFMT_A8B8G8R8:return {format,4,0,{0xff,0xff00,0xff0000,0xff000000}};
    case D3DFMT_X8B8G8R8:return {format,4,0,{0xff,0xff00,0xff0000,0}};
    case D3DFMT_R8G8B8:return {format,3,0,{0xff0000,0xff00,0xff,0}};
    case D3DFMT_R5G6B5:return {format,2,0,{0xf800,0x7e0,0x1f,0}};
    case D3DFMT_A1R5G5B5:return {format,2,0,{0x7c00,0x3e0,0x1f,0x8000}};
    case D3DFMT_X1R5G5B5:return {format,2,0,{0x7c00,0x3e0,0x1f,0}};
    case D3DFMT_A4R4G4B4:return {format,2,0,{0xf00,0xf0,0xf,0xf000}};
    case D3DFMT_X4R4G4B4:return {format,2,0,{0xf00,0xf0,0xf,0}};
    case D3DFMT_A8:return {format,1,0,{0,0,0,0xff}};
    case D3DFMT_L8:return {format,1,0,{0xff,0xff,0xff,0}};
    case D3DFMT_A8L8:return {format,2,0,{0xff,0xff,0xff,0xff00}};
    case D3DFMT_DXT1:return {format,0,8,{0,0,0,0}};
    case D3DFMT_DXT2:case D3DFMT_DXT3:case D3DFMT_DXT4:case D3DFMT_DXT5:return {format,0,16,{0,0,0,0}};
    default:return {D3DFMT_UNKNOWN,0,0,{0,0,0,0}};
    }
}
struct Level {UINT width,height,rows,rowBytes;std::size_t offset,pitch;};
// decoded holds the pixels of formats that are not stored as-is (TGA, C4); the
// level offsets then refer to decoded instead of the file data.
struct Image {D3DXIMAGE_INFO info;PixelLayout pixels;std::vector<Level> levels;std::vector<BYTE> decoded;
    const BYTE* base(const void* source)const{return decoded.empty()?static_cast<const BYTE*>(source):decoded.data();}};
inline HRESULT parseDDS(const void* source,UINT size,Image& image){
    if(!source||size<128)return D3DXERR_INVALIDDATA;
    const BYTE* data=static_cast<const BYTE*>(source);
    if(word(data)!=fourcc('D','D','S',' ')||word(data+4)!=124||word(data+76)!=32)return D3DXERR_INVALIDDATA;
    UINT width=word(data+16),height=word(data+12),mips=word(data+28);if(!mips)mips=1;
    // This subset supports 2D textures only; reject volume/cube/DX10 containers.
    if(!width||!height||width>16384||height>16384||word(data+24)>1||(word(data+112)&0x20fe00))return D3DXERR_INVALIDDATA;
    UINT maxMips=1;for(UINT side=std::max(width,height);side>1;side>>=1)++maxMips;
    if(mips>maxMips)return D3DXERR_INVALIDDATA;
    UINT flags=word(data+80),code=word(data+84),bits=word(data+88);D3DFORMAT format=D3DFMT_UNKNOWN;
    if(flags&4){
        switch(code){case fourcc('D','X','T','1'):format=D3DFMT_DXT1;break;case fourcc('D','X','T','2'):format=D3DFMT_DXT2;break;
        case fourcc('D','X','T','3'):format=D3DFMT_DXT3;break;case fourcc('D','X','T','4'):format=D3DFMT_DXT4;break;case fourcc('D','X','T','5'):format=D3DFMT_DXT5;break;default:return D3DXERR_INVALIDDATA;}
    }else{
        const D3DFORMAT formats[]={D3DFMT_A8R8G8B8,D3DFMT_X8R8G8B8,D3DFMT_A8B8G8R8,D3DFMT_X8B8G8R8,D3DFMT_R8G8B8,D3DFMT_R5G6B5,D3DFMT_A1R5G5B5,D3DFMT_X1R5G5B5,D3DFMT_A4R4G4B4,D3DFMT_X4R4G4B4,D3DFMT_A8,D3DFMT_L8,D3DFMT_A8L8};
        for(auto candidate:formats){auto p=layout(candidate);UINT expectedFlags=candidate==D3DFMT_A8?2:(candidate==D3DFMT_L8||candidate==D3DFMT_A8L8?0x20000:0x40);
            if(!(flags&expectedFlags)||bits!=p.bytes*8)continue;
            bool matches=true;
            // Luminance has only an R-mask; synthesized G/B are not header fields.
            for(UINT i=0;i<4;++i){UINT mask=(expectedFlags==0x20000&&(i==1||i==2))?0:p.mask[i];if(word(data+92+i*4)!=mask)matches=false;}
            if(matches){format=candidate;break;}}
    }
    if(format==D3DFMT_UNKNOWN)return D3DXERR_INVALIDDATA;
    image.info={width,height,1,mips,format,D3DRTYPE_TEXTURE,D3DXIFF_DDS};image.pixels=layout(format);image.levels.clear();
    // Validate the complete payload layout before choosing legacy row padding.
    // Header pitch describes only level0; older RGB DDS writers DWORD-align
    // lower rows too. Accept either exact layout, reject ambiguous extra data.
    std::size_t tightSize=128,alignedSize=128;UINT lw=width,lh=height;
    for(UINT i=0;i<mips;++i){
        UINT rows=image.pixels.block?(lh+3)/4:lh;
        std::size_t row=image.pixels.block?((lw+3)/4)*image.pixels.block:lw*image.pixels.bytes;
        std::size_t tight=row,aligned=image.pixels.block?row:((row+3)&~std::size_t(3));
        if(i==0&&!image.pixels.block&&(word(data+8)&8)){tight=aligned=word(data+20);if(tight<row)return D3DXERR_INVALIDDATA;}
        tightSize+=tight*rows;alignedSize+=aligned*rows;lw=std::max(1U,lw/2);lh=std::max(1U,lh/2);
    }
    bool alignedRows=false;
    if(tightSize!=size){if(!image.pixels.block&&alignedSize==size)alignedRows=true;else return D3DXERR_INVALIDDATA;}
    std::size_t offset=128;
    for(UINT i=0;i<mips;++i){
        UINT rows=image.pixels.block?(height+3)/4:height;
        UINT rowBytes=image.pixels.block?((width+3)/4)*image.pixels.block:width*image.pixels.bytes;
        std::size_t pitch=alignedRows?((rowBytes+3)&~std::size_t(3)):rowBytes;
        // Top-level header pitch takes precedence over the inferred lower layout.
        if(i==0&&!image.pixels.block&&(word(data+8)&8)){pitch=word(data+20);if(pitch<rowBytes)return D3DXERR_INVALIDDATA;}
        std::size_t bytes=pitch*rows;if(offset>size||bytes>size-offset)return D3DXERR_INVALIDDATA;
        image.levels.push_back({width,height,rows,rowBytes,offset,pitch});offset+=bytes;
        width=std::max(1U,width/2);height=std::max(1U,height/2);
    }
    return D3D_OK;
}
// Truevision TGA (C4: the client's Intro/Outro images). Image types 2
// (uncompressed) and 10 (run-length) with 24 or 32 bits per pixel, either
// origin; other types fail. D3DX reports 24-bit files as D3DFMT_R8G8B8 and
// 32-bit files as D3DFMT_A8R8G8B8. The pixels are decoded to 32-bit
// B,G,R,A rows, top row first (X8R8G8B8 for 24-bit files).
inline HRESULT parseTGA(const void* source,UINT size,Image& image){
    const BYTE* d=static_cast<const BYTE*>(source);
    if(!source||size<18)return D3DXERR_INVALIDDATA;
    const UINT idLength=d[0],colorMapType=d[1],type=d[2],bpp=d[16],descriptor=d[17];
    const UINT width=UINT(d[12])|(UINT(d[13])<<8),height=UINT(d[14])|(UINT(d[15])<<8);
    if(colorMapType!=0||(type!=2&&type!=10)||(bpp!=24&&bpp!=32)||!width||!height||width>16384||height>16384)return D3DXERR_INVALIDDATA;
    const UINT bytes=bpp/8;std::size_t at=18+idLength;
    const std::size_t pixels=std::size_t(width)*height;
    // Validate the data length before the allocation: raw data holds every
    // pixel; a run-length packet of 1+bytes file bytes holds at most 128 pixels.
    if(at>size)return D3DXERR_INVALIDDATA;
    if(type==2&&pixels*bytes>size-at)return D3DXERR_INVALIDDATA;
    if(type==10&&pixels>(size-at)/(1+bytes)*128+128)return D3DXERR_INVALIDDATA;
    image.decoded.assign(pixels*4,0);
    const bool topDown=(descriptor&0x20)!=0,rightToLeft=(descriptor&0x10)!=0;
    auto store=[&](std::size_t index,const BYTE* p){
        const UINT x=UINT(index%width),y=UINT(index/width);
        const UINT dx=rightToLeft?width-1-x:x,dy=topDown?y:height-1-y;
        BYTE* o=&image.decoded[(std::size_t(dy)*width+dx)*4];o[0]=p[0];o[1]=p[1];o[2]=p[2];o[3]=bytes==4?p[3]:255;};
    const std::size_t total=std::size_t(width)*height;
    if(type==2){
        if(at>size||total*bytes>size-at)return D3DXERR_INVALIDDATA;
        for(std::size_t i=0;i<total;++i)store(i,d+at+i*bytes);
    }else{
        std::size_t i=0;
        while(i<total){
            if(at>=size)return D3DXERR_INVALIDDATA;
            const BYTE header=d[at++];const std::size_t count=std::size_t(header&0x7f)+1;
            if(count>total-i)return D3DXERR_INVALIDDATA;
            if(header&0x80){if(bytes>size-at)return D3DXERR_INVALIDDATA;for(std::size_t k=0;k<count;++k)store(i++,d+at);at+=bytes;}
            else{if(count*bytes>size-at)return D3DXERR_INVALIDDATA;for(std::size_t k=0;k<count;++k){store(i++,d+at);at+=bytes;}}
        }
    }
    image.info={width,height,1,1,bytes==4?D3DFMT_A8R8G8B8:D3DFMT_R8G8B8,D3DRTYPE_TEXTURE,D3DXIFF_TGA};
    image.pixels=layout(bytes==4?D3DFMT_A8R8G8B8:D3DFMT_X8R8G8B8);
    image.levels.assign(1,Level{width,height,height,width*4,0,std::size_t(width)*4});
    return D3D_OK;
}
inline HRESULT parse(const void* source,UINT size,Image& image){
    if(source&&size>=4&&word(static_cast<const BYTE*>(source))==fourcc('D','D','S',' '))return parseDDS(source,size,image);
    return parseTGA(source,size,image);
}
inline UINT shift(std::uint32_t mask){UINT s=0;while(mask&&!(mask&1)){++s;mask>>=1;}return s;}
inline UINT channel(std::uint32_t pixel,std::uint32_t mask,UINT absent){if(!mask)return absent;UINT s=shift(mask),maximum=mask>>s;return (((pixel&mask)>>s)*255+maximum/2)/maximum;}
inline std::uint32_t pack(UINT value,std::uint32_t mask){if(!mask)return 0;UINT s=shift(mask),maximum=mask>>s;return ((value*maximum+127)/255)<<s;}
inline void bcColor(const BYTE* block,D3DFORMAT format,UINT x,UINT y,float rgba[4]){
    const BYTE* color=block+(format==D3DFMT_DXT1?0:8);
    UINT first=UINT(color[0])|(UINT(color[1])<<8),second=UINT(color[2])|(UINT(color[3])<<8);
    float colors[4][3] = {};
    for(UINT c=0;c<3;++c){UINT shift=c==0?11:(c==1?5:0),bits=c==1?6:5,mask=(1U<<bits)-1;
        float a=float((first>>shift)&mask)*(1.f/float(mask));
        float b=float((second>>shift)&mask)*(1.f/float(mask));
        colors[0][c]=a;colors[1][c]=b;
        // The original D3DX9 decoder selects the three-color branch from
        // endpoint order for BC2 too (verified with original DLL readbacks).
        if(first>second){
            colors[2][c]=a+(b-a)*(1.f/3.f);colors[3][c]=a+(b-a)*(2.f/3.f);
        }else colors[2][c]=a+(b-a)*.5f;
    }
    UINT index=(word(color+4)>>(2*(y*4+x)))&3;
    for(UINT c=0;c<3;++c)rgba[c]=colors[index][c];
    rgba[3]=(first<=second&&format==D3DFMT_DXT1&&index==3)?0.f:1.f;
    if(format==D3DFMT_DXT3){UINT pixel=y*4+x;rgba[3]=float((block[pixel/2]>>(4*(pixel%2)))&15)*(1.f/15.f);}
}
inline void bcPixel(const BYTE* block,D3DFORMAT format,UINT x,UINT y,UINT rgba[4]){
    float channels[4];bcColor(block,format,x,y,channels);
    for(UINT c=0;c<4;++c)rgba[c]=UINT(channels[c]*255.f+.5f);
}
inline HRESULT copyLevel(const BYTE* source,const Level& level,const PixelLayout& input,const PixelLayout& output,const D3DLOCKED_RECT& lock){
    UINT rowBytes=output.block?((level.width+3)/4)*output.block:level.width*output.bytes;
    if(!lock.pBits||lock.Pitch<int(rowBytes))return D3DERR_INVALIDCALL;
    if(input.format==output.format&&input.block&&(level.width<4||level.height<4)&&
       (input.format==D3DFMT_DXT1||input.format==D3DFMT_DXT3)){
        // D3DX expands the logical 1/2-pixel mip periodically to one 4x4
        // block, then re-encodes. Copying unused compressed texels differs.
        for(UINT by=0;by<level.height;by+=4)for(UINT bx=0;bx<level.width;bx+=4){
            fourstory_bc::HDRColorA colors[16];
            const BYTE* block=source+level.offset+(by/4)*level.pitch+(bx/4)*input.block;
            for(UINT y=0;y<4;++y)for(UINT x=0;x<4;++x){float rgba[4];
                bcColor(block,input.format,x%std::min(4U,level.width),y%std::min(4U,level.height),rgba);
                colors[y*4+x]=fourstory_bc::HDRColorA(rgba[0],rgba[1],rgba[2],rgba[3]);
            }
            BYTE* dest=static_cast<BYTE*>(lock.pBits)+std::size_t(by/4)*lock.Pitch+(bx/4)*output.block;
            fourstory_bc::encodeColors(colors,output.format==D3DFMT_DXT1,dest);
        }
        return D3D_OK;
    }
    if(input.format==output.format){for(UINT y=0;y<level.rows;++y)std::memcpy(static_cast<BYTE*>(lock.pBits)+std::size_t(y)*lock.Pitch,source+level.offset+y*level.pitch,rowBytes);return D3D_OK;}
    if(output.block){
        if(input.block||(output.format!=D3DFMT_DXT1&&output.format!=D3DFMT_DXT3))return D3DXERR_INVALIDDATA;
        for(UINT by=0;by<level.height;by+=4)for(UINT bx=0;bx<level.width;bx+=4){
            BYTE rgba[16][4];
            for(UINT y=0;y<4;++y)for(UINT x=0;x<4;++x){
                UINT sx=bx+x%std::min(4U,level.width-bx),sy=by+y%std::min(4U,level.height-by);
                const BYTE* p=source+level.offset+sy*level.pitch+sx*input.bytes;
                std::uint32_t pixel=0;for(UINT i=0;i<input.bytes;++i)pixel|=std::uint32_t(p[i])<<(8*i);
                for(UINT c=0;c<4;++c)rgba[y*4+x][c]=BYTE(channel(pixel,input.mask[c],c==3?255:0));
            }
            BYTE* dest=static_cast<BYTE*>(lock.pBits)+std::size_t(by/4)*lock.Pitch+(bx/4)*output.block;
            fourstory_bc::encode(rgba,output.format==D3DFMT_DXT1,dest);
        }
        return D3D_OK;
    }
    if(input.block&&input.format!=D3DFMT_DXT1&&input.format!=D3DFMT_DXT3)return D3DXERR_INVALIDDATA;
    for(UINT y=0;y<level.height;++y)for(UINT x=0;x<level.width;++x){
        UINT rgba[4];
        if(input.block)bcPixel(source+level.offset+(y/4)*level.pitch+(x/4)*input.block,input.format,x%4,y%4,rgba);
        else{const BYTE* p=source+level.offset+y*level.pitch+x*input.bytes;std::uint32_t pixel=0;for(UINT i=0;i<input.bytes;++i)pixel|=std::uint32_t(p[i])<<(i*8);
            for(UINT c=0;c<4;++c)rgba[c]=channel(pixel,input.mask[c],c==3?255:0);}
        std::uint32_t converted=0;for(UINT c=0;c<4;++c)converted|=pack(rgba[c],output.mask[c]);
        BYTE* dest=static_cast<BYTE*>(lock.pBits)+std::size_t(y)*lock.Pitch+x*output.bytes;for(UINT i=0;i<output.bytes;++i)dest[i]=BYTE(converted>>(i*8));
    }
    return D3D_OK;
}
}
inline HRESULT D3DXGetImageInfoFromFileInMemory(const void* source,UINT size,D3DXIMAGE_INFO* info){
    if(!source||!size)return D3DERR_INVALIDCALL;
    try{fourstory_dds::Image image;HRESULT hr=fourstory_dds::parse(source,size,image);if(SUCCEEDED(hr)&&info)*info=image.info;return hr;}
    catch(const std::bad_alloc&){return E_OUTOFMEMORY;}
}
inline HRESULT D3DXCreateTextureFromFileInMemoryEx(IDirect3DDevice9* device,const void* source,UINT size,UINT width,UINT height,UINT mips,DWORD usage,D3DFORMAT format,D3DPOOL pool,DWORD filter,DWORD mipFilter,D3DCOLOR colorKey,D3DXIMAGE_INFO* info,PALETTEENTRY* palette,IDirect3DTexture9** texture){
    if(!texture)return D3DERR_INVALIDCALL;*texture=nullptr;
    if(!device||!source||!size)return D3DERR_INVALIDCALL;
    try{
        fourstory_dds::Image image;HRESULT hr=fourstory_dds::parse(source,size,image);if(FAILED(hr))return hr;
        // Compressed NPOT edge rules are outside this measured subset.
        if(image.pixels.block&&((image.info.Width&(image.info.Width-1))||(image.info.Height&(image.info.Height-1))))return D3DXERR_INVALIDDATA;
        bool defaultWidth=width==D3DX_DEFAULT,defaultHeight=height==D3DX_DEFAULT;
        auto powerOfTwo=[](UINT n){UINT p=1;while(p<n)p*=2;return p;};
        if(defaultWidth)width=powerOfTwo(image.info.Width);
        else if(!width||width==D3DX_DEFAULT_NONPOW2||width==D3DX_FROM_FILE)width=image.info.Width;
        if(defaultHeight)height=powerOfTwo(image.info.Height);
        else if(!height||height==D3DX_DEFAULT_NONPOW2||height==D3DX_FROM_FILE)height=image.info.Height;
        UINT fullMips=1;for(UINT side=std::max(width,height);side>1;side>>=1)++fullMips;
        if(!mips||mips==D3DX_DEFAULT)mips=fullMips;else if(mips==D3DX_FROM_FILE)mips=image.info.MipLevels;
        if(format==D3DFMT_UNKNOWN||format==D3DFMT_FROM_FILE)format=image.info.Format;
        // R8G8B8 is not a texture format of Direct3D 9 devices; D3DX selects X8R8G8B8 (INFERRED, D3DXCheckTextureRequirements).
        if(format==D3DFMT_R8G8B8&&image.info.ImageFileFormat==D3DXIFF_TGA)format=D3DFMT_X8R8G8B8;
        auto output=fourstory_dds::layout(format);
        // FILTER_NONE default NPOT expansion copies the top-left pixels and
        // zero-fills the remainder; it does not scale the source image.
        bool padded=width!=image.info.Width||height!=image.info.Height;
        if(padded&&((width!=image.info.Width&&!defaultWidth)||(height!=image.info.Height&&!defaultHeight)||image.pixels.block||output.block))return D3DXERR_INVALIDDATA;
        // No other resizing, mip generation/skipping, palette or color-key fallback.
        if(mips>image.info.MipLevels||!mips||filter!=D3DX_FILTER_NONE||(mipFilter!=D3DX_DEFAULT&&mipFilter!=D3DX_FILTER_NONE)||colorKey||palette||output.format==D3DFMT_UNKNOWN)return D3DXERR_INVALIDDATA;
        if(format!=image.info.Format){
            if(output.block&&(image.pixels.block||(format!=D3DFMT_DXT1&&format!=D3DFMT_DXT3)))return D3DXERR_INVALIDDATA;
            if(image.pixels.block&&image.info.Format!=D3DFMT_DXT1&&image.info.Format!=D3DFMT_DXT3)return D3DXERR_INVALIDDATA;
        }
        // Converting RGB to luminance needs a defined colorimetric rule, not OR.
        if(format!=image.info.Format&&(format==D3DFMT_L8||format==D3DFMT_A8L8))return D3DXERR_INVALIDDATA;
        IDirect3DTexture9* result=nullptr;hr=device->CreateTexture(width,height,mips,usage,format,pool,&result,nullptr);
        if(FAILED(hr)){if(result)result->Release();return hr;}if(!result)return D3DERR_INVALIDCALL;
        if(result->GetLevelCount()!=mips){result->Release();return D3DERR_INVALIDCALL;}
        for(UINT i=0;i<mips;++i){
            D3DSURFACE_DESC desc;hr=result->GetLevelDesc(i,&desc);
            if(SUCCEEDED(hr)&&(desc.Width!=std::max(1U,width>>i)||desc.Height!=std::max(1U,height>>i)||desc.Format!=format))hr=D3DERR_INVALIDCALL;
            if(FAILED(hr)){result->Release();return hr;}
            D3DLOCKED_RECT lock;hr=result->LockRect(i,&lock,nullptr,0);if(FAILED(hr)){result->Release();return hr;}
            if(padded){
                UINT activeBytes=desc.Width*output.bytes;
                if(!lock.pBits||lock.Pitch<int(activeBytes))hr=D3DERR_INVALIDCALL;
                else for(UINT row=0;row<desc.Height;++row)
                    std::memset(static_cast<BYTE*>(lock.pBits)+std::size_t(row)*lock.Pitch,0,activeBytes);
            }
            if(SUCCEEDED(hr))hr=fourstory_dds::copyLevel(image.base(source),image.levels[i],image.pixels,output,lock);
            HRESULT unlock=result->UnlockRect(i);if(SUCCEEDED(hr))hr=unlock;
            if(FAILED(hr)){result->Release();return hr;}
        }
        if(info)*info=image.info;*texture=result;return D3D_OK;
    }catch(const std::bad_alloc&){return E_OUTOFMEMORY;}
}

namespace fourstory_dds {
inline bool rect(const RECT* requested,UINT width,UINT height,RECT& out){
    out=requested?*requested:RECT{0,0,LONG(width),LONG(height)};
    return out.left>=0&&out.top>=0&&out.right>out.left&&out.bottom>out.top&&UINT(out.right)<=width&&UINT(out.bottom)<=height;
}
inline HRESULT region(const BYTE* source,std::size_t sourcePitch,UINT sourceWidth,UINT sourceHeight,const PixelLayout& input,const RECT& from,const PixelLayout& output,UINT destinationWidth,UINT destinationHeight,const RECT& to,const D3DLOCKED_RECT& lock){
    UINT width=UINT(from.right-from.left),height=UINT(from.bottom-from.top);
    if(width!=UINT(to.right-to.left)||height!=UINT(to.bottom-to.top))return D3DXERR_INVALIDDATA;
    UINT sourceRow=input.block?((sourceWidth+3)/4)*input.block:sourceWidth*input.bytes;
    UINT destinationRow=output.block?((UINT(to.right)+3)/4)*output.block:UINT(to.right)*output.bytes;
    if(!source||sourcePitch<sourceRow||!lock.pBits||lock.Pitch<int(destinationRow))return D3DERR_INVALIDCALL;
    if(input.format==output.format&&input.block){
        if(from.left%4||from.top%4||to.left%4||to.top%4||(UINT(from.right)!=sourceWidth&&from.right%4)||(UINT(from.bottom)!=sourceHeight&&from.bottom%4)||(UINT(to.right)!=destinationWidth&&to.right%4)||(UINT(to.bottom)!=destinationHeight&&to.bottom%4))return D3DXERR_INVALIDDATA;
        UINT rows=(height+3)/4,rowBytes=((width+3)/4)*input.block;
        for(UINT y=0;y<rows;++y)std::memcpy(static_cast<BYTE*>(lock.pBits)+std::size_t(to.top/4+y)*lock.Pitch+(to.left/4)*output.block,source+std::size_t(from.top/4+y)*sourcePitch+(from.left/4)*input.block,rowBytes);
        return D3D_OK;
    }
    if(output.block||input.format==D3DFMT_UNKNOWN||output.format==D3DFMT_UNKNOWN||(input.block&&input.format!=D3DFMT_DXT1&&input.format!=D3DFMT_DXT3))return D3DXERR_INVALIDDATA;
    if(input.format!=output.format&&(output.format==D3DFMT_L8||output.format==D3DFMT_A8L8))return D3DXERR_INVALIDDATA;
    for(UINT y=0;y<height;++y)for(UINT x=0;x<width;++x){
        UINT sx=UINT(from.left)+x,sy=UINT(from.top)+y;BYTE* destination=static_cast<BYTE*>(lock.pBits)+std::size_t(to.top+y)*lock.Pitch+(to.left+x)*output.bytes;
        if(input.format==output.format){std::memcpy(destination,source+sy*sourcePitch+sx*input.bytes,output.bytes);continue;}
        UINT rgba[4];if(input.block)bcPixel(source+(sy/4)*sourcePitch+(sx/4)*input.block,input.format,sx%4,sy%4,rgba);
        else{const BYTE* p=source+sy*sourcePitch+sx*input.bytes;std::uint32_t pixel=0;for(UINT i=0;i<input.bytes;++i)pixel|=std::uint32_t(p[i])<<(i*8);for(UINT c=0;c<4;++c)rgba[c]=channel(pixel,input.mask[c],c==3?255:0);}
        std::uint32_t pixel=0;for(UINT c=0;c<4;++c)pixel|=pack(rgba[c],output.mask[c]);for(UINT i=0;i<output.bytes;++i)destination[i]=BYTE(pixel>>(i*8));
    }
    return D3D_OK;
}
inline HRESULT loadSurface(IDirect3DSurface9* destination,const RECT* destinationRect,const BYTE* source,std::size_t sourcePitch,UINT width,UINT height,const PixelLayout& input,const RECT* sourceRect){
    D3DSURFACE_DESC desc;HRESULT hr=destination->GetDesc(&desc);if(FAILED(hr))return hr;
    auto output=layout(desc.Format);RECT from,to;
    if(!rect(sourceRect,width,height,from)||!rect(destinationRect,desc.Width,desc.Height,to))return D3DERR_INVALIDCALL;
    if(from.right-from.left!=to.right-to.left||from.bottom-from.top!=to.bottom-to.top)return D3DXERR_INVALIDDATA;
    D3DLOCKED_RECT lock;hr=destination->LockRect(&lock,nullptr,0);if(FAILED(hr))return hr;
    hr=region(source,sourcePitch,width,height,input,from,output,desc.Width,desc.Height,to,lock);HRESULT unlock=destination->UnlockRect();return FAILED(hr)?hr:unlock;
}
}
inline HRESULT D3DXLoadSurfaceFromFileInMemory(IDirect3DSurface9* destination,const PALETTEENTRY* palette,const RECT* destinationRect,const void* source,UINT size,const RECT* sourceRect,DWORD filter,D3DCOLOR colorKey,D3DXIMAGE_INFO* info){
    if(!destination||!source||!size)return D3DERR_INVALIDCALL;
    if(palette||filter!=D3DX_FILTER_NONE||colorKey)return D3DXERR_INVALIDDATA;
    try{fourstory_dds::Image image;HRESULT hr=fourstory_dds::parse(source,size,image);if(FAILED(hr))return hr;
        const auto& level=image.levels[0];hr=fourstory_dds::loadSurface(destination,destinationRect,image.base(source)+level.offset,level.pitch,level.width,level.height,image.pixels,sourceRect);
        if(SUCCEEDED(hr)&&info)*info=image.info;return hr;
    }catch(const std::bad_alloc&){return E_OUTOFMEMORY;}
}
inline HRESULT D3DXLoadSurfaceFromSurface(IDirect3DSurface9* destination,const PALETTEENTRY* destinationPalette,const RECT* destinationRect,IDirect3DSurface9* source,const PALETTEENTRY* sourcePalette,const RECT* sourceRect,DWORD filter,D3DCOLOR colorKey){
    if(!destination||!source)return D3DERR_INVALIDCALL;
    if(destinationPalette||sourcePalette||filter!=D3DX_FILTER_NONE||colorKey)return D3DXERR_INVALIDDATA;
    D3DSURFACE_DESC desc;HRESULT hr=source->GetDesc(&desc);if(FAILED(hr))return hr;auto pixels=fourstory_dds::layout(desc.Format);
    if(!desc.Width||!desc.Height||desc.Width>16384||desc.Height>16384||pixels.format==D3DFMT_UNKNOWN)return D3DXERR_INVALIDDATA;
    UINT rows=pixels.block?(desc.Height+3)/4:desc.Height,pitch=pixels.block?((desc.Width+3)/4)*pixels.block:desc.Width*pixels.bytes;
    try{
        // Snapshot before locking destination supports overlap and identical surfaces.
        std::vector<BYTE> snapshot(std::size_t(rows)*pitch);D3DLOCKED_RECT lock;hr=source->LockRect(&lock,nullptr,D3DLOCK_READONLY);if(FAILED(hr))return hr;
        if(!lock.pBits||lock.Pitch<int(pitch)){source->UnlockRect();return D3DERR_INVALIDCALL;}
        for(UINT y=0;y<rows;++y)std::memcpy(snapshot.data()+std::size_t(y)*pitch,static_cast<const BYTE*>(lock.pBits)+std::size_t(y)*lock.Pitch,pitch);
        hr=source->UnlockRect();if(FAILED(hr))return hr;
        return fourstory_dds::loadSurface(destination,destinationRect,snapshot.data(),pitch,desc.Width,desc.Height,pixels,sourceRect);
    }catch(const std::bad_alloc&){return E_OUTOFMEMORY;}
}
inline HRESULT D3DXCreateTexture(IDirect3DDevice9* device,UINT width,UINT height,UINT mips,DWORD usage,D3DFORMAT format,D3DPOOL pool,IDirect3DTexture9** texture){
    if(!texture)return D3DERR_INVALIDCALL;*texture=nullptr;if(!device)return D3DERR_INVALIDCALL;
    if(width==D3DX_DEFAULT&&height==D3DX_DEFAULT)width=height=256;
    else if(width==D3DX_DEFAULT)width=height;else if(height==D3DX_DEFAULT)height=width;
    if(!width)width=1;if(!height)height=1;if(width>16384||height>16384)return D3DERR_INVALIDCALL;
    if(format==D3DFMT_UNKNOWN)format=D3DFMT_A8R8G8B8;
    UINT full=1;for(UINT side=std::max(width,height);side>1;side>>=1)++full;
    mips=!mips||mips==D3DX_DEFAULT?full:std::min(mips,full);
    HRESULT hr=device->CreateTexture(width,height,mips,usage,format,pool,texture,nullptr);
    if(FAILED(hr)){if(*texture)(*texture)->Release();*texture=nullptr;return hr;}
    return *texture?D3D_OK:D3DERR_INVALIDCALL;
}

#ifndef D3DXERR_INVALIDDATA
#define D3DXERR_INVALIDDATA ((HRESULT)0x88760B59L)
#endif
// File form of the loader (C4, TClientWnd.cpp): reads the file (Windows path
// rules of native_path) and decodes it with the in-memory loader above. The
// output is null on failure.
inline HRESULT D3DXCreateTextureFromFile(IDirect3DDevice9* device,const char* file,IDirect3DTexture9** texture){
    if(!texture)return D3DERR_INVALIDCALL;
    *texture=nullptr;
    if(!device||!file)return D3DERR_INVALIDCALL;
    FILE* f=std::fopen(fourstory::compat::native_path(file).c_str(),"rb");
    if(!f)return D3DXERR_INVALIDDATA;
    std::vector<unsigned char> data;unsigned char block[65536];size_t n;
    while((n=std::fread(block,1,sizeof(block),f))>0)data.insert(data.end(),block,block+n);
    std::fclose(f);
    if(data.empty())return D3DXERR_INVALIDDATA;
    // D3DX defaults filter and build a full mip chain; this subset has no
    // resampling or mip generation, so the file's levels load unfiltered
    // (difference to Windows: no generated mips; NOT VERIFIED visually).
    return D3DXCreateTextureFromFileInMemoryEx(device,data.data(),static_cast<UINT>(data.size()),D3DX_DEFAULT,D3DX_DEFAULT,D3DX_FROM_FILE,0,D3DFMT_UNKNOWN,D3DPOOL_MANAGED,D3DX_FILTER_NONE,D3DX_FILTER_NONE,0,nullptr,nullptr,texture);
}

// Save paths are outside the loader subset. Failure is explicit and outputs clear.
struct ID3DXBuffer : IUnknown {
    virtual void* STDMETHODCALLTYPE GetBufferPointer()=0;
    virtual DWORD STDMETHODCALLTYPE GetBufferSize()=0;
};
using LPD3DXBUFFER=ID3DXBuffer*;
// Shader assembly (C4). The native build has no D3DX assembler: the source
// bytes are looked up by SHA-256 in the bytecode that the original
// D3DX9_43.dll produced on Windows (native/client/reference/shaders). Unknown
// sources fail with D3DXERR_INVALIDDATA and an error text; nothing is guessed.
// Implementation: native/client/app/src/d3dx_native.cpp.
struct D3DXMACRO{const char* Name;const char* Definition;};
using LPD3DXMACRO=D3DXMACRO*;
struct ID3DXInclude;
using LPD3DXINCLUDE=ID3DXInclude*;
#ifndef D3DXERR_INVALIDDATA
#define D3DXERR_INVALIDDATA ((HRESULT)0x88760B59L)
#endif
HRESULT D3DXAssembleShader(const char* source,UINT size,const D3DXMACRO* defines,LPD3DXINCLUDE include,DWORD flags,LPD3DXBUFFER* shader,LPD3DXBUFFER* errors);
HRESULT D3DXCreateBuffer(DWORD size,LPD3DXBUFFER* buffer);
inline HRESULT D3DXSaveTextureToFile(const char*,D3DXIMAGE_FILEFORMAT,IDirect3DBaseTexture9*,const PALETTEENTRY*){return D3DERR_NOTAVAILABLE;}
inline HRESULT D3DXSaveSurfaceToFile(const char*,D3DXIMAGE_FILEFORMAT,IDirect3DSurface9*,const PALETTEENTRY*,const RECT*){return D3DERR_NOTAVAILABLE;}
inline HRESULT D3DXSaveSurfaceToFileInMemory(ID3DXBuffer** out,D3DXIMAGE_FILEFORMAT,IDirect3DSurface9*,const PALETTEENTRY*,const RECT*){if(!out)return D3DERR_INVALIDCALL;*out=nullptr;return D3DERR_NOTAVAILABLE;}
