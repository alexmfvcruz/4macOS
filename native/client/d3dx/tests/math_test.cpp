#include <d3dx9.h>
#include <cassert>
#include <cmath>
#include <iostream>
void close(float a,float b){assert(std::fabs(a-b)<2e-5f);}
int main(){
    D3DXVECTOR2 v2(3,4);close(D3DXVec2Length(&v2),5);
    D3DXVECTOR3 a(1,2,3),b(5,6,7);assert(D3DXVec3Lerp(&a,&a,&b,.25f)==&a);close(a.x,2);close(a.y,3);close(a.z,4);
    D3DXVec3Lerp(&b,&a,&b,2);close(b.x,8);close(b.y,9);close(b.z,10);
    D3DXQUATERNION q;assert(D3DXQuaternionIdentity(&q)==&q);close(q.w,1);close(q.x,0);
    D3DXVECTOR3 axis(0,0,7);D3DXQuaternionRotationAxis(&q,&axis,D3DX_PI/2);close(q.z,std::sqrt(.5f));close(q.w,std::sqrt(.5f));
    D3DXMATRIX matrix;D3DXMatrixRotationQuaternion(&matrix,&q);D3DXVECTOR3 point(1,0,0),out;D3DXVec3TransformCoord(&out,&point,&matrix);close(out.x,0);close(out.y,1);close(out.z,0);
    for(int i=0;i<4;++i)for(int j=0;j<4;++j)matrix.m[i][j]=float(i*4+j);
    D3DXMatrixTranspose(&matrix,&matrix);close(matrix.m[1][3],13);close(matrix.m[3][1],7);
    D3DXMatrixTransformation(&matrix,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr);for(int i=0;i<4;++i)for(int j=0;j<4;++j)close(matrix.m[i][j],i==j?1.f:0.f);
    D3DXVECTOR3 sc(2,3,0),scale(2,3,4),rc(1,1,0),translation(5,6,7),input(3,4,1);
    D3DXMatrixTransformation(&matrix,&sc,nullptr,&scale,&rc,&q,&translation);D3DXVec3TransformCoord(&out,&input,&matrix);
    // Independent operations: scale about(2,3,0) ->(4,6,4), rotate90 about(1,1,0) ->(-4,4,4), translate ->(1,10,11).
    close(out.x,1);close(out.y,10);close(out.z,11);
    D3DXMatrixTransformation(&matrix,nullptr,&q,&scale,nullptr,nullptr,nullptr);D3DXVec3TransformCoord(&out,&input,&matrix);
    // A rotated scaling frame exchanges x/y scale factors.
    close(out.x,9);close(out.y,8);close(out.z,4);
    D3DXVECTOR3 center(0,0,0),origin(-3,0,0),direction(2,0,0);assert(D3DXSphereBoundProbe(&center,1,&origin,&direction));
    direction.x=-2;assert(!D3DXSphereBoundProbe(&center,1,&origin,&direction));origin={0,0,0};assert(D3DXSphereBoundProbe(&center,1,&origin,&direction));
    origin={-3,1,0};direction={2,0,0};assert(D3DXSphereBoundProbe(&center,1,&origin,&direction));origin.y=1.01f;assert(!D3DXSphereBoundProbe(&center,1,&origin,&direction));
    direction={0,0,0};assert(!D3DXSphereBoundProbe(&center,1,&origin,&direction));
    std::cout<<"PASS eight D3DX math functions, aliases, centers, scaling frame and ray cases\n";
}
