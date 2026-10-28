#include "../world_tournament_state.hpp"
#include <cassert>
#include <memory>
#include <iostream>
using namespace fourstory::server::world_data;
int main() {
    TournamentState state;
    assert(!state.GetTournament(1) && !state.GetCurrentTournamentEntry(1) && !state.FindTNMTPlayer(1) && !state.TournamentGetEntryCount());
    auto* first=new TOURNAMENTENTRY();first->m_bEntryID=1;first->m_bGroup=2;
    auto* second=new TOURNAMENTENTRY();second->m_bEntryID=2;second->m_bGroup=3;
    state.m_mapTournament[7]={{1,first},{2,second}};
    state.m_tournament.m_mapEntry=state.GetTournament(7);state.m_tournament.m_bGroup=2;
    assert(state.GetCurrentTournamentEntry(1)==first && state.TournamentGetEntryCount()==1);
    auto* chief=new TNMTPLAYER();chief->m_dwCharID=10;chief->m_bSlotID=3;
    state.AddTNMTPlayer(first,chief,TNMTSTEP_1st,chief);
    assert(first->m_map1st.at(10)==chief && chief->m_bEntryID==1 && chief->m_dwChiefID==10 && state.FindTNMTPlayer(10)==chief);
    auto* normal=new TNMTPLAYER();normal->m_dwCharID=11;
    state.AddTNMTPlayer(first,normal,TNMTSTEP_NORMAL,chief);assert(first->m_mapNormal.at(11)==normal && normal->m_bSlotID==3);
    auto* party=new TNMTPLAYER();party->m_dwCharID=12;
    state.AddTNMTPlayer(first,party,TNMTSTEP_PARTY,chief);assert(chief->m_mapParty.at(12)==party);
    state.AddTNMTPlayer(first,chief,TNMTSTEP_MATCH,chief);assert(first->m_mapPlayer.at(10)==chief && state.m_mapTNMTPlayer.size()==3);
    auto conflict=std::make_unique<TNMTPLAYER>();conflict->m_dwCharID=10;bool rejected=false;
    try{state.AddTNMTPlayer(first,conflict.get(),TNMTSTEP_MATCH,chief);}catch(const std::invalid_argument&){rejected=true;}assert(rejected && state.FindTNMTPlayer(10)==chief);
    state.clear();assert(state.m_mapTournament.empty() && state.m_mapTNMTPlayer.empty() && !state.m_tournament.m_mapEntry);
    state.clear();
    auto* entry=new TOURNAMENTENTRY();entry->m_bEntryID=1;entry->m_dwFeeBack=23;
    state.m_mapTournament[7]={{1,entry}};
    state.m_tournament.m_wID=7;state.m_tournament.m_mapEntry=state.GetTournament(7);
    for(DWORD id=10;id<20;++id) {
        auto* player=new TNMTPLAYER();player->m_dwCharID=id;player->m_bCountry=TCONTRY_D;
        player->m_bLevel=1;player->m_dwMonthRank=id;
        state.AddTNMTPlayer(entry,player,TNMTSTEP_NORMAL,player);
    }
    unsigned refunds=0;
    state.TournamentSelectPlayer([]{return 0;},[](DWORD){return 0;},[&](DWORD id,DWORD fee){assert(id>=18 && fee==23);++refunds;});
    assert(refunds==2 && state.m_mapTNMTPlayer.size()==8 && entry->m_mapPlayer.size()==8 && state.m_tournament.m_bSelected);
    bool slots[TOURNAMENT_SLOT]{};
    for(const auto& item:state.m_mapTNMTPlayer) {
        assert(item.second->m_bSlotID<TOURNAMENT_SLOT && !slots[item.second->m_bSlotID]);
        slots[item.second->m_bSlotID]=true;
    }
    state.clear();
    auto* failed_entry=new TOURNAMENTENTRY();failed_entry->m_bEntryID=1;
    state.m_mapTournament[7]={{1,failed_entry}};state.m_tournament.m_wID=7;
    auto* failed_player=new TNMTPLAYER();failed_player->m_dwCharID=20;failed_player->m_bCountry=TCONTRY_D;failed_player->m_bLevel=1;
    state.AddTNMTPlayer(failed_entry,failed_player,TNMTSTEP_NORMAL,failed_player);
    rejected=false;
    try{state.TournamentSelectPlayer([]{return 0;},[](DWORD bound){return bound;},[](DWORD,DWORD){});}
    catch(const std::invalid_argument&){rejected=true;}assert(rejected && state.FindTNMTPlayer(20)==failed_player);
    state.clear();
    auto* refund_entry=new TOURNAMENTENTRY();refund_entry->m_bEntryID=1;refund_entry->m_dwFeeBack=23;
    state.m_mapTournament[7]={{1,refund_entry}};state.m_tournament.m_wID=7;
    state.m_tournament.m_mapEntry=state.GetTournament(7);
    for(DWORD id=20;id<30;++id) {
        auto* player=new TNMTPLAYER();player->m_dwCharID=id;player->m_bCountry=TCONTRY_D;
        player->m_bLevel=1;player->m_dwMonthRank=id;
        state.AddTNMTPlayer(refund_entry,player,TNMTSTEP_NORMAL,player);
    }
    rejected=false;
    try{state.TournamentSelectPlayer([]{return 0;},[](DWORD){return 0;},[](DWORD id,DWORD){if(id==29) throw std::runtime_error("refund failed");});}
    catch(const std::runtime_error&){rejected=true;}
    assert(rejected && state.m_mapTNMTPlayer.size()==9 && !state.FindTNMTPlayer(28) && state.FindTNMTPlayer(29));
    std::cout<<"PASS original World tournament entry/group lookup, four player stages, alias ownership and conflicting identity rejection\n";
}
