#pragma once
#include "world_guild.hpp"
namespace fourstory::server::world_data {
class GuildRecruitmentState {
public:
    MAPTGUILDWANTED m_mapTGuildWanted;
    MAPVTGUILDTACTICSWANTED m_mapTGuildTacticsWanted;
    // These roots own applicants. Wanted-record maps are borrowed aliases.
    MAPTGUILDWANTEDAPP m_mapTGuildWantedApp;
    MAPTGUILDTACTICSWANTEDAPP m_mapTGuildTacticsWantedApp;
    INT64 m_timeCurrent=0;
    GuildRecruitmentState()=default;
    GuildRecruitmentState(const GuildRecruitmentState&)=delete;
    GuildRecruitmentState& operator=(const GuildRecruitmentState&)=delete;
    ~GuildRecruitmentState() {clear();}
    void clear() {
        m_mapTGuildWanted.clear();m_mapTGuildTacticsWanted.clear();
        for(auto& entry:m_mapTGuildWantedApp) delete entry.second;
        for(auto& entry:m_mapTGuildTacticsWantedApp) delete entry.second;
        m_mapTGuildWantedApp.clear();m_mapTGuildTacticsWantedApp.clear();
    }
BYTE AddGuildWanted(CTGuild * pGuild,
									  BYTE bMinLevel,
									  BYTE bMaxLevel,
									  CString strTitle,
									  CString strText,
									  INT64 dlEndTime) {
#include "../../source/4Story 5.0 Source Files/Server/TWorldSvr/AddGuildWanted1.inc"
}
BYTE AddGuildTacticsWanted(CTGuild * pGuild,
                                             DWORD dwID,
                                             DWORD dwPoint,
                                             DWORD dwGold,
                                             DWORD dwSilver,
                                             DWORD dwCooper,
                                             BYTE bDay,
                                             BYTE bMinLevel,
                                             BYTE bMaxLevel,
                                             CString strTitle,
                                             CString strText,
											 INT64 dlEndTime) {
#include "../../source/4Story 5.0 Source Files/Server/TWorldSvr/AddGuildTacticsWanted1.inc"
}
LPTGUILDWANTED FindGuildWanted(DWORD dwGuildID) {
#include "../../source/4Story 5.0 Source Files/Server/TWorldSvr/FindGuildWanted1.inc"
}
LPTGUILDTACTICSWANTED FindGuildTacticsWanted(DWORD dwGuildID, DWORD dwID) {
#include "../../source/4Story 5.0 Source Files/Server/TWorldSvr/FindGuildTacticsWanted1.inc"
}
LPTGUILDTACTICSWANTED FindGuildTacticsWanted(DWORD dwID) {
#include "../../source/4Story 5.0 Source Files/Server/TWorldSvr/FindGuildTacticsWanted2.inc"
}
LPTGUILDWANTEDAPP FindGuildWantedApp(DWORD dwCharID) {
#include "../../source/4Story 5.0 Source Files/Server/TWorldSvr/FindGuildWantedApp1.inc"
}
LPTGUILDTACTICSWANTEDAPP FindGuildTacticsWantedApp(DWORD dwCharID) {
#include "../../source/4Story 5.0 Source Files/Server/TWorldSvr/FindGuildTacticsWantedApp1.inc"
}
LPVTGUILDTACTICSWANTED FindGuildTacticsList(DWORD dwGuildID) {
#include "../../source/4Story 5.0 Source Files/Server/TWorldSvr/FindGuildTacticsList1.inc"
}
BYTE AddGuildWantedApp(DWORD dwID,
										 DWORD dwCharID,
										 CString strNAME,
										 BYTE bLevel,
										 BYTE bClass,
										 LPTCHARACTER pTCHAR = nullptr) {
#include "../../source/4Story 5.0 Source Files/Server/TWorldSvr/AddGuildWantedApp1.inc"
}
BYTE AddGuildTacticsWantedApp(DWORD dwID,
												DWORD dwCharID,
												CString strNAME,
												BYTE bLevel,
												BYTE bClass,
												DWORD dwGuildID,
												LPTCHARACTER pTCHAR = nullptr) {
#include "../../source/4Story 5.0 Source Files/Server/TWorldSvr/AddGuildTacticsWantedApp1.inc"
}
BYTE GetWarCountry(LPTCHARACTER pTCHAR) {
#include "../../source/4Story 5.0 Source Files/Server/TWorldSvr/GetWarCountry1.inc"
}
};
}
