#include "../world_tournament_state.hpp"
#include "../../packet/wire.hpp"
#include <cassert>
#include <iostream>
#include <memory>
using namespace fourstory::server;
using namespace fourstory::server::world_data;
int main() {
    TournamentState state;unsigned updates=0,deletes=0,packets=0;
    TournamentState::ScheduleCallbacks callbacks{
        [&](std::vector<BYTE> bytes){story::Writer expected(SM_TOURNAMENTEVENT_ACK);expected.u32(0);expected.u8(TET_SCHEDULEDEL);expected.u16(5);assert(bytes==expected.finish());++packets;},
        [&](WORD id){assert(id==5);++updates;},[&](WORD id){assert(id==5);++deletes;}};
    TBATTLETIME battle;std::memset(&battle,0,sizeof(battle));battle.m_bWeek=1;battle.m_bDay=1;battle.m_dwBattleStart=3600;
    const auto now=CalendarTime(2024,1,1,0,0,0).GetTime();
    auto schedule=std::make_unique<MAPTOURNAMENTSTEP>();
    TOURNAMENTSTEP first{};first.m_bStep=2;first.m_bGroup=1;first.m_dwPeriod=60;
    TOURNAMENTSTEP second{};second.m_bStep=3;second.m_bGroup=1;second.m_dwPeriod=120;
    schedule->emplace(258,first);schedule->emplace(259,second);
    assert(state.SetTournamentTime(schedule.get(),battle,5,0,TRUE,0,0,TNMTSTEP_READY,now,callbacks)==5);
    auto* owned=schedule.release();
    const auto start=CalendarTime(2024,1,7,1,0,0).GetTime();
    assert(owned->at(258).m_dStart==start && owned->at(258).m_dEnd==start+60);
    assert(owned->at(259).m_dStart==start+60 && owned->at(259).m_dEnd==start+180);
    assert(state.m_wTournamentID==5 && state.m_mapTournamentSchedule.at(5).m_mapStep==owned);
    // Original built-in schedule collision emits deletion of the prior event.
    auto collision=std::make_unique<MAPTOURNAMENTSTEP>();collision->emplace(258,first);
    assert(state.SetTournamentTime(collision.get(),battle,1,0,TRUE,0,0,TNMTSTEP_READY,now,callbacks)==0);
    assert(state.m_mapTournamentSchedule.empty() && updates==1 && deletes==1 && packets==1);
    auto december=std::make_unique<MAPTOURNAMENTSTEP>();december->emplace(258,first);
    const auto december_now=CalendarTime(2024,12,31,0,0,0).GetTime();
    assert(state.SetTournamentTime(december.get(),battle,6,1,TRUE,0,0,TNMTSTEP_READY,december_now,callbacks)==6);
    assert(december->at(258).m_dStart==CalendarTime(2025,1,5,1,0,0).GetTime());december.release();
    unsigned update_packets=0,clears=0;
    state.UpdateSchedule(6,[&](std::vector<BYTE> bytes){
        story::Writer expected(SM_TOURNAMENTUPDATE_REQ);expected.u16(6);expected.u8(1);
        expected.u8(first.m_bGroup);expected.u8(first.m_bStep);expected.u32(first.m_dwPeriod);
        expected.i64(CalendarTime(2025,1,5,1,0,0).GetTime());
        assert(bytes==expected.finish());++update_packets;
    },[&]{++clears;});
    assert(state.m_TournamentSchedule.m_wID==6 && state.m_TournamentSchedule.m_mapStep==state.m_mapTournamentSchedule.at(6).m_mapStep);
    state.clear();
    state.UpdateSchedule(0,[&](std::vector<BYTE> bytes){story::Writer expected(SM_TOURNAMENTUPDATE_REQ);expected.u16(0);expected.u8(0);assert(bytes==expected.finish());++update_packets;},[&]{++clears;});
    assert(update_packets==2 && clears==2 && !state.m_TournamentSchedule.m_mapStep);
    std::cout<<"PASS original tournament nth-weekday/period scheduling, December rollover, overlap deletion packet and callbacks\n";
}
