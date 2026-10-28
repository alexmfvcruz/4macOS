#ifdef FOURSTORY_NATIVE_SERVER
	return m_serviceControl.stop(pService->m_dwID) ? TRUE : FALSE;
#else
	SC_HANDLE hscm = OpenSCManager(pService->m_pMachine->m_vPriAddr.front(),NULL,SC_MANAGER_ALL_ACCESS);
	if(!hscm)
		return FALSE;

	BYTE bResult = TRUE;
	SERVICE_STATUS ss;
	ZeroMemory(&ss, sizeof(SERVICE_STATUS) );

	SC_HANDLE hsc = OpenService( hscm, pService->m_strName, SERVICE_ALL_ACCESS );
	if( !hsc || !::ControlService(hsc, SERVICE_CONTROL_STOP, &ss) )
		bResult = FALSE;

	CloseServiceHandle(hsc);
	CloseServiceHandle(hscm);

	return bResult;

#endif
