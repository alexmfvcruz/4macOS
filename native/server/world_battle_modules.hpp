#pragma once
#include "world_battle_schedule.hpp"
#include "calendar_time.hpp"
namespace fourstory::server::world_data {
#include "../../source/4Story 5.0 Source Files/Server/TWorldSvr/BowSystem.h"
// Scope the original static logging API to the current startup/runtime call.
inline thread_local std::function<void(const CString&)> battle_logger;
class BattleLogScope {
    std::function<void(const CString&)> previous;
public:
    explicit BattleLogScope(std::function<void(const CString&)> logger):previous(std::move(battle_logger)) {
        if(!logger) {battle_logger=std::move(previous);throw std::invalid_argument("Missing World battle logger");}
        battle_logger=std::move(logger);
    }
    ~BattleLogScope(){battle_logger=std::move(previous);}
};
inline void CTBowSystem::Log(CString value) {
    if(!battle_logger) throw std::logic_error("Missing World battle log scope");
    battle_logger(value);
}
inline void CBRSystem::Log(CString value) {CTBowSystem::Log(value);}
inline CTBowSystem::CTBowSystem(WORD wMapID,BYTE bMinPlayersCount,BYTE bMaxNationDifference) {
#include "../../source/4Story 5.0 Source Files/Server/TWorldSvr/CTBowSystemConstruct.inc"
}
inline CTBowSystem::~CTBowSystem() {
#include "../../source/4Story 5.0 Source Files/Server/TWorldSvr/CTBowSystemDestroy.inc"
}
inline void CTBowSystem::Release() {
#define BOW_TEAM_COUNT 2
#define BOW_MAX_POINTS 10
#include "../../source/4Story 5.0 Source Files/Server/TWorldSvr/CTBowSystemRelease.inc"
#undef BOW_TEAM_COUNT
#undef BOW_MAX_POINTS
}
inline void CTBowSystem::SetDurations(DWORD dwAlarmDur,DWORD dwBuyTimeDur,DWORD dwBattleDur) {
#include "../../source/4Story 5.0 Source Files/Server/TWorldSvr/CTBowSystemSetDurations.inc"
}
inline CBRSystem::CBRSystem(DWORD dwAlarmDur,DWORD dwBuyTimeDur,DWORD dwBattleDur,BYTE bMinPlayerCount) {
#define DEFAULT_BR_MAP (WORD)710
#include "../../source/4Story 5.0 Source Files/Server/TWorldSvr/CBRSystemConstruct.inc"
#undef DEFAULT_BR_MAP
}
inline CBRSystem::~CBRSystem() {
#include "../../source/4Story 5.0 Source Files/Server/TWorldSvr/CBRSystemDestroy.inc"
}
inline void CBRSystem::Release() {
#include "../../source/4Story 5.0 Source Files/Server/TWorldSvr/CBRSystemRelease.inc"
}
inline void CBRSystem::AddMap(WORD wMapID,CString strName) {
#include "../../source/4Story 5.0 Source Files/Server/TWorldSvr/CBRSystemAddMap.inc"
}
inline void validate_original_battle_times(const VECTORDWORD& times) {
    if(times.empty() || times.size()>255) throw std::invalid_argument("Invalid World battle time count");
    for(DWORD time:times) if(time>=86400) throw std::invalid_argument("Invalid World battle time");
}
inline void CTBowSystem::Init() {
    validate_original_battle_times(m_vTIMES);
    auto time=fourstory::server::CalendarTime::GetCurrentTime();
    const DWORD current_seconds=time.GetHour()*3600+time.GetMinute()*60+time.GetSecond();
    using std::sort;
#include "../../source/4Story 5.0 Source Files/Server/TWorldSvr/BowInitializeSchedule.inc"
}
inline void CBRSystem::Init() {
    validate_original_battle_times(m_vTIMES);m_vBattleType.clear();
    auto time=fourstory::server::CalendarTime::GetCurrentTime();
    const DWORD current_seconds=time.GetHour()*3600+time.GetMinute()*60+time.GetSecond();
    using std::sort;
#include "../../source/4Story 5.0 Source Files/Server/TWorldSvr/BRInitializeSchedule.inc"
}
}
