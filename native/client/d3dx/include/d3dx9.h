// D3DX subset for the original data libraries. Row-vector convention.
#pragma once
// C1's math-only build retains its value-only include path. C2/C3 use the
// canonical device header first, as specified by board contract item 11.
#if __has_include(<d3d9.h>)
#include <d3d9.h>
#define FOURSTORY_D3DX_DEVICE_API 1
#else
#include <d3d9types.h>
#endif
#include <cmath>
#include <algorithm>
#include <cstring>
#define D3DX_PI 3.14159265358979323846f
struct D3DXVECTOR2 {
    float x,y;
    D3DXVECTOR2() = default;
    D3DXVECTOR2(float x_,float y_):x(x_),y(y_){}
    explicit D3DXVECTOR2(const float* p):x(p[0]),y(p[1]){}
    D3DXVECTOR2 operator+(const D3DXVECTOR2& v)const{return {x+v.x,y+v.y};}
    D3DXVECTOR2 operator-(const D3DXVECTOR2& v)const{return {x-v.x,y-v.y};}
    D3DXVECTOR2 operator*(float f)const{return {x*f,y*f};}
    D3DXVECTOR2 operator/(float f)const{return {x/f,y/f};}
    D3DXVECTOR2& operator+=(const D3DXVECTOR2& v){x+=v.x;y+=v.y;return *this;}
    D3DXVECTOR2& operator-=(const D3DXVECTOR2& v){x-=v.x;y-=v.y;return *this;}
    D3DXVECTOR2& operator*=(float f){x*=f;y*=f;return *this;}
    D3DXVECTOR2& operator/=(float f){float inverse=1.f/f;x*=inverse;y*=inverse;return *this;}
    bool operator==(const D3DXVECTOR2& v)const{return x==v.x&&y==v.y;}
    bool operator!=(const D3DXVECTOR2& v)const{return !(*this==v);}
};
inline D3DXVECTOR2 operator*(float f,const D3DXVECTOR2& v){return v*f;}
struct D3DXVECTOR4 {
    float x,y,z,w;
    D3DXVECTOR4()=default;
    D3DXVECTOR4(float a,float b,float c,float d):x(a),y(b),z(c),w(d){}
    explicit D3DXVECTOR4(const float* p):x(p[0]),y(p[1]),z(p[2]),w(p[3]){}
    D3DXVECTOR4 operator*(float f)const{return {x*f,y*f,z*f,w*f};}
};
using LPD3DXVECTOR4=D3DXVECTOR4*;
struct D3DXVECTOR3 : D3DVECTOR {
    D3DXVECTOR3() = default;
    D3DXVECTOR3(float x_,float y_,float z_){x=x_;y=y_;z=z_;}
    explicit D3DXVECTOR3(const float* p){x=p[0];y=p[1];z=p[2];}
    D3DXVECTOR3(const D3DVECTOR& v){x=v.x;y=v.y;z=v.z;}
    D3DXVECTOR3 operator+(const D3DXVECTOR3& v)const{return {x+v.x,y+v.y,z+v.z};}
    D3DXVECTOR3 operator-(const D3DXVECTOR3& v)const{return {x-v.x,y-v.y,z-v.z};}
    D3DXVECTOR3 operator-()const{return {-x,-y,-z};}
    D3DXVECTOR3 operator*(float f)const{return {x*f,y*f,z*f};}
    D3DXVECTOR3 operator/(float f)const{return {x/f,y/f,z/f};}
    D3DXVECTOR3& operator+=(const D3DXVECTOR3& v){x+=v.x;y+=v.y;z+=v.z;return *this;}
    D3DXVECTOR3& operator-=(const D3DXVECTOR3& v){x-=v.x;y-=v.y;z-=v.z;return *this;}
    D3DXVECTOR3& operator*=(float f){x*=f;y*=f;z*=f;return *this;}
    D3DXVECTOR3& operator/=(float f){x/=f;y/=f;z/=f;return *this;}
    bool operator==(const D3DXVECTOR3& v)const{return x==v.x&&y==v.y&&z==v.z;}
    bool operator!=(const D3DXVECTOR3& v)const{return !(*this==v);}
};
inline D3DXVECTOR3 operator*(float f,const D3DXVECTOR3& v){return v*f;}
struct D3DXMATRIX : D3DMATRIX {
    D3DXMATRIX() = default;
    explicit D3DXMATRIX(const float* p){std::memcpy(m,p,64);}
    D3DXMATRIX(float a,float b,float c,float d,float e,float f,float g,float h,float i,float j,float k,float l,float n,float o,float p,float q){float v[]={a,b,c,d,e,f,g,h,i,j,k,l,n,o,p,q};std::memcpy(m,v,64);}
    D3DXMATRIX& operator*=(const D3DXMATRIX& b){D3DXMATRIX r;for(int i=0;i<4;++i)for(int j=0;j<4;++j){r.m[i][j]=0;for(int k=0;k<4;++k)r.m[i][j]+=m[i][k]*b.m[k][j];}*this=r;return *this;}
    D3DXMATRIX& operator+=(const D3DXMATRIX& b){for(int i=0;i<4;++i)for(int j=0;j<4;++j)m[i][j]+=b.m[i][j];return *this;}
    D3DXMATRIX operator*(float f)const{D3DXMATRIX r=*this;for(int i=0;i<4;++i)for(int j=0;j<4;++j)r.m[i][j]*=f;return r;}
    D3DXMATRIX operator*(const D3DXMATRIX& b)const{D3DXMATRIX r=*this;return r*=b;}
    float& operator()(unsigned i,unsigned j){return m[i][j];}
    float operator()(unsigned i,unsigned j)const{return m[i][j];}
};
inline D3DXMATRIX operator*(float f,const D3DXMATRIX& m){return m*f;}
struct alignas(16) D3DXMATRIXA16 : D3DXMATRIX {
    using D3DXMATRIX::D3DXMATRIX;
    D3DXMATRIXA16()=default;
    D3DXMATRIXA16(const D3DXMATRIX& other){std::memcpy(m,other.m,sizeof(m));}
    D3DXMATRIXA16& operator=(const D3DXMATRIX& other){std::memcpy(m,other.m,sizeof(m));return *this;}
};
static_assert(sizeof(D3DXVECTOR4)==16&&sizeof(D3DXMATRIXA16)==64&&alignof(D3DXMATRIXA16)==16,"D3DX extended layouts");

