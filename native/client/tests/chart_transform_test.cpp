// Exercise original TChart node transformation with a nonempty, known fixture.
#define main c1_dump_main
#include "dump/dump.cpp"
#undef main
#include <cassert>
int main(int argc,char** argv){
    assert(argc==2);string nodes=string(argv[1])+"/fixture-node.tcd",maps=string(argv[1])+"/fixture-map.tcd";
    {CFile f(nodes.c_str(),CFile::modeCreate|CFile::modeWrite|CFile::typeBinary);CArchive ar(&f,CArchive::store);
     ar<<WORD(1)<<DWORD(7)<<DWORD(9)<<float(1)<<float(2)<<float(3);ar.Close();}
    {CFile f(maps.c_str(),CFile::modeCreate|CFile::modeWrite|CFile::typeBinary);CArchive ar(&f,CArchive::store);
     ar<<WORD(1)<<DWORD(11)<<DWORD(7)<<float(2)<<float(3)<<float(4)<<float(5)<<float(6)<<float(7)
       <<float(0)<<float(0)<<float(D3DX_PI/2)<<BYTE(0)<<BYTE(1);ar.Close();}
    CTChart::InitTMAPINFO(CString(nodes.c_str()),CString(maps.c_str()));assert(CTChart::m_mapTMAPINFO.size()==1);
    const TMAPINFO& map=*CTChart::m_mapTMAPINFO.at(11);assert(map.m_dwMapID==7&&map.m_bNPCCALL==1&&map.m_mapTNODE.size()==1);
    const D3DXVECTOR3& p=*map.m_mapTNODE.at(9);assert(std::fabs(p.x+1)<1e-5f&&std::fabs(p.y-8)<1e-5f&&std::fabs(p.z-19)<1e-5f);
    CTChart::ReleaseTMAPINFO();cout<<"PASS original TChart nonempty node transformation fixture\n";
}
