	if (m_pBOWServer)
	{
#ifdef FOURSTORY_NATIVE_SERVER
		native_release_server(m_pBOWServer);
#else
		delete m_pBOWServer;
#endif
		m_pBOWServer = NULL;
	}

	m_vTIMES.clear();
	Release();
