// TWorldSvr.cpp : WinMain의 구현입니다.

#include "stdafx.h"
#include "TWorldSvr.h"
#include "TWorldSvrModule.h"

#include <random>
CTWorldSvrModule _AtlModule;

extern "C" int WINAPI _tWinMain(HINSTANCE /*hInstance*/, HINSTANCE /*hPrevInstance*/, 
                                LPTSTR /*lpCmdLine*/, int nShowCmd)
{
    return _AtlModule.WinMain(nShowCmd);
}


CTWorldSvrModule::CTWorldSvrModule()
{
	memset( m_szDBPasswd, 0, ONE_KBYTE);
	memset( m_szDBUserID, 0, ONE_KBYTE);
	memset( m_szDSN, 0, ONE_KBYTE);

	m_wPort = DEF_WORLDPORT;
	m_bNumWorker = 0;
	m_bServerID = 0;
	m_bGroupID = 0;
	m_dwTMSIndex = 0;
	m_dwTacticsIndex = 0;
	m_dwRecentRecordDate = 0;
	m_dwGenRecallID = 0;
	m_wTournamentID = 1;

	m_bFirstGroupCount = FIRSTGRADEGROUPCOUNT;

	for( int i=0; i<MAX_THREAD; i++)
		m_hWorker[i] = NULL;

	m_bThreadRun = FALSE;
	m_bBatchRun = TRUE;
	m_bDBRun = TRUE;
	m_bTimerRun = TRUE;

	m_hIocpControl = NULL;
	m_hIocpWork = NULL;

	m_hBatchEvent = NULL;
	m_hDBEvent = NULL;
	m_hTimerEvent = NULL;
	m_hControl = NULL;
	m_hBatch = NULL;
	m_hDB = NULL;
	m_hTimer = NULL;

	m_mapTCHARNAME.clear();
	m_mapTCHAR.clear();
	m_mapACTIVEUSER.clear();

	m_mapSESSION.clear();
	m_mapSERVER.clear();
	m_qGenPartyID.empty();
	m_mapApplicant.clear();

	m_mapTGuildLevel.clear();
	m_mapTGuildWanted.clear();
	m_mapTGuildTacticsWanted.clear();
	m_mapTGuildWantedApp.clear();
	m_mapTGuildTacticsWantedApp.clear();
	m_mapCMGift.clear();

	for(BYTE i=0; i<WARCOUNTRY_MAXGAP; i++)
	{
		m_mapWarCountry[TCONTRY_D][i].clear();
		m_mapWarCountry[TCONTRY_C][i].clear();
	}

	m_TournamentSchedule.m_wID = 0;
	m_TournamentSchedule.m_mapStep = NULL;

	m_mapTGuild.clear();
	m_mapTGuildEx.clear();
	m_mapEVQT.clear();
	m_mapTimeEVQT.clear();
	m_mapTSvrMsg.clear();
	m_vExpired.clear();
	m_vCUSTOMTIMES.clear();

	m_accept = INVALID_SOCKET;

	m_timeCurrent = 0;
	m_bReservedPostMinute = 0;
	m_pRelay = NULL;
	m_pCtrlSvr = NULL;

	m_bCashItemSale = FALSE;  
	m_mapEVENT.clear();

	m_mapCharGuild.clear();
	m_mapCharTactics.clear();

	 
	for(BYTE i = 0; i < COUNTRY_COUNT; i++)	
		for(BYTE j = 0; j < MONTHRANKCOUNT; j++)		
			m_arMonthRank[i][j].Reset();
			
	
	for(BYTE i = 0; i < COUNTRY_COUNT; i++)	
		for(BYTE j = 0; j < FIRSTGRADEGROUPCOUNT; j++)			
			m_arFirstGradeGroup[i][j].Reset();		
	
	m_bFameRankSave = FALSE;

#ifdef __TW_APEX
	m_pApexRecv = NULL;
#endif
}

CTWorldSvrModule::~CTWorldSvrModule()
{
#ifdef __TW_APEX
	if(m_bNation == NATION_TAIWAN)
		CHSEnd();
#endif
}

void CTWorldSvrModule::OnERROR( DWORD dwErrorCode)
{
	LogEvent("ErrorCode %d",dwErrorCode);
}

DWORD CTWorldSvrModule::_ControlThread( LPVOID lpParam)
{
	CTWorldSvrModule *pModule = (CTWorldSvrModule *) lpParam;
	return pModule->ControlThread();
}

DWORD CTWorldSvrModule::_WorkThread( LPVOID lpParam)
{
	CTWorldSvrModule *pModule = (CTWorldSvrModule *) lpParam;
	return pModule->WorkThread();
}

DWORD CTWorldSvrModule::_BatchThread( LPVOID lpParam)
{
	CTWorldSvrModule *pModule = (CTWorldSvrModule *) lpParam;
	return pModule->BatchThread();
}

DWORD CTWorldSvrModule::_DBThread( LPVOID lpParam)
{
	CTWorldSvrModule *pModule = (CTWorldSvrModule *) lpParam;
	return pModule->DBThread();
}

DWORD CTWorldSvrModule::_TimerThread( LPVOID lpParam)
{
	CTWorldSvrModule *pModule = (CTWorldSvrModule *) lpParam;
	return pModule->TimerThread();
}

DWORD CTWorldSvrModule::OnEnter()
{
	while(!m_qBATCHJOB.empty())
	{
		delete m_qBATCHJOB.front();
		m_qBATCHJOB.pop();
	}

	while(!m_qDBJOB.empty())
	{
		delete m_qDBJOB.front();
		m_qDBJOB.pop();
	}

	while(!m_qTIMERJOB.empty())
	{
		delete m_qTIMERJOB.front();
		m_qTIMERJOB.pop();
	}

	m_mapSESSION.clear();
	m_mapSERVER.clear();

	InitializeCriticalSectionAndSpinCount(&m_csDBQUEUE, 4000);
	InitializeCriticalSectionAndSpinCount(&m_csQUEUE, 4000);
	InitializeCriticalSectionAndSpinCount(&m_csBATCH, 4000);
	InitializeCriticalSectionAndSpinCount(&m_csTIMERQUEUE, 4000);

	m_hBatchEvent = CreateEvent(
		NULL,
		FALSE,
		FALSE,
		NULL);

	m_hDBEvent = CreateEvent(
		NULL,
		FALSE,
		FALSE,
		NULL);

	m_hTimerEvent = CreateEvent(
		NULL,
		FALSE,
		FALSE,
		NULL);

	DWORD dwResult = LoadConfig();
	if(dwResult)
		return dwResult;

	dwResult = InitDB();
	if(dwResult)
		return dwResult;

	dwResult = LoadData();
	if(dwResult)
		return dwResult;

	dwResult = CreateThreads();
	if(dwResult)
		return dwResult;

	dwResult = InitNetwork();
	if(dwResult)
		return dwResult;

	if(!ResumeThreads())
		return EC_INITSERVICE_RESUMETHREAD;

	if(!m_tournament.m_wID)
		TournamentUpdate();

#ifdef __TW_APEX
	if(m_bNation == NATION_TAIWAN)
	{
		CHSStart(ApexSendToMap, m_pApexRecv);
		CHSSetFunc(ApexKillUser,FLAG_KILLUSER);
	}
#endif

	// Dump
	CTMiniDump::SetOption(MiniDumpWithFullMemory);

	return 0;
}

void CTWorldSvrModule::OnExit()
{
	if( m_accept != INVALID_SOCKET )
		closesocket(m_accept);

	m_listen.Close();

	SaveAllPvPRecord();

	if(m_bThreadRun)
		ClearThread();

	while(!m_qBATCHJOB.empty())
	{
		delete m_qBATCHJOB.front();
		m_qBATCHJOB.pop();
	}

	while(!m_qDBJOB.empty())
	{
		delete m_qDBJOB.front();
		m_qDBJOB.pop();
	}

	while(!m_qTIMERJOB.empty())
	{
		delete m_qTIMERJOB.front();
		m_qTIMERJOB.pop();
	}
	UpdateData();

	SetEvent(m_hBatchEvent);
	CloseHandle(m_hBatchEvent);

	SetEvent(m_hDBEvent);
	CloseHandle(m_hDBEvent);

	SetEvent(m_hTimerEvent);
	CloseHandle(m_hTimerEvent);

	DeleteCriticalSection(&m_csDBQUEUE);
	DeleteCriticalSection(&m_csQUEUE);
	DeleteCriticalSection(&m_csBATCH);
	DeleteCriticalSection(&m_csTIMERQUEUE);

	m_db.Close();
	WSACleanup();
}

DWORD CTWorldSvrModule::LoadConfig()
{
	//Load config from registry
	DWORD dwLength;
	DWORD dwValue;
	HKEY hKey;

	CString strRegKey;
	strRegKey.Empty();

//#ifdef _DEBUG
//	HKEY hOpenKey = HKEY_CURRENT_USER;
//	strRegKey.Format(_T("software\\%s"), m_szServiceName);
//#else
	HKEY hOpenKey = HKEY_LOCAL_MACHINE;
	strRegKey.Format(_T("SYSTEM\\CurrentControlSet\\Services\\%s\\Config"), m_szServiceName);
//#endif

	int nERROR = RegCreateKey( hOpenKey, strRegKey, &hKey);
	if( nERROR != ERROR_SUCCESS )
		return EC_INITSERVICE_OPENREG;

	// Load game database password
	dwLength = ONE_KBYTE;
	nERROR = RegQueryValueEx(
		hKey,
		_T("DBPasswd"),
		NULL,
		NULL,
		(LPBYTE) &m_szDBPasswd,
		&dwLength);
	if( nERROR != ERROR_SUCCESS )
		return EC_INITSERVICE_PASSWDNOTASSIGNED;

	// Load DB user ID
	dwLength = ONE_KBYTE;
	nERROR = RegQueryValueEx(
		hKey,
		_T("DBUser"),
		NULL,
		NULL,
		(LPBYTE) &m_szDBUserID,
		&dwLength);
	if( nERROR != ERROR_SUCCESS )
		return EC_INITSERVICE_PASSWDNOTASSIGNED;

	// Load account database DSN
	dwLength = ONE_KBYTE;
	nERROR = RegQueryValueEx(
		hKey,
		_T("DSN"),
		NULL,
		NULL,
		(LPBYTE) &m_szDSN,
		&dwLength);
	if( nERROR != ERROR_SUCCESS )
		return EC_INITSERVICE_DSNNOTASSIGNED;

	// Load group ID
	dwLength = sizeof(DWORD);
	nERROR = RegQueryValueEx(
		hKey,
		_T("GroupID"),
		NULL,
		NULL,
		(LPBYTE) &dwValue,
		&dwLength);
	if( nERROR != ERROR_SUCCESS )
		return EC_INITSERVICE_SVRIDNOTASSIGNED;
	else
		m_bGroupID = (BYTE) dwValue;

	// Load server ID
	dwLength = sizeof(DWORD);
	nERROR = RegQueryValueEx(
		hKey,
		_T("ServerID"),
		NULL,
		NULL,
		(LPBYTE) &dwValue,
		&dwLength);
	if( nERROR != ERROR_SUCCESS )
		return EC_INITSERVICE_SVRIDNOTASSIGNED;
	else
		m_bServerID = (BYTE) dwValue;

	// Load listen port
	dwLength = sizeof(DWORD);
	nERROR = RegQueryValueEx(
		hKey,
		_T("Port"),
		NULL,
		NULL,
		(LPBYTE) &dwValue,
		&dwLength);
	if( nERROR != ERROR_SUCCESS )
		return EC_INITSERVICE_PORTNOTASSIGNED;
	else
		m_wPort = (WORD) dwValue;

	return EC_NOERROR;
}

DWORD CTWorldSvrModule::CreateThreads()
{
	DWORD dwThread = 0;
	SYSTEM_INFO vINFO;

	m_hControl = CreateThread(
		NULL, 0,
		_ControlThread,
		(LPVOID) this,
		CREATE_SUSPENDED,
		&dwThread);

	if(!m_hControl)
		return EC_INITSERVICE_CREATETHREAD;

	m_hBatch = CreateThread(
		NULL, 0,
		_BatchThread,
		(LPVOID) this,
		CREATE_SUSPENDED,
		&dwThread);
	m_bBatchRun = TRUE;

	if(!m_hBatch)
		return EC_INITSERVICE_CREATETHREAD;

	m_hDB = CreateThread(
		NULL, 0,
		_DBThread,
		(LPVOID) this,
		CREATE_SUSPENDED,
		&dwThread);
	m_bDBRun = TRUE;

	if(!m_hDB)
		return EC_INITSERVICE_CREATETHREAD;

	m_hTimer = CreateThread(
		NULL, 0,
		_TimerThread,
		(LPVOID) this,
		CREATE_SUSPENDED,
		&dwThread);
	m_bTimerRun = TRUE;

	if(!m_hTimer)
		return EC_INITSERVICE_CREATETHREAD;

	GetSystemInfo(&vINFO);
	m_bNumWorker = (BYTE) (2 * vINFO.dwNumberOfProcessors);

	for( BYTE i=0; i<m_bNumWorker; i++)
	{
		m_hWorker[i] = CreateThread(
			NULL, 0,
			_WorkThread,
			(LPVOID) this,
			CREATE_SUSPENDED,
			&dwThread);

		if(!m_hWorker[i])
			return EC_INITSERVICE_CREATETHREAD;
	}

	return EC_NOERROR;
}

BYTE CTWorldSvrModule::ResumeThreads()
{
	if( ResumeThread(m_hControl) < 0 )
		return FALSE;

	if( ResumeThread(m_hBatch) < 0 )
		return FALSE;

	if( ResumeThread(m_hDB) < 0 )
		return FALSE;

	if( ResumeThread(m_hTimer) < 0 )
		return FALSE;

	for( BYTE i=0; i<m_bNumWorker; i++)
		if( ResumeThread(m_hWorker[i]) < 0 )
			return FALSE;
	m_bThreadRun = TRUE;

	return TRUE;
}

DWORD CTWorldSvrModule::InitNetwork()
{
	WSADATA wsaDATA;
	WORD wVersionRequested = MAKEWORD( 2, 2);

	int nERROR = WSAStartup( wVersionRequested, &wsaDATA);
	if(nERROR)
		return EC_INITSERVICE_SOCKLIBFAILED;

	if( LOBYTE(wsaDATA.wVersion) != 2 ||
		HIBYTE(wsaDATA.wVersion) != 2 )
		return EC_INITSERVICE_INVALIDSOCKLIB;

	m_hIocpControl = CreateIoCompletionPort(
		INVALID_HANDLE_VALUE,
		NULL, 0, 0);

	if(!m_hIocpControl)
		return EC_INITSERVICE_CREATEIOCP;

	m_hIocpWork = CreateIoCompletionPort(
		INVALID_HANDLE_VALUE,
		NULL, 0, 0);

	if(!m_hIocpWork)
		return EC_INITSERVICE_CREATEIOCP;

	if(!m_listen.Listen(m_wPort))
		return EC_INITSERVICE_LISTENFAILED;

	m_hIocpControl = CreateIoCompletionPort(
		(HANDLE) m_listen.m_sock,
		m_hIocpControl,
		COMP_ACCEPT, 0);

	if(!m_hIocpControl)
		return EC_INITSERVICE_CREATEIOCP;

	if(!WaitForConnect())
		return EC_INITSERVICE_WAITFORCONNECT;

	return EC_NOERROR;
}

BYTE CTWorldSvrModule::WaitForConnect()
{
	DWORD dwRead = 0;

	if(!CSession::CreateSocket(m_accept))
		return FALSE;

	if(!AcceptEx(
		m_listen.m_sock,
		m_accept,
		m_vAccept.GetBuffer(), 0,
		sizeof(SOCKADDR) + 16,
		sizeof(SOCKADDR) + 16,
		&dwRead,
		(LPOVERLAPPED) &m_ovAccept) &&
		WSAGetLastError() != ERROR_IO_PENDING )
		return FALSE;

	return TRUE;
}

BYTE CTWorldSvrModule::Accept()
{
	if(m_accept == INVALID_SOCKET)
	{
		m_vAccept.Clear();
		return FALSE;
	}

	CTServer *pSERVER = new CTServer();

	pSERVER->Open( m_accept, m_vAccept);
	pSERVER->m_Recv.ExpandIoBuffer(RECV_SVR_SIZE);
	
	m_accept = INVALID_SOCKET;
	m_vAccept.Clear();

	HANDLE hIocpWork = CreateIoCompletionPort(
		(HANDLE) pSERVER->m_sock,
		m_hIocpWork,
		COMP_SESSION, 0);

	SMART_LOCKCS(&m_csBATCH);

	m_mapSESSION.insert( MAPTSERVER::value_type( (DWORD_PTR) pSERVER, pSERVER));
	ATLTRACE2("SESSION INSERT %s, %d\n", inet_ntoa(pSERVER->m_addr.sin_addr), m_mapSESSION.size());

	if( !hIocpWork || !pSERVER->WaitForMessage() )
	{
		pSERVER->Close();
		m_mapSESSION.erase((DWORD_PTR) pSERVER);
		delete pSERVER;

		return FALSE;
	}

	return TRUE;
}

void CTWorldSvrModule::ClearThread()
{
	PostQueuedCompletionStatus(
		m_hIocpWork, 0,
		COMP_EXIT, NULL);

	WaitForMultipleObjects(
		m_bNumWorker,
		m_hWorker,
		TRUE, INFINITE);

	for( BYTE i=0; i<m_bNumWorker; i++)
		CloseHandle(m_hWorker[i]);

	PostQueuedCompletionStatus(
		m_hIocpControl, 0,
		COMP_EXIT, NULL);

	WaitForSingleObject( m_hControl, INFINITE);
	CloseHandle(m_hControl);

	EnterCriticalSection(&m_csQUEUE);
	m_bBatchRun = FALSE;
	LeaveCriticalSection(&m_csQUEUE);
	SetEvent(m_hBatchEvent);
	WaitForSingleObject( m_hBatch, INFINITE);

	EnterCriticalSection(&m_csDBQUEUE);
	m_bDBRun = FALSE;
	LeaveCriticalSection(&m_csDBQUEUE);
	SetEvent(m_hDBEvent);
	WaitForSingleObject( m_hDB, INFINITE);

	EnterCriticalSection(&m_csTIMERQUEUE);
	m_bTimerRun = FALSE;
	LeaveCriticalSection(&m_csTIMERQUEUE);
	SetEvent(m_hTimerEvent);
	WaitForSingleObject( m_hTimer, INFINITE);

	CloseHandle(m_hIocpControl);
	CloseHandle(m_hIocpWork);
}

void CTWorldSvrModule::UpdateData()
{
	MAPTCHARACTER::iterator itCHAR;
	MAPTSERVER::iterator itSERVER;
	MAPTPARTY::iterator itParty;
	MAPTGUILDLEVEL::iterator itGuildLevel;
	MAPTGUILD::iterator itGuild;
	MAPTCORPS::iterator itCorps;
	MAPTMS::iterator itTMS;
	MAPTEVENTQUARTER::iterator itEQT;
	MAPTGUILDWANTEDAPP::iterator itGWA;
	MAPTGUILDTACTICSWANTEDAPP::iterator itGTWA;
	MAPTOURNAMENT::iterator itTNMT;
	MAPTOURNAMENTSCHEDULE::iterator itTNMTS;
	MAPCMGIFT::iterator itCMGift;

	for(itEQT = m_mapEVQT.begin(); itEQT != m_mapEVQT.end(); itEQT++)
		delete (*itEQT).second;

	for(itTMS = m_mapTMS.begin(); itTMS != m_mapTMS.end(); itTMS++)
		delete (*itTMS).second;

	for( itSERVER = m_mapSESSION.begin(); itSERVER != m_mapSESSION.end(); itSERVER++)
		delete (*itSERVER).second;

	for( itCHAR = m_mapTCHAR.begin(); itCHAR != m_mapTCHAR.end(); itCHAR++)
		delete (*itCHAR).second;

	for( itParty = m_mapTParty.begin(); itParty != m_mapTParty.end(); itParty++)
		delete (*itParty).second;

	for( itGuildLevel = m_mapTGuildLevel.begin(); itGuildLevel != m_mapTGuildLevel.end(); itGuildLevel++)
		delete (*itGuildLevel).second;

	for( itGuild = m_mapTGuild.begin(); itGuild != m_mapTGuild.end(); itGuild++)
		delete (*itGuild).second;

	for( itCorps = m_mapTCorps.begin(); itCorps != m_mapTCorps.end(); itCorps++)
		delete (*itCorps).second;

	for(itGWA = m_mapTGuildWantedApp.begin(); itGWA != m_mapTGuildWantedApp.end(); itGWA++)
		delete (*itGWA).second;

	for(itGTWA = m_mapTGuildTacticsWantedApp.begin(); itGTWA != m_mapTGuildTacticsWantedApp.end(); itGTWA++)
		delete (*itGTWA).second;

	for(itTNMT = m_mapTournament.begin(); itTNMT != m_mapTournament.end(); itTNMT++)
	{
		MAPTOURNAMENTENTRY::iterator itTNMTE;
		for(itTNMTE=(*itTNMT).second.begin(); itTNMTE!=(*itTNMT).second.end(); itTNMTE++)
			delete (*itTNMTE).second;
	}

	for(itTNMTS = m_mapTournamentSchedule.begin(); itTNMTS!=m_mapTournamentSchedule.end(); itTNMTS++)
		delete (*itTNMTS).second.m_mapStep;

	for(BYTE i=0; i<WARCOUNTRY_MAXGAP; i++)
	{
		m_mapWarCountry[TCONTRY_D][i].clear();
		m_mapWarCountry[TCONTRY_C][i].clear();
	}
	
	for( itCMGift = m_mapCMGift.begin(); itCMGift != m_mapCMGift.end(); itCMGift++)
		delete (*itCMGift).second;

	m_vTOPERATOR.clear();
	m_mapTMS.clear();
	m_mapTCHARNAME.clear();
	m_mapTCHAR.clear();
	m_mapACTIVEUSER.clear();
	m_mapTParty.clear();
	m_mapTGuildLevel.clear();
	m_mapTGuild.clear();
	m_mapTGuildEx.clear();
	m_mapTCorps.clear();
	m_mapTimeEVQT.clear();
	m_mapEVQT.clear();
	m_mapTSvrMsg.clear();
	m_mapTGuildWanted.clear();
	m_mapTGuildTacticsWanted.clear();
	m_mapTGuildWantedApp.clear();
	m_mapTGuildTacticsWantedApp.clear();
	m_mapSESSION.clear();
	m_mapSERVER.clear();
	m_mapTournament.clear();
	m_qGenPartyID.empty();
	m_vExpired.clear();
	m_mapTournamentSchedule.clear();
	m_mapTournamentTime.clear();
	m_mapRank.clear();
	m_mapMonthRank.clear();
	m_mapCMGift.clear();
	m_vCUSTOMTIMES.clear();
}

void CTWorldSvrModule::OnInvalidSession( CTWorldSession *pSession)
{
	if(pSession->OnInvalidSession())
		ClosingSession(pSession);
}

void CTWorldSvrModule::OnCloseSession( CTWorldSession *pSession)
{
	CTServer *pSERVER = (CTServer *) pSession;
	SMART_LOCKCS(&m_csBATCH);

	MAPTSERVER::iterator finder = m_mapSESSION.find((DWORD_PTR) pSERVER);
	if( finder == m_mapSESSION.end() )
		return;
	m_mapSESSION.erase(finder);
	ATLTRACE2("SESSION DELETE %d, %d\n", pSERVER->m_wID, m_mapSESSION.size());

	finder = m_mapSERVER.find(pSERVER->m_wID);
	if( finder != m_mapSERVER.end() )
		m_mapSERVER.erase(finder);

	if((CTWorldSession *)m_pRelay == pSession)
	{
		m_pRelay = NULL;

		MAPTSERVER::iterator it;
		for(it=m_mapSERVER.begin(); it!=m_mapSERVER.end(); it++)
			(*it).second->SendMW_RELAYCONNECT_REQ(0, FALSE);
	}
	else if( (CTWorldSession *)m_pCtrlSvr == pSession)	
		m_pCtrlSvr = NULL;	

	delete pSERVER;
}

void CTWorldSvrModule::ClosingSession( CTWorldSession *pSession)
{
	// pSession에 대한 패킷처리가 완료되는 시점을 알림
	// pSession에 대한 오버랩 오퍼래이션이 완료된 것을 확인후 호출 하여야 함.
	EnterCriticalSection(&m_csBATCH);
	MAPTSERVER::iterator finder = m_mapSESSION.find((DWORD_PTR) pSession);

	if( finder == m_mapSESSION.end() ||
		pSession->m_bClosing )
	{
		LeaveCriticalSection(&m_csBATCH);
		return;
	}

	pSession->m_bClosing = TRUE;
	LeaveCriticalSection(&m_csBATCH);

	LPPACKETBUF pBUF = new PACKETBUF();
	pBUF->m_packet.SetID(SM_DELSESSION_REQ);
	pBUF->m_pSESSION = pSession;

	EnterCriticalSection(&m_csQUEUE);
	m_qBATCHJOB.push(pBUF);
	LeaveCriticalSection(&m_csQUEUE);
	SetEvent(m_hBatchEvent);
}

void CTWorldSvrModule::CloseSession( CTWorldSession *pSession)
{
	pSession->CloseSession();
}

DWORD CTWorldSvrModule::InitDB()
{
#include "InitDatabase.inc"
}

DWORD CTWorldSvrModule::LoadData()
{
#include "LoadStartupData.inc"
}

DWORD CTWorldSvrModule::ControlThread()
{
	std::random_device rd;
	srand(rd());
	while(TRUE)
	{
		DWORD dwCompKey = COMP_NULL;
		DWORD dwIoBytes = 0;

		LPOVERLAPPED pOV = NULL;

		if(!GetQueuedCompletionStatus(
			m_hIocpControl,
			&dwIoBytes,
			&dwCompKey,
			&pOV, INFINITE) &&
			GetLastError() != WAIT_TIMEOUT )
		{
			switch(dwCompKey)
			{
			case COMP_ACCEPT	:
				if(!WaitForConnect())
					LogEvent(_T("WaitForConnect : Error"));

				break;
			}
		}
		else if(pOV)
		{
			switch(dwCompKey)
			{
			case COMP_ACCEPT	:
				if(!Accept())
					LogEvent(_T("Accept : Error"));

				if(!WaitForConnect())
					LogEvent(_T("WaitForConnect : Error"));

				break;

			case COMP_CLOSE		: OnCloseSession((CTWorldSession *) pOV); break;
			}
		}
		else if( dwCompKey == COMP_EXIT )
			return 0;
	}

	return 0;
}

