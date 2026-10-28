// Scalar BC1/BC2 encoder adapted from Microsoft DirectXTex BC.cpp and BC.h.
// Copyright (c) Microsoft Corporation. Licensed under the MIT License.
// Upstream commit: 1acf4ebe6330e06b7eb52e8db63d4f824ad77427
// https://github.com/microsoft/DirectXTex/tree/1acf4ebe6330e06b7eb52e8db63d4f824ad77427
// See ../third_party/DirectXTex-LICENSE.txt and DirectXTex-NOTICE.md.
// Adaptations: scalar types/SAL removal, header linkage, RGBA-byte wrapper;
// optimizer, quantization, color-key threshold and channel weights retained.
#pragma once
#include <cstdint>
#include <cstddef>
#include <cassert>
#include <cfloat>
#include <cstring>
namespace fourstory_bc {
constexpr size_t NUM_PIXELS_PER_BLOCK = 16;
struct HDRColorA {float r,g,b,a;HDRColorA()=default;HDRColorA(float r_,float g_,float b_,float a_):r(r_),g(g_),b(b_),a(a_){} };
struct D3DX_BC1 {uint16_t rgb[2];uint32_t bitmap;};
static const HDRColorA g_Luminance(0.2125f/0.7154f,1.f,0.0721f/0.7154f,1.f);
static const HDRColorA g_LuminanceInv(0.7154f/0.2125f,1.f,0.7154f/0.0721f,1.f);
enum {BC_FLAGS_DITHER_RGB=0x10000, BC_FLAGS_DITHER_A=0x20000, BC_FLAGS_UNIFORM=0x40000};
static HDRColorA* HDRColorALerp(HDRColorA* o,const HDRColorA* a,const HDRColorA* b,float s){o->r=a->r+s*(b->r-a->r);o->g=a->g+s*(b->g-a->g);o->b=a->b+s*(b->b-a->b);o->a=a->a+s*(b->a-a->a);return o;}
    inline void Decode565( HDRColorA* pColor,  const uint16_t w565) noexcept
    {
        pColor->r = static_cast<float>((w565 >> 11) & 31) * (1.0f / 31.0f);
        pColor->g = static_cast<float>((w565 >> 5) & 63) * (1.0f / 63.0f);
        pColor->b = static_cast<float>((w565 >> 0) & 31) * (1.0f / 31.0f);
        pColor->a = 1.0f;
    }
    inline uint16_t Encode565( const HDRColorA* pColor) noexcept
    {
        HDRColorA Color;

        Color.r = (pColor->r < 0.0f) ? 0.0f : (pColor->r > 1.0f) ? 1.0f : pColor->r;
        Color.g = (pColor->g < 0.0f) ? 0.0f : (pColor->g > 1.0f) ? 1.0f : pColor->g;
        Color.b = (pColor->b < 0.0f) ? 0.0f : (pColor->b > 1.0f) ? 1.0f : pColor->b;
        Color.a = pColor->a;

        uint16_t w;

        w = static_cast<uint16_t>((static_cast<int32_t>(Color.r * 31.0f + 0.5f) << 11) | (static_cast<int32_t>(Color.g * 63.0f + 0.5f) << 5)
                                  | (static_cast<int32_t>(Color.b * 31.0f + 0.5f) << 0));

        return w;
    }
    inline void OptimizeRGB( HDRColorA*                     pX,
         HDRColorA*                                  pY,
         const HDRColorA* pPoints,
        uint32_t                                          cSteps,
        uint32_t                                          flags) noexcept
    {
        constexpr float    fEpsilon = (0.25f / 64.0f) * (0.25f / 64.0f);
        static const float pC3[]    = { 2.0f / 2.0f, 1.0f / 2.0f, 0.0f / 2.0f };
        static const float pD3[]    = { 0.0f / 2.0f, 1.0f / 2.0f, 2.0f / 2.0f };
        static const float pC4[]    = { 3.0f / 3.0f, 2.0f / 3.0f, 1.0f / 3.0f, 0.0f / 3.0f };
        static const float pD4[]    = { 0.0f / 3.0f, 1.0f / 3.0f, 2.0f / 3.0f, 3.0f / 3.0f };

        const float* pC = (3 == cSteps) ? pC3 : pC4;
        const float* pD = (3 == cSteps) ? pD3 : pD4;

        // Find Min and Max points, as starting point
        HDRColorA X = (flags & BC_FLAGS_UNIFORM) ? HDRColorA(1.f, 1.f, 1.f, 1.f) : g_Luminance;
        HDRColorA Y = HDRColorA(0.0f, 0.0f, 0.0f, 1.0f);

        for (size_t iPoint = 0; iPoint < NUM_PIXELS_PER_BLOCK; iPoint++)
        {
            {
                if (pPoints[iPoint].r < X.r)
                    X.r = pPoints[iPoint].r;

                if (pPoints[iPoint].g < X.g)
                    X.g = pPoints[iPoint].g;

                if (pPoints[iPoint].b < X.b)
                    X.b = pPoints[iPoint].b;

                if (pPoints[iPoint].r > Y.r)
                    Y.r = pPoints[iPoint].r;

                if (pPoints[iPoint].g > Y.g)
                    Y.g = pPoints[iPoint].g;

                if (pPoints[iPoint].b > Y.b)
                    Y.b = pPoints[iPoint].b;
            }
        }

        // Diagonal axis
        const HDRColorA AB(Y.r - X.r, Y.g - X.g, Y.b - X.b, 0.0f);

        const float fAB = AB.r * AB.r + AB.g * AB.g + AB.b * AB.b;

        // Single color block.. no need to root-find
        if (fAB < FLT_MIN)
        {
            pX->r = X.r;
            pX->g = X.g;
            pX->b = X.b;
            pX->a = 1.0f;
            pY->r = Y.r;
            pY->g = Y.g;
            pY->b = Y.b;
            pY->a = 1.0f;
            return;
        }

        // Try all four axis directions, to determine which diagonal best fits data
        const float fABInv = 1.0f / fAB;

        HDRColorA Dir(AB.r * fABInv, AB.g * fABInv, AB.b * fABInv, 0.0f);

        const HDRColorA Mid((X.r + Y.r) * 0.5f, (X.g + Y.g) * 0.5f, (X.b + Y.b) * 0.5f, 0.0f);

        float fDir[4] = {};

        for (size_t iPoint = 0; iPoint < NUM_PIXELS_PER_BLOCK; iPoint++)
        {
            HDRColorA Pt;
            Pt.r = (pPoints[iPoint].r - Mid.r) * Dir.r;
            Pt.g = (pPoints[iPoint].g - Mid.g) * Dir.g;
            Pt.b = (pPoints[iPoint].b - Mid.b) * Dir.b;
            Pt.a = 0.0f;

            float f;

            f = Pt.r + Pt.g + Pt.b;
            fDir[0] += f * f;

            f = Pt.r + Pt.g - Pt.b;
            fDir[1] += f * f;

            f = Pt.r - Pt.g + Pt.b;
            fDir[2] += f * f;

            f = Pt.r - Pt.g - Pt.b;
            fDir[3] += f * f;
        }

        float  fDirMax = fDir[0];
        size_t iDirMax = 0;

        for (size_t iDir = 1; iDir < 4; iDir++)
        {
            if (fDir[iDir] > fDirMax)
            {
                fDirMax = fDir[iDir];
                iDirMax = iDir;
            }
        }

        if (iDirMax & 2)
        {
            const float f = X.g;
            X.g           = Y.g;
            Y.g           = f;
        }

        if (iDirMax & 1)
        {
            const float f = X.b;
            X.b           = Y.b;
            Y.b           = f;
        }

        // Two color block.. no need to root-find
        if (fAB < 1.0f / 4096.0f)
        {
            pX->r = X.r;
            pX->g = X.g;
            pX->b = X.b;
            pX->a = 1.0f;
            pY->r = Y.r;
            pY->g = Y.g;
            pY->b = Y.b;
            pY->a = 1.0f;
            return;
        }

        // Use Newton's Method to find local minima of sum-of-squares error.
        const auto fSteps = static_cast<float>(cSteps - 1);

        for (size_t iIteration = 0; iIteration < 8; iIteration++)
        {
            // Calculate new steps
            HDRColorA pSteps[4];

            for (size_t iStep = 0; iStep < cSteps; iStep++)
            {
                pSteps[iStep].r = X.r * pC[iStep] + Y.r * pD[iStep];
                pSteps[iStep].g = X.g * pC[iStep] + Y.g * pD[iStep];
                pSteps[iStep].b = X.b * pC[iStep] + Y.b * pD[iStep];
                pSteps[iStep].a = 1.0f;
            }

            // Calculate color direction
            Dir.r = Y.r - X.r;
            Dir.g = Y.g - X.g;
            Dir.b = Y.b - X.b;

            const float fLen = (Dir.r * Dir.r + Dir.g * Dir.g + Dir.b * Dir.b);

            if (fLen < (1.0f / 4096.0f))
                break;

            const float fScale = fSteps / fLen;

            Dir.r *= fScale;
            Dir.g *= fScale;
            Dir.b *= fScale;

            // Evaluate function, and derivatives
            float     d2X = 0.f;
            float     d2Y = 0.f;
            HDRColorA dX  = {};
            HDRColorA dY  = {};

            for (size_t iPoint = 0; iPoint < NUM_PIXELS_PER_BLOCK; iPoint++)
            {
                const float fDot
                    = (pPoints[iPoint].r - X.r) * Dir.r + (pPoints[iPoint].g - X.g) * Dir.g + (pPoints[iPoint].b - X.b) * Dir.b;

                uint32_t iStep;
                if (fDot <= 0.0f)
                    iStep = 0;
                else if (fDot >= fSteps)
                    iStep = cSteps - 1;
                else
                    iStep = uint32_t(fDot + 0.5f);

                HDRColorA Diff;
                Diff.r = pSteps[iStep].r - pPoints[iPoint].r;
                Diff.g = pSteps[iStep].g - pPoints[iPoint].g;
                Diff.b = pSteps[iStep].b - pPoints[iPoint].b;
                Diff.a = 0.0f;

                const float fC = pC[iStep] * (1.0f / 8.0f);
                const float fD = pD[iStep] * (1.0f / 8.0f);

                d2X += fC * pC[iStep];
                dX.r += fC * Diff.r;
                dX.g += fC * Diff.g;
                dX.b += fC * Diff.b;

                d2Y += fD * pD[iStep];
                dY.r += fD * Diff.r;
                dY.g += fD * Diff.g;
                dY.b += fD * Diff.b;
            }

            // Move endpoints
            if (d2X > 0.0f)
            {
                const float f = -1.0f / d2X;

                X.r += dX.r * f;
                X.g += dX.g * f;
                X.b += dX.b * f;
            }

            if (d2Y > 0.0f)
            {
                const float f = -1.0f / d2Y;

                Y.r += dY.r * f;
                Y.g += dY.g * f;
                Y.b += dY.b * f;
            }

            if ((dX.r * dX.r < fEpsilon) && (dX.g * dX.g < fEpsilon) && (dX.b * dX.b < fEpsilon) && (dY.r * dY.r < fEpsilon)
                && (dY.g * dY.g < fEpsilon) && (dY.b * dY.b < fEpsilon))
            {
                break;
            }
        }

        pX->r = X.r;
        pX->g = X.g;
        pX->b = X.b;
        pX->a = 1.0f;
        pY->r = Y.r;
        pY->g = Y.g;
        pY->b = Y.b;
        pY->a = 1.0f;
    }
    inline void EncodeBC1( D3DX_BC1*                        pBC,
         const HDRColorA* pColor,
        bool                                              bColorKey,
        float                                             threshold,
        uint32_t                                          flags) noexcept
    {
        assert(pBC && pColor);
        static_assert(sizeof(D3DX_BC1) == 8, "D3DX_BC1 should be 8 bytes");

        // Determine if we need to colorkey this block
        uint32_t uSteps;

        if (bColorKey)
        {
            size_t uColorKey = 0;

            for (size_t i = 0; i < NUM_PIXELS_PER_BLOCK; ++i)
            {
                if (pColor[i].a < threshold)
                    uColorKey++;
            }

            if (NUM_PIXELS_PER_BLOCK == uColorKey)
            {
                pBC->rgb[0] = 0x0000;
                pBC->rgb[1] = 0xffff;
                pBC->bitmap = 0xffffffff;
                return;
            }

            uSteps = (uColorKey > 0) ? 3u : 4u;
        }
        else
        {
            uSteps = 4u;
        }

        // Quantize block to R56B5, using Floyd Stienberg error diffusion.  This
        // increases the chance that colors will map directly to the quantized
        // axis endpoints.
        HDRColorA Color[NUM_PIXELS_PER_BLOCK];
        HDRColorA Error[NUM_PIXELS_PER_BLOCK];

        if (flags & BC_FLAGS_DITHER_RGB)
            std::memset(Error, 0x00, NUM_PIXELS_PER_BLOCK * sizeof(HDRColorA));

        size_t i;
        for (i = 0; i < NUM_PIXELS_PER_BLOCK; ++i)
        {
            HDRColorA Clr;
            Clr.r = pColor[i].r;
            Clr.g = pColor[i].g;
            Clr.b = pColor[i].b;
            Clr.a = 1.0f;

            if (flags & BC_FLAGS_DITHER_RGB)
            {
                Clr.r += Error[i].r;
                Clr.g += Error[i].g;
                Clr.b += Error[i].b;
            }

            Color[i].r = static_cast<float>(static_cast<int32_t>(Clr.r * 31.0f + 0.5f)) * (1.0f / 31.0f);
            Color[i].g = static_cast<float>(static_cast<int32_t>(Clr.g * 63.0f + 0.5f)) * (1.0f / 63.0f);
            Color[i].b = static_cast<float>(static_cast<int32_t>(Clr.b * 31.0f + 0.5f)) * (1.0f / 31.0f);

            Color[i].a = 1.0f;

            if (flags & BC_FLAGS_DITHER_RGB)
            {
                HDRColorA Diff;
                Diff.r = Color[i].a * (Clr.r - Color[i].r);
                Diff.g = Color[i].a * (Clr.g - Color[i].g);
                Diff.b = Color[i].a * (Clr.b - Color[i].b);
                Diff.a = 0.0f;

                if (3 != (i & 3))
                {
                    assert(i < 15);
                    assert(i < 15);
                    Error[i + 1].r += Diff.r * (7.0f / 16.0f);
                    Error[i + 1].g += Diff.g * (7.0f / 16.0f);
                    Error[i + 1].b += Diff.b * (7.0f / 16.0f);
                }

                if (i < 12)
                {
                    if (i & 3)
                    {
                        Error[i + 3].r += Diff.r * (3.0f / 16.0f);
                        Error[i + 3].g += Diff.g * (3.0f / 16.0f);
                        Error[i + 3].b += Diff.b * (3.0f / 16.0f);
                    }

                    Error[i + 4].r += Diff.r * (5.0f / 16.0f);
                    Error[i + 4].g += Diff.g * (5.0f / 16.0f);
                    Error[i + 4].b += Diff.b * (5.0f / 16.0f);

                    if (3 != (i & 3))
                    {
                        assert(i < 11);
                        assert(i < 11);
                        Error[i + 5].r += Diff.r * (1.0f / 16.0f);
                        Error[i + 5].g += Diff.g * (1.0f / 16.0f);
                        Error[i + 5].b += Diff.b * (1.0f / 16.0f);
                    }
                }
            }

            if (!(flags & BC_FLAGS_UNIFORM))
            {
                Color[i].r *= g_Luminance.r;
                Color[i].g *= g_Luminance.g;
                Color[i].b *= g_Luminance.b;
            }
        }

        // Perform 6D root finding function to find two endpoints of color axis.
        // Then quantize and sort the endpoints depending on mode.
        HDRColorA ColorA, ColorB, ColorC, ColorD;

        OptimizeRGB(&ColorA, &ColorB, Color, uSteps, flags);

        if (flags & BC_FLAGS_UNIFORM)
        {
            ColorC = ColorA;
            ColorD = ColorB;
        }
        else
        {
            ColorC.r = ColorA.r * g_LuminanceInv.r;
            ColorC.g = ColorA.g * g_LuminanceInv.g;
            ColorC.b = ColorA.b * g_LuminanceInv.b;
            ColorC.a = ColorA.a;

            ColorD.r = ColorB.r * g_LuminanceInv.r;
            ColorD.g = ColorB.g * g_LuminanceInv.g;
            ColorD.b = ColorB.b * g_LuminanceInv.b;
            ColorD.a = ColorB.a;
        }

        const uint16_t wColorA = Encode565(&ColorC);
        const uint16_t wColorB = Encode565(&ColorD);

        if ((uSteps == 4) && (wColorA == wColorB))
        {
            pBC->rgb[0] = wColorA;
            pBC->rgb[1] = wColorB;
            pBC->bitmap = 0x00000000;
            return;
        }

        Decode565(&ColorC, wColorA);
        Decode565(&ColorD, wColorB);

        if (flags & BC_FLAGS_UNIFORM)
        {
            ColorA = ColorC;
            ColorB = ColorD;
        }
        else
        {
            ColorA.r = ColorC.r * g_Luminance.r;
            ColorA.g = ColorC.g * g_Luminance.g;
            ColorA.b = ColorC.b * g_Luminance.b;

            ColorB.r = ColorD.r * g_Luminance.r;
            ColorB.g = ColorD.g * g_Luminance.g;
            ColorB.b = ColorD.b * g_Luminance.b;
        }

        // Calculate color steps
        HDRColorA Step[4];

        if ((3 == uSteps) == (wColorA <= wColorB))
        {
            pBC->rgb[0] = wColorA;
            pBC->rgb[1] = wColorB;

            Step[0] = ColorA;
            Step[1] = ColorB;
        }
        else
        {
            pBC->rgb[0] = wColorB;
            pBC->rgb[1] = wColorA;

            Step[0] = ColorB;
            Step[1] = ColorA;
        }

        static const size_t pSteps3[] = { 0, 2, 1 };
        static const size_t pSteps4[] = { 0, 2, 3, 1 };
        const size_t*       pSteps;

        if (3 == uSteps)
        {
            pSteps = pSteps3;

            HDRColorALerp(&Step[2], &Step[0], &Step[1], 0.5f);
        }
        else
        {
            pSteps = pSteps4;

            HDRColorALerp(&Step[2], &Step[0], &Step[1], 1.0f / 3.0f);
            HDRColorALerp(&Step[3], &Step[0], &Step[1], 2.0f / 3.0f);
        }

        // Calculate color direction
        HDRColorA Dir;
        Dir.r = Step[1].r - Step[0].r;
        Dir.g = Step[1].g - Step[0].g;
        Dir.b = Step[1].b - Step[0].b;
        Dir.a = 0.0f;

        const auto  fSteps = static_cast<float>(uSteps - 1);
        const float fScale = (wColorA != wColorB) ? (fSteps / (Dir.r * Dir.r + Dir.g * Dir.g + Dir.b * Dir.b)) : 0.0f;

        Dir.r *= fScale;
        Dir.g *= fScale;
        Dir.b *= fScale;

        // Encode colors
        uint32_t dw = 0;
        if (flags & BC_FLAGS_DITHER_RGB)
            std::memset(Error, 0x00, NUM_PIXELS_PER_BLOCK * sizeof(HDRColorA));

        for (i = 0; i < NUM_PIXELS_PER_BLOCK; ++i)
        {
            if ((3 == uSteps) && (pColor[i].a < threshold))
            {
                dw = (3u << 30) | (dw >> 2);
            }
            else
            {
                HDRColorA Clr;
                if (flags & BC_FLAGS_UNIFORM)
                {
                    Clr.r = pColor[i].r;
                    Clr.g = pColor[i].g;
                    Clr.b = pColor[i].b;
                }
                else
                {
                    Clr.r = pColor[i].r * g_Luminance.r;
                    Clr.g = pColor[i].g * g_Luminance.g;
                    Clr.b = pColor[i].b * g_Luminance.b;
                }
                Clr.a = 1.0f;

                if (flags & BC_FLAGS_DITHER_RGB)
                {
                    Clr.r += Error[i].r;
                    Clr.g += Error[i].g;
                    Clr.b += Error[i].b;
                }

                const float fDot = (Clr.r - Step[0].r) * Dir.r + (Clr.g - Step[0].g) * Dir.g + (Clr.b - Step[0].b) * Dir.b;

                uint32_t iStep;
                if (fDot <= 0.0f)
                    iStep = 0;
                else if (fDot >= fSteps)
                    iStep = 1;
                else
                    iStep = uint32_t(pSteps[uint32_t(fDot + 0.5f)]);

                dw = (iStep << 30) | (dw >> 2);

                if (flags & BC_FLAGS_DITHER_RGB)
                {
                    HDRColorA Diff;
                    Diff.r = Color[i].a * (Clr.r - Step[iStep].r);
                    Diff.g = Color[i].a * (Clr.g - Step[iStep].g);
                    Diff.b = Color[i].a * (Clr.b - Step[iStep].b);
                    Diff.a = 0.0f;

                    if (3 != (i & 3))
                    {
                        Error[i + 1].r += Diff.r * (7.0f / 16.0f);
                        Error[i + 1].g += Diff.g * (7.0f / 16.0f);
                        Error[i + 1].b += Diff.b * (7.0f / 16.0f);
                    }

                    if (i < 12)
                    {
                        if (i & 3)
                        {
                            Error[i + 3].r += Diff.r * (3.0f / 16.0f);
                            Error[i + 3].g += Diff.g * (3.0f / 16.0f);
                            Error[i + 3].b += Diff.b * (3.0f / 16.0f);
                        }

                        Error[i + 4].r += Diff.r * (5.0f / 16.0f);
                        Error[i + 4].g += Diff.g * (5.0f / 16.0f);
                        Error[i + 4].b += Diff.b * (5.0f / 16.0f);

                        if (3 != (i & 3))
                        {
                            Error[i + 5].r += Diff.r * (1.0f / 16.0f);
                            Error[i + 5].g += Diff.g * (1.0f / 16.0f);
                            Error[i + 5].b += Diff.b * (1.0f / 16.0f);
                        }
                    }
                }
            }
        }

        pBC->bitmap = dw;
    }

// Only the no-dither, perceptually weighted path used by C2 is exposed.
// Float input preserves the precision of compressed-to-compressed edge copies.
inline void encodeColors(const HDRColorA colors[16], bool bc1, uint8_t* output) noexcept
{
    if(!bc1){
        std::memset(output,0,8);
        for(size_t i=0;i<16;++i)
            output[i/2]|=uint8_t(uint32_t(colors[i].a*15.f+.5f)<<((i&1)*4));
        output+=8;
    }
    D3DX_BC1 block{};
    EncodeBC1(&block,colors,bc1,.5f,0);
    std::memcpy(output,&block,sizeof(block));
}
inline void encode(const uint8_t rgba[16][4], bool bc1, uint8_t* output) noexcept
{
    HDRColorA colors[16];
    for(size_t i=0;i<16;++i)
        colors[i]=HDRColorA(rgba[i][0]*(1.f/255.f),rgba[i][1]*(1.f/255.f),
                           rgba[i][2]*(1.f/255.f),rgba[i][3]*(1.f/255.f));
    encodeColors(colors,bc1,output);
}
} // namespace fourstory_bc
