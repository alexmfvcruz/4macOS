#if defined(FOURSTORY_NATIVE_ODBC) && !defined(FOURSTORY_NATIVE_MODULE)
#include "../../../../native/server/odbc_platform.hpp"
#include "../../Lib/Own/TProtocol/include/NetCode.h"
#include "ManagerClasses.h"
#include "../../Lib/Own/TProtocol/include/CTProtocol.h"
#include "../../../../native/server/control_startup_data.hpp"
#include "../TNetLib/ErrorCode.h"
#include <queue>
using std::queue;
#include "../TNetLib/TNetDef.h"
namespace fourstory::server::queries::TControlSvr {
#include "TControlConstants.h"
#include "DBAccess.h"
#else
#include "StdAfx.h"
#endif

BEGIN_INIT_QUERY(TControlSvr)
	PREDEFINE_QUERY(CTBLMachine)
	PREDEFINE_QUERY(CTBLNetwork)
	PREDEFINE_QUERY(CTBLGroup)
	PREDEFINE_QUERY(CTBLSvrType)
	PREDEFINE_QUERY(CTBLServer)
	PREDEFINE_QUERY(CTBLIPAddr)
	PREDEFINE_QUERY(CTBLManager)
	PREDEFINE_QUERY(CTBLEvent)
	PREDEFINE_QUERY(CTBLPreVersion)

	PREDEFINE_QUERY(CSPUpdatePatch)
	PREDEFINE_QUERY(CSPUserProtectedAdd)
	PREDEFINE_QUERY(CSPSvrStatusSMS)
	PREDEFINE_QUERY(CSPLoadService)
	PREDEFINE_QUERY(CSPOPLogin)
	PREDEFINE_QUERY(CSPEventUpdate)
	PREDEFINE_QUERY(CTBLCashShopItem)
	PREDEFINE_QUERY(CSPUpdatePrePatch)
	PREDEFINE_QUERY(CSPBetaToVer)
	PREDEFINE_QUERY(CSPDeletePreVersion)
	PREDEFINE_QUERY(CSPUserProtectedAdd)

END_INIT_QUERY()

#if defined(FOURSTORY_NATIVE_ODBC) && !defined(FOURSTORY_NATIVE_MODULE)
void ParseEventValue(control_data::EVENTINFO& stEVENTINFO,CString str,BYTE bEventID) {
    using namespace control_data;
#include "ParseEventValue.inc"
}
DWORD LoadStartupData(CSqlDatabase& m_db,control_data::StartupData& state,const std::function<void()>& platform) {
    using namespace control_data;
    state.clear();
    auto& m_mapTSVRTEMP=state.services;auto& m_mapTMachine=state.machines;
    auto& m_mapTGroup=state.groups;auto& m_mapTSvrType=state.types;
    auto& m_svraddr=state.address;auto& m_strMyAddr=state.own_address;
    auto& m_dwEventIndex=state.event_index;auto& m_mapEVENT=state.events;
    constexpr DWORD DCSVC_STAT_STOPPED=1;
    auto lstrcmp=[](const char* a,const char* b){return std::strcmp(a,b);};
    auto OpenPlatform=platform;
    auto ParseStrValue=ParseEventValue;
#include "LoadStartupData.inc"
}
BOOL AuthenticateManager(CSqlDatabase& m_db,const char* id,const char* password,BYTE& authority) {
    DEFINE_QUERY(&m_db, CSPOPLogin)
    // Query storage is fixed-width; never truncate credentials or reuse an old return value.
    if(std::strlen(id)>=sizeof(query->m_szID) || std::strlen(password)>=sizeof(query->m_szPW)) return FALSE;
    std::strcpy(query->m_szID,id);
    std::strcpy(query->m_szPW,password);
    query->m_nRET=0;
    if(!query->Call() || query->m_nRET<MANAGER_ALL || query->m_nRET>=MANAGER_COUNT) return FALSE;
    authority=static_cast<BYTE>(query->m_nRET);
    UNDEFINE_QUERY()
    return TRUE;
}
#endif

#if defined(FOURSTORY_NATIVE_ODBC) && !defined(FOURSTORY_NATIVE_MODULE)
}
BOOL InitQueryTControlSvr(CSqlDatabase* database) {
	return fourstory::server::queries::TControlSvr::InitQueryTControlSvr(database);
}
#endif

#if defined(FOURSTORY_NATIVE_ODBC) && !defined(FOURSTORY_NATIVE_MODULE)
BOOL AuthenticateOriginalControlManager(CSqlDatabase* database,const char* id,const char* password,BYTE* authority) {
    if(!authority) return FALSE;
    *authority=0;
    if(!database || !id || !password || !database->GetQuery("CSPOPLogin")) return FALSE;
    return fourstory::server::queries::TControlSvr::AuthenticateManager(*database,id,password,*authority);
}
#endif

#if defined(FOURSTORY_NATIVE_ODBC) && !defined(FOURSTORY_NATIVE_MODULE)
DWORD LoadOriginalControlStartup(CSqlDatabase* database,fourstory::server::control_data::StartupData* state,
    const std::function<void()>& platform) {
    if(!database || !state || !platform) throw std::invalid_argument("Missing Control startup dependencies");
    for(const auto* name:{"CTBLMachine","CTBLNetwork","CTBLGroup","CTBLSvrType","CTBLServer","CTBLIPAddr","CSPLoadService","CTBLEvent"})
        if(!database->GetQuery(name)) return EC_INITSERVICE_PREPAREQUERY;
    return fourstory::server::queries::TControlSvr::LoadStartupData(*database,*state,platform);
}
#endif
