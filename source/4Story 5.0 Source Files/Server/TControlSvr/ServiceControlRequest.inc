
	CTManager * pManager = (CTManager *)pBUF->m_pSESSION;
	if(!pManager->CheckAuthority(MANAGER_SERVICE))  
		return EC_NOERROR;	

	DWORD dwServiceID;
	BYTE bStart;
	pBUF->m_packet
		>> dwServiceID
		>> bStart;	

	MAPTSVRTEMP::iterator it = m_mapTSVRTEMP.find(dwServiceID);
	if(it == m_mapTSVRTEMP.end())
		return EC_NOERROR;

	BYTE bRet = FALSE;
	switch((*it).second->m_dwStatus)
	{
	case DCSVC_STAT_STOPPED:
		if(bStart && StartService((*it).second))
		{
			//(*it).second->m_bManagerControl = TRUE;
			bRet = TRUE;
			LogEvent( "start SvrControl: %s",(*it).second->m_strName);
		}		

		break;

	case DCSVC_STAT_RUNNING:
		if(!bStart && StopService((*it).second))
		{			
			(*it).second->m_bManagerControl = FALSE;
			
			if( (*it).second->m_pSvrType->m_bType == SVRGRP_WORLDSVR ) // 월드서버이면
			{
				MAPTSVRTEMP::iterator itM;
				for( itM = m_mapTSVRTEMP.begin(); itM != m_mapTSVRTEMP.end(); itM++)
				{
					if( (*itM).second->m_pGroup->m_bGroupID == (*it).second->m_pGroup->m_bGroupID ) 
					{
						(*itM).second->m_bManagerControl = FALSE;
						(*itM).second->m_dwAutoStartTick = GetTickCount();
						LogEvent( "Stop SvrControl: %s",(*itM).second->m_strName);
					}
				}
			}
			bRet = TRUE;
		}
		break;
	default:
		bRet = ACK_FAILED;
	}

	pManager->SendCT_SERVICECONTROL_ACK(bRet);

	return EC_NOERROR;
