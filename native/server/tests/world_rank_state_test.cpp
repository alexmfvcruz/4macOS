#include "../world_rank_state.hpp"
#include <cassert>
#include <iostream>
using namespace fourstory::server::world_data;
int main() {
 WorldRankState state;DWORD total=99,month=99;
 state.GetRanking(10,total,month);assert(total==0 && month==0);
 state.m_mapRank.emplace(10,42);state.m_mapMonthRank.emplace(10,7);
 state.GetRanking(10,total,month);assert(total==42 && month==7);
 state.GetRanking(11,total,month);assert(total==0 && month==0);
 state.m_bFirstGroupCount=3;state.m_arFirstGradeGroup[0][0].m_dwCharID=100;
 state.m_arFirstGradeGroup[0][1].m_dwCharID=101;state.m_arFirstGradeGroup[0][2].m_dwCharID=102;
 state.m_arFirstGradeGroup[0][3].m_dwCharID=103;
 assert(!state.IsFirstGroup(0,100) && state.IsFirstGroup(0,101) && state.IsFirstGroup(0,102) && !state.IsFirstGroup(0,103));
 assert(!state.IsFirstGroup(255,101));
 state.m_bFirstGroupCount=FIRSTGRADEGROUPCOUNT+1;bool rejected=false;
 try{state.IsFirstGroup(0,101);}catch(const std::invalid_argument&){rejected=true;}assert(rejected);
 std::cout<<"PASS original World rank lookup, missing resets, first-group exclusions and array bounds\n";
}
