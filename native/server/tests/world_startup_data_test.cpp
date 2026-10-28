#include "../world_startup_data.hpp"
#include <cassert>
#include <iostream>
using namespace fourstory::server::world_data;
int main() {
 WorldStartupData state;
 state.m_mapTGuild.emplace(1,new CTGuild());state.m_mapTGuildLevel.emplace(1,new TGUILDLEVEL());
 state.m_mapEVQT.emplace(1,new TEVENTQUARTER());state.m_mapCMGift.emplace(1,new TCMGIFT());
 state.m_pBOWModule=new CTBowSystem(1,2,3);state.m_pBRModule=new CBRSystem(1,2,3,4);
 state.m_mapTGuildWantedApp.emplace(1,new TGUILDWANTEDAPP());
 state.m_mapTGuildTacticsWantedApp.emplace(2,new TGUILDTACTICSWANTEDAPP());
 state.m_mapTNMTPlayer.emplace(1,new TNMTPLAYER());
 TOURNAMENTSCHEDULE schedule{};schedule.m_mapStep=new MAPTOURNAMENTSTEP();state.m_mapTournamentSchedule.emplace(1,schedule);
 state.m_mapTournament[1].emplace(1,new TOURNAMENTENTRY());
 state.m_qGenPartyID.push(7);state.m_bNation=4;state.m_timeCurrent=123;
 state.m_mapWarCountry[0][0].emplace(1,2);state.m_mapRank.emplace(1,2);
 state.m_arMonthRank[0][0].m_dwCharID=5;state.m_arFirstGradeGroup[0][0].m_dwCharID=6;
 state.clear();state.clear();
 assert(state.m_mapTGuild.empty() && state.m_mapEVQT.empty() && state.m_mapTournament.empty() && state.m_mapTNMTPlayer.empty());
 assert(!state.m_pBOWModule && !state.m_pBRModule && state.m_qGenPartyID.empty() && !state.m_timeCurrent && !state.m_bNation);
 assert(state.m_mapRank.empty() && !state.m_arMonthRank[0][0].m_dwCharID && !state.m_arFirstGradeGroup[0][0].m_dwCharID);
 std::cout<<"PASS assembled original World startup state, populated ownership and repeated cleanup\n";
}
