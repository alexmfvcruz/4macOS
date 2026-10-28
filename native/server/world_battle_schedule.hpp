#pragma once
#include "world_data_types.hpp"
#include <functional>
namespace fourstory::server::world_data {
#include "../../source/4Story 5.0 Source Files/Server/TWorldSvr/BRSystem.h"
struct BattleSchedule {
    VECTORDWORD m_vTIMES;
    VECTORBYTE m_vBattleType;
    DWORD m_dwStart=0;
    BYTE m_bType=BR_TEAM;
};
inline void validate_battle_schedule(const BattleSchedule& state,DWORD seconds,const std::function<void(const CString&)>& log) {
    if(!log || state.m_vTIMES.empty() || state.m_vTIMES.size()>255 || seconds>=86400)
        throw std::invalid_argument("Invalid World battle schedule dependencies");
    for(auto value:state.m_vTIMES) if(value>=86400) throw std::invalid_argument("Invalid World battle time");
}
inline void initialize_bow_schedule(BattleSchedule& state,DWORD current_seconds,
                                    const std::function<void(const CString&)>& logger) {
    validate_battle_schedule(state,current_seconds,logger);
    auto& m_vTIMES=state.m_vTIMES;auto& m_dwStart=state.m_dwStart;
    auto Log=logger;using std::sort;
#include "../../source/4Story 5.0 Source Files/Server/TWorldSvr/BowInitializeSchedule.inc"
}
inline void initialize_br_schedule(BattleSchedule& state,DWORD current_seconds,
                                   const std::function<void(const CString&)>& logger) {
    validate_battle_schedule(state,current_seconds,logger);
    // This is initialization, not a repeated accumulation of mode entries.
    state.m_vBattleType.clear();
    auto& m_vTIMES=state.m_vTIMES;auto& m_dwStart=state.m_dwStart;
    auto& m_vBattleType=state.m_vBattleType;auto& m_bType=state.m_bType;
    auto Log=logger;using std::sort;
#include "../../source/4Story 5.0 Source Files/Server/TWorldSvr/BRInitializeSchedule.inc"
}
}
