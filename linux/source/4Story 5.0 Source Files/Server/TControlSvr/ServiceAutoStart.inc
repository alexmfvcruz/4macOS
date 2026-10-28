				if(ss.dwCurrentState == DCSVC_STAT_STOPPED 
					&& pService->m_dwStatus ==  DCSVC_STAT_STOPPED
					&& m_bAutoStart == TRUE 
					&& pService->m_bManagerControl == TRUE)
				{
					DWORD dwCurTick = GetTickCount();
					if( pService->m_dwAutoStartTick + 60000 < dwCurTick) // 1분후에 시작
					{								
						StartService(pService);			
						pService->m_dwAutoStartTick = dwCurTick;
						LogEvent("QS_Auto Start : %s ", pService->m_strName);
					}
				}