DWORD CTWorldSvrModule::WorkThread()
{
	std::random_device rd;
	srand(rd());
	BOOL bRun = TRUE;

	while(bRun)
	{
		DWORD dwCompKey = COMP_NULL;
		DWORD dwIoBytes = 0;

		LPOVERLAPPED pOV = NULL;

		if(!GetQueuedCompletionStatus(
			m_hIocpWork,
			&dwIoBytes,
			&dwCompKey,
			&pOV, INFINITE) &&
			GetLastError() != WAIT_TIMEOUT )
		{
			switch(dwCompKey)
			{
			case COMP_SESSION	:
				if(pOV)
				{
					CTWorldSession *pSession = (CTWorldSession *) ((COverlappedEx *) pOV)->m_pOBJ;
					BYTE bTYPE = ((COverlappedEx *) pOV)->m_bTYPE;

					switch(bTYPE)
					{
					case TOV_SSN_RECV	:
						OnInvalidSession(pSession);
						break;
						// ***** IOCP 사용법 중 알아내기 힘든 첫번째 구문 (서버측 세션 종료) *****
						//
						// 서버가 먼저 closesocket()을 호출하여 세션을 종료한 경우이며
						// WSARecv()가 호출된 상태에서만 이 코드로 들어오며
						// 모든 오버랩 오퍼래이션이 종료된 상태이기 때문에
						// 이 소켓 핸들과 관련된 데이타는 IOCP큐에 남아있지 않다.
						// 따라서 이 스레드에서는 해당 세션에 관련된 작업 명령을 더이상 수행하지 않기 때문에
						// 다른 스레드가 허락한다면 이 구문에서 세션 포인터를 삭제해도 무방하다.
						// 세션을 삭제 하는데 가장 좋은 지점이므로 전체 시스템 설계시
						// 정상적인 세션 종료는 서버측에서 먼저 세션을 종료시키도록 설계하는 것이 안전하다.
					case TOV_SSN_SEND:
						OnSendComplete(pSession, 0);
						break;
					}
				}

				break;
			}
		}
		else if(pOV)
		{
			BYTE bTYPE = ((COverlappedEx *) pOV)->m_bTYPE;

			switch(dwCompKey)
			{
			case COMP_SESSION	:
				{
					CTWorldSession *pSession = (CTWorldSession *) ((COverlappedEx *) pOV)->m_pOBJ;

					switch(bTYPE)
					{
					case TOV_SSN_RECV	:
						if(pSession)
							ProcessSession( pSession, dwIoBytes);

						break;

					case TOV_SSN_SEND	:
						if(pSession)
							OnSendComplete( pSession, dwIoBytes);

						break;
					}
				}
				break;
			}
		}
		else
		{
			switch(dwCompKey)
			{
			case COMP_EXIT		:
				{
					PostQueuedCompletionStatus(
						m_hIocpWork, 0,
						COMP_EXIT, NULL);

					bRun = FALSE;
				}

				break;
			}
		}
	}

	return 0;
}

DWORD CTWorldSvrModule::BatchThread()
{
	std::random_device rd;
	srand(rd());

	while(TRUE)
	{
		EnterCriticalSection(&m_csQUEUE);
		if(!m_bBatchRun)
		{
			LeaveCriticalSection(&m_csQUEUE);
			break;
		}

		if(m_qBATCHJOB.empty())
		{
			LeaveCriticalSection(&m_csQUEUE);
			WaitForSingleObject( m_hBatchEvent, INFINITE);
		}
		else
		{
			LPPACKETBUF pBUF = m_qBATCHJOB.front();
			m_qBATCHJOB.pop();
			LeaveCriticalSection(&m_csQUEUE);

			EnterCriticalSection(&m_csBATCH);
			DWORD dwResult = OnReceive(pBUF);
			LeaveCriticalSection(&m_csBATCH);

			/*if(dwResult)
			{
				CloseSession((CTWorldSession *) pBUF->m_pSESSION);
				OnERROR(dwResult);
			}*/

			delete pBUF;
		}
	}

	return 0;
}

DWORD CTWorldSvrModule::DBThread()
{
	std::random_device rd;
	srand(rd());
	while(TRUE)
	{
		EnterCriticalSection(&m_csDBQUEUE);
		if(!m_bDBRun && m_qDBJOB.empty())
		{
			LeaveCriticalSection(&m_csDBQUEUE);
			break;
		}

		if(m_qDBJOB.empty())
		{
			LeaveCriticalSection(&m_csDBQUEUE);
			WaitForSingleObject( m_hDBEvent, INFINITE);
		}
		else
		{
			LPPACKETBUF pBUF = m_qDBJOB.front();
			m_qDBJOB.pop();
			LeaveCriticalSection(&m_csDBQUEUE);

			DWORD dwResult = OnReceive(pBUF);
			if(dwResult)
				OnERROR(dwResult);

			delete pBUF;
		}
	}

	return 0;
}

DWORD CTWorldSvrModule::TimerThread()
{
	std::random_device rd;
	srand(rd());
	while(TRUE)
	{
		m_timeCurrent = CTime::GetCurrentTime().GetTime();

		EnterCriticalSection(&m_csTIMERQUEUE);
		if(!m_bTimerRun)
		{
			LeaveCriticalSection(&m_csTIMERQUEUE);
			break;
		}

		if(m_qTIMERJOB.empty())
		{
			LeaveCriticalSection(&m_csTIMERQUEUE);
			WaitForSingleObject( m_hTimerEvent, WORLD_TIMER);
			CheckTGuildExtinction((DWORD)m_timeCurrent);
			CheckEventExpired();
			OnTimer();
		}
		else
		{
			LPPACKETBUF pBUF = m_qTIMERJOB.front();
			m_qTIMERJOB.pop();
			LeaveCriticalSection(&m_csTIMERQUEUE);

			DWORD dwResult = OnReceive(pBUF);
			if(dwResult)
				OnERROR(dwResult);

			delete pBUF;
		}
	}

	return 0;
}

void CTWorldSvrModule::OnSendComplete( CTWorldSession *pSession, DWORD dwIoBytes)
{
	if(pSession->SendComplete(dwIoBytes))
		ClosingSession(pSession);
}

void CTWorldSvrModule::ProcessSession( CTWorldSession *pSession, DWORD dwIoBytes)
{
	BOOL bContinue = TRUE;

	if(!pSession->Read(dwIoBytes))
	{
		// ***** IOCP 사용법 중 알아내기 힘든 두번째 구문 (클라이언트측 세션 종료) *****
		//
		// 클라이언트가 먼저 closesocket()을 호출하여 세션을 종료한 경우이며
		// WSARecv()가 호출된 상태에서만 이 코드로 들어오며
		// 모든 오버랩 오퍼래이션이 종료 되었다고 볼 수 없다.
		// 따라서 이후에 이 스레드에서 이 세션과 관련된 작업명령이 실행 될 수 있으므로
		// 여기서 세션 포인터를 삭제하면 서버가 다운될 수 있다.
		// Receive와 관련된 오버랩 오퍼레이션은 확실히 종료 되었으므로
		// Send와 관련된 오버랩 오퍼레이션이 종료되었는지를
		// 확인한 후 다른 스레드의 세션 삭제 수락과정을 거치고 세션을 삭제 해야 한다.
		OnInvalidSession(pSession);
		return;
	}

	while(bContinue)
	{
		DWORD dwResult = pSession->CheckMessage();

		switch(dwResult)
		{
		//case PACKET_INVALID		:
		case PACKET_INCOMPLETE	: bContinue = FALSE; break;
		case PACKET_COMPLETE	:
			{
				LPPACKETBUF pBUF = new PACKETBUF();

				pBUF->m_packet.Copy(&pSession->m_Recv);
				pBUF->m_pSESSION = pSession;

				SayToBATCH(pBUF);
				pSession->Flush();
			}

			break;

		default					:
			{
				if(pSession->m_Recv.GetSize() == MAX_PACKET_SIZE)
				{
					LogEvent("Check Message Overflow");
					pSession->Flush();
				}
				else
				{
					OnInvalidSession(pSession);
					return;
				}
			}
			break;
			// ***** IOCP 사용법 중 알아내기 힘든 세번째 구문 (비 정상적인 세션 종료) *****
			//
			// 클라이언트가 패킷을 변조해서 보낸다거나 네트웤 오류로 인해 세션이 비 정상적인 상태가 된 경우이며
			// WSARecv()가 호출된 상태에서만 이 코드로 들어오며
			// 모든 오버랩 오퍼래이션이 종료 되었다고 볼 수 없다.
			// 따라서 이후에 이 스레드에서 이 세션과 관련된 작업명령이 실행 될 수 있으므로
			// 여기서 세션 포인터를 삭제하면 서버가 다운될 수 있다.
			// Receive와 관련된 오버랩 오퍼레이션은 확실히 종료 되었으므로
			// Send와 관련된 오버랩 오퍼레이션이 종료되었는지를
			// 확인한 후 다른 스레드의 세션 삭제 수락과정을 거치고 세션을 삭제 해야 한다.
			//
			// *** 권장하지 않는 편법 ***
			// 혹시나 여기서 closesocket()을 호출 하여
			// 서버측 세션종료 프로세스로 들어가려는 시도는 하지 않는 것이 좋다.
			// closesocket()을 호출해도 WSARecv()가 호출되지 않은 상태이기 때문에
			// 서버측 세션종료 프로세스로 들어가지 못한다. 만약 WSARecv()를 먼저
			// 호출하고 바로 closesocket()을 호출하면 프로세스로의 진입은 가능 할 수도 있으나
			// 비 정상적인 세션을 대상으로 그런 액션을 하는 것은 위험하다.
		}
	}

	if(!pSession->WaitForMessage())
	{
		// ***** IOCP 사용법 중 알아내기 힘든 네번째 구문 (비 정상적인 세션 종료) *****
		//
		// 네트웤 오류로 인해 세션이 비 정상적인 상태에서 WSARecv()함수 호출이 실패한 경우이며
		// WSARecv()가 호출된 상태에서만 이 코드로 들어오며
		// 모든 오버랩 오퍼래이션이 종료 되었다고 볼 수 없다.
		// 따라서 이후에 이 스레드에서 이 세션과 관련된 작업명령이 실행 될 수 있으므로
		// 여기서 세션 포인터를 삭제하면 서버가 다운될 수 있다.
		// Receive와 관련된 오버랩 오퍼레이션은 확실히 종료 되었으므로
		// Send와 관련된 오버랩 오퍼레이션이 종료되었는지를
		// 확인한 후 다른 스레드의 세션 삭제 수락과정을 거치고 세션을 삭제 해야 한다.
		OnInvalidSession(pSession);
	}
}

DWORD CTWorldSvrModule::OnReceive( LPPACKETBUF pBUF)
{
	if(pBUF->m_packet.GetSize() == MAX_PACKET_SIZE)
	{
		LogEvent("Overflow Message %d",pBUF->m_packet.GetID());
		return EC_SESSION_INVALIDMSG;
	}

	switch(pBUF->m_packet.GetID())
	{
#include "WorldMessages.inc"
	}

	LogEvent("Invalid Message %d",pBUF->m_packet.GetID());
	return EC_SESSION_INVALIDMSG;
}

void CTWorldSvrModule::SayToBATCH( LPPACKETBUF pBUF)
{
	pBUF->m_packet.Rewind(FALSE);

	EnterCriticalSection(&m_csQUEUE);
	m_qBATCHJOB.push(pBUF);
	LeaveCriticalSection(&m_csQUEUE);
	SetEvent(m_hBatchEvent);
}

void CTWorldSvrModule::SayToDB( LPPACKETBUF pBUF)
{
	pBUF->m_packet.Rewind(FALSE);

	EnterCriticalSection(&m_csDBQUEUE);
	m_qDBJOB.push(pBUF);
	LeaveCriticalSection(&m_csDBQUEUE);
	SetEvent(m_hDBEvent);
}

void CTWorldSvrModule::SayToTIMER( LPPACKETBUF pBUF)
{
	pBUF->m_packet.Rewind(FALSE);

	EnterCriticalSection(&m_csTIMERQUEUE);
	m_qTIMERJOB.push(pBUF);
	LeaveCriticalSection(&m_csTIMERQUEUE);
	SetEvent(m_hTimerEvent);
}

CTServer *CTWorldSvrModule::FindMapSvr( BYTE bServerID)
{
	MAPTSERVER::iterator finder = m_mapSERVER.find( MAKEWORD( bServerID, BYTE(SVR_MAP)));

	if( finder != m_mapSERVER.end() )
		return (*finder).second;

	return NULL;
}

CTServer *CTWorldSvrModule::FindTServer( DWORD_PTR dwHandle)
{
	MAPTSERVER::iterator finder = m_mapSESSION.find(dwHandle);

	if( finder != m_mapSESSION.end() )
		return (*finder).second;

	return NULL;
}

LPTCHARACTER CTWorldSvrModule::FindTChar( DWORD dwCharID,
										  DWORD dwKEY)
{
	MAPTCHARACTER::iterator itCHAR = m_mapTCHAR.find(dwCharID);

	if( itCHAR == m_mapTCHAR.end() || dwKEY != (*itCHAR).second->m_dwKEY )
		return NULL;

	return (*itCHAR).second;
}

LPTCHARACTER CTWorldSvrModule::FindTChar(CString strName)
{
	strName.MakeUpper();
	MAPTCHARACTERNAME::iterator it = m_mapTCHARNAME.find(strName);
	if(it!=m_mapTCHARNAME.end())
		return (*it).second;
	else
		return NULL;
}

CTParty * CTWorldSvrModule::FindParty(WORD wID)
{
	MAPTPARTY::iterator it = m_mapTParty.find(wID);
	if(it!=m_mapTParty.end())
		return (*it).second;
	else
		return NULL;
}
BYTE CTWorldSvrModule::DeleteParty(WORD wID)
{
	MAPTPARTY::iterator it = m_mapTParty.find(wID);

	if(it!=m_mapTParty.end())
	{
		CTParty * pParty = (*it).second;
		for(int i=0; i<pParty->GetSize(); i++)
		{
			memset(&(pParty->GetMember(i)->m_command),0,sizeof(TCOMMAND));
			pParty->GetMember(i)->m_pParty = NULL;
			pParty->GetMember(i)->m_bPartyWaiter = FALSE;

			if(m_pRelay)
				m_pRelay->SendRW_PARTYDEL_ACK(pParty->GetMember(i)->m_dwCharID, pParty->GetID(), 0);

			PartyAttr(pParty->GetMember(i));
		}

		delete pParty;
		m_mapTParty.erase(it);
		m_qGenPartyID.push(wID);

		return TRUE;
	}
	else
		return FALSE;
}
void CTWorldSvrModule::LeaveFriend(LPTCHARACTER pChar)
{
	MAPTFRIEND::iterator itF;
	for(itF=pChar->m_mapTFRIEND.begin(); itF!=pChar->m_mapTFRIEND.end(); itF++)
	{
		LPTFRIEND pFriend = (*itF).second;
		if(pFriend->m_bConnected)
		{
			LPTCHARACTER pTarget = FindTChar(pFriend->m_strName);
			if(pTarget)
			{
                MAPTFRIEND::iterator it = pTarget->m_mapTFRIEND.find(pChar->m_dwCharID);
				if(it!=pTarget->m_mapTFRIEND.end())
					(*it).second->m_bConnected = FALSE;

				if((*itF).second->m_bType != FT_FRIEND)
				{
					CTServer * pCon = FindMapSvr(pTarget->m_bMainID);
					if(pCon)
					{
						pCon->SendMW_FRIENDCONNECTION_REQ(
							pTarget->m_dwCharID,
							pTarget->m_dwKEY,
							FRIEND_DISCONNECTION,
							pChar->m_strNAME, 0);
					}
				}
			}
		}
	}

	for(itF=pChar->m_mapTFRIEND.begin(); itF!=pChar->m_mapTFRIEND.end(); itF++)
		delete (*itF).second;
	pChar->m_mapTFRIEND.clear();
}
void CTWorldSvrModule::LeaveSoulmate(LPTCHARACTER pChar)
{
	MAPTSOULMATE::iterator it;

	for(it=pChar->m_mapTSOULMATE.begin(); it!=pChar->m_mapTSOULMATE.end(); it++)
	{
		LPTSOULMATE pSOUL = (*it).second;
		if(pSOUL->m_bConnected &&
			pSOUL->m_dwCharID != pChar->m_dwCharID)
		{
			MAPTCHARACTER::iterator itSo = m_mapTCHAR.find(pSOUL->m_dwCharID);
			if(itSo != m_mapTCHAR.end())
			{
				LPTCHARACTER pTarget = (*itSo).second;

				MAPTSOULMATE::iterator itSoul = pTarget->m_mapTSOULMATE.find(pTarget->m_dwCharID);
				if(itSoul != pTarget->m_mapTSOULMATE.end())
				{
					(*itSoul).second->m_bConnected = FALSE;
					(*itSoul).second->m_dwRegion = 0;
				}
			}
		}
		delete pSOUL;
	}

	pChar->m_mapTSOULMATE.clear();
}
void CTWorldSvrModule::LeaveTMS(LPTCHARACTER pChar)
{
	MAPDWORD::iterator it;
	for(it=pChar->m_mapTMS.begin(); it!=pChar->m_mapTMS.end(); it++)
	{
		MAPTMS::iterator find = m_mapTMS.find((*it).second);
		if(find==m_mapTMS.end())
			continue;

		LPTMS pTMS = (*find).second;
		DWORD dwTMS = pTMS->m_dwID;

		pTMS->m_strLastMember = pChar->m_strNAME;
		pTMS->m_mapMember.erase(pChar->m_dwCharID);

		MAPTCHARACTER::iterator itChar;
		for(itChar=pTMS->m_mapMember.begin(); itChar!=pTMS->m_mapMember.end(); itChar++)
		{
			CTServer * pCon = FindMapSvr((*itChar).second->m_bMainID);
			if(pCon)
				pCon->SendMW_TMSOUT_REQ(
					(*itChar).second->m_dwCharID,
					(*itChar).second->m_dwKEY,
					dwTMS,
					pChar->m_strNAME);
		}
		
		if(!pTMS->m_mapMember.size())
		{
			delete pTMS;
			m_mapTMS.erase(dwTMS);
		}
	}

	pChar->m_mapTMS.clear();
}

void CTWorldSvrModule::PartyAttr(LPTCHARACTER pChar)
{
	MAPTCHARCON::iterator itTCON;
	WORD wCommander = 0;
	WORD wPartyID = 0;

	DWORD dwChiefID = 0;
	BYTE bPartyType = PT_FREE;

	if(pChar->m_pParty)
	{
		CTCorps *pCorps = FindCorps(pChar->m_pParty->m_wCorpsID);

		if(pCorps)
			wCommander = pCorps->m_wCommander;

		dwChiefID = pChar->m_pParty->GetChiefID();
		wPartyID = pChar->m_pParty->GetID();
		bPartyType = pChar->m_pParty->m_bObtainType;
	}

	CTServer * pMain = FindMapSvr(pChar->m_bMainID);
	if(pMain)
		pMain->SendMW_PARTYATTR_REQ(
				pChar->m_dwCharID,
				pChar->m_dwKEY,
				wPartyID,
				bPartyType,
				dwChiefID,
				wCommander);

	for(itTCON=pChar->m_mapTCHARCON.begin(); itTCON!=pChar->m_mapTCHARCON.end(); itTCON++)
	{
		CTServer *pMAP = FindMapSvr((*itTCON).first);

		if(pMAP && pMAP->m_bValid && pMAP != pMain)
		{
			pMAP->SendMW_PARTYATTR_REQ(
				pChar->m_dwCharID,
				pChar->m_dwKEY,
				wPartyID,
				bPartyType,
				dwChiefID,
				wCommander);
		}
	}
}

void CTWorldSvrModule::JoinParty(CTParty * pParty, DWORD dwChiefID, LPTCHARACTER pTarget)
{
	if(!pParty->GetChiefID())
		pParty->SetChiefID(dwChiefID);

	LPTCHARACTER pChief = NULL;

	MAPTCHARACTER::iterator itCh = m_mapTCHAR.find(pParty->GetChiefID());
	if(itCh != m_mapTCHAR.end())
		pChief = (*itCh).second;

	if(!pChief)
		return;

	CTServer * pTgCon = FindMapSvr(pTarget->m_bMainID);
	CTServer * pOgCon = FindMapSvr(pChief->m_bMainID);

	if ((pTarget->m_bMainID == BOW_SERVER_ID || pChief->m_bMainID == BOW_SERVER_ID) && 
		pTarget->m_bMainID != pChief->m_bMainID)
		return;

	if ((pTarget->m_bMainID == BR_SERVER_ID || pChief->m_bMainID == BR_SERVER_ID) && 
		pTarget->m_bMainID != pChief->m_bMainID)
		return;

	if( !pTgCon || !pOgCon )
		return;

	for(int i=0; i < pParty->GetSize(); i++)
	{
		LPTCHARACTER pMember = pParty->GetMember(i);
		CTServer * pMemCon = FindMapSvr(pMember->m_bMainID);
		CTCorps *pCorps = FindCorps(pParty->m_wCorpsID);

		if(!pMemCon)
			continue;

		pTgCon->SendMW_PARTYJOIN_REQ(
			pTarget->m_dwCharID,
			pTarget->m_dwKEY,
			pParty->GetID(),
			pParty->GetChiefID(),
			pCorps ? pCorps->m_wCommander : 0,
			pMember,
			pParty->m_bObtainType);

		pMemCon->SendMW_PARTYJOIN_REQ(
			pMember->m_dwCharID,
			pMember->m_dwKEY,
			pParty->GetID(),
			pParty->GetChiefID(),
			pCorps ? pCorps->m_wCommander : 0,
			pTarget,
			pParty->m_bObtainType);
	}

	AddPartyMember( pParty, pTarget);
	PartyAttr(pTarget);
}

void CTWorldSvrModule::LeaveParty(LPTCHARACTER pChar, BYTE bKick, BYTE bDelete)
{
	if(!pChar->m_pParty)
		return;

	CTParty *pParty = pChar->m_pParty;
	CTCorps *pCorps = FindCorps(pParty->m_wCorpsID);

	BYTE bDel = FALSE;

	NotifyDelCorpsUnit( pCorps, pChar);

	if( pParty->GetSize() > 2 || !bDelete )
	{
		CTServer *pMain = FindMapSvr(pChar->m_bMainID);

		if(pParty->GetNextChief(pChar->m_dwCharID))
		{
			for(int i=0; i<pParty->GetSize(); i++)
				PartyAttr(pParty->GetMember(i));

			if(m_pRelay)
				m_pRelay->SendRW_PARTYCHGCHIEF_ACK(pParty->GetID(), pParty->GetChiefID());

			ChgSquadChief( pCorps, pParty);
			ReportEnemyList( pCorps, pParty->GetID(), pParty->GetChiefID());
		}
	}
	else
	{
		pCorps = NotifyCorpsLeave(pCorps, pParty);
		pParty->SetChiefID(0);
		bDel = TRUE;
	}

	WORD wCommander = pCorps ? pCorps->m_wCommander : 0;

	for(int i=0; i<pParty->GetSize(); i++)
	{
		CTServer *pMAP = FindMapSvr(pParty->GetMember(i)->m_bMainID);

		if(pMAP)
		{
			DWORD dwChiefID = pParty->GetMember(i) != pChar ? pParty->GetChiefID() : 0;

			pMAP->SendMW_PARTYDEL_REQ(
                pParty->GetMember(i)->m_dwCharID,
				pParty->GetMember(i)->m_dwKEY,
				pChar->m_dwCharID,
				dwChiefID,
				wCommander,
				dwChiefID ? pParty->GetID() : 0,
				bKick);
		}
	}

	pParty->DelMember(pChar->m_dwCharID);
	if(m_pRelay)
		m_pRelay->SendRW_PARTYDEL_ACK(pChar->m_dwCharID, pParty->GetID(), pParty->GetChiefID());

	PartyAttr(pChar);

	if(bDel)
	{
		if(pParty->GetSize())
			LeaveParty(pParty->GetMember(0), FALSE, FALSE);

		DeleteParty(pParty->GetID());
	}
}

void CTWorldSvrModule::CloseChar( LPTCHARACTER pTCHAR)
{
#ifdef __TW_APEX
	if(m_bNation == NATION_TAIWAN)
		ApexNotifyUserLeave(pTCHAR->m_dwCharID, pTCHAR->m_strNAME);
#endif

	if(pTCHAR->m_bCHGMainID)
	{
		CTServer * pMain = FindMapSvr(pTCHAR->m_bCHGMainID);
		if(pMain)
			pMain->SendMW_INVALIDCHAR_REQ(pTCHAR->m_dwCharID, pTCHAR->m_dwKEY, TRUE);
	}

	MAPTCHARCON::iterator itCON;

	if(pTCHAR->m_mapTFRIEND.size())
		LeaveFriend(pTCHAR);

	if(pTCHAR->m_mapTMS.size())
		LeaveTMS(pTCHAR);

	if(pTCHAR->m_pParty)
		LeaveParty( pTCHAR, 0);

	if(pTCHAR->m_mapTSOULMATE.size())
		LeaveSoulmate(pTCHAR);

	if (m_pBOWModule)
		m_pBOWModule->DeletePlayerFromQueue(pTCHAR->m_dwCharID, pTCHAR->m_dwKEY);

	if (m_pBRModule)
	{
		if (m_pBRModule->FindPlayerInPremade(pTCHAR->m_dwCharID))
			m_pBRModule->ErasePlayerFromPremade(pTCHAR->m_dwCharID);

		m_pBRModule->ErasePlayerFromQueue(pTCHAR->m_dwCharID, pTCHAR->m_dwKEY);
	}

	if(pTCHAR->m_pGuild)
	{
		pTCHAR->m_pGuild->SetMemberConnection(pTCHAR->m_dwCharID, NULL, m_timeCurrent);
		SendDM_PVPRECORD_REQ(pTCHAR->m_pGuild->m_dwID, pTCHAR->m_pGuild->FindMember(pTCHAR->m_dwCharID));












		SaveGuildStats(pTCHAR->m_pGuild);
	}

	if(pTCHAR->m_pTactics)
	{
		pTCHAR->m_pTactics->SetTacticsConnection(pTCHAR->m_dwCharID, NULL);
		SendDM_TACTICSPOINT_REQ(pTCHAR->m_pTactics->FindTactics(pTCHAR->m_dwCharID));
	}

	if(pTCHAR->m_dwTicket)
		TNMTEnterGate(pTCHAR, 0, FALSE);

	while(!pTCHAR->m_vTDEADCON.empty())
	{
		BYTE bServerID = pTCHAR->m_vTDEADCON.back();
		CTServer *pMAP = FindMapSvr(bServerID);

		if(pMAP)
		{
			pMAP->SendMW_DELCHAR_REQ(
				pTCHAR->m_dwCharID,
				pTCHAR->m_dwKEY,
				bServerID == pTCHAR->m_bMainID && pTCHAR->m_bLogout ? TRUE : FALSE,
				bServerID == pTCHAR->m_bMainID && pTCHAR->m_bSave ? TRUE : FALSE);
		}

		pTCHAR->m_vTDEADCON.pop_back();
	}

	for( itCON = pTCHAR->m_mapTCHARCON.begin(); itCON != pTCHAR->m_mapTCHARCON.end(); itCON++)
	{
		CTServer *pMAP = FindMapSvr((*itCON).first);

		if(pMAP)
		{
			pMAP->SendMW_DELCHAR_REQ(
				pTCHAR->m_dwCharID,
				pTCHAR->m_dwKEY,
				(*itCON).first == pTCHAR->m_bMainID && pTCHAR->m_bLogout ? TRUE : FALSE,
				(*itCON).first == pTCHAR->m_bMainID && pTCHAR->m_bSave ? TRUE : FALSE);
		}
	}

	CString strFindName = pTCHAR->m_strNAME;
	strFindName.MakeUpper();
	MAPTCHARACTERNAME::iterator itNAME = m_mapTCHARNAME.find(strFindName);
	MAPTCHARACTER::iterator itCHAR = m_mapTCHAR.find(pTCHAR->m_dwCharID);

	if( itNAME != m_mapTCHARNAME.end() )
		m_mapTCHARNAME.erase(itNAME);

	m_mapTCHAR.erase(itCHAR);
	delete pTCHAR;

}