struct D3DXPLANE {
    float a,b,c,d;
    D3DXPLANE() = default;
    D3DXPLANE(float a_,float b_,float c_,float d_):a(a_),b(b_),c(c_),d(d_){}
    D3DXPLANE operator-()const{return {-a,-b,-c,-d};}
};
struct D3DXQUATERNION {
    float x,y,z,w;
    D3DXQUATERNION& operator*=(const D3DXQUATERNION& other);
    D3DXQUATERNION() = default;
    D3DXQUATERNION(float x_,float y_,float z_,float w_):x(x_),y(y_),z(z_),w(w_){}
    D3DXQUATERNION operator+(const D3DXQUATERNION& q)const{return {x+q.x,y+q.y,z+q.z,w+q.w};}
    D3DXQUATERNION operator-(const D3DXQUATERNION& q)const{return {x-q.x,y-q.y,z-q.z,w-q.w};}
    D3DXQUATERNION operator*(float f)const{return {x*f,y*f,z*f,w*f};}
};
using LPD3DXVECTOR2=D3DXVECTOR2*; using LPD3DXVECTOR3=D3DXVECTOR3*;
using LPD3DXMATRIX=D3DXMATRIX*; using LPD3DXPLANE=D3DXPLANE*; using LPD3DXQUATERNION=D3DXQUATERNION*;
static_assert(sizeof(D3DXVECTOR2)==8&&sizeof(D3DXVECTOR3)==12&&sizeof(D3DXMATRIX)==64&&sizeof(D3DXPLANE)==16&&sizeof(D3DXQUATERNION)==16,"D3DX disk layouts");
inline D3DXMATRIX* D3DXMatrixIdentity(D3DXMATRIX* o){*o=D3DXMATRIX(1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1);return o;}
inline D3DXMATRIX* D3DXMatrixScaling(D3DXMATRIX* o,float x,float y,float z){D3DXMatrixIdentity(o);o->_11=x;o->_22=y;o->_33=z;return o;}
inline D3DXMATRIX* D3DXMatrixTranslation(D3DXMATRIX* o,float x,float y,float z){D3DXMatrixIdentity(o);o->_41=x;o->_42=y;o->_43=z;return o;}
inline D3DXMATRIX* D3DXMatrixRotationX(D3DXMATRIX* o,float a){D3DXMatrixIdentity(o);float c=std::cos(a),s=std::sin(a);o->_22=c;o->_23=s;o->_32=-s;o->_33=c;return o;}
inline D3DXMATRIX* D3DXMatrixRotationY(D3DXMATRIX* o,float a){D3DXMatrixIdentity(o);float c=std::cos(a),s=std::sin(a);o->_11=c;o->_13=-s;o->_31=s;o->_33=c;return o;}
inline D3DXMATRIX* D3DXMatrixRotationZ(D3DXMATRIX* o,float a){D3DXMatrixIdentity(o);float c=std::cos(a),s=std::sin(a);o->_11=c;o->_12=s;o->_21=-s;o->_22=c;return o;}
inline float D3DXVec3Dot(const D3DXVECTOR3* a,const D3DXVECTOR3* b){return a->x*b->x+a->y*b->y+a->z*b->z;}
inline float D3DXVec3LengthSq(const D3DXVECTOR3* v){return v->x*v->x+v->y*v->y+v->z*v->z;}
inline float D3DXVec3Length(const D3DXVECTOR3* v){return std::sqrt(D3DXVec3Dot(v,v));}
inline D3DXVECTOR3* D3DXVec3Cross(D3DXVECTOR3* o,const D3DXVECTOR3* a,const D3DXVECTOR3* b){D3DXVECTOR3 r(a->y*b->z-a->z*b->y,a->z*b->x-a->x*b->z,a->x*b->y-a->y*b->x);*o=r;return o;}
// Preserve the component-wise sum order of the original x64 D3DX SSE path.
inline D3DXVECTOR3* D3DXVec3Normalize(D3DXVECTOR3* o,const D3DXVECTOR3* v){
    D3DXVECTOR3 a=*v;float x=a.x*a.x,y=a.y*a.y,z=a.z*a.z;
    auto inverse=[](float n){float r=1.f/std::sqrt(n);return r<3.402823466e38f?r:3.402823466e38f;};
    *o=D3DXVECTOR3(a.x*inverse((x+y)+z),a.y*inverse((y+z)+x),a.z*inverse((z+x)+y));return o;
}
inline D3DXVECTOR3* D3DXVec3TransformCoord(D3DXVECTOR3* o,const D3DXVECTOR3* v,const D3DXMATRIX* m){float x=v->x,y=v->y,z=v->z;float w=x*m->_14+y*m->_24+z*m->_34+m->_44;*o=D3DXVECTOR3((x*m->_11+y*m->_21+z*m->_31+m->_41)/w,(x*m->_12+y*m->_22+z*m->_32+m->_42)/w,(x*m->_13+y*m->_23+z*m->_33+m->_43)/w);return o;}
inline float D3DXPlaneDotCoord(const D3DXPLANE* p,const D3DXVECTOR3* v){return p->a*v->x+p->b*v->y+p->c*v->z+p->d;}
inline float D3DXPlaneDotNormal(const D3DXPLANE* p,const D3DXVECTOR3* v){return p->a*v->x+p->b*v->y+p->c*v->z;}
inline D3DXPLANE* D3DXPlaneNormalize(D3DXPLANE* o,const D3DXPLANE* p){float n=std::sqrt(p->a*p->a+p->b*p->b+p->c*p->c);*o=n?D3DXPLANE(p->a/n,p->b/n,p->c/n,p->d/n):D3DXPLANE(0,0,0,0);return o;}
inline D3DXPLANE* D3DXPlaneFromPointNormal(D3DXPLANE* o,const D3DXVECTOR3* p,const D3DXVECTOR3* n){*o=D3DXPLANE(n->x,n->y,n->z,-D3DXVec3Dot(p,n));return o;}
inline D3DXPLANE* D3DXPlaneFromPoints(D3DXPLANE* o,const D3DXVECTOR3* a,const D3DXVECTOR3* b,const D3DXVECTOR3* c){D3DXVECTOR3 u=*b-*a,v=*c-*a,n;D3DXVec3Cross(&n,&u,&v);D3DXVec3Normalize(&n,&n);return D3DXPlaneFromPointNormal(o,a,&n);}
inline D3DXVECTOR3* D3DXPlaneIntersectLine(D3DXVECTOR3* o,const D3DXPLANE* p,const D3DXVECTOR3* a,const D3DXVECTOR3* b){D3DXVECTOR3 v=*b-*a;float denom=D3DXPlaneDotNormal(p,&v);if(denom==0)return nullptr;*o=*a-v*(D3DXPlaneDotCoord(p,a)/denom);return o;}
inline D3DXMATRIX* D3DXMatrixRotationAxis(D3DXMATRIX* o,const D3DXVECTOR3* axis,float angle){D3DXVECTOR3 n;D3DXVec3Normalize(&n,axis);float c=std::cos(angle),s=std::sin(angle),t=1-c,x=n.x,y=n.y,z=n.z;*o=D3DXMATRIX(t*x*x+c,t*x*y+s*z,t*x*z-s*y,0,t*x*y-s*z,t*y*y+c,t*y*z+s*x,0,t*x*z+s*y,t*y*z-s*x,t*z*z+c,0,0,0,0,1);return o;}
inline D3DXMATRIX* D3DXMatrixInverse(D3DXMATRIX* o,float* determinant,const D3DXMATRIX* in){
    // Pivoted Gauss-Jordan elimination; copy first to support output=input.
    double a[4][8];double det=1;
    for(int i=0;i<4;++i)for(int j=0;j<8;++j)a[i][j]=j<4?in->m[i][j]:(j-4==i?1:0);
    for(int c=0;c<4;++c){int pivot=c;for(int i=c+1;i<4;++i)if(std::fabs(a[i][c])>std::fabs(a[pivot][c]))pivot=i;
        if(a[pivot][c]==0){if(determinant)*determinant=0;return nullptr;}
        if(pivot!=c){for(int j=0;j<8;++j)std::swap(a[c][j],a[pivot][j]);det=-det;}
        double d=a[c][c];det*=d;for(int j=0;j<8;++j)a[c][j]/=d;
        for(int i=0;i<4;++i)if(i!=c){double f=a[i][c];for(int j=0;j<8;++j)a[i][j]-=f*a[c][j];}}
    for(int i=0;i<4;++i)for(int j=0;j<4;++j)o->m[i][j]=float(a[i][j+4]);
    if(determinant)*determinant=float(det);return o;
}
inline D3DXQUATERNION* D3DXQuaternionNormalize(D3DXQUATERNION* o,const D3DXQUATERNION* q){float n=std::sqrt(q->x*q->x+q->y*q->y+q->z*q->z+q->w*q->w);*o=n?*q*(1/n):D3DXQUATERNION(0,0,0,0);return o;}
inline D3DXQUATERNION* D3DXQuaternionInverse(D3DXQUATERNION* o,const D3DXQUATERNION* q){float n=(q->x*q->x+q->y*q->y)+(q->z*q->z+q->w*q->w);*o=D3DXQUATERNION(-q->x/n,-q->y/n,-q->z/n,q->w/n);return o;}
inline D3DXQUATERNION* D3DXQuaternionLn(D3DXQUATERNION* o,const D3DXQUATERNION* q){float n=std::sqrt(q->x*q->x+q->y*q->y+q->z*q->z),f=n?std::atan2(n,q->w)/n:1;*o=D3DXQUATERNION(q->x*f,q->y*f,q->z*f,0);return o;}
inline D3DXQUATERNION* D3DXQuaternionSlerp(D3DXQUATERNION* o,const D3DXQUATERNION* a,const D3DXQUATERNION* b,float t){D3DXQUATERNION v=*b;float dot=a->x*v.x+a->y*v.y+a->z*v.z+a->w*v.w;if(dot<0){v=v*-1;dot=-dot;}float x=1-t,y=t;if(dot<0.9995f){float angle=std::acos(std::min(1.f,dot)),s=std::sin(angle);x=std::sin((1-t)*angle)/s;y=std::sin(t*angle)/s;}*o=*a*x+v*y;return o;}
// D3DX quaternion product returns the composition q2*q1 (row-vector order).
inline D3DXQUATERNION operator*(const D3DXQUATERNION& a,const D3DXQUATERNION& b){return {((b.y*a.z+b.x*a.w)-b.z*a.y)+b.w*a.x,((b.y*a.w-b.x*a.z)+b.z*a.x)+b.w*a.y,((-b.y*a.x+b.x*a.y)+b.z*a.w)+b.w*a.z,((-b.y*a.y-b.x*a.x)-b.z*a.z)+b.w*a.w};}
inline D3DXQUATERNION& D3DXQUATERNION::operator*=(const D3DXQUATERNION& other){*this=*this*other;return *this;}
inline D3DXQUATERNION* D3DXQuaternionMultiply(D3DXQUATERNION* out,const D3DXQUATERNION* a,const D3DXQUATERNION* b){*out=(*a)*(*b);return out;}
inline HRESULT D3DXMatrixDecompose(D3DXVECTOR3* scale,D3DXQUATERNION* rotation,D3DXVECTOR3* translation,const D3DXMATRIX* m){
    D3DXVECTOR3 row[3]={{m->_11,m->_12,m->_13},{m->_21,m->_22,m->_23},{m->_31,m->_32,m->_33}};
    *scale=D3DXVECTOR3(D3DXVec3Length(&row[0]),D3DXVec3Length(&row[1]),D3DXVec3Length(&row[2]));
    *translation=D3DXVECTOR3(m->_41,m->_42,m->_43);
    if(scale->x==0||scale->y==0||scale->z==0)return static_cast<HRESULT>(0x8876086cU);
    D3DXVECTOR3 cross;D3DXVec3Cross(&cross,&row[1],&row[2]);if(D3DXVec3Dot(&row[0],&cross)<0)scale->x=-scale->x;
    row[0]/=scale->x;row[1]/=scale->y;row[2]/=scale->z;
    float trace=row[0].x+row[1].y+row[2].z;D3DXQUATERNION q;
    if(trace>0){float s=std::sqrt(trace+1)*2;q={ (row[1].z-row[2].y)/s,(row[2].x-row[0].z)/s,(row[0].y-row[1].x)/s,s/4};}
    else if(row[0].x>row[1].y&&row[0].x>row[2].z){float s=std::sqrt(1+row[0].x-row[1].y-row[2].z)*2;q={s/4,(row[0].y+row[1].x)/s,(row[0].z+row[2].x)/s,(row[1].z-row[2].y)/s};}
    else if(row[1].y>row[2].z){float s=std::sqrt(1+row[1].y-row[0].x-row[2].z)*2;q={(row[0].y+row[1].x)/s,s/4,(row[1].z+row[2].y)/s,(row[2].x-row[0].z)/s};}
    else {float s=std::sqrt(1+row[2].z-row[0].x-row[1].y)*2;q={(row[0].z+row[2].x)/s,(row[1].z+row[2].y)/s,s/4,(row[0].y-row[1].x)/s};}
    D3DXQuaternionNormalize(rotation,&q);return 0;
}
inline BOOL D3DXIntersectTri(const D3DXVECTOR3* a,const D3DXVECTOR3* b,const D3DXVECTOR3* c,const D3DXVECTOR3* origin,const D3DXVECTOR3* direction,float* u,float* v,float* distance){D3DXVECTOR3 e1=*b-*a,e2=*c-*a,p;D3DXVec3Cross(&p,direction,&e2);float det=D3DXVec3Dot(&e1,&p);if(det==0)return FALSE;D3DXVECTOR3 t=*origin-*a;float uu=D3DXVec3Dot(&t,&p)/det;if(uu<0||uu>1)return FALSE;D3DXVECTOR3 q;D3DXVec3Cross(&q,&t,&e1);float vv=D3DXVec3Dot(direction,&q)/det;if(vv<0||uu+vv>1)return FALSE;float dd=D3DXVec3Dot(&e2,&q)/det;if(dd<0)return FALSE;if(u)*u=uu;if(v)*v=vv;if(distance)*distance=dd;return TRUE;}

