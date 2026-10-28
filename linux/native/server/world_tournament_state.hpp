#pragma once
#include "world_data_types.hpp"
#include "calendar_time.hpp"
#include <memory>
#include "../../source/4Story 5.0 Source Files/Lib/Own/TProtocol/include/SSProtocol.h"
#include <functional>
namespace fourstory::server::world_data {
// Entry/player maps own each record once; stage/party maps are borrowed aliases.
class TournamentState {
public:
    MAPTOURNAMENT m_mapTournament;
    TOURNAMENTSTATUS m_tournament;
    MAPTNMTPLAYER m_mapTNMTPlayer;
    MAPTOURNAMENTSCHEDULE m_mapTournamentSchedule;
    MAPTOURNAMENTTIME m_mapTournamentTime;
    TOURNAMENTSCHEDULE m_TournamentSchedule{};
    WORD m_wTournamentID=0;
    struct ScheduleCallbacks {
        std::function<void(std::vector<BYTE>)> batch;
        std::function<void(WORD)> updated,delete_database;
    };
    TournamentState()=default;
    TournamentState(const TournamentState&)=delete;
    TournamentState& operator=(const TournamentState&)=delete;
    ~TournamentState() {clear();}
    void clear() {
        m_tournament=TOURNAMENTSTATUS{};
        for(auto& player:m_mapTNMTPlayer) delete player.second;
        m_mapTNMTPlayer.clear();
        for(auto& tournament:m_mapTournament)
            for(auto& entry:tournament.second) delete entry.second;
        m_mapTournament.clear();
        for(auto& schedule:m_mapTournamentSchedule) delete schedule.second.m_mapStep;
        m_mapTournamentSchedule.clear();m_mapTournamentTime.clear();
        m_TournamentSchedule={};m_wTournamentID=0;
    }
    void UpdateSchedule(WORD wTourID,const std::function<void(std::vector<BYTE>)>& batch,
                        const std::function<void()>& clear_database) {
        if(!batch || !clear_database) throw std::invalid_argument("Missing tournament update callbacks");
        for(const auto& schedule:m_mapTournamentSchedule)
            if(!schedule.second.m_mapStep || schedule.second.m_mapStep->size()>255)
                throw std::invalid_argument("Invalid tournament update step map");
        auto SayToBATCH=[&](LPPACKETBUF raw) {
            std::unique_ptr<PACKETBUF> owned(raw);
            batch(std::vector<BYTE>(owned->m_packet.GetBuffer(),owned->m_packet.GetBuffer()+owned->m_packet.GetSize()));
        };
        auto SendDM_TOURNAMENTCLEAR_REQ=clear_database;
#include "../../source/4Story 5.0 Source Files/Server/TWorldSvr/TournamentUpdate.inc"
    }
    void DeleteSchedule(WORD wTourID,const ScheduleCallbacks& callbacks) {
        if(!callbacks.updated || !callbacks.delete_database) throw std::invalid_argument("Missing tournament deletion callbacks");
        auto TournamentUpdate=callbacks.updated;
        auto SendDM_TNMTEVENTSCHEDULEDEL_REQ=callbacks.delete_database;
#include "../../source/4Story 5.0 Source Files/Server/TWorldSvr/DeleteTournamentSchedule.inc"
    }
    WORD SetTournamentTime(LPMAPTOURNAMENTSTEP pTour,TBATTLETIME battletime,WORD wTourID,
                           BYTE bMonthBase,BYTE bEnable,WORD wTnmtID,BYTE bTnmtGroup,BYTE bTnmtStep,
                           std::int64_t current_time,const ScheduleCallbacks& callbacks) {
        if(!callbacks.batch || !callbacks.updated || !callbacks.delete_database)
            throw std::invalid_argument("Missing tournament schedule callbacks");
        const auto existing=m_mapTournamentSchedule.find(wTourID);
        if(existing!=m_mapTournamentSchedule.end() && existing->second.m_mapStep!=pTour)
            throw std::invalid_argument("Conflicting tournament schedule identity");
        for(const auto& schedule:m_mapTournamentSchedule)
            if(!schedule.second.m_mapStep || (schedule.second.m_mapStep==pTour && schedule.first!=wTourID))
                throw std::invalid_argument("Invalid tournament schedule ownership");
        using CTime=fourstory::server::CalendarTime;
        auto MAKEWORD=[](BYTE low,BYTE high){return static_cast<WORD>(WORD(low)|(WORD(high)<<8));};
        auto DelTournamentSchedule=[&](WORD id){DeleteSchedule(id,callbacks);};
        auto SayToBATCH=[&](LPPACKETBUF raw) {
            std::unique_ptr<PACKETBUF> owned(raw);
            callbacks.batch(std::vector<BYTE>(owned->m_packet.GetBuffer(),owned->m_packet.GetBuffer()+owned->m_packet.GetSize()));
        };
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
#include "../../source/4Story 5.0 Source Files/Server/TWorldSvr/SetTournamentTime.inc"
#pragma clang diagnostic pop
    }
    LPMAPTOURNAMENTENTRY GetTournament(WORD wTournamentID) {
#include "../../source/4Story 5.0 Source Files/Server/TWorldSvr/GetTournament.inc"
    }
    LPTOURNAMENTENTRY GetCurrentTournamentEntry(BYTE bEntry) {
#include "../../source/4Story 5.0 Source Files/Server/TWorldSvr/GetCurrentTournamentEntry.inc"
    }
    LPTNMTPLAYER FindTNMTPlayer(DWORD dwCharID) {
#include "../../source/4Story 5.0 Source Files/Server/TWorldSvr/FindTournamentPlayer.inc"
    }
    BYTE TournamentGetEntryCount() {
#include "../../source/4Story 5.0 Source Files/Server/TWorldSvr/TournamentEntryCount.inc"
    }
    void AddTNMTPlayer(LPTOURNAMENTENTRY pEntry,LPTNMTPLAYER pPlayer,BYTE bStep,LPTNMTPLAYER pChief) {
        if(!pEntry || !pPlayer || !pChief) throw std::invalid_argument("Missing tournament registration record");
        const auto existing=m_mapTNMTPlayer.find(pPlayer->m_dwCharID);
        if(existing!=m_mapTNMTPlayer.end() && existing->second!=pPlayer)
            throw std::invalid_argument("Conflicting tournament player identity");
#include "../../source/4Story 5.0 Source Files/Server/TWorldSvr/AddTournamentPlayer.inc"
    }
    void TNMTMatch(LPTOURNAMENTENTRY pEntry,MAPTNMTPLAYER map1st[TCONTRY_COUNT],
                   MAPTNMTPLAYER mapNormal[TCONTRY_COUNT],BYTE bTotal) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
#include "../../source/4Story 5.0 Source Files/Server/TWorldSvr/TournamentMatch.inc"
#pragma clang diagnostic pop
    }
    void TournamentSelectPlayer(const std::function<int()>& random,
                                const std::function<DWORD(DWORD)>& bounded_random,
                                const std::function<void(DWORD,DWORD)>& refund) {
        if(!random || !bounded_random || !refund) throw std::invalid_argument("Missing tournament selection callbacks");
        // Database rows must be safe before indexing the original country arrays.
        for(const auto& value:m_mapTNMTPlayer)
            if(!value.second || value.second->m_bCountry>=TCONTRY_COUNT)
                throw std::invalid_argument("Invalid tournament player country");
        auto rand=random;
        auto TRand=[&](DWORD bound) {
            const auto result=bounded_random(bound);
            if((bound>0 && result>=bound) || (bound==0 && result!=0))
                throw std::invalid_argument("Tournament random value outside range");
            return result;
        };
        MAPTNMTPLAYER surviving=m_mapTNMTPlayer;
        auto SendDM_TOURNAMENTPAYBACK_REQ=[&](DWORD id,DWORD fee) {
            refund(id,fee);
            surviving.erase(id); // Source immediately deletes the refunded record.
        };
        try {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
#include "../../source/4Story 5.0 Source Files/Server/TWorldSvr/TournamentSelectPlayer.inc"
#pragma clang diagnostic pop
        } catch(...) {
            m_mapTNMTPlayer=std::move(surviving);
            throw;
        }
    }
};
}
