#include "../world_startup_helpers.hpp"
#include <cassert>
#include <iostream>
using namespace fourstory::server::world_data;
int main() {
    assert(next_mission_start({300,100,200},50)==100);
    assert(next_mission_start({300,100,200},100)==100);
    assert(next_mission_start({300,100,200},101)==200);
    assert(next_mission_start({300,100,200},301)==100);
    assert(next_mission_start({0,3600},86399)==0);
    bool rejected=false;try{next_mission_start({},0);}catch(const std::invalid_argument&){rejected=true;}assert(rejected);
    rejected=false;try{next_mission_start({86400},0);}catch(const std::invalid_argument&){rejected=true;}assert(rejected);
    assert(war_country_gap(BROA_BASELEVEL-1)==WARCOUNTRY_MAXGAP);
    assert(war_country_gap(BROA_BASELEVEL)==0 && war_country_gap(BROA_BASELEVEL+9)==0 && war_country_gap(BROA_BASELEVEL+10)==1);
    CString encoded(std::string(1,static_cast<char>(0xe9)));
    assert(build_net_string(encoded,"body").bytes()==std::string("00010004")+encoded.bytes()+"body");
    assert(build_net_string("","")=="00000000");
    TGUILDLEVEL level{};MAPTGUILDLEVEL levels{{1,&level}};
    assert(find_guild_level(levels,1)==&level && find_guild_level(levels,2)==nullptr);
    CTGuild first,second,third,zero;
    first.m_dwPvPTotalPoint=100;first.m_dwPvPMonthPoint=10;
    second.m_dwPvPTotalPoint=100;second.m_dwPvPMonthPoint=30;
    third.m_dwPvPTotalPoint=50;
    MAPTGUILD guilds{{1,&first},{2,&second},{3,&third},{4,&zero}};
    calculate_guild_ranking(guilds);
    assert(first.m_dwRankTotal==1 && second.m_dwRankTotal==1 && third.m_dwRankTotal==3);
    assert(first.m_dwRankMonth==2 && second.m_dwRankMonth==1 && third.m_dwRankMonth==0 && zero.m_dwRankTotal==0);
    first.m_dwPvPTotalPoint=0;calculate_guild_ranking(guilds);
    assert(first.m_dwRankTotal==0 && third.m_dwRankTotal==2);
    std::cout<<"PASS original World guild ranking ties/zero/reset, chart lookup, level gaps and byte-length net strings\n";
}