WORD CTWorldSvrModule::GenPartyID()
{
	WORD wID = m_qGenPartyID.front();
	m_qGenPartyID.pop();
	return wID;
}

CTGuild *CTWorldSvrModule::FindTGuild(DWORD dwGuild)
{
	MAPTGUILD::iterator it = m_mapTGuild.find(dwGuild);
	if(it!=m_mapTGuild.end())
		return (*it).second;
	return NULL;
}

CTCorps *CTWorldSvrModule::FindCorps(WORD wID)
{
	MAPTCORPS::iterator it = m_mapTCorps.find(wID);
	if(it!=m_mapTCorps.end())
		return (*it).second;
	else
		return NULL;
}

BYTE CTWorldSvrModule::DeleteTGuild(DWORD dwGuildID)
{
	MAPTGUILD::iterator it = m_mapTGuild.find(dwGuildID);
	if(it!=m_mapTGuild.end())
	{
		CTGuild * pGuild = (*it).second;

		MAPTGUILDMEMBER::iterator itMem;
		for(itMem = pGuild->m_mapTMember.begin(); itMem!=pGuild->m_mapTMember.end();)
		{
			LPTGUILDMEMBER pMember = (*itMem).second;
			itMem++;

			if(pMember->m_pChar)
			{
				pMember->m_pChar->m_pGuild = NULL;
				CTServer * pCon = FindMapSvr(pMember->m_pChar->m_bMainID);
				if(pCon)
					pCon->SendMW_GUILDLEAVE_REQ(
						pMember->m_pChar->m_dwCharID,
						pMember->m_pChar->m_dwKEY,
						pMember->m_pChar->m_strNAME,
						GUILD_LEAVE_DISORGANIZATION,
						(DWORD)m_timeCurrent);
			}

			GuildMemberDel(pGuild, pMember, GUILD_LEAVE_DISORGANIZATION);
		}

		MAPTTACTICSMEMBER::iterator itTac;
		for(itTac = pGuild->m_mapTTactics.begin(); itTac!=pGuild->m_mapTTactics.end();)
		{
			LPTTACTICSMEMBER pMember = (*itTac).second;
			itTac++;

			if(pMember->m_pChar)
			{
				pMember->m_pChar->m_pTactics = NULL;
				CTServer * pCon = FindMapSvr(pMember->m_pChar->m_bMainID);
				if(pCon)
					pCon->SendMW_GUILDTACTICSKICKOUT_REQ(
						pMember->m_pChar->m_dwCharID,
						pMember->m_pChar->m_dwKEY,
						GUILD_SUCCESS,
						pMember->m_pChar->m_dwCharID,
						TRUE);
			}

			GuildTacticsDel(pGuild, pMember, 1);
		}

		delete pGuild;
		m_mapTGuild.erase(it);
		return TRUE;
	}

	return FALSE;
}

void CTWorldSvrModule::CheckTGuildExtinction(DWORD dwCurrentTime)
{
	VECTORDWORD vTGuild;
	vTGuild.clear();

	MAPDWORD::iterator itGuild;
	for( itGuild=m_mapTGuildEx.begin(); itGuild!=m_mapTGuildEx.end(); itGuild++)
	{
		DWORD dwTime = dwCurrentTime - (*itGuild).second;
		if(dwTime > GUILD_EXTINC_DURATION)
		{
			LPPACKETBUF pMsg = new PACKETBUF();
			pMsg->m_packet.SetID(DM_GUILDEXTINCTION_REQ)
				<< (*itGuild).first;
			SayToDB(pMsg);
			vTGuild.push_back((*itGuild).first);
		}
	}

	for(BYTE i=0; i<vTGuild.size(); i++)
		m_mapTGuildEx.erase(vTGuild[i]);

	vTGuild.clear();
}

void CTWorldSvrModule::SetCharLevel(LPTCHARACTER pCHAR, BYTE bLevel)
{
	if(pCHAR->m_bLevel != bLevel)
	{
		BYTE bGapN = GetWarCountryGap(bLevel);
		BYTE bCountry = GetWarCountry(pCHAR);

		if(bGapN < WARCOUNTRY_MAXGAP && bCountry < TCONTRY_B)
		{
			MAPDWORD::iterator itA;
			BYTE bGapO = GetWarCountryGap(pCHAR->m_bLevel);
			if(bGapO < WARCOUNTRY_MAXGAP && bGapO != bGapN)
			{
				itA = m_mapWarCountry[bCountry][bGapO].find(pCHAR->m_dwCharID);
				if(itA != m_mapWarCountry[bCountry][bGapO].end())
					m_mapWarCountry[bCountry][bGapO].erase(itA);
			}

			itA = m_mapWarCountry[bCountry][bGapN].find(pCHAR->m_dwCharID);
			if(itA == m_mapWarCountry[bCountry][bGapN].end())
				m_mapWarCountry[bCountry][bGapN].insert(MAPDWORD::value_type(pCHAR->m_dwCharID, 0));
		}

		pCHAR->m_bLevel = bLevel;
		if(pCHAR->m_pGuild)
			pCHAR->m_pGuild->SetMemberLevel(pCHAR->m_dwCharID, bLevel);
	}
}

void CTWorldSvrModule::SetCharStatus(LPTCHARACTER pTCHAR, DWORD dwMaxHP, DWORD dwHP, DWORD dwMaxMP, DWORD dwMP)
{
	pTCHAR->m_dwMaxHP = dwMaxHP;
	pTCHAR->m_dwHP = dwHP;
	pTCHAR->m_dwMaxMP = dwMaxMP;
	pTCHAR->m_dwMP = dwMP;
}

void CTWorldSvrModule::AddPartyMember(CTParty * pParty, LPTCHARACTER pChar)
{
	pParty->AddMember(pChar);

	if(m_pRelay)
		m_pRelay->SendRW_PARTYADD_ACK(pChar->m_dwCharID, pParty->GetID(), pParty->GetChiefID());

	CTServer * pMain = FindMapSvr(pChar->m_bMainID);
	if(!pMain)
		return;

	CTCorps * pCorps = NULL;

	if(pParty->m_wCorpsID)
	{
		pCorps = FindCorps(pParty->m_wCorpsID);
		if(pCorps)
		{
			pMain->SendMW_CORPSJOIN_REQ(
				pChar->m_dwCharID,
				pChar->m_dwKEY,
				pCorps->m_wID,
				pCorps->m_wCommander);
		}
	}

	if(pCorps)
		NotifyAddCorpsUnit(pCorps, pChar);
	else
		NotifyAddCorpsUnit(pParty, pChar);
}

void CTWorldSvrModule::BroadcastCorps(LPTCHARACTER pChar, CPacket * pPacket, WORD wMsgID)
{
	if(pChar->m_pParty->m_wCorpsID &&
		pChar->m_pParty->GetChiefID() == pChar->m_dwCharID)
	{
		CTCorps * pCorps = FindCorps(pChar->m_pParty->m_wCorpsID);
		if(pCorps)
		{
			MAPTPARTY::iterator itParty;
			for(itParty=pCorps->m_mapParty.begin(); itParty!=pCorps->m_mapParty.end(); itParty++)
			{
				if(pChar->m_pParty == (*itParty).second)
					continue;

				LPTCHARACTER pChief = (*itParty).second->GetChief();
                if(pChief)
				{
					CTServer * pMain = FindMapSvr(pChief->m_bMainID);
					if(pMain)
						pMain->RelayCorpsMsg(
							pChief->m_dwCharID,
							pChief->m_dwKEY,
							pPacket,
							wMsgID);
				}
			}
		}
	}
	
	BroadcastCorps(pChar->m_pParty, pPacket, wMsgID);
}

void CTWorldSvrModule::BroadcastCorps(CTParty * pParty, CPacket * pPacket, WORD wMsgID)
{
	for(int i=0; i<pParty->GetSize(); i++)
	{
		CTServer * pMap = FindMapSvr(pParty->GetMember(i)->m_bMainID);
		if(pMap)
			pMap->RelayCorpsMsg(
				pParty->GetMember(i)->m_dwCharID,
				pParty->GetMember(i)->m_dwKEY,
				pPacket,
				wMsgID);
	}
}

void CTWorldSvrModule::NotifyDelCorpsUnit(CTCorps * pCorps, LPTCHARACTER pChar)
{
	if(!pChar)
		return;

	if(pCorps)
	{
		MAPTPARTY::iterator itPt;
		for(itPt=pCorps->m_mapParty.begin(); itPt!=pCorps->m_mapParty.end(); itPt++)
			NotifyDelCorpsUnit((*itPt).second, pChar);
	}
	else
		NotifyDelCorpsUnit(pChar->m_pParty, pChar);
}

void CTWorldSvrModule::NotifyDelCorpsUnit(CTParty * pParty, LPTCHARACTER pChar)
{
	CTServer * pMain = FindMapSvr(pChar->m_bMainID);
	if(!pMain)
		return;

	for(int i=0; i<pParty->GetSize(); i++)
	{
		CTServer * pMap = FindMapSvr(pParty->GetMember(i)->m_bMainID);
		if(pMap)
		{
			pMap->SendMW_DELCORPSUNIT_REQ(
				pParty->GetMember(i)->m_dwCharID,
				pParty->GetMember(i)->m_dwKEY,
				pChar->m_pParty->GetID(),
				pChar->m_dwCharID);
		}

		if(pChar != pParty->GetMember(i))
			pMain->SendMW_DELCORPSUNIT_REQ(
				pChar->m_dwCharID,
				pChar->m_dwKEY,
				pParty->GetID(),
				pParty->GetMember(i)->m_dwCharID);
	}

	pMain->SendMW_DELSQUAD_REQ(
		pChar->m_dwCharID,
		pChar->m_dwKEY,
		pParty->GetID());

	pMain->SendMW_CORPSJOIN_REQ(
		pChar->m_dwCharID,
		pChar->m_dwKEY,
		0, 0);
}

void CTWorldSvrModule::NotifyAddCorpsUnit(CTCorps * pCorps, LPTCHARACTER pChar)
{
	if(!pCorps)
		return;

	MAPTPARTY::iterator itPt;
	for(itPt=pCorps->m_mapParty.begin(); itPt!=pCorps->m_mapParty.end(); itPt++)
		NotifyAddCorpsUnit((*itPt).second, pChar);
}

void CTWorldSvrModule::NotifyAddCorpsUnit(CTParty * pParty, LPTCHARACTER pChar)
{
	if(!pParty)
		return;

	CTServer * pMain = FindMapSvr(pChar->m_bMainID);
	if(!pMain)
		return;

	pMain->SendMW_ADDSQUAD_REQ(
		pChar->m_dwCharID,
		pChar->m_dwKEY,
		pParty);

	for(int i=0; i<pParty->GetSize(); i++)
	{
		CTServer * pMap = FindMapSvr(pParty->GetMember(i)->m_bMainID);
		if(pMap)
		{
			if(pParty->GetMember(i) != pChar)
				pMap->SendMW_ADDCORPSUNIT_REQ(
					pParty->GetMember(i)->m_dwCharID,
					pParty->GetMember(i)->m_dwKEY,
					pChar->m_pParty->GetID(),
					pChar->m_dwCharID,
					pChar->m_strNAME,
					1.0f,
					pParty->m_command.m_dwTgObjID,
					pChar->m_dwMaxHP,
					pChar->m_dwHP,
					pParty->m_command.m_wTgPosX,
					pParty->m_command.m_wTgPosZ,
					pChar->m_wMapID,
					0, 0, MOVE_NONE,
					pParty->m_command.m_bTgType,
					pChar->m_bLevel,
					pChar->m_bClass,
					pChar->m_bRace,
					pChar->m_bSex,
					pChar->m_bFace,
					pChar->m_bHair,
					pParty->m_command.m_bCommand);
		}
	}
}

void CTWorldSvrModule::NotifyCorpsJoin(CTCorps * pCorps, CTParty * pParty)
{
	CTServer * pMap;

	MAPTPARTY::iterator itPt;
	for(itPt=pCorps->m_mapParty.begin(); itPt!=pCorps->m_mapParty.end(); itPt++)
	{
		CTParty * pCorpsParty = (*itPt).second;
	
		for(int i=0; i<pCorpsParty->GetSize(); i++)
		{
			pMap = FindMapSvr(pCorpsParty->GetMember(i)->m_bMainID);
			if(pMap)
				pMap->SendMW_ADDSQUAD_REQ(
					pCorpsParty->GetMember(i)->m_dwCharID,
					pCorpsParty->GetMember(i)->m_dwKEY,
					pParty);
		}

		for(int i=0; i<pParty->GetSize(); i++)
		{
			pMap = FindMapSvr(pParty->GetMember(i)->m_bMainID);
			if(pMap)
				pMap->SendMW_ADDSQUAD_REQ(
					pParty->GetMember(i)->m_dwCharID,
					pParty->GetMember(i)->m_dwKEY,
					pCorpsParty);
		}
	}

	pCorps->Enter(pParty);
	CorpsJoin( pParty, pCorps->m_wCommander);
}

void CTWorldSvrModule::CorpsJoin(CTParty * pParty, WORD wCommander)
{
	if(m_pRelay)
		m_pRelay->SendRW_CORPSJOIN_ACK(pParty->GetID(), pParty->m_wCorpsID, wCommander);

	for(int i=0; i<pParty->GetSize(); i++)
	{
		CTServer * pMap = FindMapSvr(pParty->GetMember(i)->m_bMainID);

		if(pMap)
			pMap->SendMW_CORPSJOIN_REQ(
				pParty->GetMember(i)->m_dwCharID,
				pParty->GetMember(i)->m_dwKEY,
				pParty->m_wCorpsID,
				wCommander);

		PartyAttr(pParty->GetMember(i));
	}
}

CTCorps * CTWorldSvrModule::NotifyCorpsLeave(CTCorps * pCorps, CTParty * pParty)
{
	if(!pCorps)
		return NULL;

	MAPTPARTY::iterator itPt;
	for(itPt=pCorps->m_mapParty.begin(); itPt!=pCorps->m_mapParty.end(); itPt++)
	{
		if((*itPt).second->GetID() != pParty->GetID())
		{
			DelSquad((*itPt).second, pParty->GetID());
			DelSquad(pParty, (*itPt).second->GetID());
		}
	}

	if( pCorps->Leave(pParty) )
	{
		CTParty *pCorpsParty = pCorps->m_mapParty.begin()->second;

		NotifyCorpsLeave(pCorps, pCorpsParty);

		m_mapTCorps.erase(pCorps->m_wID);
		delete pCorps;
		pCorps = NULL;
	}
	else if(pCorps->GetNextCommander(pParty->GetID()))
	{
		MAPTPARTY::iterator itPt;

		for(itPt=pCorps->m_mapParty.begin(); itPt!=pCorps->m_mapParty.end(); itPt++)
			CorpsJoin((*itPt).second, pCorps->m_wCommander);
	}

	CorpsJoin( pParty, 0);

	return pCorps;
}

void CTWorldSvrModule::DelSquad(CTParty * pParty, WORD wDelParty)
{
	for(int i=0; i< pParty->GetSize(); i++)
	{
		CTServer * pMap = FindMapSvr(pParty->GetMember(i)->m_bMainID);
		if(pMap)
		{
			pMap->SendMW_DELSQUAD_REQ(
				pParty->GetMember(i)->m_dwCharID,
				pParty->GetMember(i)->m_dwKEY,
				wDelParty);
		}
	}
}

void CTWorldSvrModule::TransferCorpsCommand(LPTCHARACTER pChar,
									   BYTE bCMD,
									   BYTE bTgType,
									   DWORD dwTgID,
									   WORD wPosX,
									   WORD wPosZ,
									   WORD wSquadID,
									   DWORD dwCharID,
									   WORD wMapID)
{
	if(pChar->m_dwCharID == dwCharID)
	{
		pChar->m_command.m_bCommand = bCMD;
		pChar->m_command.m_bTgType = bTgType;
		pChar->m_command.m_dwTgObjID = dwTgID;
		pChar->m_command.m_wTgPosX = wPosX;
		pChar->m_command.m_wTgPosZ = wPosZ;
	}

	CTServer * pMap = FindMapSvr(pChar->m_bMainID);
	if(pMap)
		pMap->SendMW_CORPSCMD_REQ(
			pChar->m_dwCharID,
			pChar->m_dwKEY,
			wSquadID,
			dwCharID,
			wMapID,
			bCMD,
			dwTgID,
			bTgType,
			wPosX,
			wPosZ);
}

void CTWorldSvrModule::ChgSquadChief(CTCorps * pCorps, CTParty * pParty)
{
	WORD wSquadID = pParty->GetID();
	DWORD dwChiefID = pParty->GetChiefID();

	if(pCorps)
	{
		MAPTPARTY::iterator it;
		for(it=pCorps->m_mapParty.begin(); it!=pCorps->m_mapParty.end(); it++)
			ChgSquadChief((*it).second, wSquadID, dwChiefID);
	}
	else
		ChgSquadChief(pParty, wSquadID, dwChiefID);
}

void CTWorldSvrModule::ChgSquadChief(CTParty * pParty, WORD wSquadID, DWORD dwChiefID)
{
	for(int i=0; i<pParty->GetSize(); i++)
	{
		CTServer * pCon = FindMapSvr(pParty->GetMember(i)->m_bMainID);
		if(pCon)
			pCon->SendMW_CHGSQUADCHIEF_REQ(
				pParty->GetMember(i)->m_dwCharID,
				pParty->GetMember(i)->m_dwKEY,
				wSquadID,
				dwChiefID);
	}
}

void CTWorldSvrModule::ReportEnemyList(CTCorps * pCorps, WORD wPartyID, DWORD dwChiefID)
{
	if(!pCorps)
		return;

	MAPTPARTY::iterator it;
	for(it=pCorps->m_mapParty.begin(); it!=pCorps->m_mapParty.end(); it++)
	{
		if(wPartyID!=(*it).second->GetID())
		{
			LPTCHARACTER pChief = (*it).second->GetChief();
			CTServer * pCon = FindMapSvr(pChief->m_bMainID);
			if(pCon)
			{
				pCon->SendMW_REPORTENEMYLIST_REQ(
					pChief->m_dwCharID,
					pChief->m_dwKEY,
					wPartyID,
					dwChiefID);
				break;
			}
		}
	}
}

void CTWorldSvrModule::CheckMainCON( LPTCHARACTER pTCHAR)
{
	MAPTCHARCON::iterator itCON;

	if(!pTCHAR)
		return;

	for( itCON = pTCHAR->m_mapTCHARCON.begin(); itCON != pTCHAR->m_mapTCHARCON.end(); itCON++)
	{
		CTServer *pMAP = FindMapSvr((*itCON).first);

		if(pMAP)
		{
			pMAP->SendMW_CHECKMAIN_REQ(
				pTCHAR->m_dwCharID,
				pTCHAR->m_dwKEY,
				pTCHAR->m_bChannel,
				pTCHAR->m_wMapID,
				pTCHAR->m_fPosX,
				pTCHAR->m_fPosY,
				pTCHAR->m_fPosZ);
		}
	}
}

void CTWorldSvrModule::ClearDeadCON( LPTCHARACTER pTCHAR)
{
	if(!pTCHAR)
		return;

	while(!pTCHAR->m_vTDEADCON.empty())
	{
		CTServer *pMAP = FindMapSvr(pTCHAR->m_vTDEADCON.back());

		if(pMAP)
		{
			pMAP->SendMW_CLOSECHAR_REQ(
				pTCHAR->m_dwCharID,
				pTCHAR->m_dwKEY);
		}

		pTCHAR->m_vTDEADCON.pop_back();
	}
}

BYTE CTWorldSvrModule::PushConCess( LPTCHARACTER pTCHAR, LPPACKETBUF pBUF)
{
	LPPACKETBUF pMSG = new PACKETBUF();

	pMSG->m_packet.Copy(&pBUF->m_packet);
	pMSG->m_pSESSION = pBUF->m_pSESSION;
	pMSG->m_packet.Rewind(FALSE);
	pTCHAR->m_qConCess.push(pMSG);

	return INT(pTCHAR->m_qConCess.size()) > 1 ? TRUE : FALSE;
}

DWORD CTWorldSvrModule::PopConCess( LPTCHARACTER pTCHAR)
{
	if(pTCHAR->m_qConCess.empty())
		return EC_NOERROR;

	delete pTCHAR->m_qConCess.front();
	pTCHAR->m_qConCess.pop();

	if(!pTCHAR->m_qConCess.empty())
	{
		LPPACKETBUF pBUF = pTCHAR->m_qConCess.front();

		switch(pBUF->m_packet.GetID())
		{
		case MW_BEGINTELEPORT_ACK	: OnBeginTeleport( pTCHAR, pBUF); break;
		case MW_CHECKCONNECT_ACK	: OnCheckConnect( pTCHAR, pBUF); break;
		}
	}

	return EC_NOERROR;
}

void CTWorldSvrModule::OnBeginTeleport( LPTCHARACTER pTCHAR, LPPACKETBUF pBUF)
{
#ifdef FOURSTORY_NATIVE_MODULE
	CTServer *pSERVER = FindTServer(DWORD_PTR(pBUF->m_pSESSION));
#else
	CTServer *pSERVER = FindTServer(DWORD(DWORD_PTR(pBUF->m_pSESSION)));
#endif
	CTServer *pMAIN = FindMapSvr(pTCHAR->m_bMainID);

	if( !pSERVER || !pMAIN )
	{
		// 텔레포트를 의뢰한 맵 서버가 비활성 되었거나 메인서버가 없거나 공식적으로 배포된 클라이언트로부터의 연결이 아니거나
		// 해킹으로 의심되는 연결로부터의 텔레포트 요청
		// 클라이언트의 연결을 종료
		CTBowSystem::Log("No Server > ServerID");
		CloseChar(pTCHAR);
		return;
	}

	if( pSERVER != pMAIN )
	{
		// 현재의 메인서버로부터 요청받은 연결관리 프로세스가 아니므로 취소하고 다음 연결관리 프로세스를 실행
		PopConCess(pTCHAR);
		return;
	}

	MAPTCHARCON::iterator itCON;
	DWORD dwCharID;
	DWORD dwKEY;
	BYTE bSameChannel;
	WORD wMapID = pTCHAR->m_wMapID;
	// 캐릭터 오브젝트를 텔레포트하려는 목적지로 이동시킴
	pBUF->m_packet
		>> dwCharID
		>> dwKEY
		>> bSameChannel
		>> pTCHAR->m_bChannel
		>> pTCHAR->m_wMapID
		>> pTCHAR->m_fPosX
		>> pTCHAR->m_fPosY
		>> pTCHAR->m_fPosZ;

	if(m_pRelay)
		m_pRelay->SendRW_CHANGEMAP_ACK(dwCharID, pTCHAR->m_wMapID, MAKEWORD( BYTE(INT(pTCHAR->m_fPosX) / UNIT_SIZE), BYTE(INT(pTCHAR->m_fPosZ) / UNIT_SIZE)));

	// 유효한 연결에 텔레포트시작을 알림
	// 이때 맵 서버에서는 캐릭터를 맵에서 제외 시킨 후 지정된 메인서버는 목적지의 서버 ID를 반환한다
	for( itCON = pTCHAR->m_mapTCHARCON.begin(); itCON != pTCHAR->m_mapTCHARCON.end(); itCON++)
		if((*itCON).second->m_bValid)
		{
			CTServer *pMAP = FindMapSvr((*itCON).first);

			if(pMAP)
			{
				pMAP->SendMW_STARTTELEPORT_REQ(
					dwCharID,
					dwKEY,
					pTCHAR->m_bChannel,
					pTCHAR->m_wMapID,
					pTCHAR->m_fPosX,
					pTCHAR->m_fPosY,
					pTCHAR->m_fPosZ);
			}
		}
}

void CTWorldSvrModule::OnCheckConnect( LPTCHARACTER pTCHAR, LPPACKETBUF pBUF)
{
#ifdef FOURSTORY_NATIVE_MODULE
	CTServer *pSERVER = FindTServer(DWORD_PTR(pBUF->m_pSESSION));
#else
	CTServer *pSERVER = FindTServer(DWORD(DWORD_PTR(pBUF->m_pSESSION)));
#endif
	CTServer *pMAIN = FindMapSvr(pTCHAR->m_bMainID);

	if( !pSERVER || !pMAIN )
	{
		// 필요한 연결확인을 의뢰한 맵 서버가 비활성 되었거나 메인서버가 없거나 공식적으로 배포된 클라이언트로부터의 연결이 아니거나
		// 해킹으로 의심되는 연결로부터의 메인서버 변경 요청
		// 클라이언트의 연결을 종료
		CloseChar(pTCHAR);
		return;
	}

	if( pSERVER != pMAIN )
	{
		// 현재의 메인서버로부터 요청받은 연결관리 프로세스가 아니므로 취소하고 다음 연결관리 프로세스를 실행
		PopConCess(pTCHAR);
		return;
	}

	DWORD dwCharID;
	DWORD dwKEY;
	BYTE bCount;
	WORD wMapID = pTCHAR->m_wMapID;

	// 캐릭터 위치정보를 업데이트
	pBUF->m_packet
		>> dwCharID
		>> dwKEY
		>> pTCHAR->m_bChannel
		>> pTCHAR->m_wMapID
		>> pTCHAR->m_fPosX
		>> pTCHAR->m_fPosY
		>> pTCHAR->m_fPosZ
		>> bCount;

	if(m_pRelay)
		m_pRelay->SendRW_CHANGEMAP_ACK(dwCharID, pTCHAR->m_wMapID, MAKEWORD( BYTE(INT(pTCHAR->m_fPosX) / UNIT_SIZE), BYTE(INT(pTCHAR->m_fPosZ) / UNIT_SIZE)));

	if(!bCount)
	{
		// 체크해야 할 연결이 없으면 메인서버만 체크
		CheckMainCON(pTCHAR);
		return;
	}

	MAPBYTE mapCON;
	mapCON.clear();

	// 필요한 연결목록 구성
	for( BYTE i=0; i<bCount; i++)
	{
		BYTE bServerID;

		pBUF->m_packet
			>> bServerID;

		mapCON.insert( MAPBYTE::value_type( bServerID, bServerID));
	}

	// 필요없는 연결들을 종료대기 버퍼로 이동
	MAPTCHARCON::iterator itCON = pTCHAR->m_mapTCHARCON.begin();
	while(itCON != pTCHAR->m_mapTCHARCON.end())
	{
		MAPTCHARCON::iterator itNEXT = itCON;
		itNEXT++;

		if( mapCON.find((*itCON).first) == mapCON.end() )
		{
			pTCHAR->m_vTDEADCON.push_back((*itCON).first);

			delete (*itCON).second;
			pTCHAR->m_mapTCHARCON.erase(itCON);
		}

		itCON = itNEXT;
	}

	// 새롭게 연결되어야 하는 연결목록 구성
	MAPBYTE::iterator itID = mapCON.begin();
	while(itID != mapCON.end())
	{
		MAPBYTE::iterator itNEXT = itID;
		itNEXT++;

		if( pTCHAR->m_mapTCHARCON.find((*itID).first) != pTCHAR->m_mapTCHARCON.end() )
			mapCON.erase(itID);

		itID = itNEXT;
	}

	if(!mapCON.empty())
	{
		// 새롭게 연결되어야 하는 연결이 있으면 메인서버로 해당 연결의 접속정보(IP, 포트 등)를 요청
		// 이 때 맵 서버는 MW_ROUTE_ACK를 이용하여 접속정보를 반환하여 추가 연결관리 프로세스 실행
		CPacket *pMSG = new CPacket();

		pMSG->SetID(MW_ROUTELIST_REQ)
			<< pTCHAR->m_dwCharID
			<< pTCHAR->m_dwKEY
			<< BYTE(mapCON.size());

		for( itID = mapCON.begin(); itID != mapCON.end(); itID++)
			(*pMSG) << (*itID).first;

		pMAIN->Say(pMSG);
	}
	else
	{
		// 새롭게 연결되어야 하는 연결이 없으면 즉시 메인서버 체크
		CheckMainCON(pTCHAR);
	}

	mapCON.clear();
}

