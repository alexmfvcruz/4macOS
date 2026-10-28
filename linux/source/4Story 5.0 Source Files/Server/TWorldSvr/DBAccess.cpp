#if defined(FOURSTORY_NATIVE_ODBC) && !defined(FOURSTORY_NATIVE_MODULE)
#include "../../../../native/server/odbc_platform.hpp"
#include "../../Lib/Own/TProtocol/include/NetCode.h"
#include "../../../../native/server/world_data_types.hpp"
#include "../../../../native/server/world_database_actions.hpp"
#include "../../../../native/server/world_startup_data.hpp"
#include "../../Lib/Own/TProtocol/include/DMProtocol.h"
#include "../TNetLib/ErrorCode.h"
#include <memory>
namespace fourstory::server::queries::TWorldSvr {
using TITEM = fourstory::server::world_data::TITEM;
#include "DBAccess.h"
#else
#include "StdAfx.h"
#endif

BEGIN_INIT_QUERY(TWorldSvr)

PREDEFINE_QUERY(CTBLGuildChart)
PREDEFINE_QUERY(CTBLGuild)
PREDEFINE_QUERY(CTBLGuildArticle)
PREDEFINE_QUERY(CTBLGuildItem)
PREDEFINE_QUERY(CTBLGuildVolunteer)
PREDEFINE_QUERY(CTBLGuildMember)
PREDEFINE_QUERY(CTBLGuildTactics)
PREDEFINE_QUERY(CTBLGuildRelation)
PREDEFINE_QUERY(CTBLFriend)
PREDEFINE_QUERY(CTBLFriendTarget)
PREDEFINE_QUERY(CTBLFriendGroupTable)
PREDEFINE_QUERY(CTBLSoulmate)
PREDEFINE_QUERY(CTBLMySoulmate)
PREDEFINE_QUERY(CTBLOperatorChart)
PREDEFINE_QUERY(CTBLEventQuarterChart)
PREDEFINE_QUERY(CTBLGuildLoad)
PREDEFINE_QUERY(CTBLBattleTimeChart)
PREDEFINE_QUERY(CTBLSvrMsg)
PREDEFINE_QUERY(CTBLItemFind)
PREDEFINE_QUERY(CTBLGuildPvPointReward)
PREDEFINE_QUERY(CTBLGuildPvPRecord)
PREDEFINE_QUERY(CTBLCastleApplicant)
PREDEFINE_QUERY(CTBLGuildWanted)
PREDEFINE_QUERY(CTBLGuildTacticsWanted)
PREDEFINE_QUERY(CTBLMonthPvPointTable)  
PREDEFINE_QUERY(CTBLMonthPvPointChar)
PREDEFINE_QUERY(CTBLPvPointTable)
PREDEFINE_QUERY(CTBLStopMap)
PREDEFINE_QUERY(CTBLProtectChar)
PREDEFINE_QUERY(CTBLDeleteLevel)
PREDEFINE_QUERY(CTBLEventQuarterList) 
PREDEFINE_QUERY(CTBLTournament)
PREDEFINE_QUERY(CTBLTournamentSchedule)
PREDEFINE_QUERY(CTBLGetCharInfo)
PREDEFINE_QUERY(CTBLTournamentReward)
PREDEFINE_QUERY(CTBLTournamentPlayer)
PREDEFINE_QUERY(CTBLFirstGradeGroup)
PREDEFINE_QUERY(CTBLTournamentCurrentStep)
PREDEFINE_QUERY(CTBLTnmtEventTime)
PREDEFINE_QUERY(CTBLTnmtEventSchedule)
PREDEFINE_QUERY(CTBLTournamentEvent)
PREDEFINE_QUERY(CTBLTnmtEventReward)
PREDEFINE_QUERY(CTBLRPSGame)
PREDEFINE_QUERY(CTBLRPSGameRecord)
PREDEFINE_QUERY(CTBLActiveCharTable)
PREDEFINE_QUERY(CTBLActiveCharDel)
PREDEFINE_QUERY(CTBLCMGiftChart)
PREDEFINE_QUERY(CTBLCustomTimeChart)
PREDEFINE_QUERY(CTBLBOWSettingsChart)
PREDEFINE_QUERY(CTBLBRSettingsChart)
PREDEFINE_QUERY(CTBLGuildStats)

PREDEFINE_QUERY(CSPGuildUpdate)
PREDEFINE_QUERY(CSPGuildEstablish)
PREDEFINE_QUERY(CSPGuildDisorg)
PREDEFINE_QUERY(CSPGuildDelete)
PREDEFINE_QUERY(CSPGuildMemberAdd)
PREDEFINE_QUERY(CSPGuildLeave)
PREDEFINE_QUERY(CSPGuildDuty)
PREDEFINE_QUERY(CSPGuildPeer)
PREDEFINE_QUERY(CSPGuildKickout)
PREDEFINE_QUERY(CSPFriendInsert)
PREDEFINE_QUERY(CSPFriendErase)
PREDEFINE_QUERY(CSPFriendGroupMake)
PREDEFINE_QUERY(CSPFriendGroupDelete)
PREDEFINE_QUERY(CSPFriendGroupChange)
PREDEFINE_QUERY(CSPFriendGroupName)
PREDEFINE_QUERY(CSPProtectedSearch)
PREDEFINE_QUERY(CSPLoadService)
PREDEFINE_QUERY(CSPGuildArticleAdd)
PREDEFINE_QUERY(CSPGuildArticleDel)
PREDEFINE_QUERY(CSPGuildArticleUpdate)
PREDEFINE_QUERY(CSPGuildFame)
PREDEFINE_QUERY(CSPGuildLevel)
PREDEFINE_QUERY(CSPGuildMaxCabinet)
PREDEFINE_QUERY(CSPGuildContribution)
PREDEFINE_QUERY(CSPGuildVolunteerAdd)
PREDEFINE_QUERY(CSPGuildVolunteerDel)
PREDEFINE_QUERY(CSPGuildTacticsAdd)
PREDEFINE_QUERY(CSPGuildTacticsDel)
PREDEFINE_QUERY(CSPSoulmateReg)
PREDEFINE_QUERY(CSPSoulmateEnd)
PREDEFINE_QUERY(CSPSoulmateDel)
PREDEFINE_QUERY(CSPGetReservedPost)
PREDEFINE_QUERY(CSPClearMapCurrentUser)
PREDEFINE_QUERY(CSPGetNation)
PREDEFINE_QUERY(CSPItemStateChange)
PREDEFINE_QUERY(CSPSaveGuildPvPoint)
PREDEFINE_QUERY(CSPSaveGuildPvPRecord)
PREDEFINE_QUERY(CSPSaveGuildPointReward)
PREDEFINE_QUERY(CSPSaveCastleApplicant)
PREDEFINE_QUERY(CSPGetRecallID)
PREDEFINE_QUERY(CSPCashItemSale)
PREDEFINE_QUERY(CSPGuildWantedAdd)
PREDEFINE_QUERY(CSPGuildWantedDel)
PREDEFINE_QUERY(CSPGuildTacticsWantedAdd)
PREDEFINE_QUERY(CSPGuildTacticsWantedDel)
PREDEFINE_QUERY(CSPGuildTacticsAdd)
PREDEFINE_QUERY(CSPGuildTacticsDel)
PREDEFINE_QUERY(CSPGuildVolunteering)
PREDEFINE_QUERY(CSPGuildVolunteeringDel)
PREDEFINE_QUERY(CSPSaveTacticsGainPoint)
PREDEFINE_QUERY(CSPSaveGuildStats)
PREDEFINE_QUERY(CSPSaveMonthRank)  
PREDEFINE_QUERY(CSPInitMonthPvPoint)
PREDEFINE_QUERY(CSPInitMonthRank)
PREDEFINE_QUERY(CSPEventQuarterUpdate)  
PREDEFINE_QUERY(CSPGetItemName)
PREDEFINE_QUERY(CSPTournamentPayback)
PREDEFINE_QUERY(CSPTournamentResult)
PREDEFINE_QUERY(CSPTournamentApply)
PREDEFINE_QUERY(CSPTournamentClear)
PREDEFINE_QUERY(CSPTournamentStatus)
PREDEFINE_QUERY(CSPTnmtEventTime)
PREDEFINE_QUERY(CSPTnmtEventSchedule)
PREDEFINE_QUERY(CSPTnmtEventDel)
PREDEFINE_QUERY(CSPTnmtEventEntry)
PREDEFINE_QUERY(CSPTnmtEventReward)
PREDEFINE_QUERY(CSPHelpMessage)
PREDEFINE_QUERY(CSPRPSGameRecord)
PREDEFINE_QUERY(CSPGetLimitedLevel)
PREDEFINE_QUERY(CSPCMGiftCanTake)
PREDEFINE_QUERY(CSPCMGiftDel)
PREDEFINE_QUERY(CSPCMGiftSet)
PREDEFINE_QUERY(CSPCMGiftAdd)
PREDEFINE_QUERY(CSPAddBOWPlayer)
PREDEFINE_QUERY(CSPClearBOWPlayers)
PREDEFINE_QUERY(CSPDeleteSingleBOWPlayer)

PREDEFINE_QUERY(CSPAddBRPlayer)
PREDEFINE_QUERY(CSPClearBRPlayers)
PREDEFINE_QUERY(CSPDeleteSingleBRPlayer)
PREDEFINE_QUERY(CSPCash)



END_INIT_QUERY()

#if defined(FOURSTORY_NATIVE_ODBC) && !defined(FOURSTORY_NATIVE_MODULE)
DWORD ExecutePayback(CSqlDatabase& m_db,PACKETBUF* pBUF,const WorldDatabaseBatch& batch,BOOL& native_query_succeeded) {
    auto SayToBATCH=[&](LPPACKETBUF raw) {
        std::unique_ptr<PACKETBUF> owned(raw);
        batch(std::vector<BYTE>(owned->m_packet.GetBuffer(),owned->m_packet.GetBuffer()+owned->m_packet.GetSize()));
    };
#include "TournamentPaybackDatabase.inc"
}
DWORD ExecuteClear(CSqlDatabase& m_db,BOOL& native_query_succeeded) {
#include "TournamentClearDatabase.inc"
}
DWORD ExecuteDelete(CSqlDatabase& m_db,PACKETBUF* pBUF,BOOL& native_query_succeeded) {
#include "TournamentDeleteDatabase.inc"
}
#endif

#if defined(FOURSTORY_NATIVE_ODBC) && !defined(FOURSTORY_NATIVE_MODULE)

using namespace fourstory::server::world_data;
DWORD LoadWorldStartup(CSqlDatabase& m_db,WorldStartupData& state,const WorldStartupCallbacks& callbacks) {
    using CTime=fourstory::server::CalendarTime;
    auto MAKEWORD=[](BYTE low,BYTE high){return WORD(WORD(low)|(WORD(high)<<8));};
    auto min=[](float a,float b){return std::min(a,b);};
    BattleLogScope logs(callbacks.log);
    auto& m_arFirstGradeGroup=state.m_arFirstGradeGroup;
    auto& m_arMonthRank=state.m_arMonthRank;
    auto& m_bFirstGroupCount=state.m_bFirstGroupCount;
    auto& m_bMaxLevel=state.m_bMaxLevel;
    auto& m_bNation=state.m_bNation;
    auto& m_bRankMonth=state.m_bRankMonth;
    auto& m_battletime=state.m_battletime;
    auto& m_dwGenRecallID=state.m_dwGenRecallID;
    auto& m_dwRecentRecordDate=state.m_dwRecentRecordDate;
    auto& m_dwTacticsIndex=state.m_dwTacticsIndex;
    auto& m_mapCMGift=state.m_mapCMGift;
    auto& m_mapCharGuild=state.m_mapCharGuild;
    auto& m_mapCharTactics=state.m_mapCharTactics;
    auto& m_mapEVQT=state.m_mapEVQT;
    auto& m_mapRPSGame=state.m_mapRPSGame;
    auto& m_mapTGuild=state.m_mapTGuild;
    auto& m_mapTGuildEx=state.m_mapTGuildEx;
    auto& m_mapTGuildLevel=state.m_mapTGuildLevel;
    auto& m_mapTSvrMsg=state.m_mapTSvrMsg;
    auto& m_mapTimeEVQT=state.m_mapTimeEVQT;
    auto& m_mapTournament=state.m_mapTournament;
    auto& m_mapWarCountry=state.m_mapWarCountry;
    auto& m_pBOWModule=state.m_pBOWModule;
    auto& m_pBRModule=state.m_pBRModule;
    auto& m_qGenPartyID=state.m_qGenPartyID;
    auto& m_tournament=state.m_tournament;
    auto& m_vCUSTOMTIMES=state.m_vCUSTOMTIMES;
    auto& m_vTOPERATOR=state.m_vTOPERATOR;
    auto FindGuildTacticsWanted=[&](auto&&... args){return state.FindGuildTacticsWanted(std::forward<decltype(args)>(args)...);};
    auto AddGuildWanted=[&](auto&&... args){return state.AddGuildWanted(std::forward<decltype(args)>(args)...);};
    auto AddGuildTacticsWanted=[&](auto&&... args){return state.AddGuildTacticsWanted(std::forward<decltype(args)>(args)...);};
    auto AddGuildWantedApp=[&](auto&&... args){return state.AddGuildWantedApp(std::forward<decltype(args)>(args)...);};
    auto AddGuildTacticsWantedApp=[&](auto&&... args){return state.AddGuildTacticsWantedApp(std::forward<decltype(args)>(args)...);};
    auto GetRanking=[&](auto&&... args){return state.GetRanking(std::forward<decltype(args)>(args)...);};
    auto IsFirstGroup=[&](auto&&... args){return state.IsFirstGroup(std::forward<decltype(args)>(args)...);};
    auto GetTournament=[&](auto&&... args){return state.GetTournament(std::forward<decltype(args)>(args)...);};
    auto GetCurrentTournamentEntry=[&](auto&&... args){return state.GetCurrentTournamentEntry(std::forward<decltype(args)>(args)...);};
    auto TournamentGetEntryCount=[&](auto&&... args){return state.TournamentGetEntryCount(std::forward<decltype(args)>(args)...);};
    auto AddTNMTPlayer=[&](auto&&... args){return state.AddTNMTPlayer(std::forward<decltype(args)>(args)...);};
    auto FindTNMTPlayer=[&](auto&&... args){return state.FindTNMTPlayer(std::forward<decltype(args)>(args)...);};
    auto FindTGuild=[&](DWORD id)->CTGuild* {auto it=state.m_mapTGuild.find(id);return it==state.m_mapTGuild.end()?nullptr:it->second;};
    auto FindGuildLevel=[&](BYTE level){return find_guild_level(state.m_mapTGuildLevel,level);};
    auto BuildNetString=build_net_string;
    auto GetWarCountryGap=war_country_gap;
    auto CreateItem=create_database_item;
    auto GetNextEventTime=[&](BYTE day,BYTE hour,BYTE minute){return next_event_time(day,hour,minute,state.m_timeCurrent);};
    auto CalcGuildRanking=[&](){calculate_guild_ranking(state.m_mapTGuild);};
    auto SetTournamentTime=[&](LPMAPTOURNAMENTSTEP steps,TBATTLETIME battle,WORD id,BYTE month,BYTE enable,WORD current,BYTE group,BYTE step){return state.SetTournamentTime(steps,battle,id,month,enable,current,group,step,state.m_timeCurrent,callbacks.schedule);};
    auto TournamentSelectPlayer=[&](){state.TournamentSelectPlayer(callbacks.random,callbacks.bounded_random,callbacks.refund);};
#include "LoadStartupData.inc"
}
}
BOOL InitQueryTWorldSvr(CSqlDatabase* database) {
	return fourstory::server::queries::TWorldSvr::InitQueryTWorldSvr(database);
}
#endif

