// C4 TGA decoding of the D3DX loader (d3dx9tex.h): image info, decoded pixels
// (B,G,R,A rows, top row first), origins, run-length data and rejected input.
#include <d3dx9.h>
#include <cassert>
#include <cstdio>
#include <vector>

static std::vector<BYTE> header(BYTE type, UINT w, UINT h, BYTE bpp, BYTE descriptor) {
    std::vector<BYTE> d(18, 0);
    d[2] = type; d[12] = BYTE(w); d[13] = BYTE(w >> 8); d[14] = BYTE(h); d[15] = BYTE(h >> 8); d[16] = bpp; d[17] = descriptor;
    return d;
}
static const BYTE* px(const fourstory_dds::Image& im, UINT x, UINT y) { return &im.decoded[(y * im.info.Width + x) * 4]; }

int main() {
    // 2x2, 32-bit, bottom-left origin: file rows are bottom row first.
    auto a = header(2, 2, 2, 32, 8);
    const BYTE bottom[] = {1,2,3,4, 5,6,7,8}, top[] = {9,10,11,12, 13,14,15,16};
    a.insert(a.end(), bottom, bottom + 8); a.insert(a.end(), top, top + 8);
    D3DXIMAGE_INFO info;
    assert(D3DXGetImageInfoFromFileInMemory(a.data(), UINT(a.size()), &info) == D3D_OK);
    assert(info.Width == 2 && info.Height == 2 && info.MipLevels == 1 && info.Format == D3DFMT_A8R8G8B8 && info.ImageFileFormat == D3DXIFF_TGA);
    fourstory_dds::Image im;
    assert(fourstory_dds::parse(a.data(), UINT(a.size()), im) == D3D_OK);
    assert(px(im, 0, 0)[0] == 9 && px(im, 1, 0)[3] == 16 && px(im, 0, 1)[0] == 1 && px(im, 1, 1)[3] == 8);
    // 24-bit, top-left origin (descriptor bit 5): D3DX reports R8G8B8; alpha 255.
    auto b = header(2, 2, 1, 24, 0x20);
    const BYTE row[] = {1,2,3, 4,5,6};
    b.insert(b.end(), row, row + 6);
    fourstory_dds::Image im24;
    assert(fourstory_dds::parse(b.data(), UINT(b.size()), im24) == D3D_OK);
    assert(im24.info.Format == D3DFMT_R8G8B8 && im24.pixels.format == D3DFMT_X8R8G8B8);
    assert(px(im24, 0, 0)[0] == 1 && px(im24, 0, 0)[3] == 255 && px(im24, 1, 0)[2] == 6);
    // Right-to-left origin (bit 4).
    auto r = header(2, 2, 1, 24, 0x30);
    r.insert(r.end(), row, row + 6);
    fourstory_dds::Image imr;
    assert(fourstory_dds::parse(r.data(), UINT(r.size()), imr) == D3D_OK && px(imr, 0, 0)[0] == 4 && px(imr, 1, 0)[0] == 1);
    // Type 10 (RLE): one run of 3 pixels, one raw packet of 1 pixel, 4x1, top-left.
    auto c = header(10, 4, 1, 32, 0x28);
    const BYTE rle[] = {0x82, 10,20,30,40, 0x00, 50,60,70,80};
    c.insert(c.end(), rle, rle + sizeof(rle));
    fourstory_dds::Image imc;
    assert(fourstory_dds::parse(c.data(), UINT(c.size()), imc) == D3D_OK);
    assert(px(imc, 2, 0)[1] == 20 && px(imc, 3, 0)[0] == 50 && px(imc, 3, 0)[3] == 80);
    // Rejected: truncated data, RLE run past the end, colour-mapped and 16-bit images.
    fourstory_dds::Image bad;
    assert(fourstory_dds::parse(a.data(), UINT(a.size() - 1), bad) == D3DXERR_INVALIDDATA);
    auto over = header(10, 2, 1, 32, 0x20); const BYTE big[] = {0x85, 1,2,3,4}; over.insert(over.end(), big, big + 5);
    assert(fourstory_dds::parse(over.data(), UINT(over.size()), bad) == D3DXERR_INVALIDDATA);
    auto cmap = header(1, 1, 1, 8, 0); cmap[1] = 1; cmap.push_back(0);
    assert(fourstory_dds::parse(cmap.data(), UINT(cmap.size()), bad) == D3DXERR_INVALIDDATA);
    auto b16 = header(2, 1, 1, 16, 0); b16.push_back(0); b16.push_back(0);
    assert(fourstory_dds::parse(b16.data(), UINT(b16.size()), bad) == D3DXERR_INVALIDDATA);
    std::puts("PASS C4 TGA decoding: info, origins, RLE, rejected input");
}
