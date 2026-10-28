#if defined(FOURSTORY_NATIVE_ODBC) && !defined(FOURSTORY_NATIVE_MODULE)
#include "../../../../native/server/odbc_platform.hpp"
#include "../../Lib/Own/TProtocol/include/NetCode.h"
#include "../../Lib/Own/TProtocol/include/CTProtocol.h"
#include "../../../../native/server/login_startup_data.hpp"
#include "../TNetLib/ErrorCode.h"
namespace fourstory::server::queries::TLoginSvr {
#include "DBAccess.h"
#else
#include "StdAfx.h"
#endif

BEGIN_INIT_QUERY(TLoginSvr)
	PREDEFINE_QUERY(CTBLChannel)
	PREDEFINE_QUERY(CTBLGroup)
	PREDEFINE_QUERY(CTBLGroupList)
	PREDEFINE_QUERY(CTBLVeteranChart)

	PREDEFINE_QUERY(CSPCheckPasswd)
	PREDEFINE_QUERY(CSPLogout)
	PREDEFINE_QUERY(CSPLogin)
	PREDEFINE_QUERY(CSPCheckIP)

	PREDEFINE_QUERY(CSPRoute)
	PREDEFINE_QUERY(CSPVeteran)
	PREDEFINE_QUERY(CSPLoadService)
	PREDEFINE_QUERY(CSPTestLogin) // Çö½Â·æ CS_TESTLOGIN_REQ
	PREDEFINE_QUERY(CSPAgreement)

	PREDEFINE_QUERY(CTBLUserCount)
	PREDEFINE_QUERY(CSPClearLoginUser)
	PREDEFINE_QUERY(CSPGetNation)
	PREDEFINE_QUERY(CSPLoginJP)
	PREDEFINE_QUERY(CSPCheckForbiddenHWID)
	PREDEFINE_QUERY(CSPGetBanReason)
	PREDEFINE_QUERY(CSPAddNewMACAddress)
	PREDEFINE_QUERY(CSPGetClientSha)
#ifdef USE_HWID_AUTH
	PREDEFINE_QUERY(CSPCheckHwid)
	PREDEFINE_QUERY(CSPLogLoginAttempt)
#endif
END_INIT_QUERY()

BEGIN_INIT_QUERY(TGameSvr)
	PREDEFINE_QUERY(CTBLChar)
	PREDEFINE_QUERY(CTBLItem)

	PREDEFINE_QUERY(CSPGetGuildInfo)
	PREDEFINE_QUERY(CSPFindServerID)
	PREDEFINE_QUERY(CSPCreateChar)
	PREDEFINE_QUERY(CSPDeleteChar)
	PREDEFINE_QUERY(CSPFindBOWPlayer)
	PREDEFINE_QUERY(CSPFindBRPlayer)
END_INIT_QUERY()

#if defined(FOURSTORY_NATIVE_ODBC) && !defined(FOURSTORY_NATIVE_MODULE)
DWORD LoadStartupData(CSqlDatabase& m_db,LoginStartupData& state,const char* client_reference_path) {
    using MAPDWORD=std::map<DWORD,DWORD>;
    using TVETERAN=tagTVETERAN;
    auto& m_addrCtrl=state.control_address;auto& m_bNation=state.nation;
    auto& m_mapCurrentUser=state.current_users;auto& m_vVETERAN=state.veterans;
    auto& m_clientReference=state.client_reference;auto& m_dlCheckFile=state.client_checksum;
#include "LoadStartupData.inc"
}
void ClearLoginUsers(CSqlDatabase& m_db) {
#include "ClearLoginUsers.inc"
}
}
BOOL InitQueryTLoginSvr(CSqlDatabase* database) {
	return fourstory::server::queries::TLoginSvr::InitQueryTLoginSvr(database);
}
#endif

#if defined(FOURSTORY_NATIVE_ODBC) && !defined(FOURSTORY_NATIVE_MODULE)
void ClearOriginalLoginUsers(CSqlDatabase* database) {
	if(!database) throw std::invalid_argument("Null Login database");
	fourstory::server::queries::TLoginSvr::ClearLoginUsers(*database);
}
#endif

#if defined(FOURSTORY_NATIVE_ODBC) && !defined(FOURSTORY_NATIVE_MODULE)
DWORD LoadOriginalLoginStartup(CSqlDatabase* database,LoginStartupData* state,const char* path) {
    if(!database || !state) throw std::invalid_argument("Null Login startup state");
    if(!database->GetQuery("CSPLoadService") || !database->GetQuery("CSPGetNation") ||
       !database->GetQuery("CTBLUserCount") || !database->GetQuery("CTBLVeteranChart"))
        return EC_INITSERVICE_PREPAREQUERY;
    return fourstory::server::queries::TLoginSvr::LoadStartupData(*database,*state,path);
}
#endif
