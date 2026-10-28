		if(pService->m_dwStatus != ss.dwCurrentState)
		{			
			BYTE bSMS = 0;

			// 서비스가 Stop 됐을 때.
			if(ss.dwCurrentState == DCSVC_STAT_STOPPED)
			{
				LogEvent("QS_Stopped Server : %s", pService->m_strName);
				pService->m_dwStopCount += 1;
				pService->m_dwAutoStartTick = GetTickCount();

				if(pService->m_bManagerControl == TRUE)
					bSMS = 1;
			}
			else if(pService->m_dwStopCount != 0 &&
				pService->m_dwStatus != DCSVC_STAT_CANNOTCONTROL &&
				ss.dwCurrentState == DCSVC_STAT_RUNNING)
			{
				if(pService->m_bManagerControl == TRUE)
					bSMS = 2;				
			}

			if(ss.dwCurrentState == DCSVC_STAT_RUNNING)
				pService->m_bManagerControl = 1;

			pService->m_dwStatus = ss.dwCurrentState;

			LPPACKETBUF pMsg = new PACKETBUF();
			pMsg->m_packet.SetID(CT_SERVICECHANGE_REQ)
				<< pService->m_dwID
				<< pService->m_dwStatus
				<< bSMS
				<< pService->m_pSvrType->m_bType;

			SayToBATCH(pMsg);
		}
