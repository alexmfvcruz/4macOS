#include "../map_quest_state.hpp"
#include <cassert>
#include <iostream>
using namespace fourstory::server::map_data;
int main() {
 QuestState state;
 auto* root=new QUESTTEMP();root->m_dwQuestID=100;root->m_bTriggerType=TT_KILLMON;root->m_dwTriggerID=50;
 state.m_vQUESTTEMP.push_back(root);state.AddQuestTemp(root);
 auto* child=new QUESTTEMP();child->m_dwQuestID=101;child->m_dwParentID=100;child->m_bTriggerType=TT_KILLMON;child->m_dwTriggerID=50;
 root->m_mapCHILD.emplace(101,child);state.AddQuestTemp(child);
 child->m_vCondition.push_back(new QUESTCONDITION());child->m_vTerm.push_back(new QUESTTERM());child->m_vReward.push_back(new QUESTREWARD());
 assert(state.m_mapQUESTTEMP.at(101)==child);
 auto* trigger=state.m_mapTRIGGER.at(TT_KILLMON)->at(50);
 assert(trigger->size()==2 && trigger->at(0)==root && trigger->at(1)==child);
 bool rejected=false;try{state.AddQuestTemp(child);}catch(const std::invalid_argument&){rejected=true;}assert(rejected && trigger->size()==2);
 state.clear();state.clear();assert(state.m_mapQUESTTEMP.empty() && state.m_mapTRIGGER.empty() && state.m_vQUESTTEMP.empty());
 std::cout<<"PASS original Map quest trigger indexing, child aliases, duplicate rejection and populated recursive cleanup\n";
}