HRESULT CTWorldSvrModule::PreMessageLoop( int nShowCmd)
{
	DWORD dwResult = OnEnter();

	if(dwResult)
	{
		OnERROR(dwResult);
#ifdef FOURSTORY_NATIVE_MODULE
		// Several original OnERROR bodies are empty; record the failing code.
		LogEvent("Native startup failed: OnEnter error code %u", dwResult);
#endif
		return E_FAIL;
	}
	m_dwThreadID = GetCurrentThreadId();

	return CAtlServiceModuleT<CTWorldSvrModule,IDS_SERVICENAME>::PreMessageLoop(nShowCmd);
}

HRESULT CTWorldSvrModule::PostMessageLoop()
{
	OnExit();
	return CAtlServiceModuleT<CTWorldSvrModule,IDS_SERVICENAME>::PostMessageLoop();
}

LPTGUILDLEVEL CTWorldSvrModule::FindGuildLevel(BYTE bLevel)
{
#include "FindGuildLevel.inc"
}

void CTWorldSvrModule::OnTimer()
{
	CTime curtime(m_timeCurrent);
	INT nCH = curtime.GetHour();
	INT nCM = curtime.GetMinute();
	INT nCS = curtime.GetSecond();
	INT nCD = curtime.GetDayOfWeek();
	INT bCurDay = curtime.GetDay();
	INT bCurMonth = curtime.GetMonth();

	CheckEventQuarter();

	if(m_bReservedPostMinute != nCM )  
	{
		m_bReservedPostMinute = nCM;

		LPPACKETBUF pBUF = new PACKETBUF();
		pBUF->m_packet.SetID(DM_RESERVEDPOSTSEND_REQ);

		SayToDB(pBUF);

		CheckHourlyReward();
		RecalcCountryBalance();
	}

	if(m_TournamentSchedule.m_wID)
	{
		LPMAPTOURNAMENTSTEP pTournament = m_TournamentSchedule.m_mapStep;
		WORD wTournamentID = m_TournamentSchedule.m_wID;

		MAPTOURNAMENTSTEP::iterator itTStep;
		MAPTOURNAMENTSTEP::reverse_iterator itRS = pTournament->rbegin();

		for(itTStep=pTournament->begin(); itTStep!=pTournament->end(); itTStep++)
		{
			LPTOURNAMENTSTEP pStep = &((*itTStep).second);
			if(!pStep->m_dwPeriod)
				continue;

			if(pStep->m_dStart > m_timeCurrent)
				break;

			if(pStep->m_dStart && pStep->m_dStart <= m_timeCurrent)
			{
				pStep->m_dStart = 0;

				LPPACKETBUF pMSG = new PACKETBUF();
				pMSG->m_packet.SetID(SM_TOURNAMENT_REQ)
					<< wTournamentID
					<< pStep->m_bGroup
					<< pStep->m_bStep
					<< pStep->m_dwPeriod;

				SayToBATCH(pMSG);

				SendDM_TOURNAMENTSTATUS_REQ(wTournamentID, pStep->m_bGroup, pStep->m_bStep);

				break;
			}

			if(pStep->m_bStep == TNMTSTEP_END &&
				pStep->m_dEnd &&
				pStep->m_dEnd <= m_timeCurrent &&
				(*itRS).second.m_bGroup == pStep->m_bGroup)
			{
				pStep->m_dEnd = 0;

				MAPTOURNAMENTTIME::iterator itTm = m_mapTournamentTime.find(wTournamentID);
				if(itTm != m_mapTournamentTime.end())
					SetTournamentTime(pTournament, (*itTm).second, wTournamentID, TRUE, TRUE);

				TournamentUpdate(wTournamentID);

				break;
			}
		}
	}

	if(bCurMonth != m_bRankMonth && !m_bFameRankSave)
	{
		m_bFameRankSave = TRUE;
		LPPACKETBUF pMSG = new PACKETBUF();
		pMSG->m_packet.SetID(SM_MONTHRANKSAVE_REQ);

		SayToBATCH(pMSG);
	}

	static WORD wActiveCharUpdate = MAKEWORD(bCurMonth, bCurDay);
	WORD wCurActive = MAKEWORD(bCurMonth, bCurDay);
	if(nCD == 1 && wActiveCharUpdate != wCurActive)
	{
		wActiveCharUpdate = wCurActive;
		LPPACKETBUF pMSG = new PACKETBUF();
		pMSG->m_packet.SetID(DM_ACTIVECHARUPDATE_REQ);
		SayToDB(pMSG);
	}

	static INT nChangeDay = bCurDay;
	if(nChangeDay != bCurDay)
	{
		nChangeDay = bCurDay;
		LPPACKETBUF pMSG = new PACKETBUF();
		pMSG->m_packet.SetID(SM_CHANGEDAY_REQ);
		SayToBATCH(pMSG);
	}

	DWORD dwCLT = nCH*60*60+nCM*60+nCS;
	if (m_pBOWModule && m_pBOWModule->m_pBOWServer)
	{
		DWORD dwBOWTotal = m_pBOWModule->m_dwAlarmDur + m_pBOWModule->m_dwBuyTimeDur + m_pBOWModule->m_dwBattleDur;

		if(dwCLT > m_pBOWModule->m_dwStart && dwCLT < m_pBOWModule->m_dwStart + dwBOWTotal + 15)
		{
			DWORD dwTimeElapsed = dwCLT - m_pBOWModule->m_dwStart;
			if(dwTimeElapsed <= m_pBOWModule->m_dwAlarmDur)
				m_pBOWModule->SetStatus(BS_ALARM, m_pBOWModule->m_dwAlarmDur - dwTimeElapsed);
			else if(dwTimeElapsed >= m_pBOWModule->m_dwAlarmDur && 
				dwTimeElapsed <= m_pBOWModule->m_dwAlarmDur + m_pBOWModule->m_dwBuyTimeDur)
				m_pBOWModule->SetStatus(BS_PEACE, m_pBOWModule->m_dwAlarmDur + m_pBOWModule->m_dwBuyTimeDur - dwTimeElapsed);
			else if(dwTimeElapsed >= m_pBOWModule->m_dwAlarmDur + m_pBOWModule->m_dwBuyTimeDur && 
				dwTimeElapsed <= m_pBOWModule->m_dwAlarmDur + m_pBOWModule->m_dwBuyTimeDur + m_pBOWModule->m_dwBattleDur)
				m_pBOWModule->SetStatus(BS_BATTLE, m_pBOWModule->m_dwAlarmDur + m_pBOWModule->m_dwBuyTimeDur + m_pBOWModule->m_dwBattleDur - dwTimeElapsed);
			else if(dwCLT >= m_pBOWModule->m_dwStart + m_pBOWModule->m_dwAlarmDur + m_pBOWModule->m_dwBuyTimeDur + m_pBOWModule->m_dwBattleDur)
				m_pBOWModule->SetStatus(BS_BODPEACE, m_pBOWModule->m_dwAlarmDur + m_pBOWModule->m_dwBuyTimeDur + m_pBOWModule->m_dwBattleDur + 15 - dwTimeElapsed);

			m_pBOWModule->m_bRunning = TRUE;
		}
	}

	if (m_pBRModule)
	{
		DWORD dwBRTotal = m_pBRModule->m_dwAlarmDur + m_pBRModule->m_dwBuyTimeDur + m_pBRModule->m_dwBattleDur;

		if(dwCLT > m_pBRModule->m_dwStart && dwCLT < m_pBRModule->m_dwStart + dwBRTotal + 15)
		{
			DWORD dwTimeElapsed = dwCLT - m_pBRModule->m_dwStart;
			if(dwTimeElapsed <= m_pBRModule->m_dwAlarmDur)
				m_pBRModule->SetStatus(BS_ALARM, m_pBRModule->m_dwAlarmDur - dwTimeElapsed);
			else if(dwTimeElapsed >= m_pBRModule->m_dwAlarmDur && 
				dwTimeElapsed <= m_pBRModule->m_dwAlarmDur + m_pBRModule->m_dwBuyTimeDur)
				m_pBRModule->SetStatus(BS_PEACE, m_pBRModule->m_dwAlarmDur + m_pBRModule->m_dwBuyTimeDur - dwTimeElapsed);
			else if(dwTimeElapsed >= m_pBRModule->m_dwAlarmDur + m_pBRModule->m_dwBuyTimeDur && 
				dwTimeElapsed <= m_pBRModule->m_dwAlarmDur + m_pBRModule->m_dwBuyTimeDur + m_pBRModule->m_dwBattleDur)
				m_pBRModule->SetStatus(BS_BATTLE, m_pBRModule->m_dwAlarmDur + m_pBRModule->m_dwBuyTimeDur + m_pBRModule->m_dwBattleDur - dwTimeElapsed);
			else if(dwCLT >= m_pBRModule->m_dwStart + m_pBRModule->m_dwAlarmDur + m_pBRModule->m_dwBuyTimeDur + m_pBRModule->m_dwBattleDur)
				m_pBRModule->SetStatus(BS_BODPEACE, m_pBRModule->m_dwAlarmDur + m_pBRModule->m_dwBuyTimeDur + m_pBRModule->m_dwBattleDur + 15 - dwTimeElapsed);

			m_pBRModule->m_bRunning = TRUE;

			if (m_pBRModule->m_bStatus == BS_BATTLE)
			{
				BYTE bAliveTeam = 0;
				for (MAPBRTEAMS::iterator itTeam = m_pBRModule->m_mapBRTeam.begin(); itTeam != m_pBRModule->m_mapBRTeam.end(); ++itTeam)
				{
					if (IsSomeoneAliveInTeam((*itTeam).second))
					{
						bAliveTeam++;
						continue;
					}
				}

				if (bAliveTeam == 1)
				{
					m_pBRModule->m_pBRServer->SendMW_BRCOMMANDEXEC_REQ(BR_END);
					m_pBRModule->OnEnd();
				}
			}
		}
	}
	
	LPTBATTLETIME pBattle;
	if(m_battletime[BT_CASTLE].m_bDay == nCD)
		pBattle = &(m_battletime[BT_CASTLE]);
	else if( dwCLT >= (m_battletime[BT_SKYGARDEN].m_dwBattleStart - m_battletime[BT_SKYGARDEN].m_dwAlarmStart - 60) &&
		dwCLT <= (m_battletime[BT_SKYGARDEN].m_dwBattleStart + m_battletime[BT_SKYGARDEN].m_dwBattleDur + m_battletime[BT_SKYGARDEN].m_dwPeaceDur + 60))	
		pBattle = &(m_battletime[BT_SKYGARDEN]);
	else
		pBattle = &(m_battletime[BT_LOCAL]);

	if(!pBattle->m_bRun)
	{
		if(m_battletime[BT_MISSION].m_bRun)
			pBattle = &(m_battletime[BT_MISSION]);
		else if(m_battletime[BT_MISSION].m_dwBattleStart)
		{
			DWORD dwCLT = nCH*60*60+nCM*60+nCS;
			DWORD dwStartLeftTime = dwCLT > pBattle->m_dwBattleStart ? DAY_ONE - dwCLT + pBattle->m_dwBattleStart :
								pBattle->m_dwBattleStart - dwCLT;
			DWORD dwBattleEnd = (pBattle->m_dwBattleStart + pBattle->m_dwBattleDur + pBattle->m_dwPeaceDur) % DAY_ONE;
			DWORD dwEndLeftTime = dwCLT > dwBattleEnd ? DAY_ONE - dwCLT + dwBattleEnd : dwBattleEnd - dwCLT;


			DWORD dwRealStartTime = m_battletime[BT_MISSION].m_dwBattleStart - m_battletime[BT_MISSION].m_dwAlarmStart - 60; // EU MATH
			DWORD dwRealEndTime = m_battletime[BT_MISSION].m_dwBattleStart + m_battletime[BT_MISSION].m_dwBattleDur + m_battletime[BT_MISSION].m_dwAlarmEnd + 60; 

			if((dwCLT >= dwRealStartTime - DAY_ONE && dwCLT <= dwRealEndTime - DAY_ONE) ||
			   (dwCLT >= dwRealStartTime && dwCLT <= dwRealEndTime))
				pBattle = &(m_battletime[BT_MISSION]); //I've wasted so much time :(
		}
	}

	if(pBattle->m_dwBattleStart)
	{
		BYTE bPrevStatus = pBattle->m_bStatus;
		BYTE bAlarm = FALSE;
		DWORD dwLeftTime = 0;

		DWORD dwStartLeftTime = dwCLT > pBattle->m_dwBattleStart ? DAY_ONE - dwCLT + pBattle->m_dwBattleStart :
							pBattle->m_dwBattleStart - dwCLT;

		DWORD dwBattleEnd = (pBattle->m_dwBattleStart + pBattle->m_dwBattleDur) % DAY_ONE;
		DWORD dwEndLeftTime = dwCLT > dwBattleEnd ? DAY_ONE - dwCLT + dwBattleEnd : dwBattleEnd - dwCLT;
		pBattle->m_dwLeftTime = dwEndLeftTime;

		switch(pBattle->m_bStatus)
		{
		case BS_NORMAL:
			{
				if(dwStartLeftTime <= pBattle->m_dwAlarmStart)
				{
					DWORD dwInterval = dwStartLeftTime > 600 ? 600 : (dwStartLeftTime > 60 ? 120 : 10);
					if(dwStartLeftTime && dwStartLeftTime % dwInterval == 0)
					{
						dwLeftTime = dwStartLeftTime;
						bAlarm = TRUE;
						pBattle->m_bRun = TRUE;
					}
				}
				else if(dwEndLeftTime <= pBattle->m_dwBattleDur)
				{
					pBattle->m_bStatus = BS_BATTLE;
					dwLeftTime = dwEndLeftTime;
					pBattle->m_bRun = TRUE;
				}
			}
			break;
		case BS_BATTLE:
			{
				if(dwEndLeftTime > pBattle->m_dwBattleDur)
					pBattle->m_bStatus = BS_PEACE;
				else if(dwEndLeftTime && dwEndLeftTime <= pBattle->m_dwAlarmEnd)
				{
					DWORD dwInterval = dwEndLeftTime > 60 ? 60 : 10;
					if(dwEndLeftTime % dwInterval == 0)
					{
						dwLeftTime = dwEndLeftTime;
						bAlarm = TRUE;
					}
				}
			}
			break;
		case BS_PEACE:
			{
				DWORD dwPeaceEnd = (pBattle->m_dwBattleStart + pBattle->m_dwBattleDur + pBattle->m_dwPeaceDur) % DAY_ONE;
				DWORD dwPeaceLeftTime = dwCLT > dwPeaceEnd ? DAY_ONE - dwCLT + dwPeaceEnd : dwPeaceEnd - dwCLT;

				if(dwPeaceLeftTime > pBattle->m_dwPeaceDur)
				{
					pBattle->m_bStatus = BS_NORMAL;
					pBattle->m_bRun = FALSE;
					DWORD dwNextTime = 0;

					if(pBattle->m_bType == BT_MISSION)
					{
						for( BYTE i = 0; i < m_vCUSTOMTIMES.size(); ++i )
						{
							if( m_vCUSTOMTIMES[ i ] == pBattle->m_dwBattleStart || m_vCUSTOMTIMES[ i ] == pBattle->m_dwBattleStart - DAY_ONE )
							{
								if( ++i == m_vCUSTOMTIMES.size()/*)*std::max_element(m_vCUSTOMTIMES.begin(), m_vCUSTOMTIMES.end()) == m_vCUSTOMTIMES[ ++i ]*/ )
									pBattle->m_dwBattleStart = m_vCUSTOMTIMES.front() + DAY_ONE;
								else
									pBattle->m_dwBattleStart = m_vCUSTOMTIMES[ i ] != pBattle->m_dwBattleStart ? 
									m_vCUSTOMTIMES[ i ] : m_vCUSTOMTIMES[ ++i ]; //Avoid bugs, but it's made by me, so there wouldn't be any anyways.
								break;
							}
						}
					}
				}
			}
			break;
		default:
			break;
		}

		if(bAlarm || pBattle->m_bStatus != bPrevStatus)
		{
			if(pBattle->m_bStatus == BS_PEACE)
				dwLeftTime = pBattle->m_dwPeaceDur;

			LPPACKETBUF pMSG = new PACKETBUF();
			pMSG->m_packet.SetID(SM_BATTLESTATUS_REQ)
				<< pBattle->m_bType
				<< pBattle->m_bStatus
				<< pBattle->m_dwBattleStart
				<< dwLeftTime;

			SayToBATCH(pMSG);
		}
	}
}

void CTWorldSvrModule::CheckSoulmateEnd(LPTCHARACTER pTCHAR, LPTSOULMATE pTSOUL)
{
	if(!pTCHAR || !pTSOUL )
		return;

	if(pTSOUL->m_dwTime)
		return;

	BYTE bLevel = pTSOUL->m_bLevel;

	// 레벨비교
	if(abs(pTCHAR->m_bLevel - bLevel) > SOULMATE_LEVEL)
		SendDM_SOULMATEEND_REQ(pTCHAR->m_dwCharID, pTCHAR->m_dwKEY, (DWORD)m_timeCurrent);
}

void CTWorldSvrModule::RegSoulmate(LPTCHARACTER pTCHAR, LPTCHARACTER pTarget, BYTE bSearch,BYTE bNpcInvenID, BYTE bNpcItemID)
{
	CTServer * pCon = FindMapSvr(pTCHAR->m_bMainID);
	if(!pCon)
		return;

	LPTSOULMATE pTSOUL = NULL;

	MAPTSOULMATE::iterator find = pTCHAR->m_mapTSOULMATE.find(pTCHAR->m_dwCharID);
	if(find!=pTCHAR->m_mapTSOULMATE.end())
	{
		pTSOUL = (*find).second;
		LPTCHARACTER pSoul = FindTChar(pTSOUL->m_strName);
		if(pSoul)
		{
			MAPTSOULMATE::iterator itPlayer = pSoul->m_mapTSOULMATE.find(pTCHAR->m_dwCharID);
			if(itPlayer!=pSoul->m_mapTSOULMATE.end())
			{
				delete (*itPlayer).second;
				pSoul->m_mapTSOULMATE.erase(itPlayer);
			}
		}
	}
	else
	{
		pTSOUL = new TSOULMATE();
		pTSOUL->m_dwCharID = pTCHAR->m_dwCharID;
		pTCHAR->m_mapTSOULMATE.insert(MAPTSOULMATE::value_type(pTSOUL->m_dwCharID, pTSOUL));
	}

	pTCHAR->m_dwSoulSilence = 0;
	pTSOUL->m_dwTarget = pTarget->m_dwCharID;
	pTSOUL->m_strName = pTarget->m_strNAME;
	pTSOUL->m_bLevel = pTarget->m_bLevel;
	pTSOUL->m_bConnected = TRUE;
	pTSOUL->m_bClass = pTarget->m_bClass;
	pTSOUL->m_dwRegion = pTarget->m_dwRegion;
	pTSOUL->m_dwTime = 0;

	LPTSOULMATE pSoulTarget = new TSOULMATE();
	pSoulTarget->m_dwCharID = pTCHAR->m_dwCharID;
	pSoulTarget->m_dwTarget = pTCHAR->m_dwCharID;
	pSoulTarget->m_strName = pTCHAR->m_strNAME;
	pSoulTarget->m_bLevel = pTCHAR->m_bLevel;
	pSoulTarget->m_bConnected = TRUE;
	pSoulTarget->m_bClass = pTCHAR->m_bClass;
	pSoulTarget->m_dwRegion = pTCHAR->m_dwRegion;
	pSoulTarget->m_dwTime = 0;
	pTarget->m_mapTSOULMATE.insert(MAPTSOULMATE::value_type(pSoulTarget->m_dwCharID, pSoulTarget));

	if(bSearch)
		pCon->SendMW_SOULMATESEARCH_REQ(
			pTCHAR->m_dwCharID,
			pTCHAR->m_dwKEY,
			SOULMATE_SUCCESS,
			pTarget->m_dwCharID,
			pTarget->m_strNAME,
			pTarget->m_dwRegion,
			bNpcInvenID,
			bNpcItemID);
	else
		pCon->SendMW_SOULMATEREG_REQ(
			pTCHAR->m_dwCharID,
			pTCHAR->m_dwKEY,
			SOULMATE_SUCCESS,
			TRUE,
			bNpcInvenID,
			bNpcItemID,
			pTarget->m_dwCharID,
			pTarget->m_strNAME,
			pTarget->m_dwRegion);
}

void CTWorldSvrModule::SoulmateDel(LPTCHARACTER pTCHAR, DWORD dwSoul)
{
	MAPTSOULMATE::iterator find = pTCHAR->m_mapTSOULMATE.find(pTCHAR->m_dwCharID);
	if(find==pTCHAR->m_mapTSOULMATE.end())
		return;

	LPTSOULMATE pTSOUL = (*find).second;
	if(pTSOUL->m_dwTarget != dwSoul)
		return;

	LPTCHARACTER pSoul = FindTChar(pTSOUL->m_strName);
	if(pSoul)
	{
		MAPTSOULMATE::iterator itPlayer = pSoul->m_mapTSOULMATE.find(pTCHAR->m_dwCharID);
		if(itPlayer!=pSoul->m_mapTSOULMATE.end())
		{
			delete (*itPlayer).second;
			pSoul->m_mapTSOULMATE.erase(itPlayer);
		}
	}

	pTCHAR->m_mapTSOULMATE.erase(find);

	CTServer * pCon = FindMapSvr(pTCHAR->m_bMainID);
	if(pCon)
		pCon->SendMW_SOULMATEEND_REQ(
			pTCHAR->m_dwCharID,
			pTCHAR->m_dwKEY,
			SOULMATE_SUCCESS,
			0);

	delete pTSOUL;
}

void CTWorldSvrModule::SoulmateEnd(LPTCHARACTER pTCHAR, DWORD dwTime)
{
	MAPTSOULMATE::iterator find = pTCHAR->m_mapTSOULMATE.find(pTCHAR->m_dwCharID);
	if(find==pTCHAR->m_mapTSOULMATE.end())
		return;

	LPTSOULMATE pTSOUL = (*find).second;

	pTCHAR->m_dwSoulSilence = dwTime;

	LPTCHARACTER pSoul = FindTChar(pTSOUL->m_strName);

	if(pSoul)
	{
		MAPTSOULMATE::iterator itPlayer = pSoul->m_mapTSOULMATE.find(pTCHAR->m_dwCharID);
		if(itPlayer!=pSoul->m_mapTSOULMATE.end())
		{
			delete (*itPlayer).second;
			pSoul->m_mapTSOULMATE.erase(itPlayer);
		}
	}

	CTServer * pMAP = FindMapSvr(pTCHAR->m_bMainID);
	if(pMAP)
		pMAP->SendMW_SOULMATEEND_REQ(
			pTCHAR->m_dwCharID,
			pTCHAR->m_dwKEY,
			SOULMATE_SUCCESS,
			dwTime);

	delete pTSOUL;
	pTCHAR->m_mapTSOULMATE.erase(find);
}

void CTWorldSvrModule::EraseFriend(LPTCHARACTER pCHAR, DWORD dwTarget)
{
	CTServer * pCon = FindMapSvr(pCHAR->m_bMainID);
	if(!pCon)
		return;

	MAPTFRIEND::iterator it = pCHAR->m_mapTFRIEND.find(dwTarget);
	if(it==pCHAR->m_mapTFRIEND.end())
	{
		pCon->SendMW_FRIENDERASE_REQ(
			pCHAR->m_dwCharID,
			pCHAR->m_dwKEY,
			FRIEND_NOTFOUND,
			dwTarget);

		return;
	}

	if((*it).second->m_bType == FT_FRIENDFRIEND)
	{
		if((*it).second->m_bConnected)
		{
			LPTCHARACTER pTarget = FindTChar((*it).second->m_strName);
			if(pTarget)
			{
				MAPTFRIEND::iterator itFr = pTarget->m_mapTFRIEND.find(pCHAR->m_dwCharID);
				if(itFr != pTarget->m_mapTFRIEND.end())
					(*itFr).second->m_bType = FT_FRIEND;
			}
		}

		(*it).second->m_bType = FT_TARGET;
	}
	else if((*it).second->m_bType == FT_FRIEND)
	{
		if((*it).second->m_bConnected)
		{
			LPTCHARACTER pTarget = FindTChar((*it).second->m_strName);
			if(pTarget)
			{
				MAPTFRIEND::iterator itFr = pTarget->m_mapTFRIEND.find(pCHAR->m_dwCharID);
				if(itFr != pTarget->m_mapTFRIEND.end())
				{
					delete (*itFr).second;
					pTarget->m_mapTFRIEND.erase(itFr);
				}
			}
		}

		delete (*it).second;
		pCHAR->m_mapTFRIEND.erase(it);
	}

	pCon->SendMW_FRIENDERASE_REQ(
		pCHAR->m_dwCharID,
		pCHAR->m_dwKEY,
		FRIEND_SUCCESS,
		dwTarget);
}

void CTWorldSvrModule::SaveAllPvPRecord()
{
	MAPTGUILD::iterator it;
	for(it=m_mapTGuild.begin(); it!=m_mapTGuild.end(); it++)
	{
		CTGuild * pGuild = (*it).second;

		MAPTGUILDMEMBER::iterator itM;
		for(itM=pGuild->m_mapTMember.begin(); itM!=pGuild->m_mapTMember.end(); itM++)
			SendDM_PVPRECORD_REQ(pGuild->m_dwID, (*itM).second);

		MAPTTACTICSMEMBER::iterator itT;
		for(itT=pGuild->m_mapTTactics.begin(); itT!=pGuild->m_mapTTactics.end(); itT++)
			SendDM_TACTICSPOINT_REQ((*itT).second);
	}
}

