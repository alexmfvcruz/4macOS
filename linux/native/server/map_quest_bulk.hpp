#pragma once
// Include after the Map query declarations. This cache lives only while the
// original quest forest is assembled; gameplay queries still use ODBC.
#include <tuple>
#include <type_traits>
#include <chrono>
#include <cstdlib>

namespace fourstory::server::quest_bulk {
template<class Q> struct Traits;
#define QUEST_TRAITS(Query, Key, Sql, ...) \
    template<> struct Traits<Query> { \
        static constexpr auto key = &Query::Key; \
        static const char* sql() { return Sql; } \
        static auto fields(Query& q) { return std::tie(__VA_ARGS__); } \
    };
QUEST_TRAITS(CTBLQuestChart, m_dwParentID,
    "SELECT dwQuestID,dwTriggerID,bTriggerType,bForceRun,bCountMax,bType,bConditionCheck,dwParentID FROM TQUESTCHART ORDER BY bMain DESC",
    q.m_dwQuestID,q.m_dwTriggerID,q.m_bTriggerType,q.m_bForceRun,q.m_bCountMax,q.m_bType,q.m_bConditionCheck)
QUEST_TRAITS(CTBLQuestConditionChart, m_dwQuestID,
    "SELECT dwConditionID,bConditionType,bCount,dwQuestID FROM TQCONDITIONCHART ORDER BY bConditionType DESC",
    q.m_dwConditionID,q.m_bConditionType,q.m_bCount)
QUEST_TRAITS(CTBLQuestRewardChart, m_dwQuestID,
    "SELECT dwRewardID,bRewardType,bTakeMethod,bTakeData,bCount,dwQuestID FROM TQREWARDCHART WITH (NOLOCK)",
    q.m_dwRewardID,q.m_bRewardType,q.m_bTakeMethod,q.m_bTakeData,q.m_bCount)
QUEST_TRAITS(CTBLQuestTermChart, m_dwQuestID,
    "SELECT dwTermID,bTermType,bCount,dwQuestID FROM TQUESTTERMCHART WITH (NOLOCK) ORDER BY dwID",
    q.m_dwTermID,q.m_bTermType,q.m_bCount)
#undef QUEST_TRAITS

template<class Tuple> auto copy_fields(const Tuple& fields) {
    return std::apply([](const auto&... value) {return std::make_tuple(value...);}, fields);
}

template<class Q> class Source : public Q {
public:
    DWORD group = 0;
    explicit Source(CSqlDatabase* db) : Q(db) { this->Init(db, Traits<Q>::sql()); }
    int GetNumParam() override {return 0;}
    int GetNumCol() override {return Q::GetNumCol()+1;}
    SQLRETURN BindParameters(BOOL = FALSE) override {return SQL_SUCCESS;}
    SQLRETURN BindColumns() override {
        auto result=Q::BindColumns();
        return SQL_SUCCEEDED(result) ? this->BindCol(GetNumCol(), &group) : result;
    }
};

template<class Q> class Rows {
    using Row=decltype(copy_fields(Traits<Q>::fields(std::declval<Q&>())));
    std::map<DWORD,std::vector<Row>> groups;
    const std::vector<Row>* current = nullptr;
    std::size_t position = 0;
    bool passthrough = false;
public:
    std::size_t count = 0, requests = 0, database_requests = 0;
    void load(CSqlDatabase* db) {
        Source<Q> source(db);
        native_query_open(static_cast<CSqlBase&>(source));
        while(native_query_fetch(static_cast<CSqlBase&>(source))) {
            for(int col=0;col<source.GetNumCol();++col)
                if(source.IsNull(col)) throw std::runtime_error("NULL in bulk quest startup chart");
            groups[source.group].push_back(copy_fields(Traits<Q>::fields(source)));
            ++count;
        }
        source.Close();
    }
    bool open(Q& query) {
        ++requests;
        auto found=groups.find(query.*Traits<Q>::key);
        current=found==groups.end() ? nullptr : &found->second;
        position=0;
        // Keep the original query for multi-row groups whose tied/unspecified
        // ordering could affect the root or trigger vectors. Terms have the
        // original globally unique dwID ordering in both paths.
        passthrough = current && current->size()>1 && !std::is_same_v<Q,CTBLQuestTermChart>;
        if(passthrough) ++database_requests;
        return passthrough ? native_query_open(static_cast<CSqlBase&>(query)) : true;
    }
    bool fetch(Q& query) {
        if(passthrough) return native_query_fetch(static_cast<CSqlBase&>(query));
        if(!current || position==current->size()) return false;
        Traits<Q>::fields(query)=(*current)[position++];
        return true;
    }
};

struct Cache {
    Rows<CTBLQuestChart> quests;
    Rows<CTBLQuestConditionChart> conditions;
    Rows<CTBLQuestRewardChart> rewards;
    Rows<CTBLQuestTermChart> terms;
};
inline thread_local Cache* active = nullptr;
class Scope {
    Cache cache;
    Cache* previous = active;
public:
    explicit Scope(CSqlDatabase* db) {
        const char* setting=std::getenv("FOURSTORY_BULK_QUESTS");
        if(setting && std::strcmp(setting,"0")==0) return;
        auto start=std::chrono::steady_clock::now();
        cache.quests.load(db);cache.conditions.load(db);
        cache.rewards.load(db);cache.terms.load(db);
        active=&cache;
        auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count();
        std::fprintf(stderr,"MAP_QUEST_BULK preload_queries=4 quests=%zu conditions=%zu rewards=%zu terms=%zu load_ms=%lld\n",
            cache.quests.count,cache.conditions.count,cache.rewards.count,cache.terms.count,static_cast<long long>(ms));
    }
    ~Scope() {
        if(active==&cache) {
            auto requests=cache.quests.requests+cache.conditions.requests+cache.rewards.requests+cache.terms.requests;
            auto database=cache.quests.database_requests+cache.conditions.database_requests+cache.rewards.database_requests+cache.terms.database_requests;
            std::fprintf(stderr,"MAP_QUEST_CACHE requests=%zu database_queries=%zu cache_hits=%zu\n",requests,database+4,requests-database);
        }
        active=previous;
    }
    Scope(const Scope&)=delete;
    Scope& operator=(const Scope&)=delete;
};
}
#define QUEST_CACHED(Query, Member) \
    inline bool native_query_open(Query& q) { \
        auto* cache=fourstory::server::quest_bulk::active; \
        return cache ? cache->Member.open(q) : native_query_open(static_cast<CSqlBase&>(q)); \
    } \
    inline bool native_query_fetch(Query& q) { \
        auto* cache=fourstory::server::quest_bulk::active; \
        return cache ? cache->Member.fetch(q) : native_query_fetch(static_cast<CSqlBase&>(q)); \
    }
QUEST_CACHED(CTBLQuestChart, quests)
QUEST_CACHED(CTBLQuestConditionChart, conditions)
QUEST_CACHED(CTBLQuestRewardChart, rewards)
QUEST_CACHED(CTBLQuestTermChart, terms)
#undef QUEST_CACHED
