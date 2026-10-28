#ifdef FOURSTORY_NATIVE_SERVER
	return m_serviceControl.start(pService->m_dwID) ? TRUE : FALSE;
#else
	SC_HANDLE hscm = OpenSCManager(pService->m_pMachine->m_vPriAddr.front(),NULL,SC_MANAGER_ALL_ACCESS);
	if(!hscm)
		return FALSE;

	BYTE bResult = TRUE;

	SC_HANDLE hsc = OpenService(hscm, pService->m_strName, SERVICE_ALL_ACCESS);
	if( !hsc || !::StartService(hsc, 0, NULL) )
		bResult = FALSE;
	
	CloseServiceHandle(hsc);
	CloseServiceHandle(hscm);

	return bResult;

#endif
