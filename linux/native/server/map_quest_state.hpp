#pragma once
#include "map_data_types.hpp"
namespace fourstory::server::map_data {
class QuestState {
public:
    VQUESTTEMP m_vQUESTTEMP; // Own roots; each original QUESTTEMP owns its children.
    MAPQUESTTEMP m_mapQUESTTEMP; // Borrowed index of the full tree.
    MAPMAPVQUESTTEMP m_mapTRIGGER; // Own index containers; contained quests are borrowed.
    QuestState()=default;
    QuestState(const QuestState&)=delete;
    QuestState& operator=(const QuestState&)=delete;
    ~QuestState(){clear();}
    void clear() {
        for(auto& type:m_mapTRIGGER) {
            for(auto& trigger:*type.second) delete trigger.second;
            delete type.second;
        }
        m_mapTRIGGER.clear();m_mapQUESTTEMP.clear();
        for(auto* root:m_vQUESTTEMP) delete root;
        m_vQUESTTEMP.clear();
    }
    void AddQuestTemp(LPQUESTTEMP pQUEST) {
        if(!pQUEST) throw std::invalid_argument("Missing Map quest template");
        if(m_mapQUESTTEMP.count(pQUEST->m_dwQuestID)) throw std::invalid_argument("Duplicate Map quest identity");
#include "../../source/4Story 5.0 Source Files/Server/TMapSvr/AddQuestTemp.inc"
    }
};
}

extern void LoadOriginalMapQuest(CSqlDatabase*,fourstory::server::map_data::QuestState*,
    fourstory::server::map_data::LPQUESTTEMP,
    fourstory::server::map_data::MAPTSTORAGEITEM&,
    const fourstory::server::map_data::MAPTMONSTERTEMP&);

// Owns the full root forest; failure clears partial state. Item and monster
// dependencies must be the actual previously loaded Map chart objects.
extern void LoadOriginalMapQuestRoots(CSqlDatabase*,fourstory::server::map_data::QuestState*,
    fourstory::server::map_data::MAPTSTORAGEITEM&,
    const fourstory::server::map_data::MAPTMONSTERTEMP&);