void CTWorldSvrModule::ResetCastleApply(CTGuild * pGuild, WORD wCastleID)
{
	MAPTGUILDMEMBER::iterator itM;
	for(itM=pGuild->m_mapTMember.begin(); itM!=pGuild->m_mapTMember.end(); itM++)
	{
		if((*itM).second->m_wCastle != wCastleID)
			continue;

		(*itM).second->m_wCastle = 0;
		(*itM).second->m_bCamp = 0;

		LPTCHARACTER pChar = (*itM).second->m_pChar;
		if(pChar)
		{
			CTServer * pMap = FindMapSvr(pChar->m_bMainID);
			if(pMap)
				pMap->SendMW_CASTLEAPPLY_REQ(
					pChar->m_dwCharID,
					pChar->m_dwKEY,
					CBS_SUCCESS,
					0,
					pChar->m_dwCharID,
					0); 
		}
	}

	MAPTTACTICSMEMBER::iterator itT;
	for(itT=pGuild->m_mapTTactics.begin(); itT!=pGuild->m_mapTTactics.end(); itT++)
	{
		if((*itT).second->m_wCastle != wCastleID)
			continue;

		(*itT).second->m_wCastle = 0;
		(*itT).second->m_bCamp = 0;

		LPTCHARACTER pChar = (*itT).second->m_pChar;
		if(pChar)
		{
			CTServer * pMap = FindMapSvr(pChar->m_bMainID);
			if(pMap)
				pMap->SendMW_CASTLEAPPLY_REQ(
					pChar->m_dwCharID,
					pChar->m_dwKEY,
					CBS_SUCCESS,
					0,
					pChar->m_dwCharID,
					0); 
		}
	}
}

DWORD CTWorldSvrModule::GenRecallID()
{
	return ++m_dwGenRecallID;
}

BYTE CTWorldSvrModule::CheckCorpsJoin(CTParty * pOrigin, CTParty * pTarget)
{
	if(pOrigin->m_wCorpsID && pTarget->m_wCorpsID)
		return CORPS_WRONG_TARGET;

	if(pOrigin->m_wCorpsID)
	{
		CTCorps * pCorps = FindCorps(pOrigin->m_wCorpsID);
		if(!pCorps)
			return CORPS_WRONG_TARGET;

		if(pCorps->m_mapParty.size() >= MAX_CORPS_PARTY)
			return CORPS_MAX_PARTY;
	}

	if(pTarget->m_wCorpsID)
	{
		CTCorps * pCorps = FindCorps(pTarget->m_wCorpsID);
		if(!pCorps)
			return CORPS_WRONG_TARGET;

		if(pCorps->m_mapParty.size() >= MAX_CORPS_PARTY)
			return CORPS_MAX_PARTY;
	}

	return 0;
}

BYTE CTWorldSvrModule::AddGuildWanted(CTGuild * pGuild,
									  BYTE bMinLevel,
									  BYTE bMaxLevel,
									  CString strTitle,
									  CString strText,
									  INT64 dlEndTime)
{
#include "AddGuildWanted1.inc"
}

BYTE CTWorldSvrModule::AddGuildTacticsWanted(CTGuild * pGuild,
                                             DWORD dwID,
                                             DWORD dwPoint,
                                             DWORD dwGold,
                                             DWORD dwSilver,
                                             DWORD dwCooper,
                                             BYTE bDay,
                                             BYTE bMinLevel,
                                             BYTE bMaxLevel,
                                             CString strTitle,
                                             CString strText,
											 INT64 dlEndTime)
{
#include "AddGuildTacticsWanted1.inc"
}

BYTE CTWorldSvrModule::DelGuildWanted(CTGuild * pGuild)
{
	if(!pGuild)
		return GUILD_FAIL;

	MAPTGUILDWANTED::iterator it = m_mapTGuildWanted.find(pGuild->m_dwID);
	if(it==m_mapTGuildWanted.end())
		return GUILD_FAIL;

	while(!(*it).second.m_mapTWantedApp.empty())
		DelGuildWantedApp(((*it).second.m_mapTWantedApp.begin())->second->m_dwCharID);

	m_mapTGuildWanted.erase(it);

	return GUILD_SUCCESS;
}

BYTE CTWorldSvrModule::DelGuildTacticsWanted(CTGuild * pGuild, DWORD dwID)
{
	if(!pGuild)
		return GUILD_FAIL;

	LPVTGUILDTACTICSWANTED pList = FindGuildTacticsList(pGuild->m_dwID);
	if(!pList)
		return GUILD_FAIL;

	for(DWORD i=0; i<pList->size(); i++)
	{
		if((*pList)[i].m_dwID == dwID)
		{
			while(!(*pList)[i].m_mapTWantedApp.empty())
				DelGuildTacticsWantedApp(((*pList)[i].m_mapTWantedApp.begin())->second->m_dwCharID);

			pList->erase(pList->begin() + i);
			return GUILD_SUCCESS;
		}
	}

	return GUILD_FAIL;
}

LPTGUILDWANTED CTWorldSvrModule::FindGuildWanted(DWORD dwGuildID)
{
#include "FindGuildWanted1.inc"
}

LPTGUILDTACTICSWANTED CTWorldSvrModule::FindGuildTacticsWanted(DWORD dwGuildID, DWORD dwID)
{
#include "FindGuildTacticsWanted1.inc"
}

LPTGUILDTACTICSWANTED CTWorldSvrModule::FindGuildTacticsWanted(DWORD dwID)
{
#include "FindGuildTacticsWanted2.inc"
}

LPTGUILDWANTEDAPP CTWorldSvrModule::FindGuildWantedApp(DWORD dwCharID)
{
#include "FindGuildWantedApp1.inc"
}

LPTGUILDTACTICSWANTEDAPP CTWorldSvrModule::FindGuildTacticsWantedApp(DWORD dwCharID)
{
#include "FindGuildTacticsWantedApp1.inc"
}

LPVTGUILDTACTICSWANTED CTWorldSvrModule::FindGuildTacticsList(DWORD dwGuildID)
{
#include "FindGuildTacticsList1.inc"
}

BYTE CTWorldSvrModule::AddGuildWantedApp(DWORD dwID,
										 DWORD dwCharID,
										 CString strNAME,
										 BYTE bLevel,
										 BYTE bClass,
										 LPTCHARACTER pTCHAR)
{
#include "AddGuildWantedApp1.inc"
}
BYTE CTWorldSvrModule::AddGuildTacticsWantedApp(DWORD dwID,
												DWORD dwCharID,
												CString strNAME,
												BYTE bLevel,
												BYTE bClass,
												DWORD dwGuildID,
												LPTCHARACTER pTCHAR)
{
#include "AddGuildTacticsWantedApp1.inc"
}
BYTE CTWorldSvrModule::DelGuildWantedApp(DWORD dwCharID)
{
	MAPTGUILDWANTEDAPP::iterator it = m_mapTGuildWantedApp.find(dwCharID);
	if(it==m_mapTGuildWantedApp.end())
		return GUILD_FAIL;

	LPTGUILDWANTEDAPP pApp = (*it).second;
	LPTGUILDWANTED pWanted = FindGuildWanted(pApp->m_dwWantedID);
	if(pWanted)
		pWanted->m_mapTWantedApp.erase(dwCharID);

	delete pApp;
	m_mapTGuildWantedApp.erase(it);

	SendDM_GUILDVOLUNTEERINGDEL_REQ(GUILDAPP_MEMBER, dwCharID);
	return GUILD_SUCCESS;

}
BYTE CTWorldSvrModule::DelGuildTacticsWantedApp(DWORD dwCharID)
{
	MAPTGUILDTACTICSWANTEDAPP::iterator it = m_mapTGuildTacticsWantedApp.find(dwCharID);
	if(it==m_mapTGuildTacticsWantedApp.end())
		return GUILD_FAIL;

	LPTGUILDTACTICSWANTEDAPP pApp = (*it).second;
	LPTGUILDTACTICSWANTED pWanted = FindGuildTacticsWanted(pApp->m_dwWantedGuildID, pApp->m_dwWantedID);
	if(pWanted)
		pWanted->m_mapTWantedApp.erase(dwCharID);

	delete pApp;
	m_mapTGuildTacticsWantedApp.erase(it);

	SendDM_GUILDVOLUNTEERINGDEL_REQ(GUILDAPP_TACTICS, dwCharID);
	return GUILD_SUCCESS;
}

BYTE CTWorldSvrModule::ApplyGuildApp(DWORD dwCharID)
{
	LPTGUILDWANTEDAPP pApp = FindGuildWantedApp(dwCharID);
	if(!pApp)
		return GUILD_FAIL;

	CTGuild * pGuild = FindTGuild(pApp->m_dwWantedID);
	if(!pGuild)
		return GUILD_FAIL;

	MAPDWORD::iterator itGM = m_mapCharGuild.find(dwCharID);
	if(itGM != m_mapCharGuild.end())
		return GUILD_HAVEGUILD;

	if(!pGuild->CanAddMember())
		return GUILD_MEMBER_FULL;

	if(pGuild->FindMember(dwCharID))
		return GUILD_ALREADYMEMBER;

	if(pGuild->FindTactics(dwCharID))
		return GUILD_SAMEGUILDTACTICS;

	if(!pGuild->FindChief())
		return GUILD_FAIL;

	LPTGUILDMEMBER pMember = new TGUILDMEMBER();
	pMember->m_bDuty = GUILD_DUTY_NONE;
	pMember->m_bPeer = GUILD_PEER_NONE;
	pMember->m_dwID = pApp->m_dwCharID;
	pMember->m_pChar = NULL;
	pMember->m_bLevel = pApp->m_bLevel;
	pMember->m_bClass = pApp->m_bClass;
	pMember->m_strName = pApp->m_strName;
	pMember->m_dwTactics = FindCharTacticsID(dwCharID);
	pMember->m_bWarCountry = pGuild->m_bCountry;
	GuildMemberAdd(pGuild, pMember);
	NotifyAddGuildMember(pMember, pGuild);

	return GUILD_SUCCESS;
}

BYTE CTWorldSvrModule::ApplyGuildTacticsApp(DWORD dwCharID)
{
	LPTGUILDTACTICSWANTEDAPP pApp = FindGuildTacticsWantedApp(dwCharID);
	if(!pApp)
		return GUILD_FAIL;

	CTGuild * pGuild = FindTGuild(pApp->m_dwWantedGuildID);
	if(!pGuild)
		return GUILD_FAIL;

	MAPDWORD::iterator itTM = m_mapCharTactics.find(dwCharID);
	if(itTM != m_mapCharTactics.end())
		return GUILD_HAVEGUILD;

	MAPDWORD::iterator itGM = m_mapCharGuild.find(dwCharID);
	if(itGM != m_mapCharGuild.end())
	{
		CTGuild * pTG = FindTGuild((*itGM).second);
		if(pTG)
		{
			LPTGUILDMEMBER pGM = pTG->FindMember(dwCharID);
			if(pGM && pGM->m_bDuty >= GUILD_DUTY_VICECHIEF)
				return GUILD_NODUTY;
		}
	}

	if(pGuild->FindTactics(dwCharID))
		return GUILD_ALREADYMEMBER;

	if(pGuild->FindMember(dwCharID))
		return GUILD_SAMEGUILDTACTICS;

	if(pGuild->m_dwPvPUseablePoint < pApp->m_dwPoint)
		return GUILD_NOPOINT;

	if(!pGuild->CanAddTactics())
		return GUILD_MEMBER_FULL;

	if(!pGuild->UseMoney(pApp->m_dwGold, pApp->m_dwSilver, pApp->m_dwCooper, TRUE))
		return GUILD_NOMONEY;

	pGuild->UsePvPoint(pApp->m_dwPoint, PVP_USEABLE);

	LPTTACTICSMEMBER pMember = new TTACTICSMEMBER();
	pMember->m_dwID = pApp->m_dwCharID;
	pMember->m_strName = pApp->m_strName;
	pMember->m_bLevel = pApp->m_bLevel;
	pMember->m_bClass = pApp->m_bClass;
	pMember->m_dwRewardPoint = pApp->m_dwPoint;
	pMember->m_dlRewardMoney = CalcMoney(pApp->m_dwGold, pApp->m_dwSilver, pApp->m_dwCooper);
	pMember->m_dwGainPoint = 0;
	pMember->m_bDay = pApp->m_bDay;
	pMember->m_dlEndTime = m_timeCurrent + pApp->m_bDay * DAY_ONE;
	pMember->m_pChar = NULL;

	LPTCHARACTER pChar = FindTChar(pMember->m_strName);
	if(pChar)
	{
		pMember->m_pChar = pChar;
		pMember->m_bLevel = pChar->m_bLevel;
		pChar->m_pTactics = pGuild;

		CTServer * pServer = FindMapSvr(pChar->m_bMainID);
		if(pServer)
			pServer->SendMW_GUILDTACTICSREPLY_REQ(
				pChar->m_dwCharID,
				pChar->m_dwKEY,
				GUILD_SUCCESS,
				pMember->m_dwID,
				pGuild->m_dwID,
				pGuild->m_strName,
				pMember->m_strName,
				pApp->m_dwGold,
				pApp->m_dwSilver,
				pApp->m_dwCooper);
	}

	LPTGUILDMEMBER pChief = pGuild->FindChief();
	if(pChief && pChief->m_pChar)
	{
		CTServer * pServer = FindMapSvr(pChief->m_pChar->m_bMainID);
		if(pServer)
			pServer->SendMW_GUILDTACTICSREPLY_REQ(
				pChief->m_pChar->m_dwCharID,
				pChief->m_pChar->m_dwKEY,
				GUILD_SUCCESS,
				pMember->m_dwID,
				pGuild->m_dwID,
				pGuild->m_strName,
				pMember->m_strName,
				pApp->m_dwGold,
				pApp->m_dwSilver,
				pApp->m_dwCooper);
	}

	GuildTacticsAdd(pGuild, pMember);

	return GUILD_SUCCESS;
}

void CTWorldSvrModule::NotifyAddGuildMember(LPTGUILDMEMBER pMember, CTGuild * pGuild)
{
	LPTCHARACTER pChar = FindTChar(pMember->m_strName);
	if(pChar)
	{
		pMember->m_pChar = pChar;
		pChar->m_pGuild = pGuild;

		CTServer * pServer = FindMapSvr(pChar->m_bMainID);
		if(pServer)
			pServer->SendMW_GUILDJOIN_REQ(
				pChar->m_dwCharID,
				pChar->m_dwKEY,
				GUILD_JOIN_SUCCESS,
				pGuild->m_dwID,
				pGuild->m_dwFame,
				pGuild->m_dwFameColor,
				pGuild->m_strName,
				pMember->m_dwID,
				pMember->m_strName);
	}

	LPTGUILDMEMBER pChief = pGuild->FindChief();
	if(!pChief)
		return;

	if(pMember->m_dwID == pChief->m_dwID)
		return;

	LPTCHARACTER pChiefChar = FindTChar(pChief->m_strName);
	if(pChiefChar)
	{
		CTServer * pServer = FindMapSvr(pChiefChar->m_bMainID);
		if(pServer)
			pServer->SendMW_GUILDJOIN_REQ(
				pChiefChar->m_dwCharID,
				pChiefChar->m_dwKEY,
				GUILD_JOIN_SUCCESS,
				pGuild->m_dwID,
				pGuild->m_dwFame,
				pGuild->m_dwFameColor,
				pGuild->m_strName,
				pMember->m_dwID,
				pMember->m_strName);
	}
}

void CTWorldSvrModule::NotifyGuildWantedList(DWORD dwCharID, DWORD dwKey, CTServer * pServer)
{
	LPTCHARACTER pTCHAR = FindTChar(dwCharID, dwKey);

	if(!pTCHAR)	return;

	LPTGUILDWANTEDAPP pApp = FindGuildWantedApp(dwCharID);

	pServer->SendMW_GUILDWANTEDLIST_REQ(
		pTCHAR->m_dwCharID,
		pTCHAR->m_dwKEY,
		pTCHAR->m_bCountry,
		pApp,
		&m_mapTGuildWanted);
}
void CTWorldSvrModule::NotifyGuildTacticsWantedList(DWORD dwCharID, DWORD dwKey, CTServer * pServer)
{
	LPTCHARACTER pTCHAR = FindTChar(dwCharID, dwKey);

	if(!pTCHAR)	return;

	LPTGUILDTACTICSWANTEDAPP pApp = FindGuildTacticsWantedApp(dwCharID);

	pServer->SendMW_GUILDTACTICSWANTEDLIST_REQ(
		pTCHAR->m_dwCharID,
		pTCHAR->m_dwKEY,
		GetWarCountry(pTCHAR),
		pApp,
		&m_mapTGuildTacticsWanted);
}

void CTWorldSvrModule::NotifyGuildVolunteerList(DWORD dwCharID, DWORD dwKey, CTServer * pServer)
{
	LPTCHARACTER pTCHAR = FindTChar(dwCharID, dwKey);

	if(!pTCHAR || !pTCHAR->m_pGuild)
		return;

	LPTGUILDWANTED pWanted = FindGuildWanted(pTCHAR->m_pGuild->m_dwID);
	if(pWanted)
	{
		MAPTGUILDWANTEDAPP::iterator it;
		for(it=pWanted->m_mapTWantedApp.begin(); it!=pWanted->m_mapTWantedApp.end(); it++)
		{
			LPTCHARACTER pTarget = FindTChar((*it).second->m_strName);
			if(pTarget)
				(*it).second->m_dwRegion = pTarget->m_dwRegion;
			else
				(*it).second->m_dwRegion = 0;
		}
	}

	pServer->SendMW_GUILDVOLUNTEERLIST_REQ(
		dwCharID,
		dwKey,
		pWanted ? &(pWanted->m_mapTWantedApp) : NULL);
}
void CTWorldSvrModule::NotifyGuildTacticsVolunteerList(DWORD dwCharID, DWORD dwKey, CTServer * pServer)
{
	LPTCHARACTER pTCHAR = FindTChar(dwCharID, dwKey);

	if(!pTCHAR || !pTCHAR->m_pGuild)
		return;

	MAPTGUILDTACTICSWANTEDAPP app;
	app.clear();

	LPVTGUILDTACTICSWANTED pWanted = FindGuildTacticsList(pTCHAR->m_pGuild->m_dwID);
	if(pWanted)
	{
		MAPTGUILDTACTICSWANTEDAPP::iterator it;
		for(DWORD i=0; i<pWanted->size(); i++)
		{
			for(it=(*pWanted)[i].m_mapTWantedApp.begin(); it!=(*pWanted)[i].m_mapTWantedApp.end(); it++)
			{
				LPTCHARACTER pTarget = FindTChar((*it).second->m_strName);
				if(pTarget)
					(*it).second->m_dwRegion = pTarget->m_dwRegion;
				else
					(*it).second->m_dwRegion = 0;

				app.insert(MAPTGUILDTACTICSWANTEDAPP::value_type((*it).first, (*it).second));
			}
		}
	}

	pServer->SendMW_GUILDTACTICSVOLUNTEERLIST_REQ(
		dwCharID,
		dwKey,
		&app);

	app.clear();
}

CTGuild * CTWorldSvrModule::GetCurGuild(LPTCHARACTER pTCHAR)
{
	if(pTCHAR->m_pTactics)
		return pTCHAR->m_pTactics;

	if(pTCHAR->m_pGuild)
		return pTCHAR->m_pGuild;

	return NULL;
}

DWORD CTWorldSvrModule::FindCharTacticsID(DWORD dwCharID)
{
	MAPTGUILD::iterator it;
	for(it=m_mapTGuild.begin(); it!=m_mapTGuild.end(); it++)
	{
		if((*it).second->FindTactics(dwCharID))
			return (*it).second->m_dwID;
	}

	return 0;
}

void CTWorldSvrModule::OnEventExpired(BYTE bInsert,
									  BYTE bType,
									  INT64 timeExpired,
									  DWORD dwValue_1=0,
									  DWORD dwValue_2=0)
{
	LPPACKETBUF pMSG = new PACKETBUF();
	pMSG->m_packet.SetID(SM_EVENTEXPIRED_REQ)
		<< bInsert
		<< bType
		<< timeExpired
		<< dwValue_1
		<< dwValue_2;

	SayToTIMER(pMSG);
}

void CTWorldSvrModule::CheckEventExpired()
{
	VTEXPIREDBUF::iterator it = m_vExpired.begin();
	while(it != m_vExpired.end())
	{
		if((*it).m_timeExpired <= m_timeCurrent )
		{
			LPPACKETBUF pBUF = new PACKETBUF();
			pBUF->m_packet.SetID(SM_EVENTEXPIRED_ACK)
				<< (*it).m_bType
				<< (*it).m_timeExpired
				<< (*it).m_dwValue_1
				<< (*it).m_dwValue_2;

			SayToBATCH(pBUF);

			it = m_vExpired.erase(it);
		}
		else
			break;
	}
}

void CTWorldSvrModule::SendPost(BYTE bType,
								DWORD dwValue,
								DWORD dwSenderID,
								CString strSender,
								DWORD dwRecvID,
								CString strRecver,
								INT64 dlValue)
{
	if(m_mapSERVER.empty())
		return;

	MAPTSERVER::iterator finder = m_mapSERVER.begin();

	(*finder).second->SendMW_WORLDPOSTSEND_REQ(
		bType,
		dwValue,
		dwSenderID,
		strSender,
		dwRecvID,
		strRecver,
		dlValue);
}

void CTWorldSvrModule::SendPost(BYTE	bType,
								DWORD	dwRecvID,
								CString	strRecver,
								CString	strTitle,
								CString	strMessage,
								WORD	wItemID,
								BYTE	bItemNum,
								WORD	wUseTime)
{
	if(m_mapSERVER.empty() || bType != WPT_LOTITEM)
		return;

	MAPTSERVER::iterator finder = m_mapSERVER.begin();

	(*finder).second->SendMW_WORLDPOSTSEND_REQ(
		bType,
		dwRecvID,
		strRecver,
		strTitle,
		strMessage,
		wItemID,
		bItemNum,
		wUseTime);
}

void CTWorldSvrModule::NotifyCastleApply(WORD wCastle, CTGuild * pGuild)
{
	WORD wValue = pGuild->GetCastleApplicantCount(wCastle);

	MAPTSERVER::iterator finder;
	for(finder = m_mapSERVER.begin(); finder != m_mapSERVER.end(); finder++)
	{
		(*finder).second->SendMW_CASTLEAPPLICANTCOUNT_REQ(
			wCastle,
			pGuild->m_dwID,
			HIBYTE(wValue),
			LOBYTE(wValue));
	}
}

void CTWorldSvrModule::GuildMemberAdd(CTGuild * pGuild, LPTGUILDMEMBER pMember)
{
	if(m_pRelay)
		m_pRelay->SendRW_GUILDADD_ACK(pMember->m_dwID, pGuild->m_dwID, pGuild->m_dwChief);

	SendDM_GUILDMEMBERADD_REQ(
		pGuild->m_dwID,
		pMember->m_dwID,
		pMember->m_bLevel,
		pMember->m_bDuty);

	DelGuildWantedApp(pMember->m_dwID);
	m_mapCharGuild.insert(MAPDWORD::value_type(pMember->m_dwID, pGuild->m_dwID));
	pGuild->AddMember(pMember);
}
void CTWorldSvrModule::GuildMemberDel(CTGuild * pGuild, LPTGUILDMEMBER pMember, BYTE bReason)
{
	if(m_pRelay)
		m_pRelay->SendRW_GUILDDEL_ACK(pMember->m_dwID, pGuild->m_dwID);

	SendDM_GUILDLEAVE_REQ(
		pGuild->m_dwID,
		pMember->m_dwID,
		bReason,
		DWORD(m_timeCurrent));

	m_mapCharGuild.erase(pMember->m_dwID);
	pGuild->DelMember(pMember);
}
void CTWorldSvrModule::GuildTacticsAdd(CTGuild * pGuild, LPTTACTICSMEMBER pMember)
{
	if(m_pRelay)
		m_pRelay->SendRW_TACTICSADD_ACK(pMember->m_dwID, pGuild->m_dwID, pGuild->m_dwChief);

	SendDM_GUILDTACTICSADD_REQ(
		pGuild->m_dwID,
		pMember->m_dwID,
		pMember->m_dwRewardPoint,
		pMember->m_dlRewardMoney,
		pMember->m_bDay,
		pMember->m_dlEndTime);

	DelGuildTacticsWantedApp(pMember->m_dwID);
	m_mapCharTactics.insert(MAPDWORD::value_type(pMember->m_dwID, pGuild->m_dwID));
	pGuild->AddTactics(pMember);

	MAPDWORD::iterator itOG = m_mapCharGuild.find(pMember->m_dwID);
	if(itOG != m_mapCharGuild.end())
	{
		CTGuild * pOG = FindTGuild(itOG->second);
		if(pOG)
		{
			LPTGUILDMEMBER pOM = pOG->FindMember(pMember->m_dwID);
			if(pOM && pOM->m_wCastle)
			{
				WORD wCastle = pOM->m_wCastle;
				pOM->m_wCastle = 0;
				pOM->m_bCamp = 0;

				SendDM_CASTLEAPPLY_REQ(0, pMember->m_dwID, 0);
				NotifyCastleApply(wCastle, pOG);
			}
		}
	}
}
BYTE CTWorldSvrModule::GuildTacticsDel(CTGuild * pGuild, LPTTACTICSMEMBER pMember, BYTE bKick)
{
	if(bKick)
	{
		pGuild->PointLog(pMember->m_dwRewardPoint, pMember->m_strName, m_timeCurrent);
		SendDM_GUILDPOINTREWARD_REQ(pGuild->m_dwID, pMember->m_dwRewardPoint, pMember->m_strName, pGuild->m_dwPvPTotalPoint, pGuild->m_dwPvPUseablePoint);
	}

	MAPTCHARACTER::iterator it = m_mapTCHAR.find(pMember->m_dwID);
	if(it!=m_mapTCHAR.end())
	{
		LPTCHARACTER pTarget = (*it).second;
		pTarget->m_pTactics = NULL;

		CTServer * pServer = FindMapSvr(pTarget->m_bMainID);
		if(pServer)
		{
			pServer->SendMW_GUILDTACTICSKICKOUT_REQ(
				pTarget->m_dwCharID,
				pTarget->m_dwKEY,
				GUILD_SUCCESS,
				pMember->m_dwID,
				bKick);

			if( bKick && pTarget->m_bSave)
				pServer->SendMW_GAINPVPPOINT_REQ(pMember->m_dwID, pMember->m_dwRewardPoint, PVPE_GUILD, PVP_USEABLE, TRUE);
		}
	}

	BYTE bWP = WPT_NONE;
	if(bKick == 1)
		bWP = WPT_TACTICSKICK;
	else if(bKick >= 2)
		bWP = WPT_TACTICSEND;

	LPTGUILDMEMBER pChief = pGuild->FindChief();
	if(bWP != WPT_NONE && pChief)
		SendPost(bWP, pMember->m_dwRewardPoint, pChief->m_dwID, pChief->m_strName, pMember->m_dwID, pMember->m_strName, pMember->m_dlRewardMoney);

	if(m_pRelay)
		m_pRelay->SendRW_TACTICSDEL_ACK(pMember->m_dwID, pGuild->m_dwID);

	SendDM_GUILDTACTICSDEL_REQ(pMember->m_dwID);

	m_mapCharTactics.erase(pMember->m_dwID);
	pGuild->DelTactics(pMember, bKick);

	return GUILD_SUCCESS;
}

