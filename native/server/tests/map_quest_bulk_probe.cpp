// Read-only live-database equivalence/performance probe. No service process,
// startup procedures, player writes or cluster control operations are called.
#include "../map_quest_state.hpp"
using namespace fourstory::server::map_data;
using TITEMQUERY=fourstory::server::map_data::tagTITEMQUERY;
#include "../../../source/4Story 5.0 Source Files/Server/TMapSvr/DBAccess.h"
#include "../map_quest_bulk.hpp"
#include <iostream>
#include <sstream>
#include <cassert>

#define FOURSTORY_NATIVE_MODULE
class Loader : public QuestState {
    CSqlDatabase& m_db;
    MAPTSTORAGEITEM m_mapTQuestItem;
    TMONSTER monster{};
public:
    explicit Loader(CSqlDatabase& db) : m_db(db) {}
    LPTMONSTER FindTMonster(WORD) {return &monster;}
    void LoadQuestTemp(LPQUESTTEMP pQUEST) {
#include "../../../source/4Story 5.0 Source Files/Server/TMapSvr/LoadQuestTemp.inc"
    }
    void load() {
#include "../../../source/4Story 5.0 Source Files/Server/TMapSvr/LoadQuestRoots.inc"
    }
    std::string snapshot() {
        std::ostringstream out;
        for(auto* root:m_vQUESTTEMP) out<<"root "<<root->m_dwQuestID<<'\n';
        for(auto& entry:m_mapQUESTTEMP) {
            auto* q=entry.second;
            out<<q->m_dwQuestID<<','<<q->m_dwParentID<<','<<q->m_dwTriggerID<<','<<int(q->m_bTriggerType)<<','
               <<int(q->m_bForceRun)<<','<<int(q->m_bCountMax)<<','<<int(q->m_bType)<<','<<int(q->m_bConditionCheck)<<'\n';
            for(auto& child:q->m_mapCHILD) out<<"child "<<child.first<<'\n';
            for(auto* c:q->m_vCondition) out<<"condition "<<c->m_dwConditionID<<','<<int(c->m_bConditionType)<<','<<int(c->m_bCount)<<'\n';
            for(auto* r:q->m_vReward) out<<"reward "<<r->m_dwRewardID<<','<<int(r->m_bRewardType)<<','<<int(r->m_bTakeMethod)<<','<<int(r->m_bTakeData)<<','<<int(r->m_bCount)<<'\n';
            for(auto* t:q->m_vTerm) out<<"term "<<t->m_dwTermID<<','<<int(t->m_bTermType)<<','<<int(t->m_bCount)<<'\n';
        }
        for(auto& type:m_mapTRIGGER) for(auto& trigger:*type.second) {
            out<<"trigger "<<int(type.first)<<','<<trigger.first<<':';
            for(auto* q:*trigger.second) out<<q->m_dwQuestID<<',';
            out<<'\n';
        }
        out<<"monsterflag "<<int(monster.m_bCheckPartyQuest)<<'\n';
        return out.str();
    }
};
std::size_t first_difference(const std::string& baseline, const std::string& snapshot) {
    return std::mismatch(baseline.begin(),baseline.end(),snapshot.begin(),snapshot.end()).first-baseline.begin();
}
int main(int argc, char** argv) {
    if(argc==2 && std::strcmp(argv[1],"--self-test")==0) {
        assert(first_difference("same prefix then more","same prefix")==11);
        CSqlDatabase closed;
        CTBLQuestChart query(&closed);query.m_dwParentID=1;
        fourstory::server::quest_bulk::Rows<CTBLQuestChart> empty;
        assert(empty.open(query) && !empty.fetch(query));
        fourstory::server::quest_bulk::Cache previous;
        fourstory::server::quest_bulk::active=&previous;
        setenv("FOURSTORY_BULK_QUESTS","1",1);
        bool failed=false;
        try {fourstory::server::quest_bulk::Scope scope(&closed);}
        catch(const std::runtime_error&) {failed=true;}
        assert(failed && fourstory::server::quest_bulk::active==&previous);
        fourstory::server::quest_bulk::active=nullptr;
        setenv("FOURSTORY_BULK_QUESTS","0",1);
        {fourstory::server::quest_bulk::Scope disabled(&closed);assert(!fourstory::server::quest_bulk::active);}
        assert(!fourstory::server::quest_bulk::active);
        std::cout<<"PASS shortened snapshot, empty lookup, failed preload cleanup, disabled cache\n";
        return 0;
    }
    const char* password=std::getenv("FOURSTORY_DB_PASSWORD");
    if(!password) {std::cerr<<"Set FOURSTORY_DB_PASSWORD through the private local environment\n";return 2;}
    CSqlDatabase db;
    if(!db.Open("4Story_Native_Game","sa",password)) {std::cerr<<"Database connection failed\n";return 2;}
    db.AddQuery(new CTBLQuestChart(&db),"CTBLQuestChart");
    db.AddQuery(new CTBLQuestConditionChart(&db),"CTBLQuestConditionChart");
    db.AddQuery(new CTBLQuestRewardChart(&db),"CTBLQuestRewardChart");
    db.AddQuery(new CTBLQuestTermChart(&db),"CTBLQuestTermChart");
    std::string baseline;
    for(int pass=0;pass<2;++pass) {
        setenv("FOURSTORY_BULK_QUESTS",pass ? "1" : "0",1);
        auto start=std::chrono::steady_clock::now();
        Loader loader(db);loader.load();
        if(fourstory::server::quest_bulk::active) throw std::runtime_error("Quest cache escaped startup scope");
        auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count();
        auto snapshot=loader.snapshot();
        std::cout<<(pass ? "bulk" : "baseline")<<" quests="<<loader.m_mapQUESTTEMP.size()<<" roots="<<loader.m_vQUESTTEMP.size()<<" ms="<<ms<<" snapshot_bytes="<<snapshot.size()<<std::endl;
        if(!pass) baseline=std::move(snapshot);
        else if(snapshot!=baseline) {auto at=first_difference(baseline,snapshot); std::cerr<<"FAIL mismatch at "<<at<<" old="<<baseline.substr(at,160)<<" new="<<snapshot.substr(at,160)<<"\n";return 1;}
    }
    std::cout<<"PASS exact ordered quest/tree/trigger snapshots match\n";
}
