#pragma once
#include "world_guild_recruitment.hpp"
#include "world_tournament_state.hpp"
#include "world_rank_state.hpp"
#include "world_battle_modules.hpp"
#include "world_database_item.hpp"
#include "world_event_time.hpp"
#include "world_startup_helpers.hpp"
namespace fourstory::server::world_data {
struct WorldStartupData : GuildRecruitmentState,TournamentState,WorldRankState {
    VECTORDWORD m_vTOPERATOR,m_vCUSTOMTIMES;
    MAPTGUILDLEVEL m_mapTGuildLevel;
    MAPTGUILD m_mapTGuild;
    MAPTEVENTQUARTER m_mapEVQT;
    MAPTIMEQUARTER m_mapTimeEVQT;
    MAPTSTRING m_mapTSvrMsg;
    QUEUEWORD m_qGenPartyID;
    DWORD m_dwTacticsIndex=0,m_dwGenRecallID=0,m_dwRecentRecordDate=0;
    MAPDWORD m_mapTGuildEx,m_mapWarCountry[TCONTRY_B][WARCOUNTRY_MAXGAP],m_mapCharGuild,m_mapCharTactics;
    TBATTLETIME m_battletime[BT_COUNT]{};
    CTBowSystem* m_pBOWModule=nullptr;
    CBRSystem* m_pBRModule=nullptr;
    BYTE m_bNation=0,m_bMaxLevel=0;
    MAPRPSGAME m_mapRPSGame;
    MAPCMGIFT m_mapCMGift;
    WorldStartupData()=default;
    WorldStartupData(const WorldStartupData&)=delete;
    WorldStartupData& operator=(const WorldStartupData&)=delete;
    ~WorldStartupData(){clear();}
    void clear() {
        GuildRecruitmentState::clear();TournamentState::clear();
        delete m_pBOWModule;delete m_pBRModule;m_pBOWModule=nullptr;m_pBRModule=nullptr;
        for(auto& entry:m_mapTGuild) delete entry.second;
        for(auto& entry:m_mapTGuildLevel) delete entry.second;
        for(auto& entry:m_mapEVQT) delete entry.second;
        for(auto& entry:m_mapCMGift) delete entry.second;
        m_mapTGuild.clear();m_mapTGuildLevel.clear();m_mapEVQT.clear();m_mapCMGift.clear();
        m_mapTimeEVQT.clear();m_mapTSvrMsg.clear();m_mapTGuildEx.clear();m_mapCharGuild.clear();m_mapCharTactics.clear();
        for(auto& country:m_mapWarCountry) for(auto& gap:country) gap.clear();
        m_mapRPSGame.clear();m_vTOPERATOR.clear();m_vCUSTOMTIMES.clear();m_qGenPartyID={};
        m_mapRank.clear();m_mapMonthRank.clear();
        for(auto& country:m_arMonthRank) for(auto& rank:country) rank=MONTHRANKER{};
        for(auto& country:m_arFirstGradeGroup) for(auto& rank:country) rank=MONTHRANKER{};
        for(auto& battle:m_battletime) battle=TBATTLETIME{};
        m_dwTacticsIndex=0;m_dwGenRecallID=0;m_dwRecentRecordDate=0;m_bNation=0;m_bMaxLevel=0;
        m_bFirstGroupCount=0;m_bRankMonth=0;m_timeCurrent=0;
    }
};
struct WorldStartupCallbacks {
    TournamentState::ScheduleCallbacks schedule;
    std::function<int()> random;
    std::function<DWORD(DWORD)> bounded_random;
    std::function<void(DWORD,DWORD)> refund;
    std::function<void(const CString&)> log;
};
}
extern DWORD LoadOriginalWorldStartup(CSqlDatabase*,fourstory::server::world_data::WorldStartupData*,
    const fourstory::server::world_data::WorldStartupCallbacks&);