CString CTWorldSvrModule::GetSvrMsg(DWORD dwID)
{
	MAPTSTRING::iterator it = m_mapTSvrMsg.find(dwID);
	if(it!=m_mapTSvrMsg.end())
		return (*it).second;
	else
		return NAME_NULL;
}

CString CTWorldSvrModule::BuildNetString( const CString& strHeader, const CString& strBody)
{
#include "BuildNetString.inc"
}

LPTITEM CTWorldSvrModule::CreateItem(CPacket * pPacket)
{
	LPTITEM pItem = new TITEM();

	(*pPacket)
		>> pItem->m_dlID
		>> pItem->m_bItemID
		>> pItem->m_wItemID
		>> pItem->m_bLevel
		>> pItem->m_bGem
		>> pItem->m_wMoggItemID
		>> pItem->m_bCount
		>> pItem->m_bGLevel
		>> pItem->m_dwDuraMax
		>> pItem->m_dwDuraCur
		>> pItem->m_bRefineCur
		>> pItem->m_dEndTime
		>> pItem->m_bGradeEffect
		>> pItem->m_dwExtValue[IEV_ELD]
		>> pItem->m_dwExtValue[IEV_WRAP]
		>> pItem->m_dwExtValue[IEV_COLOR]
		>> pItem->m_dwExtValue[IEV_GUILD]
		>> pItem->m_bMagicCount;

	for(BYTE i=0; i<pItem->m_bMagicCount; i++)
	{
		(*pPacket)
			>> pItem->m_bMagic[i]
			>> pItem->m_wValue[i];
	}

	return pItem;
}

LPTITEM CTWorldSvrModule::CreateItem(TITEM &item)
{
#include "CreateDatabaseItem.inc"
}

void CTWorldSvrModule::WrapItem(CPacket * pPacket, TITEM &item)
{
	(*pPacket)
		<< item.m_dlID
		<< item.m_bItemID
		<< item.m_wItemID
		<< item.m_bLevel
		<< item.m_bGem
		<< item.m_wMoggItemID
		<< item.m_bCount
		<< item.m_bGLevel
		<< item.m_dwDuraMax
		<< item.m_dwDuraCur
		<< item.m_bRefineCur
		<< item.m_dEndTime
		<< item.m_bGradeEffect
		<< item.m_dwExtValue[IEV_ELD]
		<< item.m_dwExtValue[IEV_WRAP]
		<< item.m_dwExtValue[IEV_COLOR]
		<< item.m_dwExtValue[IEV_GUILD]
		<< item.m_bMagicCount;

	for(BYTE i=0; i<item.m_bMagicCount; i++)
	{
		(*pPacket)
			<< item.m_bMagic[i]
			<< item.m_wValue[i];
	}
}


void CTWorldSvrModule::MonthRankReset()
{
	for(BYTE i = 0; i < COUNTRY_COUNT; i++)
        for(BYTE j = 1; j < MONTHRANKCOUNT; j++)
            m_arMonthRank[i][j].Reset();
}

void CTWorldSvrModule::AddChatBan(CString strName, INT64 dlEndTime)
{
	MAPBANCHAR::iterator it = m_mapBanChar.find(strName);
	if(it!=m_mapBanChar.end())
	{
		if(dlEndTime <= m_timeCurrent)
			m_mapBanChar.erase(it);
		else
			(*it).second = dlEndTime;
	}
	else
		m_mapBanChar.insert(MAPBANCHAR::value_type(strName, dlEndTime));
}

void CTWorldSvrModule::CheckChatBan(LPTCHARACTER pChar, BYTE bRelay)
{
	MAPBANCHAR::iterator it = m_mapBanChar.find(pChar->m_strNAME);
	if(it!=m_mapBanChar.end())
	{
		if((*it).second > m_timeCurrent)
		{
			pChar->m_nChatBanTime = (*it).second;

			if(bRelay)
			{
				if(m_pRelay)
					m_pRelay->SendRW_CHATBAN_ACK(pChar->m_strNAME,pChar->m_nChatBanTime);
			}
			else
			{
				CTServer* pServer = FindMapSvr(pChar->m_bMainID);
				if(pServer)
					pServer->SendMW_CHATBAN_REQ(pChar->m_strNAME,pChar->m_nChatBanTime,CHATBAN_SUCCESS,0,0);
			}
		}
		else
			m_mapBanChar.erase(it);
	}
}

BYTE CTWorldSvrModule::IsFirstGroup(BYTE bCountry, DWORD dwCharID)
{
#include "IsFirstGroup.inc"
}

WORD CTWorldSvrModule::SetTournamentTime(LPMAPTOURNAMENTSTEP pTour, TBATTLETIME battletime, WORD wTourID, BYTE bMonthBase, BYTE bEnable, WORD wTnmtID, BYTE bTnmtGroup, BYTE bTnmtStep)
{
#include "SetTournamentTime.inc"
}

void CTWorldSvrModule::DelTournamentSchedule(WORD wTourID)
{
#include "DeleteTournamentSchedule.inc"
}

BYTE CTWorldSvrModule::CanDoTournament(BYTE bStep, BYTE bGroup)
{
	MAPTOURNAMENT::iterator it = m_mapTournament.find(m_tournament.m_wID);
	if(it == m_mapTournament.end())
		return FALSE;

	if(m_tournament.m_bStep == bStep && (!bGroup || m_tournament.m_bGroup == bGroup))
		return TRUE;

	return FALSE;
}

LPMAPTOURNAMENTENTRY CTWorldSvrModule::GetTournament(WORD wTournamentID)
{
#include "GetTournament.inc"
}

LPTOURNAMENTENTRY CTWorldSvrModule::GetCurrentTournamentEntry(BYTE bEntry)
{
#include "GetCurrentTournamentEntry.inc"
}

void CTWorldSvrModule::AddTNMTPlayer(LPTOURNAMENTENTRY pEntry, LPTNMTPLAYER pPlayer, BYTE bStep, LPTNMTPLAYER pChief)
{
#include "AddTournamentPlayer.inc"
}

void CTWorldSvrModule::DelTNMTPlayer(LPTOURNAMENTENTRY pEntry, LPTNMTPLAYER pPlayer)
{
	pEntry->m_map1st.erase(pPlayer->m_dwCharID);
	pEntry->m_mapNormal.erase(pPlayer->m_dwCharID);
	pEntry->m_mapPlayer.erase(pPlayer->m_dwCharID);

	if(pEntry->m_bType == ENTRY_PARTY)
	{
		LPTNMTPLAYER pChief = FindTNMTPlayer(pPlayer->m_dwChiefID);
		if(pChief)
			pChief->m_mapParty.erase(pPlayer->m_dwCharID);
	}

	m_mapTNMTPlayer.erase(pPlayer->m_dwCharID);
	delete pPlayer;
}

LPTNMTPLAYER CTWorldSvrModule::FindTNMTPlayer(DWORD dwCharID)
{
#include "FindTournamentPlayer.inc"
}

BYTE CTWorldSvrModule::FindTNMTPlayerApply(CString strHWID, DWORD dwIPAddr)
{
	MAPTNMTPLAYER::iterator it;
	BYTE bCounter = NULL;

	for(it = m_mapTNMTPlayer.begin(); it != m_mapTNMTPlayer.end(); it++)
	{
		if( (*it).second->m_strHWID == strHWID ||
			(*it).second->m_dwIPAddr == dwIPAddr)
		{
			return TRUE;
		}
	}

	return FALSE;
}
void CTWorldSvrModule::GetRanking(DWORD dwCharID, DWORD & dwRank, DWORD & dwMonthRank)
{
#include "GetRanking.inc"
}

LPTNMTPLAYER CTWorldSvrModule::FindBatter(BYTE bEntry, LPTCHARACTER pTCHAR)
{
	MAPTNMTPLAYER::iterator it;
	for(it=pTCHAR->m_mapBatting.begin(); it!=pTCHAR->m_mapBatting.end(); it++)
	{
		if((*it).second->m_bEntryID == bEntry)
			return (*it).second;
	}

	return NULL;
}

void CTWorldSvrModule::ResetBatting(LPTNMTPLAYER pTarget, LPTCHARACTER pTCHAR)
{
	MAPDWORD::iterator it = pTarget->m_mapBatting.find(pTCHAR->m_dwCharID);
	if(it!=pTarget->m_mapBatting.end())
	{
		pTarget->m_dwSum -= (*it).second;
		pTarget->m_mapBatting.erase(it);
	}
}

void CTWorldSvrModule::JoinBatting(LPTNMTPLAYER pTarget, LPTCHARACTER pTCHAR)
{
	pTCHAR->m_mapBatting.insert(MAPTNMTPLAYER::value_type(pTarget->m_dwCharID, pTarget));
	pTarget->m_mapBatting.insert(MAPDWORD::value_type(pTCHAR->m_dwCharID, pTCHAR->m_dwTicket));
	pTarget->m_dwSum += pTCHAR->m_dwTicket;
}

void CTWorldSvrModule::GetBattingAmount(LPTNMTPLAYER pTarget, DWORD dwCharID, FLOAT & fRate, DWORD & dwAmount)
{
	fRate = 0;
	dwAmount = 0;

	if(!pTarget)
		return;

	MAPDWORD::iterator it = pTarget->m_mapBatting.find(dwCharID);
	if(it!=pTarget->m_mapBatting.end())
	{
		fRate = FLOAT(pTarget->m_dwSum ? m_tournament.m_dwSum / pTarget->m_dwSum : 0);
		dwAmount = DWORD(m_tournament.m_bBase * (*it).second * fRate);
	}
}

void CTWorldSvrModule::TournamentSelectPlayer()
{
#include "TournamentSelectPlayer.inc"
}

void CTWorldSvrModule::TNMTMatch(LPTOURNAMENTENTRY pEntry, MAPTNMTPLAYER map1st[TCONTRY_COUNT], MAPTNMTPLAYER mapNormal[TCONTRY_COUNT], BYTE bTotal)
{
#include "TournamentMatch.inc"
}

void CTWorldSvrModule::TournamentClear()
{
	WORD wTID = m_tournament.m_wID;

	m_tournament.m_wID = 0;
	m_tournament.m_bGroup = 0;
	m_tournament.m_bStep = TNMTSTEP_READY;
	m_tournament.m_bSelected = FALSE;
	m_tournament.m_dwSum = 0;
	m_tournament.m_bBase = 0;
	m_tournament.m_mapEntry = NULL;
	m_tournament.m_mapStep.clear();

	MAPTNMTPLAYER::iterator itTP;
	for(itTP=m_mapTNMTPlayer.begin(); itTP != m_mapTNMTPlayer.end(); itTP++)
		delete (*itTP).second;
	m_mapTNMTPlayer.clear();

	MAPTCHARACTER::iterator itC;
	for(itC=m_mapTCHAR.begin(); itC!=m_mapTCHAR.end(); itC++)
		(*itC).second->m_mapBatting.clear();

	LPMAPTOURNAMENTENTRY pTour = GetTournament(wTID);
	if(!pTour)
		return;

	MAPTOURNAMENTENTRY::iterator it;
	for(it=pTour->begin(); it!=pTour->end(); it++)
	{
		LPTOURNAMENTENTRY pEntry = (*it).second;
		pEntry->m_map1st.clear();
		pEntry->m_mapNormal.clear();
		pEntry->m_mapPlayer.clear();
	}
}

void CTWorldSvrModule::TournamentApply(LPTCHARACTER pChar, CTServer * pServer, BYTE bEntryID, CString strHWID, DWORD dwIPAddr)
{
	if(pChar->m_bCountry > TCONTRY_B)
	{
		pServer->SendMW_TOURNAMENT_REQ(pChar->m_dwCharID, pChar->m_dwKEY, MW_TOURNAMENTAPPLY_REQ, TOURNAMENT_FAIL);
		return;
	}

	LPTOURNAMENTENTRY pEntry = GetCurrentTournamentEntry(bEntryID);
	if(!pEntry)
	{
		pServer->SendMW_TOURNAMENT_REQ(pChar->m_dwCharID, pChar->m_dwKEY, MW_TOURNAMENTAPPLY_REQ, TOURNAMENT_FAIL);
		return;
	}

	BYTE bResult = TOURNAMENT_DISQUALIFY;
	BYTE bStep = TNMTSTEP_NORMAL;

	if(CanDoTournament(TNMTSTEP_1st))
	{
		for(BYTE j = 1; j < m_bFirstGroupCount; j++)
		{
			if(m_arFirstGradeGroup[pChar->m_bCountry][j].m_dwCharID == pChar->m_dwCharID)
			{
				bResult = TOURNAMENT_SUCCESS;
				bStep = TNMTSTEP_1st;
				break;
			}
		}
	}
	else if(!CanDoTournament(TNMTSTEP_NORMAL))
		bResult = TOURNAMENT_TIMEOUT;
	else
		bResult = TOURNAMENT_SUCCESS;

	if(bResult)
	{
		pServer->SendMW_TOURNAMENT_REQ(pChar->m_dwCharID, pChar->m_dwKEY, MW_TOURNAMENTAPPLY_REQ, bResult);
		return;
	}
	
	if(FindTNMTPlayer(pChar->m_dwCharID))
	{
		pServer->SendMW_TOURNAMENT_REQ(pChar->m_dwCharID, pChar->m_dwKEY, MW_TOURNAMENTAPPLY_REQ, bResult);
		return;
	}

	if(FindTNMTPlayerApply(strHWID, dwIPAddr))
	{
		pServer->SendMW_TOURNAMENT_REQ(pChar->m_dwCharID, pChar->m_dwKEY, MW_TOURNAMENTAPPLY_REQ, TOURNAMENT_FAIL);
		return;
	}
	if(pEntry->m_map1st.size() >= TOURNAMENT_SLOT)
	{
		pServer->SendMW_TOURNAMENT_REQ(pChar->m_dwCharID, pChar->m_dwKEY, MW_TOURNAMENTAPPLY_REQ, TOURNAMENT_FULL);
		return;
	}

	LPTNMTPLAYER pPlayer = new TNMTPLAYER();
	pPlayer->m_bClass = pChar->m_bClass;
	pPlayer->m_bCountry = pChar->m_bCountry;
	pPlayer->m_bLevel = pChar->m_bLevel;
	pPlayer->m_dwCharID = pChar->m_dwCharID;
	pPlayer->m_strName = pChar->m_strNAME;
	pPlayer->m_bSlotID = TOURNAMENT_SLOT;
	pPlayer->m_strHWID = strHWID;
	pPlayer->m_dwIPAddr = dwIPAddr;

	if(pChar->m_pGuild)
		pPlayer->m_strGuildName = pChar->m_pGuild->m_strName;

	GetRanking(pChar->m_dwCharID, pPlayer->m_dwRank, pPlayer->m_dwMonthRank);

	AddTNMTPlayer(pEntry, pPlayer, bStep, pPlayer);
	SendDM_TOURNAMENTAPPLY_REQ(TRUE, pPlayer->m_dwCharID, bEntryID, pPlayer->m_dwCharID, strHWID, dwIPAddr);

	CPacket* pMSG = new CPacket();
	(*pMSG)
		<< pChar->m_dwCharID
		<< pChar->m_dwKEY
		<< WORD(MW_TOURNAMENTAPPLY_REQ)
		<< bResult
		<< bEntryID;

	pServer->SendMW_TOURNAMENT_REQ(pMSG);

	TournamentApplyInfo(pChar, pServer);
}

void CTWorldSvrModule::TournamentApplyInfo(LPTCHARACTER pChar, CTServer * pServer)
{
	if(!m_tournament.m_mapEntry)
		return;

	if(m_tournament.m_bStep > TNMTSTEP_NORMAL)
		return;

	LPMAPTOURNAMENTENTRY pTour = m_tournament.m_mapEntry;

	CPacket* pMSG = new CPacket();
	(*pMSG)
		<< pChar->m_dwCharID
		<< pChar->m_dwKEY
		<< WORD(MW_TOURNAMENTAPPLYINFO_REQ)
		<< BYTE(pTour->size());

	BYTE bEntry = 0;
	LPTNMTPLAYER pI = FindTNMTPlayer(pChar->m_dwCharID);
	if(pI)
		bEntry = pI->m_bEntryID;

	MAPTOURNAMENTENTRY::iterator it;
	for(it=pTour->begin(); it!=pTour->end(); it++)
	{
		LPTOURNAMENTENTRY pEntry = (*it).second;

		(*pMSG)
			<< pEntry->m_bGroup
			<< pEntry->m_bEntryID
			<< pEntry->m_strName
			<< pEntry->m_bType
			<< pEntry->m_dwClass
			<< BYTE(pEntry->m_bEntryID == bEntry ? TRUE : FALSE)
			<< pEntry->m_dwFee
			<< pEntry->m_dwFeeBack
			<< pEntry->m_bPermitCount
			<< pEntry->m_bMinLevel
			<< ((pEntry->m_bMaxLevel==0xFF)?m_bMaxLevel:pEntry->m_bMaxLevel)
			<< BYTE(TOURNAMENT_SLOT-pEntry->m_map1st.size())
			<< WORD(pEntry->m_mapNormal.size());

		WORD wSize = pMSG->GetSize();
		BYTE bCount = 0;
		(*pMSG)
			<< bCount;
		for(BYTE i=0; i<pEntry->m_vReward.size(); i++)
		{
			if(pEntry->m_vReward[i].m_dwClass & (DWORD)pow(2, pChar->m_bClass))
			{
				(*pMSG)
					<< pEntry->m_vReward[i].m_bCheckShield
					<< pEntry->m_vReward[i].m_bChartType
					<< pEntry->m_vReward[i].m_wItemID
					<< pEntry->m_vReward[i].m_bCount;

				bCount++;
			}
		}
		memcpy(pMSG->GetBuffer()+wSize, &bCount, sizeof(bCount));

		(*pMSG)
			<< BYTE(pEntry->m_map1st.size());

		MAPTNMTPLAYER::iterator itTm;
		for(itTm=pEntry->m_map1st.begin(); itTm!=pEntry->m_map1st.end(); itTm++)
		{
			LPTNMTPLAYER pPlayer = (*itTm).second;
			(*pMSG)
				<< pPlayer->m_dwCharID
				<< pPlayer->m_bCountry
				<< pPlayer->m_strName
				<< pPlayer->m_bLevel
				<< pPlayer->m_bClass
				<< pPlayer->m_dwRank
				<< pPlayer->m_dwMonthRank;
		}
	}

	pServer->SendMW_TOURNAMENT_REQ(pMSG);	
}

void CTWorldSvrModule::TournamentJoinList(LPTCHARACTER pChar, CTServer * pServer)
{
	if(!m_tournament.m_mapEntry)
		return;

	if(!CanDoTournament(TNMTSTEP_PARTY))
		return;

	LPMAPTOURNAMENTENTRY pTour = m_tournament.m_mapEntry;

	CPacket* pMSG = new CPacket();
	(*pMSG)
		<< pChar->m_dwCharID
		<< pChar->m_dwKEY
		<< WORD(MW_TOURNAMENTJOINLIST_REQ)
		<< BYTE(pTour->size());

	BYTE bEntry = 0;
	LPTNMTPLAYER pI = FindTNMTPlayer(pChar->m_dwCharID);
	if(pI)
		bEntry = pI->m_bEntryID;

	MAPTOURNAMENTENTRY::iterator it;
	for(it=pTour->begin(); it!=pTour->end(); it++)
	{
		LPTOURNAMENTENTRY pEntry = (*it).second;

		(*pMSG)
			<< pEntry->m_bGroup
			<< pEntry->m_bEntryID
			<< pEntry->m_strName
			<< pEntry->m_bType
			<< pEntry->m_dwClass
			<< BYTE(bEntry == pEntry->m_bEntryID ? TRUE : FALSE);

		WORD wSize = pMSG->GetSize();
		BYTE bCount = 0;
		(*pMSG)
			<< bCount;
		for(BYTE i=0; i<pEntry->m_vReward.size(); i++)
		{
			if(pEntry->m_vReward[i].m_dwClass & (DWORD)pow(2, pChar->m_bClass))
			{
				(*pMSG)
					<< pEntry->m_vReward[i].m_bCheckShield
					<< pEntry->m_vReward[i].m_bChartType
					<< pEntry->m_vReward[i].m_wItemID
					<< pEntry->m_vReward[i].m_bCount;

				bCount++;
			}
		}
		memcpy(pMSG->GetBuffer()+wSize, &bCount, sizeof(bCount));


		(*pMSG)
			<< BYTE(pEntry->m_mapPlayer.size());

		MAPTNMTPLAYER::iterator itTm;
		for(itTm=pEntry->m_mapPlayer.begin(); itTm!=pEntry->m_mapPlayer.end(); itTm++)
		{
			LPTNMTPLAYER pPlayer = (*itTm).second;
			(*pMSG)
				<< pPlayer->m_dwCharID
				<< pPlayer->m_bCountry
				<< pPlayer->m_strName
				<< pPlayer->m_bLevel
				<< pPlayer->m_bClass
				<< pPlayer->m_dwRank
				<< pPlayer->m_dwMonthRank;
		}
	}

	pServer->SendMW_TOURNAMENT_REQ(pMSG);
}

void CTWorldSvrModule::TournamentPartyAdd(LPTCHARACTER pChar,
										  CTServer * pServer,
										  DWORD dwTargetID,
										  BYTE bCountry,
										  CString strTarget,
										  BYTE bLevel,
										  BYTE bClass)
{
	if(!CanDoTournament(TNMTSTEP_PARTY))
		return;

	if(!m_tournament.m_mapEntry)
		return;

	LPMAPTOURNAMENTENTRY pTour = m_tournament.m_mapEntry;

	LPTOURNAMENTENTRY pEntry = NULL;
	MAPTOURNAMENTENTRY::iterator it;
	for(it=pTour->begin(); it!=pTour->end(); it++)
	{
		pEntry = (*it).second;
		if(pEntry->m_bType == ENTRY_PARTY)
			break;
	}

	if(!pEntry)
		return;

	if(!dwTargetID || pChar->m_bCountry != bCountry)
	{
		pServer->SendMW_TOURNAMENT_REQ(pChar->m_dwCharID, pChar->m_dwKEY, MW_TOURNAMENTPARTYADD_REQ, TOURNAMENT_NOTFOUND);
		return;
	}

	if(FindTNMTPlayer(dwTargetID))
	{
		pServer->SendMW_TOURNAMENT_REQ(pChar->m_dwCharID, pChar->m_dwKEY, MW_TOURNAMENTPARTYADD_REQ, TOURNAMENT_ALREADYREG);
		return;
	}

	if(pEntry->m_bMaxLevel < bLevel || pEntry->m_bMinLevel > bLevel)
	{
		CPacket* pMSG = new CPacket();
		(*pMSG)
			<< pChar->m_dwCharID
			<< pChar->m_dwKEY
			<< WORD(MW_TOURNAMENTPARTYADD_REQ)
			<< BYTE(TOURNAMENT_LEVEL)
			<< strTarget;

		pServer->SendMW_TOURNAMENT_REQ(pMSG);	
		return;
	}

	LPTNMTPLAYER pChief = FindTNMTPlayer(pChar->m_dwCharID);
	if(!pChief)
		return;

	if(pChief->m_mapParty.size() >= 6)
	{
		pServer->SendMW_TOURNAMENT_REQ(pChar->m_dwCharID, pChar->m_dwKEY, MW_TOURNAMENTPARTYADD_REQ, TOURNAMENT_FULL);
		return;
	}
	LPTNMTPLAYER pTarget = new TNMTPLAYER();
	pTarget->m_bClass = bClass;
	pTarget->m_bCountry = bCountry;
	pTarget->m_bLevel = bLevel;
	pTarget->m_dwCharID = dwTargetID;
	pTarget->m_strName = strTarget;

	MAPDWORD::iterator itGM = m_mapCharGuild.find(dwTargetID);
	if(itGM != m_mapCharGuild.end())
	{
		CTGuild * pGuild = FindTGuild((*itGM).second);
		if(pGuild)
			pTarget->m_strGuildName = pGuild->m_strName;
	}

	GetRanking(dwTargetID, pTarget->m_dwRank, pTarget->m_dwMonthRank);

	AddTNMTPlayer(pEntry, pTarget, TNMTSTEP_PARTY, pChief);
	SendDM_TOURNAMENTAPPLY_REQ(TRUE, pTarget->m_dwCharID, pEntry->m_bEntryID, pChief->m_dwCharID);

	CPacket* pMSG = new CPacket();
	(*pMSG)
		<< pChar->m_dwCharID
		<< pChar->m_dwKEY
		<< WORD(MW_TOURNAMENTPARTYADD_REQ)
		<< BYTE(TOURNAMENT_SUCCESS)
		<< strTarget
		<< dwTargetID;

	pServer->SendMW_TOURNAMENT_REQ(pMSG);

	TournamentPartyList(pChar, pServer, pChar->m_dwCharID);
}

void CTWorldSvrModule::TournamentPartyDel(LPTCHARACTER pChar, CTServer * pServer, DWORD dwTargetID)
{
	if(!CanDoTournament(TNMTSTEP_PARTY))
		return;

	LPTNMTPLAYER pPlayer = FindTNMTPlayer(dwTargetID);
	if(!pPlayer)
		return;

	if(pPlayer->m_dwChiefID == dwTargetID ||
		(pPlayer->m_dwChiefID != pChar->m_dwCharID &&
		dwTargetID != pChar->m_dwCharID))
		return;

	DWORD dwChief = pPlayer->m_dwChiefID;

	LPTOURNAMENTENTRY pEntry = GetCurrentTournamentEntry(pPlayer->m_bEntryID);

	DelTNMTPlayer(pEntry, pPlayer);
	SendDM_TOURNAMENTAPPLY_REQ(FALSE, pPlayer->m_dwCharID);

	TournamentPartyList(pChar, pServer, dwChief);
}

void CTWorldSvrModule::TournamentPartyList(LPTCHARACTER pChar, CTServer * pServer, DWORD dwChiefID)
{
	LPTNMTPLAYER pChief = FindTNMTPlayer(dwChiefID);
	if(!pChief)
		return;

	CPacket* pMSG = new CPacket();
	(*pMSG)
		<< pChar->m_dwCharID
		<< pChar->m_dwKEY
		<< WORD(MW_TOURNAMENTPARTYLIST_REQ)
		<< dwChiefID
		<< BYTE(pChief->m_mapParty.size());

	MAPTNMTPLAYER::iterator itTm;
	for(itTm=pChief->m_mapParty.begin(); itTm!=pChief->m_mapParty.end(); itTm++)
	{
		LPTNMTPLAYER pPlayer = (*itTm).second;
		(*pMSG)
			<< pPlayer->m_dwCharID
			<< pPlayer->m_bCountry
			<< pPlayer->m_strName
			<< pPlayer->m_bLevel
			<< pPlayer->m_bClass
			<< pPlayer->m_dwRank
			<< pPlayer->m_dwMonthRank;
	}

	pServer->SendMW_TOURNAMENT_REQ(pMSG);
}

