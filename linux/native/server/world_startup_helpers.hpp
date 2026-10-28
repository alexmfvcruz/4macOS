#pragma once
#include "world_guild.hpp"
namespace fourstory::server::world_data {
// The Windows fallback dereferences find(2) when all daily starts have passed.
// Select the earliest remaining start, or wrap to the earliest next-day start.
inline DWORD next_mission_start(VECTORDWORD times,DWORD seconds) {
    if(times.empty() || seconds>=86400) throw std::invalid_argument("Invalid World mission schedule");
    for(DWORD time:times) if(time>=86400) throw std::invalid_argument("Invalid World mission time");
    std::sort(times.begin(),times.end());
    auto next=std::lower_bound(times.begin(),times.end(),seconds);
    return next==times.end()?times.front():*next;
}
inline LPTGUILDLEVEL find_guild_level(MAPTGUILDLEVEL& m_mapTGuildLevel,BYTE bLevel) {
#include "../../source/4Story 5.0 Source Files/Server/TWorldSvr/FindGuildLevel.inc"
}
inline CString build_net_string(const CString& strHeader,const CString& strBody) {
#include "../../source/4Story 5.0 Source Files/Server/TWorldSvr/BuildNetString.inc"
}
inline BYTE war_country_gap(BYTE bLevel) {
#include "../../source/4Story 5.0 Source Files/Server/TWorldSvr/GetWarCountryGap.inc"
}
inline void calculate_guild_ranking(MAPTGUILD& m_mapTGuild) {
#include "../../source/4Story 5.0 Source Files/Server/TWorldSvr/CalcGuildRanking.inc"
}
}
