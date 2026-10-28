	m_mapTSvrMsg.clear();

	DEFINE_QUERY(&m_db, CSPGetNation)
	if(!query->Call())
		return EC_INITSERVICE_NATION;
	m_bNation = query->m_bNation;
	if(!m_bNation)
		return EC_INITSERVICE_NATION;
	UNDEFINE_QUERY()

	DEFINE_QUERY(&m_db, CSPGetRecallID)
	if(!query->Call())
		return EC_INITSERVICE_NATION;
	m_dwGenRecallID = query->m_dwGenID;
	UNDEFINE_QUERY()

	DEFINE_QUERY(&m_db, CTBLActiveCharTable)
	if(query->Open())
	{
		BYTE bWarCountry;
		while(query->Fetch())
		{
			bWarCountry = query->m_bCountry < TCONTRY_B ? query->m_bCountry : (query->IsNull(2) ? TCONTRY_N : query->m_bAidCountry);
			if(bWarCountry >= TCONTRY_B)
				continue;

			BYTE bGap = GetWarCountryGap(query->m_bLevel);
			if(bGap >= WARCOUNTRY_MAXGAP)
				continue;
            
			m_mapWarCountry[bWarCountry][bGap].insert(MAPDWORD::value_type(query->m_dwCharID, 0));
		}
		query->Close();
	}
	UNDEFINE_QUERY()

	DEFINE_QUERY(&m_db, CTBLRPSGame)
	if(query->Open())
	{
		while(query->Fetch())
		{
			TRPSGAME rps;
			rps.m_bType = query->m_bType;
			rps.m_bWinCount = query->m_bWinCount;
			rps.m_wWinKeep = query->m_wWinKeep;
			rps.m_wWinPeriod = query->m_wWinPeriod;
			memcpy(rps.m_bProb, query->m_bProb, sizeof(rps.m_bProb));

			m_mapRPSGame.insert(MAPRPSGAME::value_type(MAKEWORD(rps.m_bType, rps.m_bWinCount), rps));
		}
		query->Close();
	}
	UNDEFINE_QUERY()

	DEFINE_QUERY(&m_db, CTBLRPSGameRecord)
	if(query->Open())
	{
		while(query->Fetch())
		{
			MAPRPSGAME::iterator it = m_mapRPSGame.find(MAKEWORD(query->m_bType, query->m_bWinCount));
			if(it!=m_mapRPSGame.end())
				(*it).second.m_vWinDate.push_back(__DBTOTIME(query->m_dWinDate));
		}
		query->Close();
	}
	UNDEFINE_QUERY()

	DEFINE_QUERY(&m_db, CTBLSvrMsg)
	if(query->Open())
	{
		while(query->Fetch())
			m_mapTSvrMsg.insert(MAPTSTRING::value_type(query->m_dwID, query->m_szMessage));
		query->Close();
	}
	UNDEFINE_QUERY()

	DEFINE_QUERY(&m_db, CTBLEventQuarterChart)
	if(query->Open())
	{
		while(query->Fetch())
		{
			LPTEVENTQUARTER pQT = new TEVENTQUARTER();
			pQT->m_wID = query->m_wID;
			pQT->m_bDay = query->m_bDay;
			pQT->m_bHour = query->m_bHour;
			pQT->m_bMinute = query->m_bMinute;
			pQT->m_strPresent = query->m_szPresent;
			pQT->m_strAnnounce = BuildNetString(NAME_NULL, query->m_szAnnounce);
			pQT->m_bNotice = FALSE;
			pQT->m_nTime = GetNextEventTime(pQT->m_bDay, pQT->m_bHour, pQT->m_bMinute);

			m_mapEVQT.insert(MAPTEVENTQUARTER::value_type(pQT->m_wID, pQT));
			m_mapTimeEVQT.insert(MAPTIMEQUARTER::value_type(pQT->m_nTime, pQT->m_wID));
		}
		query->Close();
	}
	UNDEFINE_QUERY()

	DEFINE_QUERY(&m_db, CTBLGuildChart);
	if(query->Open())
	{
		while(query->Fetch())
		{
			LPTGUILDLEVEL pLevel = new TGUILDLEVEL();
			pLevel->m_bLevel = query->m_bLevel;
			pLevel->m_dwEXP = query->m_dwEXP;
			pLevel->m_bMaxCnt = query->m_bMaxCnt;
			pLevel->m_bMinCnt = query->m_bMinCnt;
			pLevel->m_bCabinetCnt = query->m_bCabinetCnt;
			pLevel->m_bTacticsCnt = query->m_bTacticsCnt;
			pLevel->m_bBattleSetCnt = query->m_bBattleSetCnt;
			pLevel->m_bGuardCnt = query->m_bGuardCnt;
			pLevel->m_bRoyalGuardCnt = query->m_bRoyalGuardCnt;
			pLevel->m_bTurretCnt = query->m_bTurretCnt;
			for(BYTE i=0; i<MAX_GUILD_PEER_COUNT; i++)
				pLevel->m_bPeer[i] = query->m_bPeer[i];

			m_mapTGuildLevel.insert(MAPTGUILDLEVEL::value_type(pLevel->m_bLevel, pLevel));
		}
		query->Close();
	}
	UNDEFINE_QUERY();

	DEFINE_QUERY(&m_db, CTBLGuild);
	if(query->Open())
	{
		while(query->Fetch())
		{
			CTGuild * pGuild = new CTGuild();
			pGuild->m_dwID = query->m_dwID;
			pGuild->m_strName = query->m_szName;
			pGuild->m_dwFame = query->m_dwFame;
			pGuild->m_dwFameColor = query->m_dwFameColor;
			pGuild->m_bMaxCabinet = query->m_bMaxCabinet;
			pGuild->m_bGPoint = query->m_bGPoint;
			pGuild->m_bLevel = query->m_bLevel;
			pGuild->m_dwChief = query->m_dwChief;
			pGuild->m_dwExp = query->m_dwExp;
			pGuild->m_dwGI = query->m_dwGI;
			pGuild->m_bStatus = query->m_bStatus;
			pGuild->m_dwGold = query->m_dwGold;
			pGuild->m_dwSilver = query->m_dwSilver;
			pGuild->m_dwCooper = query->m_dwCooper;
			pGuild->m_bDisorg = query->m_bDisorg;
			pGuild->m_dwTime = query->m_dwTime;
			pGuild->m_timeEstablish = __DBTOTIME(query->m_timeEstablish);
			pGuild->m_dwPvPTotalPoint = query->m_dwPvPTotalPoint;
			pGuild->m_dwPvPUseablePoint = query->m_dwPvPUseablePoint;
			pGuild->m_dwPvPMonthPoint = query->m_dwPvPMonthPoint;

			m_mapTGuild.insert(MAPTGUILD::value_type(pGuild->m_dwID, pGuild));
			if(pGuild->m_bDisorg)
				m_mapTGuildEx.insert(MAPDWORD::value_type(pGuild->m_dwID, pGuild->m_dwTime));

			pGuild->m_pTLEVEL = FindGuildLevel(pGuild->m_bLevel);
		}
		query->Close();
	}
	UNDEFINE_QUERY();

	MAPTGUILD::iterator it;
	m_mapCharGuild.clear();
	m_mapCharTactics.clear();

	for(it=m_mapTGuild.begin(); it!=m_mapTGuild.end(); it++)
	{
		CTGuild * pGuild = (*it).second;
		DEFINE_QUERY(&m_db, CTBLGuildArticle)
		query->m_dwGuildID = pGuild->m_dwID;
		if(query->Open())
		{
			while(query->Fetch())
			{
				DWORD dwID = query->m_dwID;
				BYTE bDuty = query->m_bDuty;
				CString strWritter = query->m_szWritter;
				CString strTitle = query->m_szTitle;
				CString strArticle = query->m_szArticle;
				DWORD dwTime = query->m_dwTime;
				(*it).second->AddArticle(dwID, bDuty, strWritter, strTitle, strArticle, dwTime);
			}
			query->Close();
		}
		UNDEFINE_QUERY()

		DEFINE_QUERY(&m_db, CTBLGuildItem)
		query->m_dwOwnerID = pGuild->m_dwID;
		query->m_bOwnerType = TOWNER_GUILD;
		query->m_bStorageType = STORAGE_CABINET;

		if(query->Open())
		{
			while(query->Fetch())
			{
				query->m_item.m_dEndTime = __DBTOTIME(query->m_dEndTime);
				LPTITEM pItem = CreateItem(query->m_item);
				pGuild->PutInItem(pItem->m_dwItemID, pItem);
			}
			query->Close();
		}
		UNDEFINE_QUERY()

		DEFINE_QUERY(&m_db, CTBLGuildMember)
		query->m_dwGuildID = pGuild->m_dwID;
		if(query->Open())
		{
			while(query->Fetch())
			{
				LPTGUILDMEMBER pMember = new TGUILDMEMBER();
				pMember->m_dwGuildID = pGuild->m_dwID;
				pMember->m_dwID = query->m_dwMemberID;
				pMember->m_strName = query->m_szName;
				pMember->m_bLevel = query->m_bLevel;
				pMember->m_bClass = query->m_bClass;
				pMember->m_bDuty = query->m_bDuty;
				pMember->m_bPeer = query->m_bPeer;
				pMember->m_pChar = NULL;
				pMember->m_wCastle = 0;
				pMember->m_bCamp = CAMP_NONE;
				pMember->m_dwTactics = 0;
				pMember->m_bWarCountry = query->IsNull(3) ? query->m_bCountry : query->m_bWarCountry;
				pMember->m_dlConnectedDate = __DBTOTIME(query->m_dConnectedDate);

				pGuild->m_mapTMember.insert(MAPTGUILDMEMBER::value_type(pMember->m_dwID, pMember));
				if(pMember->m_bDuty==GUILD_DUTY_CHIEF)
				{
					pGuild->m_dwChief = pMember->m_dwID;
					pGuild->m_strChief = pMember->m_strName;
					pGuild->m_bCountry = query->m_bCountry;
				}
				m_mapCharGuild.insert(MAPDWORD::value_type(pMember->m_dwID, pGuild->m_dwID));
			}
			query->Close();
		}
		UNDEFINE_QUERY()

		DEFINE_QUERY(&m_db, CTBLGuildTactics)
		query->m_dwGuildID = pGuild->m_dwID;
		if(query->Open())
		{
			while(query->Fetch())
			{
				LPTTACTICSMEMBER pMember = new TTACTICSMEMBER();
				pMember->m_dwID = query->m_dwMemberID;
				pMember->m_strName = query->m_szName;
				pMember->m_bLevel = query->m_bLevel;
				pMember->m_bClass = query->m_bClass;
				pMember->m_dwRewardPoint = query->m_dwRewardPoint;
				pMember->m_dlRewardMoney = query->m_dlRewardMoney;
				pMember->m_dwGainPoint = query->m_dwGainPoint;
				pMember->m_bDay = query->m_bDay;
				pMember->m_dlEndTime = __DBTOTIME(query->m_timeEnd);
				pMember->m_pChar = NULL;
				pGuild->m_mapTTactics.insert(MAPTTACTICSMEMBER::value_type(pMember->m_dwID, pMember));
				m_mapCharTactics.insert(MAPDWORD::value_type(pMember->m_dwID, pGuild->m_dwID));
			}
			query->Close();
		}
		UNDEFINE_QUERY()

		DEFINE_QUERY(&m_db, CTBLGuildPvPointReward)
		query->m_dwGuildID = pGuild->m_dwID;
		if(query->Open())
		{
			while(query->Fetch())
			{
				TGUILDPOINTREWARD log;
				log.m_strName = query->m_szName;
				log.m_dwPoint = query->m_dwPoint;
				log.m_dlDate = __DBTOTIME(query->m_dlDate);
				(*it).second->m_vPointReward.push_back(log);
			}
			query->Close();
		}
		UNDEFINE_QUERY()

		DEFINE_QUERY(&m_db, CTBLGuildPvPRecord)
		query->m_dwGuildID = pGuild->m_dwID;
		if(query->Open())
		{
			while(query->Fetch())
			{
				LPTGUILDMEMBER pMember = pGuild->FindMember(query->m_dwCharID);
				if(pMember)
				{
					TENTRYRECORD rec;
					rec.m_dwDate = query->m_dwDate;
					rec.m_wKillCount = query->m_wKillCount;
					rec.m_wDieCount = query->m_wDieCount;
					rec.m_bLoad = TRUE;
					for(BYTE e=0; e<PVPE_COUNT; e++)
					{
						rec.m_aGainPoint[e] = query->m_aPoint[e];
					}

					pMember->m_vRecord.push_back(rec);

					if(m_dwRecentRecordDate < query->m_dwDate)
						m_dwRecentRecordDate = query->m_dwDate;
				}
			}
			query->Close();
		}
		UNDEFINE_QUERY()
	}

	for(it=m_mapTGuild.begin(); it!=m_mapTGuild.end(); it++)
		(*it).second->CalcWeekRecord(m_dwRecentRecordDate);

	MAPDWORD::iterator itGM;
	MAPDWORD::iterator itTM;

	for(itTM = m_mapCharTactics.begin(); itTM != m_mapCharTactics.end(); itTM++)
	{
		itGM = m_mapCharGuild.find((*itTM).first);
		if(itGM != m_mapCharGuild.end())
		{
			CTGuild * pGuild = FindTGuild((*itTM).second);
			if(pGuild)
			{
				LPTGUILDMEMBER pGm = pGuild->FindMember((*itTM).first);
				if(pGm)
					pGm->m_dwTactics = (*itTM).second;
			}
		}
	}

	DEFINE_QUERY(&m_db, CTBLCastleApplicant)
	if(query->Open())
	{
		while(query->Fetch())
		{
			itTM = m_mapCharTactics.find(query->m_dwCharID);
			if(itTM != m_mapCharTactics.end())
			{
				CTGuild * pGuild = FindTGuild((*itTM).second);
				if(pGuild)
				{
					LPTTACTICSMEMBER pTactics = pGuild->FindTactics(query->m_dwCharID);
					if(pTactics)
					{
						pTactics->m_wCastle = query->m_wCastle;
						pTactics->m_bCamp = query->m_bCamp;
					}
				}
			}
			else
			{
				itGM = m_mapCharGuild.find(query->m_dwCharID);
				if(itGM != m_mapCharGuild.end())
				{
					CTGuild * pGuild = FindTGuild((*itGM).second);
					if(pGuild)
					{
						LPTGUILDMEMBER pGm = pGuild->FindMember((*itGM).first);
						if(pGm)
						{
							pGm->m_wCastle = query->m_wCastle;
							pGm->m_bCamp = query->m_bCamp;
						}
					}
				}
			}
		}
		query->Close();
	}
	UNDEFINE_QUERY()

	DEFINE_QUERY(&m_db, CTBLGuildRelation)
	if(query->Open())
	{
		while(query->Fetch())
		{
			switch(query->m_bType)
			{
			case RELATION_ALLIENCE:
				it = m_mapTGuild.find(query->m_dwGuildOne);
				if(it!=m_mapTGuild.end())
					(*it).second->m_vAllience.push_back(query->m_dwGuildTwo);
				it = m_mapTGuild.find(query->m_dwGuildTwo);
				if(it!=m_mapTGuild.end())
					(*it).second->m_vAllience.push_back(query->m_dwGuildOne);
				break;
			case RELATION_ENEMY:
				it = m_mapTGuild.find(query->m_dwGuildOne);
				if(it!=m_mapTGuild.end())
					(*it).second->m_vEnemy.push_back(query->m_dwGuildTwo);
				it = m_mapTGuild.find(query->m_dwGuildTwo);
				if(it!=m_mapTGuild.end())
					(*it).second->m_vEnemy.push_back(query->m_dwGuildOne);
				break;
			default:
				break;
			}
		}
		query->Close();
	}
	UNDEFINE_QUERY()

	DEFINE_QUERY(&m_db, CTBLGuildStats)
		if (query->Open())
		{
			while (query->Fetch())
			{
				auto itGuild = m_mapTGuild.find(query->m_dwGuildID);
				if (itGuild == m_mapTGuild.end())
					continue;

				CTGuild* pGuild = (*itGuild).second;
				pGuild->m_bStatLevel = query->m_bLevel;
				pGuild->m_bStatPoint = query->m_bSkillPoint;
				pGuild->m_dwStatExp = query->m_dwExp;
			}
			query->Close();
		}
	UNDEFINE_QUERY();
	DEFINE_QUERY( &m_db, CTBLOperatorChart)
	if(query->Open())
	{
				while(query->Fetch())
			m_vTOPERATOR.push_back(query->m_dwOperator);

		query->Close();
	}
	UNDEFINE_QUERY()

	DEFINE_QUERY(&m_db, CTBLBOWSettingsChart);
	if(query->Open())
	{
		while(query->Fetch())
		{
			if(!m_pBOWModule)
			{
				m_pBOWModule = new CTBowSystem(query->m_wMapID, query->m_bMinPlayersCount, query->m_bMaxNationDifference);
				m_pBOWModule->SetDurations(query->m_dwAlarmDur, query->m_dwBuyTimeDur, query->m_dwBattleDur);
				CTBowSystem::Log("BoW System has been initiliazed");
				break;
			}
		}
	}
	query->Close();
	UNDEFINE_QUERY();

	DEFINE_QUERY(&m_db, CTBLBRSettingsChart);
	if(query->Open())
	{
		while(query->Fetch())
		{
			if(!m_pBRModule)
			{
				m_pBRModule = new CBRSystem(query->m_dwAlarmDur, query->m_dwBuyTimeDur, query->m_dwBattleDur, query->m_bMinPlayerCount);
				CBRSystem::Log("Battle Royale System has been initialized!");
				break;
			}
		}
	}
	query->Close();
	UNDEFINE_QUERY();
	
	DEFINE_QUERY( &m_db, CTBLCustomTimeChart);
	m_vCUSTOMTIMES.clear();
	if(query->Open())
	{
		while(query->Fetch())
		{
			switch(query->m_bType)
			{
			case BT_MISSION:
				m_vCUSTOMTIMES.push_back(query->m_dwTime);
				break;
			case BT_BOW:
				if (m_pBOWModule)
					m_pBOWModule->m_vTIMES.push_back(query->m_dwTime);
				break;
			case BT_BR:
				if (m_pBRModule)
					m_pBRModule->m_vTIMES.push_back(query->m_dwTime);
				break;
			}
		}

		if (m_pBOWModule)
			m_pBOWModule->Init();
		if (m_pBRModule)
			m_pBRModule->Init();

		query->Close();
	}

	UNDEFINE_QUERY();
	DEFINE_QUERY(&m_db, CTBLBattleTimeChart)
		if(query->Open())


		{
			while(query->Fetch())
			{
				if(query->m_bType < BT_COUNT)
				{
					m_battletime[query->m_bType].m_bType = query->m_bType;
					m_battletime[query->m_bType].m_bStatus = BS_NORMAL;
					m_battletime[query->m_bType].m_dwBattleDur = query->m_dwBattleDur;
					m_battletime[query->m_bType].m_dwBattleStart = query->m_dwBattleStart;
					m_battletime[query->m_bType].m_dwAlarmStart = query->m_dwAlarmStart;
					m_battletime[query->m_bType].m_dwAlarmEnd = query->m_dwAlarmEnd;
					m_battletime[query->m_bType].m_dwPeaceDur = query->m_dwPeaceDur;
					m_battletime[query->m_bType].m_bDay = query->m_bDay;
					m_battletime[query->m_bType].m_bWeek = query->m_bWeek;







					if(query->m_bType == BT_MISSION && query->m_dwBattleStart)
					{
						CTime curtime(CTime::GetCurrentTime());
						DWORD dwCH = curtime.GetHour();
						DWORD dwCM = curtime.GetMinute();
						DWORD dwCS = curtime.GetSecond();
						DWORD dwWhoreTimeStamp = (dwCH * HOUR_ONE) + (dwCM * 60) + dwCS;
#ifdef FOURSTORY_NATIVE_SERVER
						m_battletime[query->m_bType].m_dwBattleStart = next_mission_start(m_vCUSTOMTIMES,dwWhoreTimeStamp);
#else
						MAPDWORD mapCLOSE;




						for (VECTORDWORD::iterator it = m_vCUSTOMTIMES.begin(); it != m_vCUSTOMTIMES.end(); ++it)
							mapCLOSE.insert(std::make_pair((DWORD)*it-dwWhoreTimeStamp, *it));



						DWORD dwClosestTime = mapCLOSE.begin()->second;
						if(mapCLOSE.begin()->second < dwWhoreTimeStamp)
#ifdef FOURSTORY_NATIVE_MODULE
							m_battletime[query->m_bType].m_dwBattleStart = mapCLOSE.find( 2 ) == mapCLOSE.end() ? 0 : mapCLOSE.find( 2 )->second; // the original dereferences end(); 0 selects dwClosestTime below
#else
							m_battletime[query->m_bType].m_dwBattleStart = mapCLOSE.find( 2 )->second;
#endif
						else
							m_battletime[query->m_bType].m_dwBattleStart = dwClosestTime;

						if(m_battletime[query->m_bType].m_dwBattleStart <= 0)
							m_battletime[query->m_bType].m_dwBattleStart = dwClosestTime;
#endif
					}
				}
			}

			m_battletime[BT_LOCAL].m_dwPeaceDur = 180;
			query->Close();
		}
	UNDEFINE_QUERY()






























	DEFINE_QUERY(&m_db, CTBLGuildWanted)
	if(query->Open())
	{
		while(query->Fetch())
		{
			CTGuild * pGuild = FindTGuild(query->m_dwGuildID);
			if(pGuild)
				AddGuildWanted(pGuild, query->m_bMinLevel, query->m_bMaxLevel, query->m_szTitle, query->m_szText, __DBTOTIME(query->m_timeEnd));
		}
		query->Close();
	}
	UNDEFINE_QUERY()

	DEFINE_QUERY(&m_db, CTBLGuildTacticsWanted)
	if(query->Open())
	{
		while(query->Fetch())
		{
			CTGuild * pGuild = FindTGuild(query->m_dwGuildID);
			if(pGuild)
				AddGuildTacticsWanted(
					pGuild,
					query->m_dwID,
					query->m_dwPvPoint,
					query->m_dwGold,
					query->m_dwSilver,
					query->m_dwCooper,
					query->m_bDay,
					query->m_bMinLevel,
					query->m_bMaxLevel,
					query->m_szTitle,
					query->m_szText,
					__DBTOTIME(query->m_timeEnd));

			m_dwTacticsIndex = query->m_dwID;
		}
		query->Close();
	}
	UNDEFINE_QUERY()

	DEFINE_QUERY(&m_db, CTBLGuildVolunteer)
	if(query->Open())
	{
		while(query->Fetch())
		{
			if(query->m_bType == GUILDAPP_MEMBER)
				AddGuildWantedApp(query->m_dwID, query->m_dwCharID, query->m_szName, query->m_bLevel, query->m_bClass);
			else
			{
				LPTGUILDTACTICSWANTED pWanted = FindGuildTacticsWanted(query->m_dwID);
				if(pWanted)
					AddGuildTacticsWantedApp(query->m_dwID, query->m_dwCharID, query->m_szName, query->m_bLevel, query->m_bClass, pWanted->m_dwGuildID);
			}
		}
		query->Close();
	}
	UNDEFINE_QUERY()

	for(WORD gp=0x100; gp<0xFFFF; gp++)
		m_qGenPartyID.push(gp);

	for(BYTE i=0; i<COUNTRY_COUNT; i++)
	{
		DEFINE_QUERY(&m_db, CTBLPvPointTable)
		query->m_bCountry = i;
		if(query->Open())
		{
			if(query->Fetch())
			{
				MONTHRANKER stMONTHRANKER;
				stMONTHRANKER.m_dwCharID = query->m_dwCharID;
				stMONTHRANKER.m_strName = query->m_szName;
				stMONTHRANKER.m_dwTotalPoint = query->m_dwTotalPoint;
				stMONTHRANKER.m_dwMonthPoint = 0;
				stMONTHRANKER.m_wMonthWin = 0;
				stMONTHRANKER.m_wMonthLose = 0;
				stMONTHRANKER.m_dwTotalWin = query->m_dwTotalWin;
				stMONTHRANKER.m_dwTotalLose = query->m_dwTotalLose;
				stMONTHRANKER.m_bCountry = query->m_bCountry;
				stMONTHRANKER.m_bLevel = query->m_bLevel;
				stMONTHRANKER.m_bClass = query->m_bClass;			
				stMONTHRANKER.m_bRace = query->m_bRace;
				stMONTHRANKER.m_bSex = query->m_bSex;
				stMONTHRANKER.m_bFace = query->m_bFace;
				stMONTHRANKER.m_bHair = query->m_bHair;
				stMONTHRANKER.m_strSay = NAME_NULL;
				if(!query->IsNull(11))
					stMONTHRANKER.m_strGuild = query->m_szGuild;
				else
					stMONTHRANKER.m_strGuild = NAME_NULL;

				m_arMonthRank[i][0] = stMONTHRANKER;
			}
			query->Close();
		}
		UNDEFINE_QUERY()
	}

	for(BYTE i=0; i<COUNTRY_COUNT; i++)
	{
		DEFINE_QUERY(&m_db,CTBLMonthPvPointChar)
		query->m_dwCharID = m_arMonthRank[i][0].m_dwCharID;
		if(query->Open())
		{
			if(query->Fetch())
			{
				m_arMonthRank[i][0].m_dwMonthPoint = query->m_dwMonthPoint;
				m_arMonthRank[i][0].m_wMonthWin = query->m_wMonthWin;
				m_arMonthRank[i][0].m_wMonthLose = query->m_wMonthLose;
				m_arMonthRank[i][0].m_strSay = query->m_szSay;
			}
			query->Close();
		}
		UNDEFINE_QUERY()
	}

	for(BYTE i=0; i<COUNTRY_COUNT; i++)
	{
		DEFINE_QUERY(&m_db, CTBLMonthPvPointTable)
		query->m_bCountry = i;
		if(query->Open())
		{
			BYTE bOrder = 1;
			while(query->Fetch())
			{
#ifdef FOURSTORY_NATIVE_SERVER
				if(bOrder>=MONTHRANKCOUNT || query->m_bCountry>=COUNTRY_COUNT) throw std::runtime_error("Invalid World monthly rank row");
#endif
				m_arMonthRank[i][bOrder].m_dwCharID = query->m_dwCharID;
				m_arMonthRank[i][bOrder].m_strName = query->m_szName;
				m_arMonthRank[i][bOrder].m_dwTotalPoint = query->m_dwTotalPoint;
				m_arMonthRank[i][bOrder].m_dwMonthPoint = query->m_dwMonthPoint;
				m_arMonthRank[i][bOrder].m_wMonthWin = query->m_wMonthWin;
				m_arMonthRank[i][bOrder].m_wMonthLose = query->m_wMonthLose;
				m_arMonthRank[i][bOrder].m_dwTotalWin = query->m_dwTotalWin;
				m_arMonthRank[i][bOrder].m_dwTotalLose = query->m_dwTotalLose;
				m_arMonthRank[i][bOrder].m_bCountry = query->m_bCountry;
				m_arMonthRank[i][bOrder].m_bLevel = query->m_bLevel;
				m_arMonthRank[i][bOrder].m_bClass = query->m_bClass;
				m_arMonthRank[i][bOrder].m_bRace = query->m_bRace;
				m_arMonthRank[i][bOrder].m_bSex = query->m_bSex;
				m_arMonthRank[i][bOrder].m_bFace = query->m_bFace;
				m_arMonthRank[i][bOrder].m_bHair = query->m_bHair;
				m_arMonthRank[i][bOrder].m_strSay = query->m_szSay;
				if(!query->IsNull(16))
					m_arMonthRank[query->m_bCountry][bOrder].m_strGuild = query->m_szGuild;
				else
					m_arMonthRank[query->m_bCountry][bOrder].m_strGuild = NAME_NULL;

				bOrder++;			
			}
			query->Close();
		}
		UNDEFINE_QUERY()
	}

	DEFINE_QUERY(&m_db,CTBLFirstGradeGroup)
	CTime t(CTime::GetCurrentTime());
	BYTE bRankMonth = t.GetMonth();
	bRankMonth--;
	if(bRankMonth == 0)
		bRankMonth = 12;

	query->m_bMonth = bRankMonth;
	
	if(query->Open())
	{
		while(query->Fetch())
		{
#ifdef FOURSTORY_NATIVE_SERVER
			if(query->m_bCountry>=COUNTRY_COUNT || query->m_bRank>=FIRSTGRADEGROUPCOUNT) throw std::runtime_error("Invalid World first-grade rank row");
#endif
			m_arFirstGradeGroup[query->m_bCountry][query->m_bRank].m_dwCharID = query->m_dwCharID;
			m_arFirstGradeGroup[query->m_bCountry][query->m_bRank].m_strName = query->m_szName;
			m_arFirstGradeGroup[query->m_bCountry][query->m_bRank].m_dwTotalPoint = query->m_dwTotalPoint;
			m_arFirstGradeGroup[query->m_bCountry][query->m_bRank].m_dwMonthPoint = query->m_dwMonthPoint;
			m_arFirstGradeGroup[query->m_bCountry][query->m_bRank].m_wMonthWin = query->m_wMonthWin;
			m_arFirstGradeGroup[query->m_bCountry][query->m_bRank].m_wMonthLose = query->m_wMonthLose;
			m_arFirstGradeGroup[query->m_bCountry][query->m_bRank].m_dwTotalWin = query->m_dwTotalWin;
			m_arFirstGradeGroup[query->m_bCountry][query->m_bRank].m_dwTotalLose = query->m_dwTotalLose;
			m_arFirstGradeGroup[query->m_bCountry][query->m_bRank].m_bCountry = query->m_bCountry;
			m_arFirstGradeGroup[query->m_bCountry][query->m_bRank].m_bLevel = query->m_bLevel;
			m_arFirstGradeGroup[query->m_bCountry][query->m_bRank].m_bClass = query->m_bClass;
			m_arFirstGradeGroup[query->m_bCountry][query->m_bRank].m_bRace = query->m_bRace;
			m_arFirstGradeGroup[query->m_bCountry][query->m_bRank].m_bSex = query->m_bSex;
			m_arFirstGradeGroup[query->m_bCountry][query->m_bRank].m_bFace = query->m_bFace;
			m_arFirstGradeGroup[query->m_bCountry][query->m_bRank].m_bHair = query->m_bHair;
			m_arFirstGradeGroup[query->m_bCountry][query->m_bRank].m_strSay = query->m_szSay;
			m_arFirstGradeGroup[query->m_bCountry][query->m_bRank].m_strGuild = query->m_szGuild;
		}
		query->Close();
	}
	UNDEFINE_QUERY()

	DEFINE_QUERY(&m_db, CSPGetLimitedLevel)
	if(!query->Call())
		return EC_INITSERVICE_LIMITEDLEVEL;
	m_bMaxLevel = query->m_bMaxLevel;
	UNDEFINE_QUERY()

	WORD wTnmtID = 0;
	BYTE bTnmtGroup = 0;
	BYTE bTnmtStep = TNMTSTEP_READY;
	DEFINE_QUERY(&m_db, CTBLTournamentCurrentStep)
	if(query->Open())
	{
		if(query->Fetch())
		{
			wTnmtID = query->m_wID;
			bTnmtGroup = query->m_bGroup;
			bTnmtStep = query->m_bStep;
		}

		query->Close();
	}
	UNDEFINE_QUERY()

	LPMAPTOURNAMENTSTEP pTS = NULL;
	DEFINE_QUERY(&m_db, CTBLTournamentSchedule)
	if(query->Open())
	{
		pTS = new MAPTOURNAMENTSTEP();

		while(query->Fetch())
		{
			TOURNAMENTSTEP tour;
			tour.m_bGroup = query->m_bGroup;
			tour.m_bStep = query->m_bStep;
			tour.m_dwPeriod = query->m_dwPeriod;

			pTS->insert(MAPTOURNAMENTSTEP::value_type(MAKEWORD(tour.m_bStep, tour.m_bGroup), tour));
		}
		query->Close();
	}
	UNDEFINE_QUERY()

	WORD wTourID = SetTournamentTime(pTS, m_battletime[BT_TOURNAMENT], 1, FALSE, TRUE, wTnmtID, bTnmtGroup, bTnmtStep);
	if(wTourID)
	{
		MAPTOURNAMENTENTRY mapTNMTE;
		mapTNMTE.clear();

		DEFINE_QUERY(&m_db, CTBLTournament)
		if(query->Open())
		{
			while(query->Fetch())
			{
				LPTOURNAMENTENTRY pTour = new TOURNAMENTENTRY();
				pTour->m_bGroup = query->m_bGroup;
				pTour->m_bEntryID = query->m_bEntryID;
				pTour->m_strName = query->m_szName;
				pTour->m_bType = query->m_bType;
				pTour->m_dwClass = query->m_dwClass;
				pTour->m_dwFee = query->m_dwFee;
				pTour->m_dwFeeBack = query->m_dwFeeBack;
				pTour->m_wPermitItemID = query->m_wPermitItemID;
				pTour->m_bPermitCount = query->m_bPermitCount;
				pTour->m_bMinLevel = 0;
				pTour->m_bMaxLevel = 0xFF;
				pTour->m_map1st.clear();
				pTour->m_mapNormal.clear();
				pTour->m_mapPlayer.clear();

				mapTNMTE.insert(MAPTOURNAMENTENTRY::value_type(pTour->m_bEntryID, pTour));
			}
			query->Close();
		}
		UNDEFINE_QUERY()

		if(!mapTNMTE.empty())
			m_bFirstGroupCount = BYTE(min(mapTNMTE.size() * 4.0f / 3.0f + 1.999f, FIRSTGRADEGROUPCOUNT));

		m_mapTournament.insert(MAPTOURNAMENT::value_type(wTourID, mapTNMTE));

		DEFINE_QUERY(&m_db, CTBLTournamentReward)
		if(query->Open())
		{
			while(query->Fetch())
			{
				MAPTOURNAMENTENTRY::iterator itEt = mapTNMTE.find(query->m_bEntryID);
				if(itEt!=mapTNMTE.end())
				{
					LPTOURNAMENTENTRY pEntry = (*itEt).second;

					TNMTREWARD reward;
					reward.m_bChartType = query->m_bChartType;
					reward.m_wItemID = query->m_wItemID;
					reward.m_bCount = query->m_bCount;
					reward.m_dwClass = query->m_dwClass;
					reward.m_bCheckShield = query->m_bCheckShield;
					pEntry->m_vReward.push_back(reward);
				}
			}

			query->Close();
		}
		UNDEFINE_QUERY()
	}
	else
		delete pTS;

	MAPTOURNAMENTTIME mapte;
	mapte.clear();
	DEFINE_QUERY(&m_db, CTBLTnmtEventTime)
	if(query->Open())
	{
		while(query->Fetch())
		{
			TBATTLETIME bt;
			bt.m_bWeek = query->m_bWeek;
			bt.m_bDay = query->m_bDay;
			bt.m_dwBattleStart = query->m_dwStart;

			mapte.insert(MAPTOURNAMENTTIME::value_type(query->m_wTourID, bt));
		}
		query->Close();
	}
	UNDEFINE_QUERY()

	MAPTOURNAMENTTIME::iterator itTT;
	for(itTT=mapte.begin(); itTT!=mapte.end(); itTT++)
	{
		WORD wTEID = (*itTT).first;

		LPMAPTOURNAMENTSTEP pTES = NULL;
		DEFINE_QUERY(&m_db, CTBLTnmtEventSchedule)
		query->m_wTourID = wTEID;
		if(query->Open())
		{
			pTES = new MAPTOURNAMENTSTEP();

			while(query->Fetch())
			{
				TOURNAMENTSTEP tour;
				tour.m_bStep = query->m_bStep;
				tour.m_dwPeriod = query->m_dwPeriod;

				pTES->insert(MAPTOURNAMENTSTEP::value_type(tour.m_bStep, tour));
			}
			query->Close();
		}
		UNDEFINE_QUERY()

		WORD wTourID = SetTournamentTime(pTES, (*itTT).second, wTEID, FALSE, TRUE, wTnmtID, bTnmtGroup, bTnmtStep);
		if(wTourID)
		{
			MAPTOURNAMENTENTRY mapTNMTE;
			mapTNMTE.clear();

			DEFINE_QUERY(&m_db, CTBLTournamentEvent)
			query->m_wTourID = wTEID;
			if(query->Open())
			{
				while(query->Fetch())
				{
					LPTOURNAMENTENTRY pTour = new TOURNAMENTENTRY();
					pTour->m_bEntryID = query->m_bEntryID;
					pTour->m_strName = query->m_szName;
					pTour->m_bType = query->m_bType;
					pTour->m_dwClass = query->m_dwClass;
					pTour->m_dwFee = query->m_dwFee;
					pTour->m_dwFeeBack = query->m_dwFeeBack;
					pTour->m_wPermitItemID = query->m_wPermitItemID;
					pTour->m_bPermitCount = query->m_bPermitCount;
					pTour->m_bMinLevel = query->m_bMinLevel;
					pTour->m_bMaxLevel = query->m_bMaxLevel;
					pTour->m_map1st.clear();
					pTour->m_mapNormal.clear();
					pTour->m_mapPlayer.clear();

					mapTNMTE.insert(MAPTOURNAMENTENTRY::value_type(pTour->m_bEntryID, pTour));
				}
				query->Close();
			}
			UNDEFINE_QUERY()

			m_mapTournament.insert(MAPTOURNAMENT::value_type(wTourID, mapTNMTE));

			DEFINE_QUERY(&m_db, CTBLTnmtEventReward)
			query->m_wTourID = wTEID;
			if(query->Open())
			{
				while(query->Fetch())
				{
					MAPTOURNAMENTENTRY::iterator itEt = mapTNMTE.find(query->m_bEntryID);
					if(itEt!=mapTNMTE.end())
					{
						LPTOURNAMENTENTRY pEntry = (*itEt).second;

						TNMTREWARD reward;
						reward.m_bChartType = query->m_bChartType;
						reward.m_wItemID = query->m_wItemID;
						reward.m_bCount = query->m_bCount;
						reward.m_dwClass = query->m_dwClass;
						reward.m_bCheckShield = query->m_bCheckShield;
						pEntry->m_vReward.push_back(reward);
					}
				}

				query->Close();
			}
			UNDEFINE_QUERY()
		}
		else
			delete pTES;
	}

	if(m_tournament.m_wID)
	{
		LPMAPTOURNAMENTENTRY pEntry = GetTournament(m_tournament.m_wID);
		if(pEntry)
		{
			m_tournament.m_mapEntry = pEntry;

			BYTE bEC = TournamentGetEntryCount();
			if(bEC)
				m_tournament.m_bBase = BYTE(TOURNAMENT_BASEPRIZE / bEC);
			else
				m_tournament.m_bBase = 0;

			BYTE b1St;
			MAPTOURNAMENTSTEP::iterator itStep = m_tournament.m_mapStep.find(TNMTSTEP_1st);
			if(itStep != m_tournament.m_mapStep.end() && (*itStep).second.m_dwPeriod)
				b1St = TRUE;
			else
				b1St = FALSE;

			DEFINE_QUERY(&m_db, CTBLTournamentPlayer)
			if(query->Open())
			{
				MAPTNMTPLAYER mapTP;
				mapTP.clear();
				BYTE bStep;

				while(query->Fetch())
				{
					LPTOURNAMENTENTRY pEt = GetCurrentTournamentEntry(query->m_bEntry);
					if(!pEt) continue;

					LPTNMTPLAYER pTarget = new TNMTPLAYER();

					pTarget->m_bEntryID = query->m_bEntry;
					pTarget->m_bClass = query->m_bClass;
					pTarget->m_bCountry = query->m_bCountry;
					pTarget->m_bLevel = query->m_bLevel;
					pTarget->m_dwCharID = query->m_dwCharID;
					pTarget->m_strName = query->m_szName;
					pTarget->m_dwChiefID = query->m_dwChiefID;
					pTarget->m_strHWID = query->m_szHWID;
					pTarget->m_dwIPAddr = query->m_dwIPAddr;

					if(query->m_bStep == TNMTSTEP_QFINAL)
						pTarget->m_bResult[MATCH_QFINAL] = query->m_bResult;
					else if(query->m_bStep == TNMTSTEP_SFINAL)
					{
						pTarget->m_bResult[MATCH_QFINAL] = TNMTWIN_WIN;
						pTarget->m_bResult[MATCH_SFINAL] = query->m_bResult;
					}
					else if(query->m_bStep == TNMTSTEP_FINAL)
					{
						pTarget->m_bResult[MATCH_QFINAL] = TNMTWIN_WIN;
						pTarget->m_bResult[MATCH_SFINAL] = TNMTWIN_WIN;
						pTarget->m_bResult[MATCH_FINAL] = query->m_bResult;
					}

					MAPDWORD::iterator itGM = m_mapCharGuild.find(pTarget->m_dwCharID);
					if(itGM != m_mapCharGuild.end())
					{
						CTGuild * pGuild = FindTGuild((*itGM).second);
						if(pGuild)
							pTarget->m_strGuildName = pGuild->m_strName;
					}

					GetRanking(pTarget->m_dwCharID, pTarget->m_dwRank, pTarget->m_dwMonthRank);

					if(b1St && IsFirstGroup(pTarget->m_bCountry, pTarget->m_dwCharID))
						bStep = TNMTSTEP_1st;
					else
						bStep = TNMTSTEP_NORMAL;

					if(query->m_dwCharID == query->m_dwChiefID)
						AddTNMTPlayer(pEt, pTarget, bStep, pTarget);
					else
						mapTP.insert(MAPTNMTPLAYER::value_type(pTarget->m_dwCharID, pTarget));
				}
				query->Close();

				if(m_tournament.m_bStep >= TNMTSTEP_PARTY)
					TournamentSelectPlayer();

				MAPTNMTPLAYER::iterator itTP;
				for(itTP=mapTP.begin(); itTP != mapTP.end(); itTP++)
				{
					LPTOURNAMENTENTRY pEt = GetCurrentTournamentEntry((*itTP).second->m_bEntryID);
					if(pEt)
					{
						LPTNMTPLAYER pChief = FindTNMTPlayer((*itTP).second->m_dwChiefID);
						if(pChief)
							AddTNMTPlayer(pEt, (*itTP).second, TNMTSTEP_PARTY, pChief);
						else
							delete (*itTP).second;
					}
					else
						delete (*itTP).second;
				}
				mapTP.clear();
			}
			UNDEFINE_QUERY()
		}
	}

	CalcGuildRanking();

	CTime t = CTime::GetCurrentTime().GetTime();
	m_bRankMonth = t.GetMonth();	

	DEFINE_QUERY(&m_db, CTBLCMGiftChart)

	if(query->Open())
	{
		while(query->Fetch())
		{
			LPCMGIFT pGift = new TCMGIFT();

			pGift->m_wGiftID		= query->m_wGiftID;
			pGift->m_bGiftType		= query->m_bGiftType;
			pGift->m_dwValue		= query->m_dwValue;
			pGift->m_bCount			= query->m_bCount;
			pGift->m_bTakeType		= query->m_bTakeType;
			pGift->m_bMaxTakeCount	= query->m_bMaxTakeCount;
			pGift->m_bToolOnly		= query->m_bToolOnly;
			pGift->m_wErrGiftID		= query->m_wErrGiftID;
			pGift->m_strTitle		= query->m_szTitle;
			pGift->m_strMsg			= query->m_szMsg;

			m_mapCMGift.insert(MAPCMGIFT::value_type(pGift->m_wGiftID, pGift));
		}
	}




















































	UNDEFINE_QUERY()

	return EC_NOERROR;
