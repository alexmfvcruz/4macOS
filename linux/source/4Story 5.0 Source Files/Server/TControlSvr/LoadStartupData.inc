	m_mapTSVRTEMP.clear();

	DEFINE_QUERY(&m_db, CTBLMachine);
	if(query->Open())
	{
		while(query->Fetch())
		{
			LPTMACHINE pMachine = new TMACHINE();
			pMachine->m_strNetwork = _T("");
			pMachine->m_bMachineID = query->m_bMachineID;
			pMachine->m_strName = query->m_szName;
			pMachine->m_bRouteID = query->m_bRouteID;
			pMachine->m_bClose = TRUE;			
			m_mapTMachine.insert(MAPTMACHINE::value_type(pMachine->m_bMachineID, pMachine));
		}
		query->Close();
	}
	UNDEFINE_QUERY();

	DEFINE_QUERY(&m_db, CTBLNetwork);
	if(query->Open())
	{
		while(query->Fetch())
		{
			MAPTMACHINE::iterator find = m_mapTMachine.find(query->m_bMachineID);
			if(find!=m_mapTMachine.end())
				(*find).second->m_strNetwork = query->m_szNetwork;
		}
		query->Close();
	}
	UNDEFINE_QUERY();

	DEFINE_QUERY(&m_db, CTBLGroup)
	if(query->Open())
	{
		while(query->Fetch())
		{
			LPTGROUP pGroup = new TGROUP();
			pGroup->m_bGroupID = query->m_bGroupID;
			pGroup->m_strName = query->m_szName;
			m_mapTGroup.insert(MAPTGROUP::value_type(pGroup->m_bGroupID, pGroup));
		}
		query->Close();
	}
	UNDEFINE_QUERY();

	DEFINE_QUERY(&m_db, CTBLSvrType)
	if(query->Open())
	{
		while(query->Fetch())
		{
			LPTSVRTYPE pType = new TSVRTYPE();
			pType->m_bType = query->m_bType;
			pType->m_strName = query->m_szName;
			m_mapTSvrType.insert(MAPTSVRTYPE::value_type(pType->m_bType, pType));
		}
		query->Close();
	}
	UNDEFINE_QUERY();

	DEFINE_QUERY(&m_db, CTBLServer)
	if(query->Open())
	{
		while(query->Fetch())
		{
#ifndef FOURSTORY_NATIVE_ODBC
			CTime t(CTime::GetCurrentTime());
#endif
			LPTSVRTEMP pSvr = new TSVRTEMP();
			pSvr->m_bServerID = query->m_bServerID;
			pSvr->m_wPort = query->m_wPort;
			pSvr->m_strName = query->m_szName;
			pSvr->m_dwStatus = DCSVC_STAT_STOPPED;
			pSvr->m_bManagerControl = FALSE;
			pSvr->m_dwAutoStartTick = 0;
			pSvr->m_nLatestStop = 0;

			MAPTGROUP::iterator itGp = m_mapTGroup.find(query->m_bGroupID);
			if(itGp != m_mapTGroup.end())
				pSvr->m_pGroup = (*itGp).second;
			MAPTMACHINE::iterator itMc = m_mapTMachine.find(query->m_bMachineID);
			if(itMc != m_mapTMachine.end())			
				pSvr->m_pMachine = (*itMc).second;
				
			MAPTSVRTYPE::iterator itTy = m_mapTSvrType.find(query->m_bType);
			if(itTy != m_mapTSvrType.end())
				pSvr->m_pSvrType = (*itTy).second;

			if(itGp == m_mapTGroup.end() ||
				itMc == m_mapTMachine.end() ||
				itTy == m_mapTSvrType.end())
			{
				delete pSvr;
				continue;
			}
		
			pSvr->m_dwID = MAKESVRID(query->m_bGroupID, query->m_bType, query->m_bServerID);
			m_mapTSVRTEMP.insert(MAPTSVRTEMP::value_type(pSvr->m_dwID, pSvr));
		}
		query->Close();
	}
	UNDEFINE_QUERY();

	MAPTMACHINE::iterator itMc;
	for(itMc = m_mapTMachine.begin(); itMc != m_mapTMachine.end(); itMc++)
	{
		DEFINE_QUERY(&m_db, CTBLIPAddr);
		query->m_bMachineID = (*itMc).first;
		if(query->Open())
		{
			while(query->Fetch())
			{
				if(lstrcmp(query->m_szIPAddr, _T("")) != 0)
					(*itMc).second->m_vIPAddr.push_back(query->m_szIPAddr);
				if(lstrcmp(query->m_szPriAddr, _T("")) != 0)
					(*itMc).second->m_vPriAddr.push_back(query->m_szPriAddr);
				if(query->m_bMachineID == 1)
					m_svraddr.sin_addr.s_addr = inet_addr(query->m_szIPAddr);
			}
			query->Close();
		}
		UNDEFINE_QUERY();
	}


	//DEFINE_QUERY(&m_db, CTBLManager)
	//if(query->Open())
	//{
	//	while(query->Fetch())
	//	{			
	//		LPTMANAGERTEMP pManager = new TMANAGERTEMP();
	//		pManager->m_strPasswd = query->m_szPasswd;
	//		pManager->m_bAuthority = query->m_bAuthority; 
	//		m_mapMANTEMP.insert(MAPTMANAGERTEMP::value_type(query->m_szID, pManager));
	//	}
	//	query->Close();
	//}
	//UNDEFINE_QUERY();

	DEFINE_QUERY(&m_db, CSPLoadService)
	query->m_bWorld = SVRGRP_NULL;
	query->m_bServiceGroup = SVRGRP_CTLSVR;
	if(!query->Call())
		return EC_INITSERVICE_DBOPENFAILED;
	m_strMyAddr = query->m_szIP;
	UNDEFINE_QUERY();

	OpenPlatform();


	m_dwEventIndex = 0;
	CString strValue;
	DEFINE_QUERY(&m_db, CTBLEvent)
	if(query->Open())
	{
		while(query->Fetch())
		{
			EVENTINFO stEVENTINFO;
			stEVENTINFO.m_dwIndex = query->m_dwIndex;
			stEVENTINFO.m_bID = query->m_bID;
			stEVENTINFO.m_bGroupID = query->m_bGroupID;			
		    stEVENTINFO.m_bSvrID = query->m_bSvrID;
			stEVENTINFO.m_bSvrType = query->m_bSvrType;
			stEVENTINFO.m_dStartDate = __DBTOTIME(query->m_dStartDate);
			stEVENTINFO.m_dEndDate = __DBTOTIME(query->m_dEndDate);
			stEVENTINFO.m_wValue = query->m_wValue;
			stEVENTINFO.m_wMapID = query->m_wMapID;  
			stEVENTINFO.m_dwStartAlarm = query->m_dwStartAlarm;
			stEVENTINFO.m_dwEndAlarm = query->m_dwEndAlarm;
			stEVENTINFO.m_strStartMsg = query->m_szStartMsg;
			//stEVENTINFO.m_strMidMsg = query->m_szMidMsg;  
			stEVENTINFO.m_strEndMsg = query->m_szEndMsg;
			stEVENTINFO.m_strTitle = query->m_szTitle;
			strValue = query->m_szValue;
			stEVENTINFO.m_bStartAlarm = FALSE;
			stEVENTINFO.m_bEndAlarm = FALSE;
			stEVENTINFO.m_bState = FALSE;
			stEVENTINFO.m_bPartTime = query->m_bPartTime;
			ParseStrValue(stEVENTINFO,strValue,stEVENTINFO.m_bID);

			m_mapEVENT.insert(MAPEVENTINFO::value_type(stEVENTINFO.m_dwIndex,stEVENTINFO));

			if(m_dwEventIndex < stEVENTINFO.m_dwIndex)
				m_dwEventIndex = stEVENTINFO.m_dwIndex;
		}
	}
	UNDEFINE_QUERY()


	return EC_NOERROR;
