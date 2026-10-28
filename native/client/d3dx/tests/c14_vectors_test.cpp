// Portable Q-C14 probe; also buildable against the original Windows SDK/D3DX.
#define NOMINMAX
#include <d3dx9.h>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
static unsigned bits(float f){unsigned out;std::memcpy(&out,&f,sizeof(out));return out;}
static void record(const char* name,const D3DXVECTOR2& v){std::printf("%s\t%08x\t%08x\n",name,bits(v.x),bits(v.y));}
int main(){
    D3DXVECTOR2 a(3,4),b(-2,5),normal;
    assert(D3DXVec2Dot(&a,&b)==14);assert(D3DXVec2LengthSq(&a)==25);
    D3DXVECTOR3 v(2,-3,6),order(4096,1,1);
    assert(D3DXVec3LengthSq(&v)==49);assert(D3DXVec3LengthSq(&order)==16777216.f);
    assert(D3DXVec2Normalize(&normal,&a)==&normal);
    assert(std::fabs(normal.x-.6f)<1e-7f&&std::fabs(normal.y-.8f)<1e-7f);
    record("normalize-3-4",normal);
    D3DXVECTOR2 alias=a;assert(D3DXVec2Normalize(&alias,&alias)==&alias);
    assert(bits(alias.x)==bits(normal.x)&&bits(alias.y)==bits(normal.y));
    D3DXVECTOR2 zero(0,0);D3DXVec2Normalize(&normal,&zero);assert(normal.x==0&&normal.y==0);record("normalize-zero",normal);
    D3DXVECTOR2 axis(-8,0);D3DXVec2Normalize(&normal,&axis);assert(normal.x==-1&&normal.y==0);record("normalize-axis",normal);
    assert(&(a*=2)==&a&&a.x==6&&a.y==8);
    a=D3DXVECTOR2(5,-7);assert(&(a/=3)==&a);
    // The SDK multiplies by one rounded reciprocal, not two divisions.
    assert(bits(a.x)==0x3fd55556u&&bits(a.y)==0xc0155556u);record("divide-reciprocal",a);
    std::puts("PASS Q-C14 vector math, zero, alias, return references and SDK arithmetic order");
}
