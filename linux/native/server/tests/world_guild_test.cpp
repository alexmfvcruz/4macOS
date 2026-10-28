#include "../world_guild.hpp"
#include <cassert>
#include <iostream>
using namespace fourstory::server::world_data;
int main() {
    CTGuild guild;guild.m_dwID=11;
    assert(guild.m_mapTMember.empty() && guild.m_bCountry==TCONTRY_N);
    guild.AddArticle(3,1,"writer","title","body",946684800);
    assert(guild.m_dwArticleIndex==3 && guild.m_strArticleTitle=="title");
    assert(guild.m_mapTArticle.at(3)->m_strDate=="2000-01-01");
    auto* member=new TGUILDMEMBER();member->m_dwID=7;member->m_strName="member";
    guild.m_mapTMember.emplace(7,member);
    assert(guild.FindMember(DWORD(7))==member && guild.FindMember(CString("member"))==member);
    assert(guild.FindMember(DWORD(8))==nullptr);
    TENTRYRECORD stale{};stale.m_dwDate=3;stale.m_wKillCount=99;
    TENTRYRECORD current{};current.m_dwDate=4;current.m_wKillCount=2;current.m_wDieCount=3;current.m_aGainPoint[0]=42;
    member->m_vRecord={stale,current};guild.CalcWeekRecord(10);
    assert(member->m_vRecord.size()==1 && member->m_weekrecord.m_wKillCount==2 && member->m_weekrecord.m_aGainPoint[0]==42);
    auto* item=new TITEM();item->m_dwItemID=12;item->m_bCount=2;guild.PutInItem(12,item);
    auto* extra=new TITEM();extra->m_dwItemID=12;extra->m_bCount=3;guild.PutInItem(12,extra);
    assert(guild.FindCabinetItem(12)==item && item->m_bCount==5);
    auto* tactics=new TTACTICSMEMBER();tactics->m_dwID=8;tactics->m_strName="tactics";
    guild.m_mapTTactics.emplace(8,tactics);
    assert(guild.FindTactics(DWORD(8))==tactics && guild.FindTactics(CString("tactics"))==tactics);
    std::cout<<"PASS original World guild startup articles, member/tactics lookup, item ownership and seven-day records\n";
}
