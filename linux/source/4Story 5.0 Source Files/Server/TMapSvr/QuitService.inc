// Original Map quit handler with an explicit native service-control boundary.



	LogSmartly(_T("SM_QUITSERVICE_REQ detected !!"));
#ifdef FOURSTORY_NATIVE_SERVER
	runtime.request_stop();
#else
	if(m_bService)
		SetServiceStatus(SERVICE_STOP_PENDING);
	PostThreadMessage( m_dwThreadID, WM_QUIT, 0, 0);
#endif




	return EC_NOERROR;
