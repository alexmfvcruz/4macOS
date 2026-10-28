#include "../world_database_item.hpp"
#include <cassert>
#include <memory>
#include <iostream>
using namespace fourstory::server::world_data;
int main() {
    TITEM row{};row.m_dlID=0x102030405060708LL;row.m_dwItemID=123;row.m_wItemID=456;
    row.m_bLevel=7;row.m_bGem=8;row.m_wMoggItemID=9;row.m_bCount=10;row.m_bGLevel=11;
    row.m_dwDuraMax=100;row.m_dwDuraCur=50;row.m_bRefineCur=3;
    row.m_dEndTime=0x1122334455667788LL;row.m_bGradeEffect=12;
    for(unsigned i=0;i<TMAGIC_MAX;++i) {row.m_bMagic[i]=static_cast<BYTE>(i+1);row.m_wValue[i]=static_cast<WORD>(200+i);row.m_dwExtValue[i]=300+i;}
    std::unique_ptr<TITEM> item(create_database_item(row));
    assert(item.get()!=&row && item->m_dlID==row.m_dlID && item->m_dwItemID==123 && item->m_wItemID==456);
    assert(item->m_bLevel==7 && item->m_bGem==8 && item->m_wMoggItemID==9 && item->m_bCount==10 && item->m_bGLevel==11);
    assert(item->m_dwDuraMax==100 && item->m_dwDuraCur==50 && item->m_bRefineCur==3 && item->m_dEndTime==row.m_dEndTime && item->m_bGradeEffect==12);
    assert(item->m_bMagicCount==TMAGIC_MAX);
    for(unsigned i=0;i<TMAGIC_MAX;++i) assert(item->m_bMagic[i]==row.m_bMagic[i] && item->m_wValue[i]==row.m_wValue[i]);
    for(auto i:{IEV_ELD,IEV_WRAP,IEV_COLOR,IEV_GUILD}) assert(item->m_dwExtValue[i]==row.m_dwExtValue[i]);
    row.m_bMagic[1]=0;item.reset(create_database_item(row));
    assert(item->m_bMagicCount==1 && item->m_bMagic[2]==0 && item->m_wValue[2]==0);
    std::cout<<"PASS original World database-item copy, 64-bit identity/expiry, all magic slots and first-empty termination\n";
}
