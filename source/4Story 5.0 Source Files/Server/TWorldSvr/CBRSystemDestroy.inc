	Release();

	if (m_pBRServer)
	{
#ifdef FOURSTORY_NATIVE_SERVER
		native_release_server(m_pBRServer);
#else
		delete m_pBRServer;
#endif
		m_pBRServer = NULL;
	}
