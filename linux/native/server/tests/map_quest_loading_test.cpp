#include "../map_quest_state.hpp"
namespace fourstory::server::queries::TMapSvr {
using namespace fourstory::server::map_data;
using TITEMQUERY=fourstory::server::map_data::tagTITEMQUERY;
#include "../../../source/4Story 5.0 Source Files/Server/TMapSvr/DBAccess.h"
}
#include <cassert>
#include <iostream>
using namespace fourstory::server::map_data;
using namespace fourstory::server::queries::TMapSvr;
int main() {
 CSqlDatabase database;QuestState state;MAPTSTORAGEITEM items;MAPTMONSTERTEMP monsters;
 auto* root=new QUESTTEMP();root->m_dwQuestID=100;
 state.m_vQUESTTEMP.push_back(root);state.AddQuestTemp(root);
 bool refused=false;
 try {LoadOriginalMapQuestRoots(&database,&state,items,monsters);}
 catch(const std::invalid_argument&) {refused=true;}
 assert(refused && state.m_mapQUESTTEMP.at(100)==root);
 assert(database.AddQuery(new CTBLQuestChart(&database),"CTBLQuestChart"));
 assert(database.AddQuery(new CTBLQuestConditionChart(&database),"CTBLQuestConditionChart"));
 assert(database.AddQuery(new CTBLQuestRewardChart(&database),"CTBLQuestRewardChart"));
 assert(database.AddQuery(new CTBLQuestTermChart(&database),"CTBLQuestTermChart"));
 refused=false;
 try {LoadOriginalMapQuestRoots(&database,&state,items,monsters);}
 catch(const std::runtime_error&) {refused=true;}
 assert(refused && state.m_vQUESTTEMP.empty() && state.m_mapQUESTTEMP.empty() && state.m_mapTRIGGER.empty());
 // A failed actual query must be retryable without stale root aliases.
 refused=false;
 try {LoadOriginalMapQuestRoots(&database,&state,items,monsters);}
 catch(const std::runtime_error&) {refused=true;}
 assert(refused && state.m_vQUESTTEMP.empty());
 std::cout<<"PASS original Map root loader dependency rejection, database failure, cleared ownership and retry\n";
}