inline float D3DXVec2Dot(const D3DXVECTOR2* a,const D3DXVECTOR2* b){return a->x*b->x+a->y*b->y;}
inline float D3DXVec2LengthSq(const D3DXVECTOR2* v){return v->x*v->x+v->y*v->y;}
// Same reciprocal/saturation convention as the canonical x64 Vec3 path above.
inline D3DXVECTOR2* D3DXVec2Normalize(D3DXVECTOR2* out,const D3DXVECTOR2* v){
    D3DXVECTOR2 a=*v;float inverse=1.f/std::sqrt(D3DXVec2LengthSq(&a));
    if(!(inverse<3.402823466e38f))inverse=3.402823466e38f;
    *out=D3DXVECTOR2(a.x*inverse,a.y*inverse);return out;
}
inline float D3DXVec2Length(const D3DXVECTOR2* v){return std::sqrt(v->x*v->x+v->y*v->y);}
inline D3DXVECTOR3* D3DXVec3Lerp(D3DXVECTOR3* out,const D3DXVECTOR3* a,const D3DXVECTOR3* b,float t){D3DXVECTOR3 r=*a+(*b-*a)*t;*out=r;return out;}
inline D3DXQUATERNION* D3DXQuaternionIdentity(D3DXQUATERNION* out){*out=D3DXQUATERNION(0,0,0,1);return out;}
inline D3DXQUATERNION* D3DXQuaternionRotationAxis(D3DXQUATERNION* out,const D3DXVECTOR3* axis,float angle){
    D3DXVECTOR3 n;D3DXVec3Normalize(&n,axis);float s=std::sin(angle*.5f);
    *out=D3DXQUATERNION(n.x*s,n.y*s,n.z*s,std::cos(angle*.5f));return out;
}
inline D3DXMATRIX* D3DXMatrixTranspose(D3DXMATRIX* out,const D3DXMATRIX* in){D3DXMATRIX r;for(int i=0;i<4;++i)for(int j=0;j<4;++j)r.m[i][j]=in->m[j][i];*out=r;return out;}
inline D3DXMATRIX* D3DXMatrixRotationQuaternion(D3DXMATRIX* out,const D3DXQUATERNION* q){
    float x=q->x,y=q->y,z=q->z,w=q->w;
    *out=D3DXMATRIX(1-2*(y*y+z*z),2*(x*y+z*w),2*(x*z-y*w),0,
                    2*(x*y-z*w),1-2*(x*x+z*z),2*(y*z+x*w),0,
                    2*(x*z+y*w),2*(y*z-x*w),1-2*(x*x+y*y),0,0,0,0,1);return out;
}
inline D3DXMATRIX* D3DXMatrixTransformation(D3DXMATRIX* out,const D3DXVECTOR3* scalingCenter,const D3DXQUATERNION* scalingRotation,const D3DXVECTOR3* scaling,const D3DXVECTOR3* rotationCenter,const D3DXQUATERNION* rotation,const D3DXVECTOR3* translation){
    // Row-vector composition: T(-sc)*Rsr^-1*S*Rsr*T(sc)*T(-rc)*R*T(rc)*T(t).
    D3DXMATRIX result,part,scaleRotation;D3DXMatrixIdentity(&result);
    if(scalingCenter){D3DXMatrixTranslation(&part,-scalingCenter->x,-scalingCenter->y,-scalingCenter->z);result*=part;}
    if(scalingRotation){D3DXMatrixRotationQuaternion(&scaleRotation,scalingRotation);D3DXMatrixTranspose(&part,&scaleRotation);result*=part;}
    if(scaling){D3DXMatrixScaling(&part,scaling->x,scaling->y,scaling->z);result*=part;}
    if(scalingRotation)result*=scaleRotation;
    if(scalingCenter){D3DXMatrixTranslation(&part,scalingCenter->x,scalingCenter->y,scalingCenter->z);result*=part;}
    if(rotationCenter){D3DXMatrixTranslation(&part,-rotationCenter->x,-rotationCenter->y,-rotationCenter->z);result*=part;}
    if(rotation){D3DXMatrixRotationQuaternion(&part,rotation);result*=part;}
    if(rotationCenter){D3DXMatrixTranslation(&part,rotationCenter->x,rotationCenter->y,rotationCenter->z);result*=part;}
    if(translation){D3DXMatrixTranslation(&part,translation->x,translation->y,translation->z);result*=part;}
    *out=result;return out;
}
// 2D form (C4, TClientGame.cpp): the 3D transformation in the z = 0 plane with
// rotations about the z axis, as the reference implementation of Wine composes it.
// Windows bit parity is NOT VERIFIED.
inline D3DXMATRIX* D3DXMatrixTransformation2D(D3DXMATRIX* out,const D3DXVECTOR2* scalingCenter,float scalingRotation,const D3DXVECTOR2* scaling,const D3DXVECTOR2* rotationCenter,float rotation,const D3DXVECTOR2* translation){
    D3DXVECTOR3 sc(scalingCenter?scalingCenter->x:0.f,scalingCenter?scalingCenter->y:0.f,0.f);
    D3DXVECTOR3 s(scaling?scaling->x:1.f,scaling?scaling->y:1.f,1.f);
    D3DXVECTOR3 rc(rotationCenter?rotationCenter->x:0.f,rotationCenter?rotationCenter->y:0.f,0.f);
    D3DXVECTOR3 t(translation?translation->x:0.f,translation?translation->y:0.f,0.f);
    D3DXQUATERNION sr(0.f,0.f,std::sin(scalingRotation/2.f),std::cos(scalingRotation/2.f));
    D3DXQUATERNION r(0.f,0.f,std::sin(rotation/2.f),std::cos(rotation/2.f));
    return D3DXMatrixTransformation(out,&sc,&sr,&s,&rc,&r,&t);
}
inline BOOL D3DXSphereBoundProbe(const D3DXVECTOR3* center,float radius,const D3DXVECTOR3* origin,const D3DXVECTOR3* direction){
    if(!center||!origin||!direction)return FALSE;
    D3DXVECTOR3 offset=*origin-*center;float a=D3DXVec3Dot(direction,direction);
    if(a==0)return FALSE;
    float c=D3DXVec3Dot(&offset,&offset)-radius*radius;
    if(c<=0)return TRUE;
    float b=D3DXVec3Dot(&offset,direction);
    return b<=0&&b*b-a*c>=0;
}
inline D3DXVECTOR4* D3DXVec3Transform(D3DXVECTOR4* out,const D3DXVECTOR3* in,const D3DXMATRIX* m){
    float x=in->x,y=in->y,z=in->z;
    *out={x*m->_11+y*m->_21+z*m->_31+m->_41,x*m->_12+y*m->_22+z*m->_32+m->_42,x*m->_13+y*m->_23+z*m->_33+m->_43,x*m->_14+y*m->_24+z*m->_34+m->_44};return out;
}
inline D3DXMATRIX* D3DXMatrixPerspectiveFovLH(D3DXMATRIX* out,float fovy,float aspect,float zn,float zf){
    float y=1/std::tan(fovy*.5f),z=zf/(zf-zn);
    *out=D3DXMATRIX(y/aspect,0,0,0,0,y,0,0,0,0,z,1,0,0,-zn*z,0);return out;
}
inline D3DXMATRIX* D3DXMatrixOrthoLH(D3DXMATRIX* out,float width,float height,float zn,float zf){
    float z=1/(zf-zn);*out=D3DXMATRIX(2/width,0,0,0,0,2/height,0,0,0,0,z,0,0,0,-zn*z,1);return out;
}
inline D3DXMATRIX* D3DXMatrixLookAtLH(D3DXMATRIX* out,const D3DXVECTOR3* eye,const D3DXVECTOR3* at,const D3DXVECTOR3* up){
    D3DXVECTOR3 z=*at-*eye,x,y;D3DXVec3Normalize(&z,&z);D3DXVec3Cross(&x,up,&z);D3DXVec3Normalize(&x,&x);D3DXVec3Cross(&y,&z,&x);
    *out=D3DXMATRIX(x.x,y.x,z.x,0,x.y,y.y,z.y,0,x.z,y.z,z.z,0,-D3DXVec3Dot(&x,eye),-D3DXVec3Dot(&y,eye),-D3DXVec3Dot(&z,eye),1);return out;
}
inline D3DXMATRIX* D3DXMatrixRotationYawPitchRoll(D3DXMATRIX* out,float yaw,float pitch,float roll){
    D3DXMATRIX x,y,z;D3DXMatrixRotationX(&x,pitch);D3DXMatrixRotationY(&y,yaw);D3DXMatrixRotationZ(&z,roll);*out=z*x*y;return out;
}
inline D3DXPLANE* D3DXPlaneTransform(D3DXPLANE* out,const D3DXPLANE* plane,const D3DXMATRIX* m){
    D3DXPLANE p=*plane;*out={p.a*m->_11+p.b*m->_21+p.c*m->_31+p.d*m->_41,p.a*m->_12+p.b*m->_22+p.c*m->_32+p.d*m->_42,p.a*m->_13+p.b*m->_23+p.c*m->_33+p.d*m->_43,p.a*m->_14+p.b*m->_24+p.c*m->_34+p.d*m->_44};return out;
}
#ifdef FOURSTORY_D3DX_DEVICE_API
#include "d3dx9tex.h"
#endif