#if defined(FOURSTORY_NATIVE_ODBC) && !defined(FOURSTORY_NATIVE_MODULE)
namespace fourstory::server {
WorldDatabaseActionResult ExecuteOriginalWorldTournamentPayback(CSqlDatabase* database,PACKETBUF* packet,const WorldDatabaseBatch& batch) {
    if(!database || !packet || !batch) throw std::invalid_argument("Missing tournament refund dependencies");
    SMART_LOCKCS(&database->m_csExecution);
    auto* query=static_cast<queries::TWorldSvr::CSPTournamentPayback*>(database->GetQuery("CSPTournamentPayback"));
    if(!query) return {EC_INITSERVICE_PREPAREQUERY,false};
    query->m_dwPostID=0;
    BOOL executed=FALSE;
    const auto code=queries::TWorldSvr::ExecutePayback(*database,packet,batch,executed);
    return {code,executed!=FALSE};
}
WorldDatabaseActionResult ExecuteOriginalWorldTournamentClear(CSqlDatabase* database) {
    if(!database) throw std::invalid_argument("Missing tournament clear database");
    if(!database->GetQuery("CSPTournamentClear")) return {EC_INITSERVICE_PREPAREQUERY,false};
    BOOL executed=FALSE;
    const auto code=queries::TWorldSvr::ExecuteClear(*database,executed);
    return {code,executed!=FALSE};
}
WorldDatabaseActionResult ExecuteOriginalWorldTournamentDelete(CSqlDatabase* database,PACKETBUF* packet) {
    if(!database || !packet) throw std::invalid_argument("Missing tournament deletion dependencies");
    if(!database->GetQuery("CSPTnmtEventDel")) return {EC_INITSERVICE_PREPAREQUERY,false};
    BOOL executed=FALSE;
    const auto code=queries::TWorldSvr::ExecuteDelete(*database,packet,executed);
    return {code,executed!=FALSE};
}
}
#endif

