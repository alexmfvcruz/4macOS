#include "../world_battle_schedule.hpp"
#include <cassert>
#include <iostream>
using namespace fourstory::server::world_data;
int main() {
    unsigned logs=0;auto log=[&](const CString& text){assert(!text.IsEmpty());++logs;};
    BattleSchedule state;state.m_vTIMES={7200,3600,10800};
    initialize_bow_schedule(state,3600,log);assert(state.m_dwStart==7200);
    initialize_bow_schedule(state,86399,log);assert(state.m_dwStart==3600);
    initialize_br_schedule(state,3600,log);assert(state.m_dwStart==7200 && state.m_bType==BR_SOLO);
    assert((state.m_vBattleType==VECTORBYTE{BR_TEAM,BR_SOLO,BR_TEAM}));
    initialize_br_schedule(state,10800,log);assert(state.m_dwStart==3600 && state.m_bType==BR_TEAM && state.m_vBattleType.size()==3);
    BattleSchedule duplicate;duplicate.m_vTIMES={3600,3600};
    initialize_br_schedule(duplicate,0,log);assert(duplicate.m_dwStart==3600 && duplicate.m_bType==BR_TEAM);
    for(unsigned size:{0u,256u}) {
        BattleSchedule invalid;invalid.m_vTIMES.resize(size,1);bool rejected=false;
        try{initialize_br_schedule(invalid,0,log);}catch(const std::invalid_argument&){rejected=true;}assert(rejected);
    }
    bool rejected=false;state.m_vTIMES={86400};
    try{initialize_bow_schedule(state,0,log);}catch(const std::invalid_argument&){rejected=true;}assert(rejected);
    assert(logs==5);
    std::cout<<"PASS original World BoW/BR schedule sorting, strict next-time, midnight wrap, team/solo modes and invalid chart guards\n";
}
