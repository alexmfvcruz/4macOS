#include "../world_guild_recruitment.hpp"
#include <cassert>
#include <iostream>
using namespace fourstory::server::world_data;
int main() {
    GuildRecruitmentState state;state.m_timeCurrent=2000;
    CTGuild guild;guild.m_dwID=17;guild.m_bCountry=TCONTRY_D;guild.m_strName="Recruiters";
    assert(state.AddGuildWanted(nullptr,10,30,"Title","Text",0)==GUILD_FAIL);
    assert(state.AddGuildWanted(&guild,10,30,"Title","Text",0)==GUILD_SUCCESS);
    auto* wanted=state.FindGuildWanted(17);
    assert(wanted && wanted->m_dlEndTime==2000+GUILDWANTED_PERIOD);
    assert(state.AddGuildWantedApp(999,1,"A",20,2)==GUILD_FAIL);
    assert(state.AddGuildWantedApp(17,1,"A",9,2)==GUILD_MISMATCHLEVEL);
    assert(state.AddGuildWantedApp(17,1,"A",31,2)==GUILD_MISMATCHLEVEL);
    assert(state.AddGuildWantedApp(17,1,"A",20,2)==GUILD_SUCCESS);
    auto* app=state.FindGuildWantedApp(1);
    assert(app && wanted->m_mapTWantedApp.at(1)==app && app->m_dwRegion==0);
    assert(state.AddGuildWantedApp(17,1,"A",20,2)==GUILD_SAME);
    assert(state.AddGuildWantedApp(18,1,"A",20,2)==GUILD_AREADYAPPLY);
    assert(state.AddGuildWanted(&guild,10,30,"Changed","Text",1999)==GUILD_SUCCESS);
    assert(state.FindGuildWanted(17)==wanted && wanted->m_mapTWantedApp.at(1)==app);
    assert(state.AddGuildWantedApp(17,2,"B",20,2)==GUILD_WANTEDEND);
    wanted->m_dlEndTime=2000;
    assert(state.AddGuildWantedApp(17,2,"B",20,2)==GUILD_SUCCESS); // Original strict < boundary.
    TCHARACTER character{};character.m_bCountry=TCONTRY_C;character.m_bAidCountry=TCONTRY_N;
    assert(state.AddGuildWantedApp(17,3,"C",20,2,&character)==GUILD_FAIL);
    character.m_bCountry=TCONTRY_D;character.m_pTactics=&guild;
    assert(state.AddGuildWantedApp(17,3,"C",20,2,&character)==GUILD_SAMEGUILDTACTICS);
    for(DWORD id=1;id<=MAX_TACTICSWANTED;++id)
        assert(state.AddGuildTacticsWanted(&guild,id,99,7,5,9,3,10,30,"Tactics","Text",3000)==GUILD_SUCCESS);
    assert(state.AddGuildTacticsWanted(&guild,6,99,7,5,9,3,10,30,"Tactics","Text",3000)==GUILD_MAXWANTED);
    auto* tactics=state.FindGuildTacticsWanted(17,1);
    assert(tactics && state.FindGuildTacticsWanted(1)==tactics);
    assert(state.AddGuildTacticsWantedApp(1,4,"D",20,2,17)==GUILD_SUCCESS);
    auto* tactics_app=state.FindGuildTacticsWantedApp(4);
    assert(tactics_app && tactics->m_mapTWantedApp.at(4)==tactics_app);
    assert(tactics_app->m_dwPoint==99 && tactics_app->m_dwGold==7 && tactics_app->m_dwSilver==5 && tactics_app->m_dwCooper==9 && tactics_app->m_bDay==3);
    assert(state.AddGuildTacticsWantedApp(2,4,"D",20,2,17)==GUILD_SAME);
    assert(state.AddGuildTacticsWantedApp(1,4,"D",20,2,18)==GUILD_AREADYAPPLY);
    character.m_pTactics=nullptr;character.m_bCountry=TCONTRY_C;character.m_bAidCountry=TCONTRY_D;
    assert(state.AddGuildTacticsWantedApp(1,5,"E",20,2,17,&character)==GUILD_SUCCESS);
    character.m_pGuild=&guild;
    assert(state.AddGuildTacticsWantedApp(1,6,"F",20,2,17,&character)==GUILD_SAMEGUILDTACTICS);
    state.clear();state.clear();
    assert(state.m_mapTGuildWantedApp.empty() && state.m_mapTGuildTacticsWantedApp.empty());
    std::cout<<"PASS original World recruitment limits, eligibility, expiry, reward fields, aliases and cleanup\n";
}