#if defined(FOURSTORY_NATIVE_ODBC) && !defined(FOURSTORY_NATIVE_MODULE)
DWORD LoadOriginalWorldStartup(CSqlDatabase* database,fourstory::server::world_data::WorldStartupData* state,const fourstory::server::world_data::WorldStartupCallbacks& callbacks) {
    if(!database || !state || !callbacks.schedule.batch || !callbacks.schedule.updated || !callbacks.schedule.delete_database || !callbacks.random || !callbacks.bounded_random || !callbacks.refund || !callbacks.log) throw std::invalid_argument("Missing World startup dependencies");
    if(!database->GetQuery("CSPGetLimitedLevel")) return EC_INITSERVICE_PREPAREQUERY;
    if(!database->GetQuery("CSPGetNation")) return EC_INITSERVICE_PREPAREQUERY;
    if(!database->GetQuery("CSPGetRecallID")) return EC_INITSERVICE_PREPAREQUERY;
    if(!database->GetQuery("CTBLActiveCharTable")) return EC_INITSERVICE_PREPAREQUERY;
    if(!database->GetQuery("CTBLBOWSettingsChart")) return EC_INITSERVICE_PREPAREQUERY;
    if(!database->GetQuery("CTBLBRSettingsChart")) return EC_INITSERVICE_PREPAREQUERY;
    if(!database->GetQuery("CTBLBattleTimeChart")) return EC_INITSERVICE_PREPAREQUERY;
    if(!database->GetQuery("CTBLCMGiftChart")) return EC_INITSERVICE_PREPAREQUERY;
    if(!database->GetQuery("CTBLCastleApplicant")) return EC_INITSERVICE_PREPAREQUERY;
    if(!database->GetQuery("CTBLCustomTimeChart")) return EC_INITSERVICE_PREPAREQUERY;
    if(!database->GetQuery("CTBLEventQuarterChart")) return EC_INITSERVICE_PREPAREQUERY;
    if(!database->GetQuery("CTBLFirstGradeGroup")) return EC_INITSERVICE_PREPAREQUERY;
    if(!database->GetQuery("CTBLGuild")) return EC_INITSERVICE_PREPAREQUERY;
    if(!database->GetQuery("CTBLGuildArticle")) return EC_INITSERVICE_PREPAREQUERY;
    if(!database->GetQuery("CTBLGuildChart")) return EC_INITSERVICE_PREPAREQUERY;
    if(!database->GetQuery("CTBLGuildItem")) return EC_INITSERVICE_PREPAREQUERY;
    if(!database->GetQuery("CTBLGuildMember")) return EC_INITSERVICE_PREPAREQUERY;
    if(!database->GetQuery("CTBLGuildPvPRecord")) return EC_INITSERVICE_PREPAREQUERY;
    if(!database->GetQuery("CTBLGuildPvPointReward")) return EC_INITSERVICE_PREPAREQUERY;
    if(!database->GetQuery("CTBLGuildRelation")) return EC_INITSERVICE_PREPAREQUERY;
    if(!database->GetQuery("CTBLGuildStats")) return EC_INITSERVICE_PREPAREQUERY;
    if(!database->GetQuery("CTBLGuildTactics")) return EC_INITSERVICE_PREPAREQUERY;
    if(!database->GetQuery("CTBLGuildTacticsWanted")) return EC_INITSERVICE_PREPAREQUERY;
    if(!database->GetQuery("CTBLGuildVolunteer")) return EC_INITSERVICE_PREPAREQUERY;
    if(!database->GetQuery("CTBLGuildWanted")) return EC_INITSERVICE_PREPAREQUERY;
    if(!database->GetQuery("CTBLMonthPvPointChar")) return EC_INITSERVICE_PREPAREQUERY;
    if(!database->GetQuery("CTBLMonthPvPointTable")) return EC_INITSERVICE_PREPAREQUERY;
    if(!database->GetQuery("CTBLOperatorChart")) return EC_INITSERVICE_PREPAREQUERY;
    if(!database->GetQuery("CTBLPvPointTable")) return EC_INITSERVICE_PREPAREQUERY;
    if(!database->GetQuery("CTBLRPSGame")) return EC_INITSERVICE_PREPAREQUERY;
    if(!database->GetQuery("CTBLRPSGameRecord")) return EC_INITSERVICE_PREPAREQUERY;
    if(!database->GetQuery("CTBLSvrMsg")) return EC_INITSERVICE_PREPAREQUERY;
    if(!database->GetQuery("CTBLTnmtEventReward")) return EC_INITSERVICE_PREPAREQUERY;
    if(!database->GetQuery("CTBLTnmtEventSchedule")) return EC_INITSERVICE_PREPAREQUERY;
    if(!database->GetQuery("CTBLTnmtEventTime")) return EC_INITSERVICE_PREPAREQUERY;
    if(!database->GetQuery("CTBLTournament")) return EC_INITSERVICE_PREPAREQUERY;
    if(!database->GetQuery("CTBLTournamentCurrentStep")) return EC_INITSERVICE_PREPAREQUERY;
    if(!database->GetQuery("CTBLTournamentEvent")) return EC_INITSERVICE_PREPAREQUERY;
    if(!database->GetQuery("CTBLTournamentPlayer")) return EC_INITSERVICE_PREPAREQUERY;
    if(!database->GetQuery("CTBLTournamentReward")) return EC_INITSERVICE_PREPAREQUERY;
    if(!database->GetQuery("CTBLTournamentSchedule")) return EC_INITSERVICE_PREPAREQUERY;
    state->clear();
    state->m_timeCurrent=fourstory::server::CalendarTime::GetCurrentTime().GetTime();
    try {
        const auto code=fourstory::server::queries::TWorldSvr::LoadWorldStartup(*database,*state,callbacks);
        if(code) state->clear();
        return code;
    } catch(...) {state->clear();throw;}
}
#endif
