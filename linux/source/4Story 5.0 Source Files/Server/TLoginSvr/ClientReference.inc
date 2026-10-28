// Original Login client reference loading with a native read-only boundary.
#ifdef FOURSTORY_NATIVE_SERVER
	m_clientReference.open(client_reference_path);
	m_dlCheckFile = m_clientReference.checksum();
#else
	TCHAR szFN[ONE_KBYTE];

	GetModuleFileName(NULL, szFN, ONE_KBYTE);
	char *pSlash = ::strrchr(szFN, '\\');
	if( pSlash )
		::lstrcpy(pSlash + 1, "\\TClient.exe");

	m_hExecFile = CreateFile(
		szFN,
		GENERIC_READ,
		FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL |
		FILE_FLAG_RANDOM_ACCESS,
		NULL);

	if(m_hExecFile != INVALID_HANDLE_VALUE)
	{
		INT64 dlValue = 0;
		DWORD dwNumberOfBytesRead = 0;

		BYTE bResult = ReadFile(m_hExecFile, (LPVOID)&dlValue, sizeof(INT64), &dwNumberOfBytesRead, NULL);
		while(bResult && dwNumberOfBytesRead)
		{
			m_dlCheckFile ^= dlValue;
			bResult = ReadFile(m_hExecFile, (LPVOID)&dlValue, sizeof(INT64), &dwNumberOfBytesRead, NULL);
		}
	}

#endif
