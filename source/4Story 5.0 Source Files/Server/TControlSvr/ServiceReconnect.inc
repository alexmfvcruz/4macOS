				if(ss.dwCurrentState == DCSVC_STAT_RUNNING &&
					pService->m_dwStatus != DCSVC_STAT_RUNNING)
				{
					//-------------------------------------------------------------------------------------
					// 서비스 변화 때 로그 남기기					
					switch(pService->m_dwStatus)
					{					
					case DCSVC_STAT_STOPPED :			LogEvent("%s Before : DCSVC_STAT_STOPPED", pService->m_strName);		 break;
					case DCSVC_STAT_START_PENDING:		LogEvent("%s Before : DCSVC_STAT_START_PENDING",pService->m_strName);	 break;
					case DCSVC_STAT_STOP_PENDING:		LogEvent("%s Before : DCSVC_STAT_STOP_PENDING",pService->m_strName);	 break;
					case DCSVC_STAT_RUNNING:			LogEvent("%s Before : DCSVC_STAT_RUNNING",pService->m_strName);			 break;
					case DCSVC_STAT_CONTINUE_PENDING:	LogEvent("%s Before : DCSVC_STAT_CONTINUE_PENDING",pService->m_strName); break;
					case DCSVC_STAT_PENDING:			LogEvent("%s Before : DCSVC_STAT_PENDING",pService->m_strName);			 break;
					case DCSVC_STAT_PAUSED:				LogEvent("%s Before : DCSVC_STAT_PAUSED",pService->m_strName);			 break;
					case DCSVC_STAT_CANNOTCONTROL:		LogEvent("%s Before : DCSVC_STAT_CANNOTCONTROL",pService->m_strName);	 break;
					default:							LogEvent("%s Before : Invalid Status",pService->m_strName);
					}					
					//-------------------------------------------------------------------------------------
					
					
					if(pService->m_dwStatus != DCSVC_STAT_CANNOTCONTROL)
					{							
#ifdef FOURSTORY_NATIVE_SERVER
						pService->m_nLatestStop = wall();
#else
						CTime t(CTime::GetCurrentTime());
						pService->m_nLatestStop = t.GetTime();
#endif
						LogEvent("Time Refresh");
					}


					if(pService->m_pSvrType->m_bType != SVRGRP_LOG )
					{
						LPPACKETBUF pBUF = new PACKETBUF();
						pBUF->m_packet.SetID(CT_NEWCONNECT_REQ)
							<< pService->m_dwID;
						SayToBATCH(pBUF);
					}
				
				}
