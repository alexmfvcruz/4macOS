// Portable capture/check of original Windows D3DX arithmetic, without tolerances.
#define NOMINMAX
#include <d3dx9.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <cstring>
int main(int argc,char** argv){
    if(argc!=2&&argc!=3)return 2;
    std::ifstream inputs(argv[1]),expected; if(argc==3)expected.open(argv[2]);
    if(!inputs||(argc==3&&!expected))return 1;
    std::string line,wanted;unsigned count=0;bool different=false;
    while(std::getline(inputs,line)){
        std::istringstream fields(line);std::string kind,extra;fields>>kind;
        unsigned bits[8]={};unsigned n=kind=="MULTIPLY"?8:4;
        if(kind!="AXIS"&&kind!="INVERSE"&&kind!="MULTIPLY"&&kind!="NORMALIZE")return 1;
        for(unsigned i=0;i<n;++i)if(!(fields>>std::hex>>bits[i]))return 1;
        if(fields>>extra)return 1;
        float v[8];std::memcpy(v,bits,sizeof(v));D3DXQUATERNION a(v[0],v[1],v[2],v[3]),out;
        if(kind=="AXIS"){D3DXVECTOR3 axis(v);D3DXQuaternionRotationAxis(&out,&axis,v[3]);}
        else if(kind=="INVERSE")D3DXQuaternionInverse(&out,&a);
        else if(kind=="NORMALIZE"){D3DXVECTOR3 axis(v),normal;D3DXVec3Normalize(&normal,&axis);out=D3DXQUATERNION(normal.x,normal.y,normal.z,0);}
        else{D3DXQUATERNION b(v[4],v[5],v[6],v[7]);D3DXQuaternionMultiply(&out,&a,&b);}
        unsigned actual[4];std::memcpy(actual,&out,sizeof(actual));std::ostringstream record;
        record<<kind<<std::hex<<std::setfill('0');for(unsigned b:actual)record<<' '<<std::setw(8)<<b;
        std::cout<<record.str()<<'\n';++count;
        if(argc==3&&(!std::getline(expected,wanted)||record.str()!=wanted))different=true;
        // The API must also preserve its result when an output aliases input.
        D3DXQUATERNION alias=a;
        if(kind=="INVERSE")D3DXQuaternionInverse(&alias,&alias);
        else if(kind=="MULTIPLY"){D3DXQUATERNION b(v[4],v[5],v[6],v[7]);D3DXQuaternionMultiply(&alias,&alias,&b);}
        else continue;
        if(std::memcmp(&alias,&out,sizeof(out)))different=true;
    }
    if(argc==3&&std::getline(expected,wanted))different=true;
    return !inputs.eof()||!count||different?1:0;
}