void CTWorldSvrModule::TournamentEventJoin(LPTCHARACTER pChar, CTServer * pServer, BYTE bEntryID, DWORD dwTargetID)
{
	LPTNMTPLAYER pTarget = FindTNMTPlayer(dwTargetID);
	if(!pTarget)
		return;

	LPTOURNAMENTENTRY pEntry = GetCurrentTournamentEntry(bEntryID);
	if(!pEntry)
		return;

	if(!CanDoTournament(TNMTSTEP_ENTER, pEntry->m_bGroup))
		return;

	if(pEntry->m_bEntryID != pTarget->m_bEntryID)
		return;

	LPTNMTPLAYER pOld = FindBatter(bEntryID, pChar);
	if(pOld)
	{
		ResetBatting(pOld, pChar);
		pChar->m_mapBatting.erase(pOld->m_dwCharID);
	}

	JoinBatting(pTarget, pChar);

	CPacket* pMSG = new CPacket();
	(*pMSG)
		<< pChar->m_dwCharID
		<< pChar->m_dwKEY
		<< WORD(MW_TOURNAMENTEVENTJOIN_REQ)
		<< BYTE(TOURNAMENT_SUCCESS);

	pServer->SendMW_TOURNAMENT_REQ(pMSG);
}

void CTWorldSvrModule::TournamentEventList(LPTCHARACTER pChar, CTServer * pServer)
{
	if(!m_tournament.m_mapEntry || !m_tournament.m_bBase)
		return;

	LPMAPTOURNAMENTENTRY pTour = m_tournament.m_mapEntry;

	CPacket* pMSG = new CPacket();
	(*pMSG)
		<< pChar->m_dwCharID
		<< pChar->m_dwKEY
		<< WORD(MW_TOURNAMENTEVENTLIST_REQ)
		<< m_tournament.m_bBase
		<< m_tournament.m_dwSum
		<< TournamentGetEntryCount();

	MAPTOURNAMENTENTRY::iterator it;
	for(it=pTour->begin(); it!=pTour->end(); it++)
	{
		LPTOURNAMENTENTRY pEntry = (*it).second;

		if(pEntry->m_bGroup != m_tournament.m_bGroup)
			continue;

		LPTNMTPLAYER pPlayer = FindBatter(pEntry->m_bEntryID, pChar);
		
		FLOAT fRate =0 ;
		DWORD dwAmount = 0;

		GetBattingAmount(pPlayer, pChar->m_dwCharID, fRate, dwAmount);

		(*pMSG)
			<< pEntry->m_bEntryID
			<< pEntry->m_strName
			<< pEntry->m_bType
			<< (pPlayer ? pPlayer->m_strName : NAME_NULL)
			<< BYTE(pPlayer ? pPlayer->m_bCountry : TCONTRY_N)
			<< fRate
			<< dwAmount;
	}

	pServer->SendMW_TOURNAMENT_REQ(pMSG);
}

void CTWorldSvrModule::TournamentEventInfo(LPTCHARACTER pChar, CTServer * pServer, BYTE bEntryID)
{
	LPTOURNAMENTENTRY pEntry = GetCurrentTournamentEntry(bEntryID);
	if(!pEntry || !pChar->m_dwTicket)
		return;

	if(!CanDoTournament(TNMTSTEP_ENTER, pEntry->m_bGroup))
		return;

	CPacket* pMSG = new CPacket();
	(*pMSG)
		<< pChar->m_dwCharID
		<< pChar->m_dwKEY
		<< WORD(MW_TOURNAMENTEVENTINFO_REQ)
		<< bEntryID
		<< m_tournament.m_bBase
		<< m_tournament.m_dwSum
		<< BYTE(pEntry->m_mapPlayer.size());

	MAPTNMTPLAYER::iterator it, itPt;
	for(it=pEntry->m_mapPlayer.begin(); it!=pEntry->m_mapPlayer.end(); it++)
	{
		LPTNMTPLAYER pPlayer = (*it).second;

		(*pMSG)
			<< pPlayer->m_dwCharID
			<< pPlayer->m_bCountry
			<< pPlayer->m_strGuildName
			<< pPlayer->m_strName
			<< pPlayer->m_bLevel
			<< pPlayer->m_bClass
			<< pPlayer->m_dwRank
			<< pPlayer->m_dwMonthRank
			<< FLOAT(pPlayer->m_dwSum ? m_tournament.m_dwSum / pPlayer->m_dwSum : 0)
			<< BYTE(pPlayer->m_mapParty.size());

		for(itPt=pPlayer->m_mapParty.begin(); itPt != pPlayer->m_mapParty.end(); itPt++)
		{
			LPTNMTPLAYER pParty = (*itPt).second;
			(*pMSG)
				<< pParty->m_dwCharID
				<< pParty->m_bCountry
				<< pParty->m_strGuildName
				<< pParty->m_strName
				<< pParty->m_bLevel
				<< pParty->m_bClass
				<< pParty->m_dwRank
				<< pParty->m_dwMonthRank;
		}
	}

	pServer->SendMW_TOURNAMENT_REQ(pMSG);
}

void CTWorldSvrModule::TournamentMatchList(LPTCHARACTER pChar, CTServer * pServer)
{
	if(!m_tournament.m_mapEntry)
		return;

	LPMAPTOURNAMENTENTRY pTour = m_tournament.m_mapEntry;

	CPacket* pMSG = new CPacket();
	(*pMSG)
		<< pChar->m_dwCharID
		<< pChar->m_dwKEY
		<< WORD(MW_TOURNAMENTMATCHLIST_REQ)
		<< BYTE(pTour->size());

	MAPTOURNAMENTENTRY::iterator it;
	for(it=pTour->begin(); it!=pTour->end(); it++)
	{
		LPTOURNAMENTENTRY pEntry = (*it).second;

		BYTE bApply = FALSE;
		LPTNMTPLAYER pPlayer = FindTNMTPlayer(pChar->m_dwCharID);
		if(pPlayer && pPlayer->m_bEntryID == pEntry->m_bEntryID)
			bApply = TRUE;

		(*pMSG)
			<< pEntry->m_bGroup
			<< pEntry->m_bEntryID
			<< pEntry->m_strName
			<< pEntry->m_bType
			<< pEntry->m_dwClass
			<< bApply;

		WORD wSize = pMSG->GetSize();
		BYTE bCount = 0;
		(*pMSG)
			<< bCount;
		for(BYTE i=0; i<pEntry->m_vReward.size(); i++)
		{
			if(pEntry->m_vReward[i].m_dwClass & (DWORD)pow(2, pChar->m_bClass))
			{
				(*pMSG)
					<< pEntry->m_vReward[i].m_bCheckShield
					<< pEntry->m_vReward[i].m_bChartType
					<< pEntry->m_vReward[i].m_wItemID
					<< pEntry->m_vReward[i].m_bCount;

				bCount++;
			}
		}
		memcpy(pMSG->GetBuffer()+wSize, &bCount, sizeof(bCount));

		(*pMSG)
			<< BYTE(pEntry->m_mapPlayer.size());

		MAPTNMTPLAYER::iterator itTm;
		for(itTm=pEntry->m_mapPlayer.begin(); itTm!=pEntry->m_mapPlayer.end(); itTm++)
		{
			LPTNMTPLAYER pPlayer = (*itTm).second;
			(*pMSG)
				<< pPlayer->m_bSlotID
				<< pPlayer->m_dwCharID
				<< pPlayer->m_bCountry
				<< pPlayer->m_strName
				<< pPlayer->m_bLevel
				<< pPlayer->m_bClass
				<< pPlayer->m_dwRank
				<< pPlayer->m_dwMonthRank
				<< pPlayer->m_bResult[MATCH_QFINAL]
				<< pPlayer->m_bResult[MATCH_SFINAL]
				<< pPlayer->m_bResult[MATCH_FINAL];
		}
	}

	pServer->SendMW_TOURNAMENT_REQ(pMSG);
}

void CTWorldSvrModule::TournamentInfo(CTServer * pServer)
{
	if(pServer)
	{
		pServer->SendMW_TOURNAMENTINFO_REQ(m_bFirstGroupCount, m_tournament.m_mapEntry, m_tournament.m_bGroup, m_tournament.m_bStep, m_bMaxLevel);
		if(m_tournament.m_bStep >= TNMTSTEP_MATCH)
			TournamentMatch();
	}
	else
	{
		MAPTSERVER::iterator finder;
		for(finder = m_mapSERVER.begin(); finder != m_mapSERVER.end(); finder++)
			(*finder).second->SendMW_TOURNAMENTINFO_REQ(m_bFirstGroupCount, m_tournament.m_mapEntry, m_tournament.m_bGroup, m_tournament.m_bStep, m_bMaxLevel);
	}
}

void CTWorldSvrModule::TournamentMatch(CTServer * pServer)
{
	if( m_tournament.m_bStep < TNMTSTEP_MATCH)
		return;

	if(!m_tournament.m_mapEntry)
		return;

	LPMAPTOURNAMENTENTRY pTour = m_tournament.m_mapEntry;

	if(pServer)
		pServer->SendMW_TOURNAMENTMATCH_REQ(&m_mapTNMTPlayer);
	else
	{
		MAPTSERVER::iterator finder;
		for(finder = m_mapSERVER.begin(); finder != m_mapSERVER.end(); finder++)
			(*finder).second->SendMW_TOURNAMENTMATCH_REQ(&m_mapTNMTPlayer);
	}
}

void CTWorldSvrModule::TournamentUpdate(WORD wTourID)
{
#include "TournamentUpdate.inc"
}

void CTWorldSvrModule::TournamentSchedule(LPTCHARACTER pChar, CTServer * pServer)
{
	if(m_tournament.m_mapStep.empty())
		return;

	MAPTOURNAMENTSTEP::iterator itStart;

	CPacket* pMSG = new CPacket();
	(*pMSG)
		<< pChar->m_dwCharID
		<< pChar->m_dwKEY
		<< WORD(MW_TOURNAMENTSCHEDULE_REQ)
		<< m_tournament.m_bGroup
		<< m_tournament.m_bStep;

	WORD wSize = pMSG->GetSize();
	BYTE bCount = 0;
	(*pMSG)
		<< bCount;

	for(itStart=m_tournament.m_mapStep.begin(); itStart!=m_tournament.m_mapStep.end(); itStart++)
	{
		if((*itStart).second.m_dwPeriod)
		{
			(*pMSG)
				<< (*itStart).second.m_bGroup
				<< (*itStart).second.m_bStep
				<< (*itStart).second.m_dStart;

			bCount++;
		}
	}
	memcpy(pMSG->GetBuffer()+wSize, &bCount, sizeof(bCount));

	pServer->SendMW_TOURNAMENT_REQ(pMSG);
}

void CTWorldSvrModule::TNMTEnterGate(LPTCHARACTER pChar, DWORD dwMoney, BYTE bEnter)
{
	MAPTNMTPLAYER::iterator it;
	for(it=pChar->m_mapBatting.begin(); it!=pChar->m_mapBatting.end(); it++)
		ResetBatting((*it).second, pChar);

	pChar->m_dwTicket = 0;
	pChar->m_mapBatting.clear();

	DWORD dwTicket = dwMoney / TOURNAMENT_BASEPRIZE;
	if(bEnter && 
		dwTicket &&
		m_tournament.m_wID && m_tournament.m_bStep >= TNMTSTEP_ENTER)
	{
		m_tournament.m_dwSum += dwTicket;
		pChar->m_dwTicket = dwTicket;
	}
}

void CTWorldSvrModule::TNMTEntryDelete(WORD wTID, BYTE bEntryID)
{
	LPMAPTOURNAMENTENTRY pTour = GetTournament(wTID);
	if(pTour)
	{
		MAPTOURNAMENTENTRY::iterator it = pTour->find(bEntryID);
		if(it!=pTour->end())
		{
			if(wTID == m_tournament.m_wID)
			{
				MAPTNMTPLAYER::iterator itTP, itCTP;
				for(itTP=m_mapTNMTPlayer.begin(); itTP!=m_mapTNMTPlayer.end();)
				{
					itCTP = itTP;
					itTP++;
					if((*itCTP).second->m_bEntryID == bEntryID)
					{
						SendDM_TOURNAMENTAPPLY_REQ(FALSE, (*itCTP).second->m_dwCharID);
						DelTNMTPlayer((*it).second, (*itCTP).second);
					}
				}
			}

			delete (*it).second;
			pTour->erase(it);
		}
	}
}

BYTE CTWorldSvrModule::TournamentGetEntryCount()
{
#include "TournamentEntryCount.inc"
}

BYTE CTWorldSvrModule::CheckMapID(WORD wMapType, WORD wMapID)
{
	switch (wMapType)
	{
	case 1:	//	Tournament
			if( 532 >= wMapID && 500 <= wMapID )
				return TRUE;
			break;
	}
	return FALSE;
}

void CTWorldSvrModule::LotteryItem(VLOTTERY& pLot, BYTE bMap, CString& szMsg, CString& szEventName)
{
	int nTok = 0;
	CString strTitle = szMsg.Tokenize(_T("|"), nTok);
	CString strMessage = szMsg.Mid(nTok, szMsg.GetLength() - nTok);

	vector<LOTPACKET> vLot;
	vector<LPTCHARACTER> vLotChar;

	//	선택한 맵이있는가?
	if(bMap)
	{
		////	맵에 맞는 캐릭터 목록 만들기
		MAPTCHARACTER::iterator Mit;
		for(Mit = m_mapTCHAR.begin(); Mit != m_mapTCHAR.end(); Mit++)
		{
			//	맵 ID 체크
			if( CheckMapID(bMap, Mit->second->m_wMapID) )
				vLotChar.push_back(Mit->second);
		}

		////	추첨!
		for(DWORD i = 0; i < pLot.size(); i++)
		{
			LOTPACKET sTempLot;
			sTempLot.m_pLot = &pLot[i];

			//	지정된 추첨자 수만큼 뽑기!
			int nWinner = pLot[i].m_wWinner;
			for(int j = 0; j < nWinner; j++)
			{
				//	추첨 대상이 없으면 끝.
				if(vLotChar.size() <= 0)
				{
					if(sTempLot.m_pChar.size() > 0)
						vLot.push_back(sTempLot);

					if(vLot.size() <= 0)
						return;
					MAPTSERVER::iterator itS;
					for(itS=m_mapSERVER.begin(); itS!=m_mapSERVER.end(); itS++)
						(*itS).second->SendMW_EVENTMSGLOTTERY_REQ(szEventName, vLot);
					return ;
				}

				DWORD dwRand = TRand((DWORD)vLotChar.size());

				vector<LPTCHARACTER>::iterator PostIt = vLotChar.begin();

				PostIt+= dwRand;
				//	당첨 메일 보내기				
				SendPost(WPT_LOTITEM, (*PostIt)->m_dwCharID, (*PostIt)->m_strNAME, strTitle, strMessage, pLot[i].m_wItemID, pLot[i].m_bNum);
				sTempLot.m_pChar.push_back(*PostIt);

				//	중복 당첨을 막기위해 리스트에서 제거.
				vLotChar.erase(PostIt);
			}
			if(sTempLot.m_pChar.size() > 0)
				vLot.push_back(sTempLot);

			//	추첨 대상이 없으면 끝.
			if(vLotChar.size() <= 0)
			{
				if(vLot.size() <= 0)
					return;
				MAPTSERVER::iterator itS;
				for(itS=m_mapSERVER.begin(); itS!=m_mapSERVER.end(); itS++)
					(*itS).second->SendMW_EVENTMSGLOTTERY_REQ(szEventName, vLot);
				return ;
			}
		}
	}
	else
	{
		////	맵 전체에서 추첨
		//	당첨자 ID 리스트, 당첨자수
		DWORD nWinnerCount = 0;

		for(DWORD i = 0; i < pLot.size(); i++)
		{
			LOTPACKET sTempLot;
			sTempLot.m_pLot = &pLot[i];

			//	지정된 추첨자 수만큼 뽑기!
			int nWinner = pLot[i].m_wWinner;
			for(int j = 0; j < nWinner; j++)
			{
				//	추첨 대상이 없으면 끝.
				if(m_mapTCHAR.size() <= nWinnerCount)
				{
					if(sTempLot.m_pChar.size() > 0)
						vLot.push_back(sTempLot);
					
					if(vLot.size() <= 0)
						return;
					MAPTSERVER::iterator itS;
					for(itS=m_mapSERVER.begin(); itS!=m_mapSERVER.end(); itS++)
						(*itS).second->SendMW_EVENTMSGLOTTERY_REQ(szEventName, vLot);
					return ;
				}

				//	추첨!
				MAPTCHARACTER::iterator PostIt = m_mapTCHAR.begin();
				DWORD dwRand =  TRand((DWORD)m_mapTCHAR.size());

				for(DWORD m = 0; m < dwRand; m++)
				{
					PostIt++;
					if(PostIt == m_mapTCHAR.end())
						return ;
				}

				//	중복 당첨 조사
				BYTE bRet = FALSE;
				for(DWORD k = 0; k < nWinnerCount; k++)
				{
					if(vLotChar[k] == PostIt->second)
					{
						bRet = TRUE;
						j--;
						break;
					}
				}

				//	중복되지 않았다면
				if(!bRet)
				{
					//	당첨 메일 보내기
					SendPost(WPT_LOTITEM, PostIt->second->m_dwCharID, PostIt->second->m_strNAME, strTitle, strMessage, pLot[i].m_wItemID, pLot[i].m_bNum);
					
					//	중복 당첨을 막기위해 당첨자 리스트에 추가
					vLotChar.push_back(PostIt->second);
					sTempLot.m_pChar.push_back(PostIt->second);
					nWinnerCount++;
				}
			}
			if(sTempLot.m_pChar.size() > 0)
				vLot.push_back(sTempLot);

			//	추첨 대상이 없으면 끝.
			if(m_mapTCHAR.size() <= nWinnerCount)
			{
				if(vLot.size() <= 0)
					return;
				MAPTSERVER::iterator itS;
				for(itS=m_mapSERVER.begin(); itS!=m_mapSERVER.end(); itS++)
					(*itS).second->SendMW_EVENTMSGLOTTERY_REQ(szEventName, vLot);
				return ;
			}
		}
	}
	if(vLot.size() <= 0)
		return;
	MAPTSERVER::iterator itS;
	for(itS=m_mapSERVER.begin(); itS!=m_mapSERVER.end(); itS++)
		(*itS).second->SendMW_EVENTMSGLOTTERY_REQ(szEventName, vLot);
}

void CTWorldSvrModule::GiftTime(LPEVENTINFO pEvent)//LPLOTTERY pLot, CString& szMsg)
{
	int nTok = 0;
	CString strTitle = pEvent->m_strLotMsg.Tokenize(_T("|"), nTok);
	CString strMessage = pEvent->m_strLotMsg.Mid(nTok, pEvent->m_strLotMsg.GetLength() - nTok);
	LPLOTTERY pLot = &pEvent->m_vLOTTERY.at(0);
	BYTE bMinLevel = HIBYTE(pEvent->m_wValue);
	BYTE bMaxLevel = LOBYTE(pEvent->m_wValue);

	//	메일 보내기
	for (MAPTCHARACTER::iterator it = m_mapTCHAR.begin(); it != m_mapTCHAR.end(); it++)
	{
		LPTCHARACTER pPlayer = it->second;
		if(bMinLevel <= pPlayer->m_bLevel && bMaxLevel >= pPlayer->m_bLevel)	
				SendPost(WPT_LOTITEM, pPlayer->m_dwCharID, pPlayer->m_strNAME, strTitle, strMessage, pLot->m_wItemID, pLot->m_bNum, pLot->m_wWinner);
	}
	
}

void CTWorldSvrModule::NotifyCastleWarInfo(CTServer * pServer)
{
	MAPCASTLEWARINFO::iterator it;

	if(pServer)
	{
		for(it=m_mapCastleWarInfo.begin(); it!=m_mapCastleWarInfo.end(); it++)
		{
			CTGuild * pDef = FindTGuild(it->second.m_dwDefGuild);
			CTGuild * pAtk = FindTGuild(it->second.m_dwAtkGuild);

			pServer->SendMW_CASTLEWARINFO_REQ(
					&(it->second),
					pDef ? pDef->m_dwID : 0,
					pDef ? pDef->m_strName : NAME_NULL,
					pDef ? pDef->m_bCountry : TCONTRY_N,
					pAtk ? pAtk->m_dwID : 0,
					pAtk ? pAtk->m_strName : NAME_NULL);
		}

		return;
	}

	for(it=m_mapCastleWarInfo.begin(); it!=m_mapCastleWarInfo.end(); it++)
	{
		it->second.m_mapEnableGuild.clear();
		it->second.m_mapEnableGuild = it->second.m_mapGuild;
		it->second.m_mapTop3[TCONTRY_D].clear();
		it->second.m_mapTop3[TCONTRY_C].clear();
		it->second.m_dwAtkGuild=0;
		it->second.m_dwDefGuild=0;
		memset(it->second.m_wCountryPoint, 0, sizeof(it->second.m_wCountryPoint));
	}

	for(it=m_mapCastleWarInfo.begin(); it!=m_mapCastleWarInfo.end(); it++)
	{
		MAPDWORD::iterator itGd;
		LPTCASTLEWARINFO pCs = &((*it).second);

		for(itGd=pCs->m_mapGuild.begin(); itGd!=pCs->m_mapGuild.end(); itGd++)
		{
			CTGuild * pGuild = FindTGuild((*itGd).first);
			if(!pGuild)
				continue;

			pCs->m_wCountryPoint[pGuild->m_bCountry] += WORD((*itGd).second);

			MAPCASTLETOP3::iterator it3;
			BYTE bId = BYTE(pCs->m_mapTop3[pGuild->m_bCountry].size() + 1);
			for(it3=pCs->m_mapTop3[pGuild->m_bCountry].begin(); it3!=pCs->m_mapTop3[pGuild->m_bCountry].end(); it3++)
			{
				CTGuild * pTop = FindTGuild(it3->second.m_dwID);
				if(!pTop || pTop->m_dwID == pGuild->m_dwID)
				{
					bId = 4;
					break;
				}

				if(it3->second.m_wPoint < itGd->second)
					bId--;
				else if(it3->second.m_wPoint == itGd->second)
				{
					DWORD dwSelGuild = CompareOccupation(pCs, pGuild->m_dwID, pCs, pTop->m_dwID);
					if(dwSelGuild == pGuild->m_dwID)
						bId--;
					else if(!dwSelGuild && CompareGuildRank(pGuild, pTop))
						bId--;
				}
			}

			if(bId < 4)
			{
				TCASTLETOP3 top3, newtop;
				newtop.m_dwID = pGuild->m_dwID;
				newtop.m_strName = pGuild->m_strName;
				newtop.m_wPoint = WORD(itGd->second);

				pCs->m_mapTop3[pGuild->m_bCountry].erase(3);
				if(bId < 3)
				{
					it3 = pCs->m_mapTop3[pGuild->m_bCountry].find(2);
					if(it3 != pCs->m_mapTop3[pGuild->m_bCountry].end())
					{
						top3 = it3->second;
						pCs->m_mapTop3[pGuild->m_bCountry].insert(MAPCASTLETOP3::value_type(3, top3));
					}
				}

				if(bId == 1)
				{
					it3 = pCs->m_mapTop3[pGuild->m_bCountry].find(1);
					if(it3 != pCs->m_mapTop3[pGuild->m_bCountry].end())
					{
						top3 = it3->second;
						pCs->m_mapTop3[pGuild->m_bCountry].erase(2);
						pCs->m_mapTop3[pGuild->m_bCountry].insert(MAPCASTLETOP3::value_type(2, top3));
					}
				}

				pCs->m_mapTop3[pGuild->m_bCountry].erase(bId);
				pCs->m_mapTop3[pGuild->m_bCountry].insert(MAPCASTLETOP3::value_type(bId, newtop));
			}
		}

		SelectCastleWarGuild(TRUE, pCs);
	}

	for(it=m_mapCastleWarInfo.begin(); it!=m_mapCastleWarInfo.end(); it++)
	{
		MAPCASTLEWARINFO::iterator itGd;
		for(itGd=m_mapCastleWarInfo.begin(); itGd!=m_mapCastleWarInfo.end(); itGd++)
			itGd->second.m_mapEnableGuild.erase(it->second.m_dwDefGuild);
	}

	for(it=m_mapCastleWarInfo.begin(); it!=m_mapCastleWarInfo.end(); it++)
		SelectCastleWarGuild(FALSE, &(it->second));

	for(it=m_mapCastleWarInfo.begin(); it!=m_mapCastleWarInfo.end(); it++)
	{
		CTGuild * pDef = FindTGuild(it->second.m_dwDefGuild);
		CTGuild * pAtk = FindTGuild(it->second.m_dwAtkGuild);

		MAPTSERVER::iterator itSv;
		for(itSv=m_mapSERVER.begin(); itSv!=m_mapSERVER.end(); itSv++)
			(*itSv).second->SendMW_CASTLEWARINFO_REQ(
				&(it->second),
				pDef ? pDef->m_dwID : 0,
				pDef ? pDef->m_strName : NAME_NULL,
				pDef ? pDef->m_bCountry : TCONTRY_N,
				pAtk ? pAtk->m_dwID : 0,
				pAtk ? pAtk->m_strName : NAME_NULL);
	}
}

CTGuild * CTWorldSvrModule::SelectCastleWarGuild(BYTE bIsDef, LPTCASTLEWARINFO pCastle)
{
	if(bIsDef)
	{
		pCastle->m_dwDefGuild = 0;
		pCastle->m_bDefCountry = TCONTRY_N;
	}
	else
		pCastle->m_dwAtkGuild = 0;

	MAPDWORD::iterator itGd;
	MAPDWORD mapTop;
	mapTop.clear();

	MAPDWORD::iterator itTop;
	CTGuild * pTop = NULL;

	for(itGd=pCastle->m_mapEnableGuild.begin(); itGd!=pCastle->m_mapEnableGuild.end(); itGd++)
	{
		CTGuild * pGuild = FindTGuild((*itGd).first);
		if(!pGuild)
			continue;

		if(!bIsDef && pGuild->m_bCountry == pCastle->m_bDefCountry)
			continue;

		if(!pTop || WORD(itTop->second) < WORD(itGd->second))
		{
			itTop = itGd;
			pTop = pGuild;
		}
		else if(WORD(itTop->second) == WORD(itGd->second))
		{
			if(pTop->m_bCountry != pGuild->m_bCountry &&
				pCastle->m_wCountryPoint[TCONTRY_D] > pCastle->m_wCountryPoint[TCONTRY_C] &&
				pGuild->m_bCountry == TCONTRY_D)
			{
				itTop = itGd;
				pTop = pGuild;
			}
			else if(pTop->m_bCountry != pGuild->m_bCountry &&
				pCastle->m_wCountryPoint[TCONTRY_C] > pCastle->m_wCountryPoint[TCONTRY_D] &&
				pGuild->m_bCountry == TCONTRY_C)
			{
				itTop = itGd;
				pTop = pGuild;
			}
			else
			{
				DWORD dwSelGuild = CompareOccupation(pCastle, pGuild->m_dwID, pCastle, pTop->m_dwID);
				if(dwSelGuild == pGuild->m_dwID)
				{
					itTop = itGd;
					pTop = pGuild;
				}
				else if(!dwSelGuild && CompareGuildRank(pGuild, pTop))
				{
					itTop = itGd;
					pTop = pGuild;
				}
			}
		}
	}

	MAPCASTLEWARINFO::iterator it;
	for(it=m_mapCastleWarInfo.begin(); it!=m_mapCastleWarInfo.end(); it++)
	{
		if(!pTop)
			break;

		itTop = pCastle->m_mapEnableGuild.find(pTop->m_dwID);
		if(itTop == pCastle->m_mapEnableGuild.end())
		{
			pTop = NULL;
			break;
		}

		DWORD dwCompare;
		if(bIsDef)
			dwCompare = it->second.m_dwDefGuild;
		else
			dwCompare = it->second.m_dwAtkGuild;

		if(dwCompare == pTop->m_dwID)
		{
			MAPDWORD::iterator itS = it->second.m_mapGuild.find(dwCompare);
			if(itS != it->second.m_mapGuild.end())
			{
				if(itS->second < itTop->second)
				{
					it->second.m_mapEnableGuild.erase(dwCompare);
					SelectCastleWarGuild(bIsDef, &(it->second));
				}
				else if(itS->second > itTop->second)
				{
					pCastle->m_mapEnableGuild.erase(dwCompare);
					pTop = SelectCastleWarGuild(bIsDef, pCastle);
				}
				else
				{
					DWORD dwSelGuild = CompareOccupation(pCastle, dwCompare, &(it->second), itS->first);
					if(dwSelGuild == dwCompare)
					{
						it->second.m_mapEnableGuild.erase(dwCompare);
						SelectCastleWarGuild(bIsDef, &(it->second));
					}
					else if(dwSelGuild == itS->first)
					{
						pCastle->m_mapEnableGuild.erase(dwCompare);
						pTop = SelectCastleWarGuild(bIsDef, pCastle);
					}
				}
			}
		}
	}

	if(pTop)
	{
		if(bIsDef)
		{
			pCastle->m_dwDefGuild = pTop->m_dwID;
			pCastle->m_bDefCountry = pTop->m_bCountry;
		}
		else
			pCastle->m_dwAtkGuild = pTop->m_dwID;

		pCastle->m_mapEnableGuild.erase(pTop->m_dwID);
	}

	return pTop;
}

