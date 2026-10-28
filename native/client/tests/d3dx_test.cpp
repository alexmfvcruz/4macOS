#include <d3dx9.h>
#include <cassert>
#include <iostream>
static bool near(float a,float b){return std::fabs(a-b)<1e-5f;}
int main(){
    D3DXMATRIX scale,rotate,translate;
    D3DXMatrixScaling(&scale,2,3,4);D3DXMatrixRotationZ(&rotate,D3DX_PI/2);D3DXMatrixTranslation(&translate,5,6,7);
    D3DXMATRIX matrix=scale*rotate*translate;D3DXVECTOR3 in(1,2,3),out;
    D3DXVec3TransformCoord(&out,&in,&matrix);assert(near(out.x,-1)&&near(out.y,8)&&near(out.z,19));
    D3DXVec3TransformCoord(&in,&in,&matrix);assert(in==out);
    D3DXMATRIX inverse;float det;assert(D3DXMatrixInverse(&inverse,&det,&matrix));assert(near(det,24));
    D3DXMATRIX identity=matrix*inverse;for(int i=0;i<4;++i)for(int j=0;j<4;++j)assert(near(identity.m[i][j],i==j?1.f:0.f));
    D3DXMATRIX alias=matrix;D3DXMatrixInverse(&alias,nullptr,&alias);for(int i=0;i<4;++i)for(int j=0;j<4;++j)assert(near(alias.m[i][j],inverse.m[i][j]));
    D3DXMATRIX singular;D3DXMatrixScaling(&singular,1,0,1);assert(!D3DXMatrixInverse(&inverse,&det,&singular)&&det==0);
    D3DXVECTOR3 axis(0,0,2);D3DXMATRIX arbitrary;D3DXMatrixRotationAxis(&arbitrary,&axis,D3DX_PI/2);
    for(int i=0;i<4;++i)for(int j=0;j<4;++j)assert(near(arbitrary.m[i][j],rotate.m[i][j]));
    D3DXVECTOR3 a(0,0,0),b(1,0,0),c(0,1,0),p(0,0,1),direction(0,0,-1);D3DXPLANE plane;
    D3DXPlaneFromPoints(&plane,&a,&b,&c);assert(near(plane.c,1)&&near(plane.d,0));
    assert(D3DXPlaneIntersectLine(&out,&plane,&p,&a)&&out==a);assert(!D3DXPlaneIntersectLine(&out,&plane,&a,&b));
    D3DXVECTOR3 origin(.25f,.25f,1);float u,v,d;assert(D3DXIntersectTri(&a,&b,&c,&origin,&direction,&u,&v,&d));assert(near(u,.25f)&&near(v,.25f)&&near(d,1));
    origin.x=2;assert(!D3DXIntersectTri(&a,&b,&c,&origin,&direction,&u,&v,&d));
    D3DXQUATERNION q(0,0,0,1),r(0,0,1,0),s;D3DXQuaternionSlerp(&s,&q,&r,.5f);assert(near(s.z,std::sqrt(.5f))&&near(s.w,std::sqrt(.5f)));
    D3DXQuaternionInverse(&s,&r);D3DXQUATERNION product=r*s;assert(near(product.w,1)&&near(product.z,0));
    D3DXQuaternionLn(&s,&q);assert(s.x==0&&s.y==0&&s.z==0&&s.w==0);
    D3DXVECTOR3 decodedScale,decodedTranslation;D3DXMatrixDecompose(&decodedScale,&s,&decodedTranslation,&matrix);
    assert(near(decodedScale.x,2)&&near(decodedScale.y,3)&&near(decodedScale.z,4));assert(decodedTranslation==D3DXVECTOR3(5,6,7));
    assert(near(std::fabs(s.z),std::sqrt(.5f))&&near(std::fabs(s.w),std::sqrt(.5f)));
    std::cout<<"PASS D3DX analytic transforms, aliasing, geometry and quaternion tests\n";
}
