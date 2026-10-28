#include "../world_battle_modules.hpp"
#include <cassert>
#include <iostream>
using namespace fourstory::server::world_data;
int main() {
 unsigned logs=0;BattleLogScope logging([&](const CString&){++logs;});
 {
  CTBowSystem bow(42,6,2);bow.SetDurations(10,20,30);
  assert(!bow.m_pBOWServer && !bow.m_bRunning && bow.m_bStatus==BS_NORMAL);
  assert(bow.m_dwAlarmDur==10 && bow.m_dwBuyTimeDur==20 && bow.m_dwBattleDur==30);
  bow.m_mapBOWPLAYERS.emplace(1,new BOWPLAYER());
  bow.m_mapBOWREG.emplace(2,new BOWTEMPPLAYER());
  bow.m_vTIMES={100,200,300};bow.Init();
  assert(bow.m_dwStart==100 || bow.m_dwStart==200 || bow.m_dwStart==300);
  bow.Release();assert(bow.m_mapBOWPLAYERS.empty() && bow.m_mapBOWREG.empty());
  bow.m_vTIMES.clear();bool rejected=false;try{bow.Init();}catch(const std::invalid_argument&){rejected=true;}
  assert(rejected);
 }
 {
  CBRSystem br(11,22,33,4);
  assert(br.GetMapID()==710 && br.m_vBRMap.size()==4 && br.m_Mode==BR_3V3 && !br.m_pBRServer);
  assert(br.m_dwAlarmDur==11 && br.m_dwBuyTimeDur==22 && br.m_dwBattleDur==33);
  br.m_vTIMES={300,100,200};br.Init();br.Init();assert(br.m_vBattleType.size()==3);
  auto* team=new TBRTEAMS();team->m_mapBRPlayer.emplace(1,new TBRPLAYERS());br.m_mapBRTeam.emplace(1,team);
  auto* premade=new TBRTEAMS();premade->m_mapBRPlayer.emplace(2,new TBRPLAYERS());br.m_mapPremadeTeam.emplace(2,premade);
  br.m_mapBRREG.emplace(3,new BRTEMPPLAYER());
 }
 assert(logs==3);
 std::cout<<"PASS original World battle constructors, durations, map catalog, scheduling and populated destruction\n";
}
