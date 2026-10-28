// Portable: build against original Windows D3DX as well as this replacement.
#define NOMINMAX
#include <d3dx9.h>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
void assertClose(float a,float b){assert(std::fabs(a-b)<2e-5f);}
void record(const char* name,const float* values,unsigned count){
    for(unsigned i=0;i<count;++i){unsigned bits;std::memcpy(&bits,values+i,4);std::printf("math\t%s\t%u\t%08x\n",name,i,bits);}
}
int main(){
    static_assert(sizeof(D3DXVECTOR4)==16&&sizeof(D3DXMATRIXA16)==64&&alignof(D3DXMATRIXA16)==16,"D3DX layouts");
    D3DXQUATERNION a(.1f,.2f,.3f,.4f),b(-.3f,.4f,.1f,.2f),expected;
    D3DXQuaternionMultiply(&expected,&a,&b);a*=b;assertClose(a.x,expected.x);assertClose(a.y,expected.y);assertClose(a.z,expected.z);assertClose(a.w,expected.w);record("quaternion-compound",&a.x,4);
    D3DXMATRIX m;D3DXMatrixPerspectiveFovLH(&m,1.2f,1.5f,.5f,100.f);record("perspective",&m._11,16);
    D3DXVECTOR3 v(0,0,.5f);D3DXVECTOR4 result;D3DXVec3Transform(&result,&v,&m);assertClose(result.z/result.w,0);
    v.z=100;D3DXVec3Transform(&result,&v,&m);assertClose(result.z/result.w,1);
    D3DXMatrixOrthoLH(&m,8,4,.5f,100);record("ortho",&m._11,16);v={4,2,100};D3DXVec3Transform(&result,&v,&m);assertClose(result.x,1);assertClose(result.y,1);assertClose(result.z,1);
    D3DXVECTOR3 eye(2,3,-4),target(5,7,8),up(0,1,0);D3DXMatrixLookAtLH(&m,&eye,&target,&up);record("lookat",&m._11,16);
    D3DXVec3Transform(&result,&eye,&m);assertClose(result.x,0);assertClose(result.y,0);assertClose(result.z,0);D3DXVec3Transform(&result,&target,&m);assertClose(result.x,0);assertClose(result.y,0);assert(result.z>0);
    D3DXMatrixRotationYawPitchRoll(&m,.3f,-.7f,.2f);record("yaw-pitch-roll",&m._11,16);
    D3DXMATRIX x,y,z;D3DXMatrixRotationX(&x,-.7f);D3DXMatrixRotationY(&y,.3f);D3DXMatrixRotationZ(&z,.2f);D3DXMATRIX product=z*x*y;
    for(int i=0;i<4;++i)for(int j=0;j<4;++j)assertClose(m.m[i][j],product.m[i][j]);
    D3DXPLANE plane(1,0,0,-2);D3DXMatrixTranslation(&m,3,0,0);D3DXMatrixInverse(&m,nullptr,&m);D3DXMatrixTranspose(&m,&m);D3DXPlaneTransform(&plane,&plane,&m);assertClose(plane.a,1);assertClose(plane.d,-5);record("plane",&plane.a,4);
    D3DXMATRIXA16 aligned=m;assert((reinterpret_cast<std::size_t>(&aligned)&15)==0);aligned=product;
    std::puts("PASS C13 math/layout/alias cases");
}
