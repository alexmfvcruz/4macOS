#pragma once
#include "world_data_types.hpp"
namespace fourstory::server::world_data {
struct WorldRankState {
    MAPDWORD m_mapRank,m_mapMonthRank;
    MONTHRANKER m_arMonthRank[COUNTRY_COUNT][MONTHRANKCOUNT]{};
    MONTHRANKER m_arFirstGradeGroup[COUNTRY_COUNT][FIRSTGRADEGROUPCOUNT]{};
    BYTE m_bFirstGroupCount=0,m_bRankMonth=0;
void GetRanking(DWORD dwCharID, DWORD & dwRank, DWORD & dwMonthRank) {
#include "../../source/4Story 5.0 Source Files/Server/TWorldSvr/GetRanking.inc"
}
BYTE IsFirstGroup(BYTE bCountry, DWORD dwCharID) {
if(bCountry>TCONTRY_B) return FALSE;
if(bCountry>=COUNTRY_COUNT || m_bFirstGroupCount>FIRSTGRADEGROUPCOUNT) throw std::invalid_argument("Invalid World first-grade rank bounds");
#include "../../source/4Story 5.0 Source Files/Server/TWorldSvr/IsFirstGroup.inc"
}
};
}