BYTE CTWorldSvrModule::CompareGuildRank(CTGuild * pG1, CTGuild * pG2)
{
	if(pG1->m_dwPvPTotalPoint > pG2->m_dwPvPTotalPoint)
		return TRUE;
	else if(pG1->m_dwPvPTotalPoint == pG2->m_dwPvPTotalPoint)
	{
		if(pG1->GetMemberSize() > pG2->GetMemberSize())
			return TRUE;
		else if(pG1->GetMemberSize() == pG2->GetMemberSize())
		{
			if(pG1->m_timeEstablish < pG2->m_timeEstablish)
				return TRUE;
		}
	}

	return FALSE;
}

DWORD CTWorldSvrModule::CompareOccupation(LPTCASTLEWARINFO pCastle1, DWORD dwG1, LPTCASTLEWARINFO pCastle2, DWORD dwG2)
{
	MAPARRAY::iterator itON = pCastle1->m_mapOccupy.find(dwG1);
	MAPARRAY::iterator itOO = pCastle2->m_mapOccupy.find(dwG2);
	if(itON == pCastle1->m_mapOccupy.end() && itOO == pCastle2->m_mapOccupy.end())
		return 0;
	if(itON != pCastle1->m_mapOccupy.end() && itOO == pCastle2->m_mapOccupy.end())
		return dwG1;
	if(itON == pCastle1->m_mapOccupy.end() && itOO != pCastle2->m_mapOccupy.end())
		return dwG2;

	for(BYTE day=m_battletime[BT_CASTLE].m_bDay; day>1; day--)
	{
		if(itON->second[day-2] > itOO->second[day-2])
			return dwG1;
		else if(itON->second[day-2] < itOO->second[day-2])
			return dwG2;
		else if(itON->second[day-2])
		{
			if(pCastle1->m_wID < pCastle2->m_wID)
				return dwG2;
			else if(pCastle1->m_wID > pCastle2->m_wID)
				return dwG1;
			else
				return 0;
		}
	}

	for(BYTE day=6;day>=m_battletime[BT_CASTLE].m_bDay; day--)
	{
		if(itON->second[day-1] > itOO->second[day-1])
			return dwG1;
		else if(itON->second[day-1] < itOO->second[day-1])
			return dwG2;
		else if(itON->second[day-1])
		{
			if(pCastle1->m_wID < pCastle2->m_wID)
				return dwG2;
			else if(pCastle1->m_wID > pCastle2->m_wID)
				return dwG1;
			else
				return 0;
		}
	}

	return 0;
}

void CTWorldSvrModule::ChangeCountry(LPTCHARACTER pTCHAR, BYTE bType, BYTE bCountry)
{
	if(pTCHAR->m_pParty)
		LeaveParty(pTCHAR, FALSE);

	DelGuildTacticsWantedApp(pTCHAR->m_dwCharID);

	if(pTCHAR->m_pTactics)
	{
		LPTTACTICSMEMBER pMember = pTCHAR->m_pTactics->FindTactics(pTCHAR->m_dwCharID);
		if(pMember)
			GuildTacticsDel(pTCHAR->m_pTactics, pMember, 2);
	}

	BYTE bGap = GetWarCountryGap(pTCHAR->m_bLevel);
	if(bGap < WARCOUNTRY_MAXGAP)
	{
		BYTE bCC = GetWarCountry(pTCHAR);
		if(bCC < TCONTRY_B)
			m_mapWarCountry[bCC][bGap].erase(pTCHAR->m_dwCharID);
		
		if(bCountry < TCONTRY_B)
			m_mapWarCountry[bCountry][bGap].insert(MAPDWORD::value_type(pTCHAR->m_dwCharID, 0));
	}

	if(pTCHAR->m_pGuild)
	{
		LPTGUILDMEMBER pGM = pTCHAR->m_pGuild->FindMember(pTCHAR->m_dwCharID);
		if(pGM)
			pGM->m_bWarCountry = bCountry;
	}

	if(bType == IK_COUNTRY)
	{
		pTCHAR->m_bCountry = bCountry;
		DelGuildWantedApp(pTCHAR->m_dwCharID);

		if(pTCHAR->m_pGuild)
		{
			LPTGUILDMEMBER pMember = pTCHAR->m_pGuild->FindMember(pTCHAR->m_dwCharID);
			if(pMember)
			{
				DWORD dwGuildID = pTCHAR->m_pGuild->m_dwID;

				GuildMemberDel(pTCHAR->m_pGuild, pMember, GUILD_LEAVE_SELF);
				pTCHAR->m_pGuild = NULL;

				CTServer * pServer = FindMapSvr(pTCHAR->m_bMainID);
				if(pServer)
					pServer->SendMW_GUILDLEAVE_REQ(pTCHAR->m_dwCharID, pTCHAR->m_dwKEY, pTCHAR->m_strNAME, GUILD_LEAVE_SELF, (DWORD)m_timeCurrent);
			}
		}

		MAPTFRIEND::iterator itFr;
		for(itFr=pTCHAR->m_mapTFRIEND.begin(); itFr!=pTCHAR->m_mapTFRIEND.end(); itFr++)
		{
			SendDM_FRIENDERASE_REQ((*itFr).second->m_dwID, pTCHAR->m_dwCharID);
			SendDM_FRIENDERASE_REQ(pTCHAR->m_dwCharID, (*itFr).second->m_dwID);
		}

		MAPTSOULMATE::iterator itSM;
		for(itSM=pTCHAR->m_mapTSOULMATE.begin(); itSM!=pTCHAR->m_mapTSOULMATE.end(); itSM++)
			SendDM_SOULMATEDEL_REQ((*itSM).second->m_dwCharID, (*itSM).second->m_dwTarget);
	}
	else
		pTCHAR->m_bAidCountry = bCountry;

	if(m_pRelay)
		m_pRelay->SendRW_CHANGENAME_ACK(pTCHAR->m_dwCharID, bType, bCountry, NAME_NULL);

	MAPTCHARCON::iterator itCON;
	for(itCON=pTCHAR->m_mapTCHARCON.begin(); itCON!=pTCHAR->m_mapTCHARCON.end(); itCON++)
	{
		if((*itCON).second->m_bValid)
		{
			CTServer *pMAP = FindMapSvr((*itCON).first);
			if(pMAP)
				pMAP->SendMW_CHANGECHARBASE_REQ(
					pTCHAR->m_dwCharID,
					pTCHAR->m_dwKEY,
					bType,
					bCountry,
					pTCHAR->m_wTitleID,
					NAME_NULL);
		}
	}

	RecalcCountryBalance();
}

BYTE CTWorldSvrModule::GetWarCountry(LPTCHARACTER pTCHAR)
{
#include "GetWarCountry1.inc"
}

BYTE CTWorldSvrModule::GetWarCountryGap(BYTE bLevel)
{
#include "GetWarCountryGap.inc"
}

BYTE CTWorldSvrModule::IsMeetingRoom(WORD wMapID, BYTE bIsSmall)
{
	BYTE bIn = wMapID >= MEETING_MAPID && wMapID <= MEETING_MAPID + MEETING_SROOM_COUNT;

	if(bIsSmall)
		bIn = bIn && wMapID != MEETING_MAPID;

	return bIn;
}

void CTWorldSvrModule::CalcGuildRanking()
{
#include "CalcGuildRanking.inc"
}

//	이벤트 날짜 만드는 함수
__int64 CTWorldSvrModule::GetNextEventTime(BYTE bWeek, BYTE bHour, BYTE bMin)
{
#include "NextEventTime.inc"
}

void CTWorldSvrModule::CheckEventQuarter()
{
	MAPTEVENTQUARTER::iterator it = m_mapEVQT.end();
	MAPTIMEQUARTER::iterator itTime = m_mapTimeEVQT.begin();
	if(itTime != m_mapTimeEVQT.end())
		it = m_mapEVQT.find(m_mapTimeEVQT.begin()->second);

	if(it != m_mapEVQT.end())
	{
		LPTEVENTQUARTER pQT = it->second;

		CTime CurTime(m_timeCurrent);
		CTime EvnetTime = CTime(itTime->first);
		CTime NoticeTime = EvnetTime - CTimeSpan(0,0,5,0);	//	시작 5분전

		
	INT nCH = CurTime.GetHour();
	INT nCM = CurTime.GetMinute();
	INT nCS = CurTime.GetSecond();
	INT nCD = CurTime.GetDayOfWeek();
	INT bCurDay = CurTime.GetDay();
	INT bCurMonth = CurTime.GetMonth();
	
	INT nCH2 = NoticeTime.GetHour();
	INT nCM2 = NoticeTime.GetMinute();
	INT nCS2 = NoticeTime.GetSecond();
	INT nCD2 = NoticeTime.GetDayOfWeek();
	INT bCurDay2 = NoticeTime.GetDay();
	INT bCurMonth2 = NoticeTime.GetMonth();

		//	보상은 없고 공지만 있는경우
		if(pQT->m_strPresent.IsEmpty())
			NoticeTime = EvnetTime;

		if(!pQT->m_bNotice && CurTime.GetTime() >= NoticeTime.GetTime()
			&& !pQT->m_strAnnounce.IsEmpty())
		{
			//	공지 띄우기.
			LPPACKETBUF pMSG = new PACKETBUF();
			pMSG->m_packet.SetID(SM_EVENTQUARTERNOTIFY_REQ)
				<< pQT->m_strAnnounce;
			SayToBATCH(pMSG);

			pQT->m_bNotice = TRUE;
		}

		if(CurTime.GetTime() >= EvnetTime.GetTime())
		{			
			if(!pQT->m_strPresent.IsEmpty())
			{
				//	당첨자 공지, 선물발송
				LPPACKETBUF pMSG = new PACKETBUF();
				pMSG->m_packet.SetID(SM_EVENTQUARTER_REQ)
					<< pQT->m_bDay
					<< pQT->m_bHour
					<< pQT->m_bMinute
					<< pQT->m_strPresent;
				SayToBATCH(pMSG);
			}

			pQT->m_bNotice = FALSE;
			pQT->m_nTime = GetNextEventTime(pQT->m_bDay, pQT->m_bHour, pQT->m_bMinute);
			m_mapTimeEVQT.erase(m_mapTimeEVQT.begin());
			m_mapTimeEVQT.insert(MAPTIMEQUARTER::value_type(pQT->m_nTime, pQT->m_wID));
		}
	}
}

void CTWorldSvrModule::BOWNotify(BYTE bStatus, DWORD dwSecond, BYTE bDefugelPoints, BYTE bCraxionPoints)
{
	MAPTSERVER::iterator itSvr;
	for(itSvr = m_mapSERVER.begin(); itSvr != m_mapSERVER.end(); ++itSvr)
		(*itSvr).second->SendMW_BOWTIMEUPDATE_ACK(bStatus, dwSecond, bDefugelPoints, bCraxionPoints);
}

void CTWorldSvrModule::TeleportBOWPlayer(const MAPBOWPLAYER& mapBOWPlayers, BOOL bTeleportIn, BYTE bWinner)
{
	if (!m_pBOWModule)
		return;

	if (bTeleportIn)
	{
		CTServer* pBOWServer = m_pBOWModule->m_pBOWServer;
		if (pBOWServer)
			pBOWServer->SendMW_ADDBOWPLAYERS_ACK(mapBOWPlayers);

		for (MAPBOWPLAYER::const_iterator itPlayer = mapBOWPlayers.begin(); itPlayer != mapBOWPlayers.end(); ++itPlayer)
		{
			LPTCHARACTER pCHAR = FindTChar((*itPlayer).second->m_dwCharID, (*itPlayer).second->m_dwKEY);
			if (pCHAR)
			{
				if (pCHAR->m_bCountry != (*itPlayer).second->m_bCountry)
					pCHAR->m_bCountry = (*itPlayer).second->m_bCountry;

				CTServer* pServer = FindMapSvr(pCHAR->m_bMainID);
				if (pServer)
					pServer->SendMW_PREPAREFORBOW_REQ((*itPlayer).second->m_dwCharID, (*itPlayer).second->m_dwKEY, (*itPlayer).second->m_bCountry);

				DEFINE_QUERY(&m_db, CSPAddBOWPlayer);
				query->m_dwCharID = pCHAR->m_dwCharID;
				query->m_dwUserID = pCHAR->m_dwUserID;
				query->Call();
				UNDEFINE_QUERY()
			}
		}
	}
	else
	{
		CTServer* pServer = m_pBOWModule->m_pBOWServer;
		if (pServer)
			pServer->SendMW_ENDBOWWAR_REQ(mapBOWPlayers, bWinner);

		DEFINE_QUERY(&m_db, CSPClearBOWPlayers);
		query->Call();
		UNDEFINE_QUERY()
	}
}

void CTWorldSvrModule::TeleportBOWPlayer(DWORD dwCharID, DWORD dwKEY, BYTE bWinner)
{
	if (!m_pBOWModule)
		return;

	LPTCHARACTER pCHAR = FindTChar(dwCharID, dwKEY);
	if (!pCHAR)
		return;

	MAPBOWPLAYER::iterator itPlayer = m_pBOWModule->m_mapBOWPLAYERS.find(dwCharID);
	if (itPlayer == m_pBOWModule->m_mapBOWPLAYERS.end())
		return;

	CTServer* pServer = m_pBOWModule->m_pBOWServer;
	if (pServer)
		pServer->SendMW_RELEASESINGLEBOWPLAYER_REQ(dwCharID, dwKEY, bWinner);

	delete (*itPlayer).second;
	m_pBOWModule->m_mapBOWPLAYERS.erase(itPlayer);
	DEFINE_QUERY(&m_db, CSPDeleteSingleBOWPlayer);
	query->m_dwUserID = pCHAR->m_dwUserID;
	query->Call();
	UNDEFINE_QUERY()
}

void CTWorldSvrModule::NotifyBOWNonQueuedPlayers()
{
	if (!m_pBOWModule)
		return;

MAPBOWPLAYER::iterator finder;
	MAPDWORD mapPlayers;
	mapPlayers.clear();

	for (MAPTCHARACTER::iterator itChar = m_mapTCHAR.begin(); itChar != m_mapTCHAR.end(); ++itChar)
	{
		finder = m_pBOWModule->m_mapBOWPLAYERS.find((*itChar).second->m_dwCharID);
		if (finder == m_pBOWModule->m_mapBOWPLAYERS.end())
			mapPlayers.insert(MAPDWORD::value_type((*itChar).second->m_dwCharID, (*itChar).second->m_dwKEY));
	}

	for (MAPDWORD::iterator itPlayer = mapPlayers.begin(); itPlayer != mapPlayers.end(); ++itPlayer)
	{
		LPTCHARACTER pCHAR = FindTChar((*itPlayer).first, (*itPlayer).second);
		if (pCHAR)
		{
			CTServer* pServer = FindMapSvr(pCHAR->m_bMainID);
			if (pServer)
				pServer->SendMW_NOTIFYNONQUEUEDPLAYER_ACK(pCHAR->m_dwCharID, pCHAR->m_dwKEY);
		}
	}
}

void CTWorldSvrModule::CheckHourlyReward()
{
	for (MAPTCHARACTER::iterator itChar = m_mapTCHAR.begin(); itChar != m_mapTCHAR.end(); ++itChar)
	{
		if (GetTickCount() - (*itChar).second->m_dwRewardTick >= 60 * 60 * 1000)
		{
			BYTE& RewardIndex = (*itChar).second->m_bRewardIndex; //Reference so we don't have to set it later
			RewardIndex++;
			if (RewardIndex >= 24)
				RewardIndex = 0;

			(*itChar).second->m_dwRewardTick = GetTickCount();

			DWORD dwCash = HOURLYREWARD_CASH; //0;
			DWORD dwBonus = 0;

		//	if (RewardIndex % 2 == 1)
		//		dwBonus = HOURLYREWARD_BONUS;
		//	else
		//		dwCash = HOURLYREWARD_CASH;

			DEFINE_QUERY(&m_db, CSPCash)
			{
				query->m_dwUserID = (*itChar).second->m_dwUserID;
				query->m_bType = 1;
				query->m_dwCash = dwCash;
				query->m_dwBonus = dwBonus;

				query->Call();
			}
			UNDEFINE_QUERY()
		}
	}
}


void CTWorldSvrModule::BRNotify(BYTE bStatus, DWORD dwSecond, BYTE bType)
{
	MAPTSERVER::iterator itSvr;
	for(itSvr = m_mapSERVER.begin(); itSvr != m_mapSERVER.end(); ++itSvr)
		(*itSvr).second->SendMW_BRTIMEUPDATE_ACK(bStatus, dwSecond, bType);
}

void CTWorldSvrModule::UpdateBRTeam(DWORD dwChiefID)
{
	if (!m_pBRModule)
		return;

	MAPBRTEAMS::iterator finder = m_pBRModule->m_mapPremadeTeam.find(dwChiefID);
	if (finder != m_pBRModule->m_mapPremadeTeam.end())
	{
		for (MAPBRPLAYERS::iterator itPlayer = (*finder).second->m_mapBRPlayer.begin(); itPlayer != (*finder).second->m_mapBRPlayer.end(); ++itPlayer)
		{
			LPTCHARACTER pCHAR = m_mapTCHAR.find((*itPlayer).second->m_dwCharID)->second;
			if (!pCHAR)
				return;

			/*
			LPTCHARACTER pCHAR = FindTChar((*itPlayer).second->m_dwCharID, (*itPlayer).second->m_dwKEY);
			if (!pCHAR)
			{
				m_pBRModule->ErasePlayerFromPremade((*itPlayer).second->m_dwCharID);
				return;
			}
			*/

			MAPBRPLAYERS::iterator itChief = (*finder).second->m_mapBRPlayer.find(dwChiefID);
			if (itChief == (*finder).second->m_mapBRPlayer.end())
				return;

			CTServer* pServer = FindMapSvr(pCHAR->m_bMainID);
			if (pServer)
				pServer->SendMW_UPDATEBRTEAM_ACK(pCHAR->m_dwCharID, pCHAR->m_dwKEY, (*itChief).second->m_strNAME, (*finder).second->m_mapBRPlayer, (*finder).second->m_bReady);
		}
	}
}

void CTWorldSvrModule::RemoveFromBRTeam(DWORD dwCharID)
{
	MAPTCHARACTER::iterator finder = m_mapTCHAR.find(dwCharID);
	if (finder != m_mapTCHAR.end())
	{
		CTServer* pServer = FindMapSvr((*finder).second->m_bMainID);
		if (pServer)
		{
			MAPBRPLAYERS emptyTeam; emptyTeam.clear();
			pServer->SendMW_UPDATEBRTEAM_ACK((*finder).second->m_dwCharID, (*finder).second->m_dwKEY, NAME_NULL, emptyTeam, FALSE);
		}
	}
}

void CTWorldSvrModule::TeleportBRTeam(const MAPBRTEAMS& mapTeams, BOOL TeleportIn)
{
	if (!m_pBRModule)
		return;

	if (TeleportIn)
	{
		CTServer* pBRServer = m_pBRModule->m_pBRServer;
		if (pBRServer)
			pBRServer->SendMW_ADDBRTEAMS_ACK(m_pBRModule->GetMapID(), mapTeams, m_pBRModule->m_Mode);

		for (MAPBRTEAMS::const_iterator itTeam = mapTeams.begin(); itTeam != mapTeams.end(); ++itTeam)
		{
			for (MAPBRPLAYERS::iterator itPlayer = (*itTeam).second->m_mapBRPlayer.begin(); itPlayer != (*itTeam).second->m_mapBRPlayer.end(); ++itPlayer)
			{
				LPTCHARACTER pCHAR = FindTChar((*itPlayer).second->m_dwCharID, (*itPlayer).second->m_dwKEY);
				if (pCHAR)
				{
					CTServer* pServer = FindMapSvr(pCHAR->m_bMainID);
					if (pServer)
						pServer->SendMW_PREPAREFORBR_REQ((*itPlayer).second->m_dwCharID, (*itPlayer).second->m_dwKEY, m_pBRModule->GetMapID());



					DEFINE_QUERY(&m_db, CSPAddBRPlayer);
					query->m_dwCharID = pCHAR->m_dwCharID;
					query->m_dwUserID = pCHAR->m_dwUserID;
					query->Call();
					UNDEFINE_QUERY()

				}
			}
		}
	}
	else
	{
		CTServer* pServer = m_pBRModule->m_pBRServer;
		if (pServer)
		{
			pServer->SendMW_ENDBRWAR_REQ(mapTeams);
		}

		DEFINE_QUERY(&m_db, CSPClearBRPlayers);
		query->Call();
		UNDEFINE_QUERY()
	}
}

BOOL CTWorldSvrModule::IsSomeoneAliveInTeam(LPTBRTEAMS pTEAM)
{
	for (MAPBRPLAYERS::iterator itPlayer = pTEAM->m_mapBRPlayer.begin(); itPlayer != pTEAM->m_mapBRPlayer.end(); ++itPlayer)
	{
		LPTCHARACTER pTCHAR = FindTChar((*itPlayer).second->m_dwCharID, (*itPlayer).second->m_dwKEY);
		if (pTCHAR && pTCHAR->m_bChannel == BR_SERVER_ID)
			return TRUE;
	}

	return FALSE;
}

void CTWorldSvrModule::TeleportBRPlayer(DWORD dwCharID, DWORD dwKEY)
{
	if (!m_pBRModule)
		return;

	LPTCHARACTER pCHAR = FindTChar(dwCharID, dwKEY);
	if (!pCHAR)
		return;

	if (!m_pBRModule->FindPlayerInTeam(dwCharID))
		return;

	CTServer* pServer = m_pBRModule->m_pBRServer;
	if (pServer)
		pServer->SendMW_RELEASESINGLEBRPLAYER_REQ(dwCharID, dwKEY);

	DEFINE_QUERY(&m_db, CSPDeleteSingleBRPlayer);
	query->m_dwUserID = pCHAR->m_dwUserID;
	query->Call();
	UNDEFINE_QUERY()
}

void CTWorldSvrModule::RecalcCountryBalance()
{
	BYTE bCount[TCONTRY_B] = { 0 };
	for (MAPTCHARACTER::iterator it = m_mapTCHAR.begin(); it != m_mapTCHAR.end(); ++it) {
		if (GetWarCountry((*it).second) < TCONTRY_B && ((*it).second->m_bMainID == 4 || (*it).second->m_bMainID == 5 || (*it).second->m_bMainID == 6 || (*it).second->m_bMainID == 7 || (*it).second->m_bMainID == 10))
			bCount[GetWarCountry((*it).second)]++;
	}

	for (MAPTSERVER::iterator itSvr = m_mapSERVER.begin(); itSvr != m_mapSERVER.end(); ++itSvr)
		(*itSvr).second->SendMW_RECALCCOUNTRYBALANCE_ACK(bCount);
}

void CTWorldSvrModule::SaveGuildStats(CTGuild* pGuild)
{
	if (!pGuild)
		return;

	if (pGuild->m_dwStatExp >= CALCULATE_NEXTGEXP(pGuild->m_bStatLevel) {
		pGuild->m_bStatLevel++;
		pGuild->m_bStatPoint++;
		pGuild->m_dwStatExp = 0;
	}

	DEFINE_QUERY(&m_db, CSPSaveGuildStats);
	query->m_GuildID = pGuild->m_dwID;
	query->m_Level = pGuild->m_bStatLevel;
	query->m_SkillPoint = pGuild->m_bStatPoint;
	query->m_Exp = pGuild->m_dwStatExp;

	query->Call();

	UNDEFINE_QUERY();
}
#ifdef __TW_APEX
// Apex Function Start/////////////////////////////////////////////////////////////////////////
long CTWorldSvrModule::ApexSendToMap(signed int nSendId, const char * pBuffer,int nLen)
{
	LPPACKETBUF pMSG = new PACKETBUF();
	pMSG->m_packet.SetID(SM_APEXDATA_REQ)
		<< nSendId
		<< nLen;

	pMSG->m_packet.AttachBinary((LPVOID)pBuffer, nLen);

	_AtlModule.SayToBATCH(pMSG);

	return EC_NOERROR;
}
long CTWorldSvrModule::ApexKillUser(signed int nId, int nAction)
{
	LPPACKETBUF pMSG = new PACKETBUF();
	pMSG->m_packet.SetID(SM_APEXKILLUSER_REQ)
		<< nId
		<< nAction;

	_AtlModule.SayToBATCH(pMSG);

	return EC_NOERROR;
}
void CTWorldSvrModule::ApexNotifyUserEnter(DWORD dwCharID, CString strName, DWORD dwIP)
{
	if(m_pApexRecv)
	{
		m_pApexRecv('L',dwCharID,strName,strName.GetLength());

		char szBuf[5] = {0};
		szBuf[0] = 0x01;
		memcpy(&szBuf[1], &dwIP, sizeof(dwIP));
		m_pApexRecv('S', dwCharID, szBuf, 5);
	}
}
void CTWorldSvrModule::ApexNotifyUserLeave(DWORD dwCharID, CString strName)
{
	if(m_pApexRecv)
		m_pApexRecv('G',dwCharID,strName,strName.GetLength());
}
void CTWorldSvrModule::ApexNotifyUserData(int nSendId,const char * pBuf,int nBufLen)
{
	if(m_pApexRecv)
		m_pApexRecv('T',nSendId,pBuf,nBufLen);
}
void CTWorldSvrModule::ApexNotifyUserReturn(int nSendId,const char * pRet)
{
	if(m_pApexRecv)
		m_pApexRecv('R',nSendId,(char*)pRet,4);
}

// Apex Function End//
#endif