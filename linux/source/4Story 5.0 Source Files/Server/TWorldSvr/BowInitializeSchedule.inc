#ifdef FOURSTORY_NATIVE_SERVER
	DWORD dwCurrentCLT = current_seconds;
#else
	CTime curtime(CTime::GetCurrentTime());
	INT nCH = curtime.GetHour();
	INT nCM = curtime.GetMinute();
	INT nCS = curtime.GetSecond();
	DWORD dwCurrentCLT = nCH * 60 * 60 + nCM * 60 + nCS;
#endif

	sort(m_vTIMES.begin(), m_vTIMES.end());
	VECTORDWORD::iterator it = std::upper_bound(m_vTIMES.begin(), m_vTIMES.end(), dwCurrentCLT);
	if (it != m_vTIMES.end())
		m_dwStart = *it;
	else
		m_dwStart = (*m_vTIMES.begin());

	if (m_dwStart < dwCurrentCLT && it != m_vTIMES.end())
	{
		for (BYTE i = 0; i < m_vTIMES.size(); ++i)
		{
			if (m_vTIMES[i] > dwCurrentCLT)
			{
				m_dwStart = m_vTIMES[i];
				break;
			}
		}
	}

	CString strLog;
	strLog.Format("First time set to %d", m_dwStart);
	Log(strLog);
