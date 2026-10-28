#import <ImageIO/ImageIO.h>
#import <CoreGraphics/CoreGraphics.h>
#include "artwork.h"
#include <cstdlib>
extern "C" uint8_t* fourstory_account_image(const uint8_t* bytes,size_t count,size_t* width,size_t* height) {
    if(!bytes||!count||!width||!height)return nullptr;
    *width=*height=0;
    CFDataRef data=CFDataCreate(kCFAllocatorDefault,bytes,count);
    if(!data)return nullptr;
    CGImageSourceRef source=CGImageSourceCreateWithData(data,nullptr);CFRelease(data);
    if(!source)return nullptr;
    CGImageRef image=CGImageSourceCreateImageAtIndex(source,0,nullptr);CFRelease(source);
    if(!image)return nullptr;
    size_t w=CGImageGetWidth(image),h=CGImageGetHeight(image);
    if(!w||!h||w>4096||h>4096){CGImageRelease(image);return nullptr;}
    auto* pixels=(uint8_t*)calloc(w*h,4);
    CGColorSpaceRef color=CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    CGContextRef context=pixels?CGBitmapContextCreate(pixels,w,h,8,w*4,color,kCGBitmapByteOrder32Little|kCGImageAlphaPremultipliedFirst):nullptr;
    CGColorSpaceRelease(color);
    if(!context){free(pixels);CGImageRelease(image);return nullptr;}
    CGContextDrawImage(context,CGRectMake(0,0,w,h),image);
    CGContextRelease(context);CGImageRelease(image);*width=w;*height=h;return pixels;
}
extern "C" void fourstory_account_image_free(uint8_t* pixels){free(pixels);}
