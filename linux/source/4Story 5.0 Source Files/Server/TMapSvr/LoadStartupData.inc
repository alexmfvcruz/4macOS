	CTMap::m_vTCHANNEL.clear();
	m_mapTLOGCHANNEL.clear();
	m_mapTCHANNEL.clear();

	m_mapTMONSPAWN.clear();
	m_mapTMONSTER.clear();
	m_mapTMONATTR.clear();
	m_mapTPET.clear();
	m_mapTTITLE.clear();
	m_mapTCMDTEMP.clear();
	m_mapTMONAI.clear();
	m_mapTLEVEL.clear();
	m_mapTCLASS.clear();
	m_mapTSKILL.clear();
	m_mapTRACE.clear();
	m_mapTItemAttr.clear();
	m_mapTItemMagic.clear();
	m_vItemSet.clear();
	m_mapTITEM.clear();
	m_mapTINDUN.clear();
	m_mapTNpc.clear();
	m_mapTLOCAL.clear();
	m_mapTPortal.clear();
	m_mapTOPERATOR.clear();
	//m_mapTSECURE.clear(); //SC
	m_mapTPostBills.clear();
	m_mapCharPostBills.clear();
	m_mapTMAGICSKILL.clear();
	m_mapTCashItem.clear();
	m_mapTGAMBLE.clear();
	m_mapTPvPointKill.clear();	
	m_mapCashGameble.clear();
	m_mapHelpMessage.clear();
	m_mapRPSGame.clear();
	m_mapArena.clear();










	DWORD dwGBCmd[GB_COUNT] = {0,1000,15000,15000,20000,300000};
	for(BYTE gb=0; gb<GB_COUNT; gb++)
		m_dwGBDelay[gb] = dwGBCmd[gb];

	for(BYTE i = 0; i < ITEMGROUP_COUNT; i++)
		m_mapTMONITEM[i].clear();

	m_mapTSvrMsg.clear();
	m_vCashCategory.clear();

	DEFINE_QUERY(&m_db,CSPQuestSendPost)
	UNDEFINE_QUERY()

	for(BYTE ik=0; ik<IK_COUNT; ik++)
		m_mapTITEMKINDATTR[ik].clear();

	DEFINE_QUERY(&m_db, CSPLoadService)
	query->m_bWorld = SVRGRP_NULL;
	query->m_bServiceGroup = SVRGRP_CTLSVR;
	if(!query->Call())
		return EC_INITSERVICE_CTRLSVRDATA;

	m_addrCtrl.sin_addr.s_addr = inet_addr(query->m_szIP);
	m_addrCtrl.sin_port = query->m_wPort;
	UNDEFINE_QUERY();

	DEFINE_QUERY(&m_db, CSPLoadService)
	query->m_bWorld = m_bGroupID;
	query->m_bServiceGroup = SVRGRP_RLYSVR;
	if(!query->Call() || !query->m_wPort)
		return EC_INITSERVICE_RELAYSVRDATA;

	m_addrRelay.sin_addr.s_addr = inet_addr(query->m_szIP);
	m_addrRelay.sin_port = query->m_wPort;
	UNDEFINE_QUERY();

	DEFINE_QUERY(&m_db, CSPInitGenItemID)
	query->m_bServerID = m_bServerID;
	if(!query->Call())
		return EC_INITSERVICE_DBOPENFAILED;
	m_dlGenItemID = query->m_dlGenItemID;
	if(m_dlGenItemID <= 0)
		return EC_INITSERVICE_DBOPENFAILED;
	UNDEFINE_QUERY()

	DEFINE_QUERY(&m_db, CSPGetLimitedLevel)
	if(!query->Call())
		return EC_INITSERVICE_LIMITEDLEVEL;
	m_bMaxLevel = query->m_bMaxLevel;
	UNDEFINE_QUERY()

	DEFINE_QUERY(&m_db, CSPGetNation)
	if(!query->Call())
		return EC_INITSERVICE_NATION;
	m_bNation = query->m_bNation;
	if(!m_bNation)
		return EC_INITSERVICE_NATION;
	UNDEFINE_QUERY()

	DEFINE_QUERY(&m_db, CTBLHelpMessage)
	if(query->Open())
	{
		while(query->Fetch())
		{
			THELPMESSAGE help;
			help.m_bID = query->m_bID;
			help.m_dlStartTime = __DBTOTIME(query->m_timeStart);
			help.m_dlEndTime = __DBTOTIME(query->m_timeEnd);
			help.m_strMessage = query->m_szMessage;
			help.m_bNotice = FALSE;
			if(!help.m_dlEndTime || help.m_dlEndTime > CTime::GetCurrentTime().GetTime())
				m_mapHelpMessage.insert(MAPHELPMESSAGE::value_type(help.m_bID, help));
		}
		query->Close();
	}
	UNDEFINE_QUERY()

	DEFINE_QUERY(&m_db, CTBLTutorialCharItem)
	if(query->Open())
	{
		while(query->Fetch())
		{
			if(query->m_bCountry >= TCONTRY_COUNT || query->m_bClass >= TCLASS_COUNT || query->m_bSex >= TSEX_COUNT)
				continue;

			m_arTutorialItem[query->m_bCountry][query->m_bClass][query->m_bSex][ES_PRMWEAPON] = query->m_wPrmWeapon;
			m_arTutorialItem[query->m_bCountry][query->m_bClass][query->m_bSex][ES_SNDWEAPON] = query->m_wSndWeapon;
			m_arTutorialItem[query->m_bCountry][query->m_bClass][query->m_bSex][ES_LONGWEAPON] = query->m_wLongWeapon;
			m_arTutorialItem[query->m_bCountry][query->m_bClass][query->m_bSex][ES_HEAD] = query->m_wHead;
			m_arTutorialItem[query->m_bCountry][query->m_bClass][query->m_bSex][ES_BACK] = query->m_wBack;
			m_arTutorialItem[query->m_bCountry][query->m_bClass][query->m_bSex][ES_BODY] = query->m_wBody;
			m_arTutorialItem[query->m_bCountry][query->m_bClass][query->m_bSex][ES_PANTS] = query->m_wPants;
			m_arTutorialItem[query->m_bCountry][query->m_bClass][query->m_bSex][ES_FOOT] = query->m_wFoot;
			m_arTutorialItem[query->m_bCountry][query->m_bClass][query->m_bSex][ES_HAND] = query->m_wHand;
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

	DEFINE_QUERY(&m_db, CTBLItemAttrChart)
	if(query->Open())
	{
		while(query->Fetch())
		{
			LPTITEMATTR pAttr = new TITEMATTR();
			pAttr->m_wID = query->m_wID;
			pAttr->m_bKind = query->m_bKind;
			pAttr->m_bGrade = query->m_bGrade;
			pAttr->m_bBlockProb = query->m_bBlockProb;
			pAttr->m_wDP = query->m_wDP;
			pAttr->m_wMDP = query->m_wMDP;
			pAttr->m_wMinAP = query->m_wMinAP;
			pAttr->m_wMaxAP = query->m_wMaxAP;
			pAttr->m_wMinMAP = query->m_wMinMAP;
			pAttr->m_wMaxMAP = query->m_wMaxMAP;
			m_mapTItemAttr.insert(MAPTITEMATTR::value_type(pAttr->m_wID, pAttr));
			m_mapTITEMKINDATTR[pAttr->m_bKind].insert(MAPTITEMATTR::value_type(pAttr->m_wID, pAttr));
		}
		query->Close();
	}
	UNDEFINE_QUERY()

	DEFINE_QUERY(&m_db, CTBLItemGradeChart)
	if(query->Open())
	{
		while(query->Fetch())
		{
			m_itemgrade[query->m_bLevel].m_bLevel = query->m_bLevel;
			m_itemgrade[query->m_bLevel].m_bGrade = query->m_bGrade;
			m_itemgrade[query->m_bLevel].m_bProb = query->m_bProb;
			m_itemgrade[query->m_bLevel].m_dwMoney = query->m_dwMoney;
		}
		query->Close();
	}
	UNDEFINE_QUERY()

	DEFINE_QUERY(&m_db, CTBLGemGradeChart)
	if(query->Open())
	{
		while(query->Fetch())
		{
#ifdef FOURSTORY_NATIVE_MODULE
			// TGEMGRADECHART carries a bGem == GEM_MAX row; the original writes past m_gemgrade.
			if(query->m_bGem >= GEM_MAX)
				continue;
#endif
			m_gemgrade[query->m_bGem].m_bGem = query->m_bGem;
			m_gemgrade[query->m_bGem].m_bProb = query->m_bProb;
		}
		query->Close();
	}
	UNDEFINE_QUERY()

	DEFINE_QUERY(&m_db, CTBLItemMagicChart)

	for(BYTE i=0; i<IK_COUNT; i++)
		m_vItemMagic[i].clear();

	if(query->Open())
	{
		while(query->Fetch())
		{
			LPTITEMMAGIC pMagic = new TITEMMAGIC();
			pMagic->m_bMagic = query->m_bMagic;
			pMagic->m_dwKind = query->m_dwKind;
			pMagic->m_wMaxValue = query->m_wMaxValue;
			pMagic->m_bRvType = query->m_bRvType;
			pMagic->m_bIsMagic = query->m_bIsMagic;
			pMagic->m_bIsRare = query->m_bIsRare;
			pMagic->m_bExclIndex = query->m_bExclIndex;
			pMagic->m_bOptionKind = query->m_bOptionKind;
			pMagic->m_wAutoSkill = query->m_wAutoSkill;
			pMagic->m_bRefine = query->m_bRefine;
			pMagic->m_wMaxBound = query->m_wMaxBound;
			pMagic->m_wRareBound = query->m_wRareBound;
			m_mapTItemMagic.insert(MAPTITEMMAGIC::value_type(pMagic->m_bMagic, pMagic));

			for(BYTE i=1; i<IK_HP; i++)
			{
				if(pMagic->m_dwKind & (1<<(i-1)))
					m_vItemMagic[i].push_back(pMagic);
			}
		}
		query->Close();
	}
	UNDEFINE_QUERY()

	DEFINE_QUERY(&m_db, CTBLItemSetChart)
	if(query->Open())
	{
		while(query->Fetch())
		{
			LPTITEMSET pSet = new TITEMSET();
			pSet->m_bLevel = query->m_bLevel;
			for(BYTE i=0; i<TMAGIC_MAX; i++)
			{
				pSet->m_pMagic[i] = GetItemMagic(query->m_bMagic[i]);
				pSet->m_bMin[i] = query->m_bMin[i];
				pSet->m_bValue[i] = query->m_bValue[i];
			}
			pSet->m_wBaseID = query->m_wBaseID;
			pSet->m_wSetID = query->m_wSetID;

			m_vItemSet.push_back(pSet);
		}
		query->Close();
	}
	UNDEFINE_QUERY()

	DEFINE_QUERY( &m_db, CTBLItemChart)
	if(query->Open())
	{
		MAPVTMONITEM::iterator itMI;

		while(query->Fetch())
		{
			LPTITEM pITEM = new TITEM();

			pITEM->m_wItemID = query->m_wItemID;
			pITEM->m_bType = query->m_bType;
			pITEM->m_bKind = query->m_bKind;
			pITEM->m_wAttrID = query->m_wAttrID;
			pITEM->m_wUseValue = query->m_wUseValue;

			pITEM->m_dwClassID = query->m_dwClassID;
			pITEM->m_dwSlotID = query->m_dwSlotID;
			pITEM->m_fPrice = query->m_fPrice;
			pITEM->m_fPvPrice = query->m_fPvPrice;
			pITEM->m_bIsSell = query->m_bIsSell;

			pITEM->m_bPrmSlotID = query->m_bPrmSlotID;
			pITEM->m_bSubSlotID = query->m_bSubSlotID;
			pITEM->m_bSlotCount = query->m_bSlotCount;
			pITEM->m_bMinRange = query->m_bMinRange;
			pITEM->m_bMaxRange = query->m_bMaxRange;
			pITEM->m_bDefaultLevel = query->m_bDefaultLevel;
			pITEM->m_bStack = query->m_bStack;
			pITEM->m_bEquipSkill = query->m_bEquipSkill;

			pITEM->m_bUseItemKind = query->m_bUseItemKind;
			pITEM->m_bUseItemCount = query->m_bUseItemCount;
			pITEM->m_bGrade = query->m_bGrade;
			pITEM->m_wUseTime = query->m_wUseTime;
			pITEM->m_bUseType = query->m_bUseType;
            pITEM->m_bCanGrade = query->m_bCanGrade;
			pITEM->m_bCanMagic = query->m_bCanMagic;
			pITEM->m_bCanRare = query->m_bCanRare;
			pITEM->m_bDropLevel = query->m_bDropLevel;
			pITEM->m_dwSpeedInc = query->m_dwSpeedInc;
			pITEM->m_bItemCountry = query->m_bItemCountry;
			pITEM->m_bSpecial = query->m_bIsSpecial;
			pITEM->m_dwDelay = query->m_dwDelay;
			pITEM->m_bGambleProb = query->m_bGambleProb;
			pITEM->m_wItemProb_G = query->m_wItemProb_G;
			pITEM->m_bCanGamble = query->m_bCanGamble;
			pITEM->m_bDestroyProb = query->m_bDestroyProb;
			pITEM->m_dwDuraMax = query->m_dwDuraMax;
			pITEM->m_bRefineMax = query->m_bRefineMax;
			pITEM->m_bCanRepair = query->m_bCanRepair;
			pITEM->m_wDelayGroupID = query->m_wDelayGroupID;
			pITEM->m_wWeight = query->m_wWeight;
			pITEM->m_bGroup = query->m_bGroupID;
			pITEM->m_bInitState = query->m_bInitState;
			pITEM->m_bCanWrap = query->m_bCanWrap;
			pITEM->m_dwCode = query->m_dwCode;
			pITEM->m_bCanColor = query->m_bCanColor;
			memcpy(pITEM->m_fRevision, query->m_fRevision, sizeof(FLOAT)*4);
			pITEM->m_bConsumable = query->m_bConsumable;
			m_mapTITEM.insert( MAPTITEMTEMP::value_type( pITEM->m_wItemID, pITEM));

			if(pITEM->m_bDropLevel)
			{
				itMI =  m_mapTMONITEM[pITEM->m_bGroup].find(pITEM->m_bDropLevel);
				if( itMI == m_mapTMONITEM[pITEM->m_bGroup].end())
				{
					VTITEMTEMP vMONITEM;
					vMONITEM.clear();
					vMONITEM.push_back(pITEM);
					m_mapTMONITEM[pITEM->m_bGroup].insert(MAPVTMONITEM::value_type(pITEM->m_bDropLevel, vMONITEM));
				}
				else
					(*itMI).second.push_back(pITEM);
			}
		}

		query->Close();
	}
	UNDEFINE_QUERY()

	DEFINE_QUERY( &m_db, CTBLTitleChart)
	if(query->Open())
	{
		while(query->Fetch())
		{
			LPTTITLETEMP pTITLE = new TTITLETEMP();

			pTITLE->m_wTitleID = query->m_wTitleID;
			pTITLE->m_wCategory = query->m_wCategory;
			pTITLE->m_bKind = query->m_bKind;
			pTITLE->m_dwRequirement = query->m_dwRequirement;

			m_mapTTITLE.insert( MAPTTITLETEMP::value_type( pTITLE->m_wTitleID, pTITLE));
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

			for(BYTE ri=0; ri<2; ri++)
			{
				rps.m_pRewardItem[ri] = NULL;
				rps.m_bItemCount[ri] = 0;

				if(query->m_wRewardItem[ri])
				{
					LPTITEM pItem = FindTItem(query->m_wRewardItem[ri]);
					if(!pItem || !query->m_bItemCount[ri])
						continue;

					rps.m_pRewardItem[ri] = pItem;
					rps.m_bItemCount[ri] = query->m_bItemCount[ri];
				}
			}

			rps.m_bType = query->m_bType;
			rps.m_bWinCount = query->m_bWinCount;
			rps.m_wItemID = query->m_wItemID;
			rps.m_wWinKeep = query->m_wWinKeep;
			rps.m_wWinPeriod = query->m_wWinPeriod;
			rps.m_dwRewardMoney = query->m_dwRewardMoney;
			memcpy(rps.m_bProb, query->m_bProb, sizeof(rps.m_bProb));
			m_mapRPSGame.insert(MAPRPSGAME::value_type(MAKEWORD(rps.m_bType, rps.m_bWinCount), rps));
		}
		query->Close();
	}
	UNDEFINE_QUERY()

	MAPCASHGAMBLE::iterator itCG;
	MAPWDWORD::iterator itCD;
	DEFINE_QUERY(&m_db, CTBLCashGambleChart)
	if(query->Open())
	{	
		while(query->Fetch())
		{
			CTItem * pItem = CreateItem(query->m_item);
			if(!pItem)
				continue;

			LPTCASHGAMBLE pCg = new TCASHGAMBLE();
			pCg->m_dwID = query->m_dwID;
			pCg->m_dwProb = query->m_dwProb;
			pCg->m_pItem = pItem;
			pCg->m_wGroup = query->m_wGroup;
			pCg->m_wUseTime = query->m_wUseTime;
			LPVTCASHGAMBLE pVTCASHGAMBLE = NULL;

			itCG = m_mapCashGameble.find(pCg->m_wGroup);
			if(itCG == m_mapCashGameble.end())
			{
				pVTCASHGAMBLE = new VTCASHGAMBLE();
				pVTCASHGAMBLE->push_back(pCg);


				m_mapCashGameble.insert(MAPCASHGAMBLE::value_type(pCg->m_wGroup,pVTCASHGAMBLE));
			}
			else
			{
				pVTCASHGAMBLE = (*itCG).second;
				pVTCASHGAMBLE->push_back(pCg);
			}

			itCD = m_mapMaxCashGambleProb.find(pCg->m_wGroup);
			if( itCD != m_mapMaxCashGambleProb.end())
				(*itCD).second += pCg->m_dwProb;
			else			
				m_mapMaxCashGambleProb.insert(MAPWDWORD::value_type(pCg->m_wGroup,pCg->m_dwProb));			
		}

		query->Close();
	}
	UNDEFINE_QUERY()

	DEFINE_QUERY( &m_db, CTBLItemMagicSkill)
	if(query->Open())
	{
		while(query->Fetch())
		{
			LPTMAGICSKILL pMSkill = new TMAGICSKILL();
			pMSkill->m_bMagic = query->m_bMagic;
			pMSkill->m_dwKind = query->m_dwKind;
			pMSkill->m_wSkillID = query->m_wSkill;
			pMSkill->m_bIsMagic = query->m_bIsMagic;
			pMSkill->m_bIsRare = query->m_bIsRare;
			pMSkill->m_bMinLevel = query->m_bMinLevel;

			m_vTMagicSkill.push_back(pMSkill);
			for(DWORD i=1 ; i<IK_COUNT; i++)
			{
#ifdef FOURSTORY_NATIVE_MODULE
				BYTE bKind = BYTE((pMSkill->m_dwKind & (1u<<((i-1)&31))) ? i : 0); // x86 masks the shift count to 5 bits
#else
				BYTE bKind = BYTE((pMSkill->m_dwKind & (1<<(i-1))) ? i : 0);
#endif
				if(bKind)
				{
					WORD wKey = MAKEWORD(pMSkill->m_bMagic, bKind);
					MAPTMAGICSKILL::iterator itMS = m_mapTMAGICSKILL.find(wKey);
					if(itMS != m_mapTMAGICSKILL.end())
						(*itMS).second.push_back(pMSkill);
					else
					{
						VTMAGICSKILL vMSkill;
						vMSkill.clear();
						vMSkill.push_back(pMSkill);
						m_mapTMAGICSKILL.insert(MAPTMAGICSKILL::value_type(wKey, vMSkill));
					}
				}
			}
		}

		query->Close();
	}
	UNDEFINE_QUERY()

	DEFINE_QUERY( &m_db, CTBLLevelChart)
	if(query->Open())
	{
		while(query->Fetch())
		{
			LPTLEVEL pTLEVEL = new TLEVEL();

			pTLEVEL->m_bLevel = query->m_bLevel;
			pTLEVEL->m_dwEXP = query->m_dwEXP;
			pTLEVEL->m_dwHP = query->m_dwHP;
			pTLEVEL->m_dwMP = query->m_dwMP;
			pTLEVEL->m_bSkillPoint = query->m_bSkillPoint;
			pTLEVEL->m_dwMoney = query->m_dwMoney;
			pTLEVEL->m_dwRegCost = query->m_dwRegCost;
			pTLEVEL->m_dwSearchCost = query->m_dwSearchCost;
			pTLEVEL->m_dwGambleCost = query->m_dwGambleCost;
			pTLEVEL->m_dwRepCost = query->m_dwRepCost;
			pTLEVEL->m_dwRepairCost = query->m_dwRepairCost;
			pTLEVEL->m_dwRefineCost = query->m_dwRefineCost;
			pTLEVEL->m_wPvPoint = query->m_wPvPoint;
			pTLEVEL->m_dwPvPMoney = query->m_dwPvPMoney;
			m_mapTLEVEL.insert( MAPTLEVEL::value_type( query->m_bLevel, pTLEVEL));
		}

		query->Close();
	}
	UNDEFINE_QUERY()

	DEFINE_QUERY( &m_db, CTBLCashCategory)
	if(query->Open())
	{
		while(query->Fetch())
		{
			LPTCASHCATEGORY pCategory = new TCASHCATEGORY();
			pCategory->m_bID = query->m_bID;
			pCategory->m_strName = query->m_szName;
			pCategory->m_vCashItem.clear();
			m_vCashCategory.push_back(pCategory);
		}
		query->Close();
	}
	UNDEFINE_QUERY()

	DEFINE_QUERY(&m_db, CTBLCashShopItem)
	BYTE bCategory = 0;
	query->m_item.Reset();
	if(query->Open())
	{
		while(query->Fetch())
		{
			CTItem *pTITEM = CreateItem(query->m_item);
			if(query->m_item.m_wItemID && !pTITEM)
				continue;

			LPTCASHITEM pCashItem = new TCASHITEM();
			pCashItem->m_wID = query->m_wID;
			pCashItem->m_wInfoID = (WORD)query->m_wInfoID;
			pCashItem->m_dwMoney = query->m_dwMoney;
			pCashItem->m_bKind = query->m_bKind;
			pCashItem->m_bCanSell = query->m_bCanSell;
			pCashItem->m_bSaleValue = query->m_bSaleValue;
			pCashItem->m_wUseTime = query->m_wUseTime;
			pCashItem->m_pItem = pTITEM;
			pCashItem->m_dwOriMoney = query->m_dwMoney;

			LPTCASHCATEGORY pCategory = FindCashCategory(query->m_bCategory);
			if(pCategory)
				pCategory->m_vCashItem.push_back(pCashItem);

			m_mapTCashItem.insert(MAPTCASHITEM::value_type(pCashItem->m_wID, pCashItem));
		}
		query->Close();
	}
	UNDEFINE_QUERY()

	DEFINE_QUERY(&m_db, CTBLGamble)
	if(query->Open())
	{
		while(query->Fetch())
		{
			LPTGAMBLE pGamble = new TGAMBLE();
			pGamble->m_bType = query->m_bType;
			pGamble->m_bKind = query->m_bKind;
			pGamble->m_wReplaceID = query->m_wReplaceID;
			pGamble->m_bCountMax = query->m_bCountMax;
			pGamble->m_bMinLevel = query->m_bMinLevel;
			pGamble->m_bMaxLevel = query->m_bMaxLevel;
			pGamble->m_wProb = query->m_wProb;

			WORD wGamKey = MAKEWORD(pGamble->m_bType, pGamble->m_bKind);
			MAPTGAMBLE::iterator it = m_mapTGAMBLE.find(wGamKey);
			if(it == m_mapTGAMBLE.end())
			{
				VTGAMBLE vGAM;
				vGAM.clear();
				vGAM.push_back(pGamble);
				m_mapTGAMBLE.insert(MAPTGAMBLE::value_type(wGamKey, vGAM));
			}
			else
			{
				(*it).second.push_back(pGamble);
			}
		}
		query->Close();
	}
	UNDEFINE_QUERY()

	DEFINE_QUERY( &m_db, CTBLClassChart)
	if(query->Open())
	{
		while(query->Fetch())
		{
			LPTSTAT pTCLASS = new TSTAT();

			pTCLASS->m_wSTR = query->m_wSTR;
			pTCLASS->m_wDEX = query->m_wDEX;
			pTCLASS->m_wCON = query->m_wCON;

			pTCLASS->m_wINT = query->m_wINT;
			pTCLASS->m_wWIS = query->m_wWIS;
			pTCLASS->m_wMEN = query->m_wMEN;

			m_mapTCLASS.insert( MAPTSTAT::value_type( query->m_bClassID, pTCLASS));
		}

		query->Close();
	}
	UNDEFINE_QUERY()

	DEFINE_QUERY( &m_db, CTBLRaceChart)
	if(query->Open())
	{
		while(query->Fetch())
		{
			LPTSTAT pTRACE = new TSTAT();

			pTRACE->m_wSTR = query->m_wSTR;
			pTRACE->m_wDEX = query->m_wDEX;
			pTRACE->m_wCON = query->m_wCON;

			pTRACE->m_wINT = query->m_wINT;
			pTRACE->m_wWIS = query->m_wWIS;
			pTRACE->m_wMEN = query->m_wMEN;

			m_mapTRACE.insert( MAPTSTAT::value_type( query->m_bRaceID, pTRACE));
		}

		query->Close();
	}
	UNDEFINE_QUERY()

	DEFINE_QUERY( &m_db, CTBLAICommand)
	if(query->Open())
	{
		while(query->Fetch())
		{
			LPTAICOMMAND pCMD = new TAICOMMAND();

			pCMD->m_bType = query->m_bType;
			pCMD->m_dwID = query->m_dwID;
			pCMD->m_vCONDITION.clear();

			m_mapTCMDTEMP.insert( MAPTAICMDTEMP::value_type( pCMD->m_dwID, pCMD));
		}

		query->Close();
	}
	UNDEFINE_QUERY()

	MAPTAICMDTEMP::iterator itCMD;
	for( itCMD = m_mapTCMDTEMP.begin(); itCMD != m_mapTCMDTEMP.end(); itCMD++)
	{
		LPTAICOMMAND pCMD = (*itCMD).second;

		DEFINE_QUERY( &m_db, CTBLAICondition)
		query->m_dwCmdID = pCMD->m_dwID;
		if(query->Open())
		{
			while(query->Fetch())
			{
				LPTAICONDITION pCON = new TAICONDITION();

				pCON->m_bType = query->m_bType;
				pCON->m_dwID = query->m_dwID;

				pCMD->m_vCONDITION.push_back(pCON);
			}

			query->Close();
		}
		UNDEFINE_QUERY()
	}

	DEFINE_QUERY( &m_db, CTBLAIChart)
	if(query->Open())
	{
		while(query->Fetch())
		{
			CTMonsterAI *pAI = FindTMonsterAI(query->m_bAIType);

			if(!pAI)
			{
				pAI = new CTMonsterAI();
				pAI->m_bAIType = query->m_bAIType;
				m_mapTMONAI.insert( MAPTMONSTERAI::value_type( pAI->m_bAIType, pAI));
			}

			MAPVTAICOMMAND::iterator itVCMD = pAI->m_mapVCOMMAND[query->m_bTriggerType].find(query->m_dwTriggerID);
			LPVTAICOMMAND pVCMD = NULL;

			if( itVCMD == pAI->m_mapVCOMMAND[query->m_bTriggerType].end() )
			{
				pVCMD = new VTAICOMMAND();
				pAI->m_mapVCOMMAND[query->m_bTriggerType].insert( MAPVTAICOMMAND::value_type( query->m_dwTriggerID, pVCMD));
			}
			else
				pVCMD = (*itVCMD).second;

			CTAICommand *pCMD = CTAICommand::CreateCMD(FindTCmdTEMP(query->m_dwCmdID));
			pVCMD->push_back(pCMD);

			pCMD->m_dwDelay = query->m_dwDelay;
			pCMD->m_bLoop = query->m_bLoop;
		}

		query->Close();
	}
	UNDEFINE_QUERY()

	DEFINE_QUERY( &m_db, CTBLIndunChart)
	if(query->Open())
	{
		while(query->Fetch())
		{
			LPTINDURN pIndurn = new TINDURN();
			pIndurn->m_wID = query->m_wID;
			pIndurn->m_wInSpawn = query->m_wInSpawn;
			pIndurn->m_wOutSpawn_D = query->m_wOutSpawn_D;
			pIndurn->m_wOutSpawn_C = query->m_wOutSpawn_C;
            m_mapTINDUN.insert(MAPTINDURN::value_type(query->m_wID, pIndurn));
		}
		query->Close();
	}
	UNDEFINE_QUERY()

	DEFINE_QUERY( &m_db, CTBLChannelList)
	if(query->Open())
	{
		while(query->Fetch())
			CTMap::m_vTCHANNEL.push_back(query->m_bChannel);

		query->Close();
	}
	UNDEFINE_QUERY()

	DEFINE_QUERY( &m_db, CTBLServerChart)
	query->m_bServerID = m_bServerID;
	if(query->Open())
	{
		while(query->Fetch())
		{
			MAPTCHANNEL::iterator itCH = m_mapTCHANNEL.find(query->m_bChannel);
			CTChannel *pCH = NULL;

			if( itCH == m_mapTCHANNEL.end() )
			{
				pCH = new CTChannel();
				pCH->m_bChannel = query->m_bChannel;

				m_mapTCHANNEL.insert( MAPTCHANNEL::value_type( pCH->m_bChannel, pCH));
			}
			else
				pCH = (*itCH).second;

			MAPTMAP::iterator itMAP = pCH->m_mapTMAP.find(query->m_wMapID);
			CTMap *pMAP = NULL;

			if( itMAP == pCH->m_mapTMAP.end() )
			{
				pMAP = new CTMap();

				pMAP->m_bChannel = pCH->m_bChannel;
				pMAP->m_wMapID = query->m_wMapID;

				if(IsIndunMap(query->m_wMapID))
					pMAP->m_bType = MAP_INDUNTEMP;

				pCH->m_mapTMAP.insert( MAPTMAP::value_type( pMAP->m_wMapID, pMAP));
			}
			else
				pMAP = (*itMAP).second;

			pMAP->m_mapTUNIT.insert( MAPWORD::value_type( query->m_wUnitID, query->m_wUnitID));
		}

		query->Close();
	}
	UNDEFINE_QUERY()

	MAPTCHANNEL::iterator itCH;
	for( itCH = m_mapTCHANNEL.begin(); itCH != m_mapTCHANNEL.end(); itCH++)
		(*itCH).second->InitChannel( &m_db, m_bServerID);

	DEFINE_QUERY( &m_db, CTBLChannelChart)
	query->m_bServerID = m_bServerID;

	if(query->Open())
	{
		while(query->Fetch())
		{
			MAPTCHANNEL::iterator itCH = m_mapTCHANNEL.find(query->m_bPhyChannel);

			if( itCH != m_mapTCHANNEL.end() )
				m_mapTLOGCHANNEL.insert( MAPTCHANNEL::value_type( MAKELONG( query->m_wMapID, WORD(query->m_bLogChannel)), (*itCH).second));
		}

		query->Close();
	}
	UNDEFINE_QUERY()

	DEFINE_QUERY( &m_db, CTBLSwitchChart)
	if(query->Open())
	{
		while(query->Fetch())
			for( itCH = m_mapTCHANNEL.begin(); itCH != m_mapTCHANNEL.end(); itCH++)
			{
				MAPTMAP::iterator itMAP = (*itCH).second->m_mapTMAP.find(query->m_wMapID);

				if( itMAP != (*itCH).second->m_mapTMAP.end() )
				{
					LPTSWITCH pTSWITCH = new TSWITCH();

					pTSWITCH->m_bLockOnClose = query->m_bLockOnClose;
					pTSWITCH->m_bLockOnOpen = query->m_bLockOnOpen;
					pTSWITCH->m_dwSwitchID = query->m_dwSwitchID;
					pTSWITCH->m_dwDuration = query->m_dwDuration;
					pTSWITCH->m_bOpened = query->m_bOpen;
					pTSWITCH->m_wMapID = query->m_wMapID;
					pTSWITCH->m_wPosX = query->m_wPosX;
					pTSWITCH->m_wPosY = query->m_wPosY;
					pTSWITCH->m_wPosZ = query->m_wPosZ;
					pTSWITCH->m_dwStartTime = 0;

					if(!(*itMAP).second->EnterMAP(pTSWITCH))
						delete pTSWITCH;
				}
			}

		query->Close();
	}
	UNDEFINE_QUERY()

	DEFINE_QUERY( &m_db, CTBLGateChart)
	if(query->Open())
	{
		while(query->Fetch())
			for( itCH = m_mapTCHANNEL.begin(); itCH != m_mapTCHANNEL.end(); itCH++)
			{
				MAPTMAP::iterator itMAP = (*itCH).second->m_mapTMAP.find(query->m_wMapID);

				if( itMAP != (*itCH).second->m_mapTMAP.end() )
				{
					LPTSWITCH pTSWITCH = (*itMAP).second->FindSwitch(query->m_dwSwitchID);

					if(pTSWITCH)
					{
						MAPTGATE::iterator itGATE = (*itMAP).second->m_mapTGATE.find(query->m_dwGateID);

						if( itGATE == (*itMAP).second->m_mapTGATE.end() )
						{
							LPTGATE pTGATE = new TGATE();

							pTGATE->m_dwSwitchID = query->m_dwSwitchID;
							pTGATE->m_dwGateID = query->m_dwGateID;
							pTGATE->m_bOpened = pTSWITCH->m_bOpened;
							pTGATE->m_wMapID = query->m_wMapID;
							pTGATE->m_wPosX = query->m_wPosX;
							pTGATE->m_wPosY = query->m_wPosY;
							pTGATE->m_wPosZ = query->m_wPosZ;
							pTGATE->m_bType = query->m_bType;

							if(!(*itMAP).second->EnterMAP(pTGATE))
								delete pTGATE;
							else
							{
								pTGATE->m_vSwitch.push_back(pTSWITCH);
								pTSWITCH->m_vGate.push_back(pTGATE);
							}
						}
						else
						{
							(*itGATE).second->m_vSwitch.push_back(pTSWITCH);
							pTSWITCH->m_vGate.push_back((*itGATE).second);
						}
					}
				}
			}

		query->Close();
	}
	UNDEFINE_QUERY()

	FLOAT f1stRateX = 0;
	m_mapTFORMULA.clear();
	DEFINE_QUERY( &m_db, CTBLFormulaChart)
	if(query->Open())
	{
		while(query->Fetch())
		{
			LPTFORMULA pTFORMULA = new TFORMULA();
			pTFORMULA->m_bID = query->m_bID;
			pTFORMULA->m_strName = query->m_szName;
			pTFORMULA->m_dwInit = query->m_dwInit;
			pTFORMULA->m_fRateX = query->m_fRateX;
			pTFORMULA->m_fRateY = query->m_fRateY;

			m_mapTFORMULA.insert(MAPTFORMULA::value_type(pTFORMULA->m_bID, pTFORMULA));

			if(pTFORMULA->m_bID == FTYPE_1ST)
				f1stRateX = pTFORMULA->m_fRateX;
		}

		query->Close();
	}
	UNDEFINE_QUERY()

	DEFINE_QUERY(&m_db, CTBLSkillChart);
	if(query->Open())
	{
		while(query->Fetch())
		{
			CTSkillTemp * pSkill = new CTSkillTemp();
			pSkill->m_wID = query->m_wID;
			pSkill->m_wPrevActiveID = query->m_wPrevActiveID;
			pSkill->m_wParentSkillID = query->m_wParentSkillID;
			pSkill->m_wMaxRange = query->m_wMaxRange;
			pSkill->m_wMinRange = query->m_wMinRange;
			pSkill->m_wPosture = query->m_wPosture;
			pSkill->m_dwConditionID = query->m_dwConditionID;
			pSkill->m_dwClassID = query->m_dwClassID;
			pSkill->m_bKind = query->m_bKind;
			pSkill->m_dwActionTime = query->m_dwActionTime;
			pSkill->m_dwDuration = query->m_dwDuration;
			pSkill->m_dwDurationInc = query->m_dwDurationInc;
			pSkill->m_dwReuseDelay = query->m_dwReuseDelay;
			pSkill->m_nReuseDelayInc = query->m_nReuseDelayInc;
			pSkill->m_dwLoopDelay = query->m_dwLoopDelay;
			pSkill->m_dwKindDelay = query->m_dwKindDelay;
			pSkill->m_dwAggro = query->m_dwAggro;
			pSkill->m_dwUseMP = query->m_dwUseMP;
			pSkill->m_dwUseHP = query->m_dwUseHP;
			pSkill->m_bStartLevel = query->m_bLevel;
			pSkill->m_bMaxLevel = query->m_bMaxLevel;
			pSkill->m_bNextLevel = query->m_bNextLevel;
			pSkill->m_bTarget = query->m_bTarget;
			pSkill->m_bTargetRange = query->m_bTargetRange;
			pSkill->m_bIsuse = query->m_bIsuse;
			pSkill->m_bTargetHit = query->m_bTargetHit;
			pSkill->m_fPrice = query->m_fPrice;
			pSkill->m_bPositive = query->m_bPositive;
			pSkill->m_bPriority = query->m_bPriority;
			pSkill->m_bSpeedApply = query->m_bSpeedApply;
			pSkill->m_dwWeapon = query->m_dwWeaponID;
			pSkill->m_bUseMPType = query->m_bUseMPType;
			pSkill->m_bUseHPType = query->m_bUseHPType;
			pSkill->m_bCanLearn = query->m_bCanLearn;
			pSkill->m_bORadius = query->m_bORadius;
			pSkill->m_bIsRide = query->m_bIsRide;
			pSkill->m_bIsDismount = query->m_bIsDismount;
			pSkill->m_wTargetActiveID = query->m_wTargetActiveID;
			pSkill->m_bMaintainType = query->m_bMaintainType;
			pSkill->m_bDuraSlot = query->m_bDuraSlot;
			pSkill->m_bCanCancel = query->m_bCanCancel;
			pSkill->m_bHitInit = query->m_bHitInit;
			pSkill->m_bHitInc = query->m_bHitInc;
			pSkill->m_bHitTest = query->m_bHitTest;
			pSkill->m_f1stRateX = f1stRateX;
			pSkill->m_wUseItem = query->m_wUseItem;
			pSkill->m_bGlobal = query->m_bGlobal;
			pSkill->m_bRadius = query->m_bRadius;
			pSkill->m_bStatic = query->m_bStatic;
			pSkill->m_bEraseAct = query->m_bEraseAct;
			pSkill->m_bEraseHide = query->m_bEraseHide;
			pSkill->m_bIsHideSkill = query->m_bIsHideSkill;
			pSkill->m_bRunFromServer = query->m_bRunFromServer;
			pSkill->m_bCheckAttacker = query->m_bCheckAttacker;
			pSkill->m_wTriggerID = query->m_wTriggerID;
			pSkill->m_wMapID = query->m_wMapID;
			pSkill->m_bRepeatCount = query->m_bRepeatCount;

			m_mapTSKILL.insert(MAPTSKILLTEMP::value_type(pSkill->m_wID, pSkill));
			if(pSkill->m_bKind == NORMAL_EQUIP)
				m_mapTEQUIPSKILL.insert(MAPTSKILLTEMP::value_type(pSkill->m_wID, pSkill));
		}
		query->Close();
	}
	UNDEFINE_QUERY();

	DEFINE_QUERY(&m_db, CTBLSkillPoint)
	if(query->Open())
	{
		while(query->Fetch())
		{
			MAPTSKILLTEMP::iterator itST = m_mapTSKILL.find(query->m_wSkillID);
			if(itST == m_mapTSKILL.end())
				continue;

			LPTSKILLPOINT pSPoint = new TSKILLPOINT();
			pSPoint->m_wSkillID = query->m_wSkillID;
			pSPoint->m_bLevel = query->m_bLevel;
			pSPoint->m_bSkillPoint = query->m_bSkillPoint;
			pSPoint->m_bGroupPoint = query->m_bGroupPoint;
			pSPoint->m_bParentLevel = query->m_bParentLevel;
			pSPoint->m_dwPayback = query->m_dwPayback;
			(*itST).second->m_mapTSkillPoint.insert(MAPTSKILLPOINT::value_type(pSPoint->m_bLevel, pSPoint));
		}
		query->Close();
	}
	UNDEFINE_QUERY()

	MAPTSKILLTEMP::iterator itSkill;
	for(itSkill = m_mapTSKILL.begin(); itSkill != m_mapTSKILL.end(); itSkill++)
	{
		CTSkillTemp * pSkill = (*itSkill).second;
		DEFINE_QUERY(&m_db, CTBLSkillData);
		query->m_wSkillID = pSkill->m_wID;
		if(query->Open())
		{
			while(query->Fetch())
			{
				LPTSKILLDATA pData = new TSKILLDATA();
				pData->m_bAction = query->m_bAction;
				pData->m_bType = query->m_bType;
				pData->m_bAttr = query->m_bAttr;
				pData->m_bExec = query->m_bExec;
				pData->m_bInc = query->m_bInc;
				pData->m_wValue = query->m_wValue;
				pData->m_wValueInc = query->m_wValueInc;
				pData->m_bCalc = query->m_bCalc;
				pSkill->m_vData.push_back(pData);
			}
			query->Close();
		}
		UNDEFINE_QUERY();
	}

	DEFINE_QUERY( &m_db, CTBLMonster)
	if(query->Open())
	{
		while(query->Fetch())
		{
			LPTMONSTER pMON = new TMONSTER();

			pMON->m_pAI = FindTMonsterAI(query->m_bAIType);
			pMON->m_wID = query->m_wID;

			pMON->m_wKind = query->m_wKind;
			pMON->m_bRoamProb = query->m_bRoamProb;
			pMON->m_bRange = query->m_bRange;
			pMON->m_wChaseRange = query->m_wChaseRange;
			pMON->m_bClass = query->m_bClass;
			pMON->m_bLevel = query->m_bLevel;
			pMON->m_bRace = query->m_bRace;
			pMON->m_bMoneyProb = query->m_bMoneyProb;
			pMON->m_dwMinMoney = query->m_dwMinMoney;
			pMON->m_dwMaxMoney = query->m_dwMaxMoney;
			pMON->m_bItemProb = query->m_bItemProb;
			pMON->m_bDropCount = query->m_bDropCount;
			pMON->m_wExp = query->m_wExp;
			pMON->m_bRecallType = query->m_bRecallType;
			pMON->m_bIsSelf = query->m_bIsSelf;
			pMON->m_bCanSelect = query->m_bCanSelect;
			pMON->m_bCanAttack = query->m_bCanAttack;
			pMON->m_bTame = query->m_bTame;
			pMON->m_bCall = query->m_bCall;
			pMON->m_bRemove = query->m_bRemove;
			pMON->m_wTransSkillID = query->m_wTransSkillID;
			pMON->m_fSize = query->m_fSize;
			pMON->m_bIsSpecial = query->m_bIsSpecial;
			pMON->m_wMonAttr = query->m_wMonAttr;
			pMON->m_wSummonAttr = query->m_wSummonAttr;
			pMON->m_bCheckPartyQuest = FALSE;


			for(int i=0; i<MAX_MON_SKILL; i++)
				pMON->m_vSKILL.push_back(query->m_wSkill[i]);

			m_mapTMONSTER.insert( MAPTMONSTERTEMP::value_type( pMON->m_wID, pMON));
		}

		query->Close();
	}
	UNDEFINE_QUERY()

	DEFINE_QUERY(&m_db, CTBLMonAttr)
	if(query->Open())
	{
		while(query->Fetch())
		{
			LPTMONATTR pMonAttr = new TMONATTR();
			pMonAttr->m_wID = query->m_wID;
			pMonAttr->m_bLevel = query->m_bLevel;
			pMonAttr->m_dwAttr = MAKELONG(pMonAttr->m_wID, pMonAttr->m_bLevel);
			pMonAttr->m_wAP = query->m_wAP;
			pMonAttr->m_wLAP = query->m_wLAP;
			pMonAttr->m_dwAtkSpeed = query->m_dwAtkSpeed;
			pMonAttr->m_wAL = query->m_wAL;
			pMonAttr->m_wMAL = query->m_wMAL;
			pMonAttr->m_wDL = query->m_wDL;
			pMonAttr->m_wMDL = query->m_wMDL;
			pMonAttr->m_bCriticalPP = query->m_bCriticalPP;
			pMonAttr->m_dwMaxHP = query->m_dwMaxHP;
			pMonAttr->m_bHPRecover = query->m_bHPRecover;
			pMonAttr->m_wMAP = query->m_wMAP;
			pMonAttr->m_bCriticalMP = query->m_bCriticalMP;
			pMonAttr->m_dwMaxMP = query->m_dwMaxMP;
			pMonAttr->m_bMPRecover = query->m_bMPRecover;
			pMonAttr->m_wDP = query->m_wDP;
			pMonAttr->m_wMDP = query->m_wMDP;
			pMonAttr->m_wMinWAP = query->m_wMinWAP;
			pMonAttr->m_wMaxWAP = query->m_wMaxWAP;
			pMonAttr->m_wWDP = query->m_wWDP;

			m_mapTMONATTR.insert(MAPTMONATTR::value_type(pMonAttr->m_dwAttr, pMonAttr));
		}
		query->Close();
	}
	UNDEFINE_QUERY()

			DEFINE_QUERY( &m_db, CTBLPet)
		if(query->Open())




		{




			while(query->Fetch())
			{
				LPTPETTEMP pPET = new TPETTEMP();
				pPET->m_bID = query->m_wID;



				pPET->m_wNMonID = query->m_wDefMonID;
				pPET->m_wUPMonID = query->m_wUpgMonID;





				m_mapTPET.insert( MAPTPETTEMP::value_type(pPET->m_bID, pPET));
			}
			query->Close();
		}
		UNDEFINE_QUERY()






	DEFINE_QUERY( &m_db, CTBLQuestMagicItemChart)
	query->m_item.Reset();
	if(query->Open())
	{
		while(query->Fetch())
		{
			CTItem * pItem = CreateItem(query->m_item);
			if(!pItem)
				continue;

			pItem->m_bCount = 1;
			pItem->m_dEndTime = query->m_wUseTime;
			pItem->m_bMagicDropLevel = query->m_bDropLevel;
			pItem->m_dwMoney = query->m_dwMoney;
            m_mapTQuestItem.insert(MAPTSTORAGEITEM::value_type(query->m_dwRewardID, pItem));			
		}
		query->Close();
	}
	UNDEFINE_QUERY()

	MAPTMONSTERTEMP::iterator itMon;
	for(itMon = m_mapTMONSTER.begin(); itMon != m_mapTMONSTER.end(); itMon++)
	{
		DEFINE_QUERY(&m_db, CTBLMonItem)
		query->m_wMonID = (*itMon).second->m_wID;
		if(query->Open())
		{
			while(query->Fetch())
			{
				LPTMONITEM pMonItem = new TMONITEM();
				pMonItem->m_bChartType = query->m_bChartType;

				if(!pMonItem->m_bChartType)
				{
					MAPTSTORAGEITEM::iterator itMI = m_mapTQuestItem.find(query->m_wItemID);
					if(itMI != m_mapTQuestItem.end())
						pMonItem->m_pMagicItem = (*itMI).second;
					else
						pMonItem->m_pMagicItem = NULL;
				}
				else
				{
					LPTITEM pTITEM = FindTItem(query->m_wItemID);
					if(pTITEM)
						pMonItem->m_pTITEM = pTITEM;
					else
						pMonItem->m_pTITEM = NULL;
				}

				pMonItem->m_wItemIDMin = query->m_wItemIDMin;
				pMonItem->m_wItemIDMax = query->m_wItemIDMax;
				pMonItem->m_wWeight	= query->m_wWeight;
				pMonItem->m_bLevelMin = query->m_bLevelMin;
				pMonItem->m_bLevelMax = query->m_bLevelMax;

				memcpy(pMonItem->m_bItemProb, query->m_bItemProb, sizeof(BYTE)*MIP_COUNT);
				pMonItem->m_bItemMagicOpt = query->m_bItemMagicOpt;
				pMonItem->m_bItemRareOpt = query->m_bItemRareOpt;
				(*itMon).second->m_dwMaxWeight += query->m_wWeight;
				(*itMon).second->m_vMONITEM.push_back(pMonItem);
			}
			query->Close();
		}
		UNDEFINE_QUERY();
	}

	m_mapExtraSpawnID.clear();
	for(WORD i = 0x8000; i < 0xFFFF; i++)
		m_mapExtraSpawnID.insert(MAPWORD::value_type(i,i));
#ifndef BR_COMPILE_MODE
	DEFINE_QUERY( &m_db, CTBLMonSpawn)
	MAPVMONSPAWN::iterator itSG;
	query->m_bServerID = m_bServerID;
	if(query->Open())
	{
		while(query->Fetch())
		{
			LPTMONSPAWN pSPAWN = new TMONSPAWN();

			pSPAWN->m_wID = query->m_wID;
			pSPAWN->m_lpvGroup = NULL;			
			pSPAWN->m_wGroup = 0;
			if(query->m_wGroup)
			{
				pSPAWN->m_wGroup = query->m_wGroup;
				itSG = m_mvSpawnGroup.find(query->m_wGroup);
				if(itSG!=m_mvSpawnGroup.end())
				{
					pSPAWN->m_lpvGroup = (*itSG).second;
					(*itSG).second->push_back(pSPAWN);
				}
				else
				{
					LPVTMONSPAWN pvGroup = new VTMONSPAWN();
					pvGroup->push_back(pSPAWN);
					pSPAWN->m_lpvGroup = pvGroup;
					m_mvSpawnGroup.insert(MAPVMONSPAWN::value_type(query->m_wGroup, pvGroup));
				}
			}

			pSPAWN->m_vMAPMON.clear();

			pSPAWN->m_wMapID = query->m_wMapID;
			pSPAWN->m_fPosX = query->m_fPosX;
			pSPAWN->m_fPosY = query->m_fPosY;
			pSPAWN->m_fPosZ = query->m_fPosZ;
			pSPAWN->m_wDir = query->m_wDir;
			pSPAWN->m_bCount = query->m_bCount;
			pSPAWN->m_bRange = query->m_bRange;
			pSPAWN->m_bArea = query->m_bArea;
			pSPAWN->m_bLink = query->m_bLink;
			pSPAWN->m_bProb = query->m_bProb;
			pSPAWN->m_bRoamType = query->m_bRoamType;
			pSPAWN->m_dwRegion = query->m_dwRegion;
			pSPAWN->m_dwDelay = query->m_dwDelay;
			pSPAWN->m_bEvent = query->m_bEvent;
			pSPAWN->m_wLocalID = query->m_wLocalID;
			pSPAWN->m_bCountry = query->m_bCountry;
			pSPAWN->m_wPartyID = query->m_wPartyID;

			m_mapTMONSPAWN.insert( MAPTMONSPAWNTEMP::value_type( pSPAWN->m_wID, pSPAWN));
			 
			MAPWORD::iterator itW = m_mapExtraSpawnID.find(pSPAWN->m_wID);
			if( itW != m_mapExtraSpawnID.end())
				m_mapExtraSpawnID.erase(itW);
		}

		query->Close();
	}
	UNDEFINE_QUERY()
#endif
	DEFINE_QUERY(&m_db, CTBLSpawnPath)
	if(query->Open())
	{
		while(query->Fetch())
		{
			MAPTMONSPAWNTEMP::iterator it = m_mapTMONSPAWN.find(query->m_wSpawnID);
			if(it!=m_mapTMONSPAWN.end())
			{
				LPTMONSPAWN pSpawn = (*it).second;
				if(pSpawn->m_vPathPoint.size() == 0)
				{
					LPTPATHPOINT pOriPath = new TPATHPOINT();
					pOriPath->m_fPosX = pSpawn->m_fPosX;
					pOriPath->m_fPosY = pSpawn->m_fPosY + 0.7f;
					pOriPath->m_fPosZ = pSpawn->m_fPosZ;
					pOriPath->m_bProb = 0;
					pOriPath->m_fRadius = query->m_fRadius;
					pSpawn->m_vPathPoint.push_back(pOriPath);
				}

				DWORD dwIndex = DWORD(pSpawn->m_vPathPoint.size()-1);
				DWORD dwDistX = DWORD(abs(pSpawn->m_vPathPoint[dwIndex]->m_fPosX - query->m_fPosX));
				DWORD dwDistZ = DWORD(abs(pSpawn->m_vPathPoint[dwIndex]->m_fPosZ - query->m_fPosZ));
				if(sqrt(double(dwDistX*dwDistX + dwDistZ*dwDistZ)) > MAX_ROAMRANGE-ZONE_HOMESIZE)
				{
					LPTPATHPOINT pPath = new TPATHPOINT();
					pPath->m_fPosX = min(pSpawn->m_vPathPoint[dwIndex]->m_fPosX, query->m_fPosX) + dwDistX/2;
					pPath->m_fPosY = query->m_fPosY;
					pPath->m_fPosZ = min(pSpawn->m_vPathPoint[dwIndex]->m_fPosZ, query->m_fPosZ) + dwDistZ/2;
					pPath->m_bProb = query->m_bProb;
					pPath->m_fRadius = query->m_fRadius;
					pSpawn->m_vPathPoint.push_back(pPath);
				}

				LPTPATHPOINT pPath = new TPATHPOINT();
				pPath->m_fPosX = query->m_fPosX;
				pPath->m_fPosY = query->m_fPosY;
				pPath->m_fPosZ = query->m_fPosZ;
				pPath->m_bProb = query->m_bProb;
				pPath->m_fRadius = query->m_fRadius;
				pSpawn->m_vPathPoint.push_back(pPath);
			}
		}
		query->Close();
	}
	UNDEFINE_QUERY()

	DEFINE_QUERY(&m_db, CTBLBattleZoneChart)
	if(query->Open())
	{
		while(query->Fetch())
		{
			LPTBATTLEZONE pZone = new TBATTLEZONE();
			pZone->m_wID = query->m_wID;
			pZone->m_strName = query->m_szName;
			pZone->m_wMapID = query->m_wMapID;
			pZone->m_wCastle = query->m_wCastle;
			pZone->m_wBossSpawnID = query->m_wBossSpawnID;
			pZone->m_wLGateKeeperSpawnID = query->m_wLGateKeeperSpawnID;
			pZone->m_wRGateKeeperSpawnID = query->m_wRGateKeeperSpawnID;
			pZone->m_wCGateKeeperSpawnID = query->m_wCGateKeeperSpawnID;
			pZone->m_dwLSwitchID = query->m_dwLSwitchID;
			pZone->m_dwRSwitchID = query->m_dwRSwitchID;
			pZone->m_dwCSwitchID = query->m_dwCSwitchID;
			pZone->m_bLine = query->m_bLine;
			pZone->m_wSkill1 = query->m_wSkill1;
			pZone->m_wSkill2 = query->m_wSkill2;
			pZone->m_bItemLevel = query->m_bItemLevel;
			pZone->m_wValorianBossDefend = query->m_wValorianBossDefend;
			pZone->m_wValorianBossAttack = query->m_wValorianBossAttack;
			pZone->m_wDerionBossDefned = query->m_wDerionBossDefned;
			pZone->m_wDerionBossAttack = query->m_wDerionBossAttack;
			pZone->m_wMiddleSpawnID = query->m_wMiddleSpawnID;
			pZone->m_wRightSpawnID = query->m_wRightSpawnID;
			pZone->m_wLeftSpawnID = query->m_wLeftSpawnID;

			MAPTITEMTEMP::iterator itI = m_mapTITEM.find(query->m_wNormalItem);
			if(itI != m_mapTITEM.end())
				pZone->m_pNormalItem = (*itI).second;
				
			else
				pZone->m_pNormalItem = NULL;

			itI = m_mapTITEM.find(query->m_wChiefItem);
			if(itI != m_mapTITEM.end())
				pZone->m_pChiefItem = (*itI).second;
			else
				pZone->m_pChiefItem = NULL;

			pZone->m_vSpawnID.clear();
			
			m_mapTBATTLEZONE.insert(MAPTBATTLEZONE::value_type(pZone->m_wID, pZone));
		}
		query->Close();
	}
	UNDEFINE_QUERY();

	DEFINE_QUERY(&m_db, CTBLMissionTable)
	if(query->Open())
	{
		while(query->Fetch())
		{
			MAPTBATTLEZONE::iterator itBz = m_mapTBATTLEZONE.find(query->m_wMissionID);
			if(itBz == m_mapTBATTLEZONE.end())
				continue;

			LPTLOCAL pLocal = new TLOCAL();
			pLocal->m_bType = LOCAL_MISSION;
			pLocal->m_wLocal = query->m_wMissionID;
			pLocal->m_bStatus = BS_NORMAL;
			pLocal->m_bCountry = query->m_bCountry;
			pLocal->m_dwGuild = 0;
			pLocal->m_wDPoint = 0;
			pLocal->m_dwAtkGuildID = 0;
			pLocal->m_dwDefGuildID = 0;
			pLocal->m_wCPoint = 0;
			pLocal->m_bDefCount = 0;
			pLocal->m_bAtkCount = 0;
			pLocal->m_bDefCountry = TCONTRY_N;
			pLocal->m_strAtkName = NAME_NULL;
			pLocal->m_strDefName = NAME_NULL;
			pLocal->m_bCanBattle = TRUE;
			pLocal->m_bOccupied = FALSE;
			pLocal->m_mapTPoint.clear();
			pLocal->m_mapKills.clear();

			pLocal->m_bMiddleOwner = NULL;
			pLocal->m_bLeftOwner = NULL;
			pLocal->m_bRightOwner = NULL;

			pLocal->m_pZone = (*itBz).second;
			pLocal->m_bValid = FALSE;

			pLocal->m_timeNextDefend = 0;
			pLocal->m_strHero = NAME_NULL;  
			pLocal->m_timeHero = 0;

			m_vMission.push_back(pLocal);
			m_mapTLOCAL.insert(MAPTLOCAL::value_type(pLocal->m_wLocal, pLocal));
		}
		query->Close();
	}
	UNDEFINE_QUERY()

	DEFINE_QUERY(&m_db, CTBLCastleTable)
	if(query->Open())
	{
		while(query->Fetch())
		{
			MAPTBATTLEZONE::iterator itBz = m_mapTBATTLEZONE.find(query->m_wCastle);
			if(itBz == m_mapTBATTLEZONE.end())
				continue;

			LPTLOCAL pLocal = new TLOCAL();
			pLocal->m_bType = LOCAL_CASTLE;
			pLocal->m_wLocal = query->m_wCastle;
			pLocal->m_bStatus = BS_NORMAL;
			pLocal->m_bCountry = query->m_bCountry;
			pLocal->m_dwGuild = query->m_dwGuild;
			pLocal->m_wDPoint = 0;
			pLocal->m_dwAtkGuildID = 0;
			pLocal->m_dwDefGuildID = 0;
			pLocal->m_wCPoint = 0;
			pLocal->m_bDefCount = 0;
			pLocal->m_bAtkCount = 0;
			pLocal->m_strAtkName = NAME_NULL;
			pLocal->m_strDefName = NAME_NULL;
			pLocal->m_bCanBattle = FALSE;
			pLocal->m_bDefCountry = TCONTRY_N;
			pLocal->m_bLastOccType = OCCUPY_DEFEND;
			pLocal->m_mapTPoint.clear();
			pLocal->m_mapKills.clear();

			pLocal->m_bMiddleOwner = NULL;
			pLocal->m_bLeftOwner = NULL;
			pLocal->m_bRightOwner = NULL;

			if(query->IsNull(3))
				pLocal->m_strGuild = NAME_NULL;
			else
				pLocal->m_strGuild = query->m_szGuildName;

			pLocal->m_pZone = (*itBz).second;
			pLocal->m_bValid = FALSE;

			pLocal->m_timeNextDefend = __DBTOTIME(query->m_dateWarTime);
			pLocal->m_strHero = query->m_szHero;  
			pLocal->m_timeHero = __DBTOTIME(query->m_dateHero);

			m_vCastle.push_back(pLocal);
			m_mapTLOCAL.insert(MAPTLOCAL::value_type(pLocal->m_wLocal, pLocal));
		}
		query->Close();
	}
	UNDEFINE_QUERY()
















































#ifdef SKYGARDEN
	DEFINE_QUERY(&m_db, CTBLSkygardenTable)
	if(query->Open())
	{
		while(query->Fetch())
		{
			MAPTBATTLEZONE::iterator itBz = m_mapTBATTLEZONE.find(query->m_wID);
			if(itBz == m_mapTBATTLEZONE.end())
				continue;

			LPTLOCAL pLocal = new TLOCAL();
			pLocal->m_bType = LOCAL_SKYGARDEN;
			pLocal->m_wLocal = query->m_wID;
			pLocal->m_bStatus = BS_NORMAL;
			pLocal->m_bCountry = query->m_bCountry;
			pLocal->m_dwGuild = 0;
			pLocal->m_wDPoint = 0;
			pLocal->m_dwAtkGuildID = 0;
			pLocal->m_dwDefGuildID = 0;
			pLocal->m_wCPoint = 0;
			pLocal->m_bDefCount = 0;
			pLocal->m_bAtkCount = 0;
			pLocal->m_strAtkName = NAME_NULL;
			pLocal->m_strDefName = NAME_NULL;
			pLocal->m_bCanBattle = TRUE;
			pLocal->m_bDefCountry = TCONTRY_N;
			pLocal->m_bLastOccType = OCCUPY_DEFEND;
			pLocal->m_mapTPoint.clear();
			pLocal->m_strGuild = NAME_NULL;
			pLocal->m_mapKills.clear();

			pLocal->m_bMiddleOwner = CAMP_DEFEND;
			pLocal->m_bLeftOwner = CAMP_DEFEND;
			pLocal->m_bRightOwner = CAMP_DEFEND;

			pLocal->m_pZone = (*itBz).second;
			pLocal->m_bValid = TRUE;

			pLocal->m_timeNextDefend = __DBTOTIME(query->m_dateWarTime);
			pLocal->m_strHero = NAME_NULL;  
			pLocal->m_timeHero = NULL;

			m_vSkygarden.push_back(pLocal);
			m_mapTLOCAL.insert(MAPTLOCAL::value_type(pLocal->m_wLocal, pLocal));
		}
		query->Close();
	}
	UNDEFINE_QUERY()
#endif


	DEFINE_QUERY(&m_db, CTBLGodTower)
	if(query->Open())
	{
		while(query->Fetch())
		{
			CTMap * pMap = FindTMap(DEFAULT_CHANNEL, 0, query->m_wMapID);
			if(pMap)
			{
				LPTGODTOWER pTower = new TGODTOWER();
				pTower->m_wID = query->m_wID;
				pTower->m_fPosX = query->m_fPosX;
				pTower->m_fPosY = query->m_fPosY;
				pTower->m_fPosZ = query->m_fPosZ;
				pTower->m_vSpawnID.clear();
				pTower->m_pGodBall = NULL;
				pTower->m_pMAP = pMap;
				pMap->EnterMAP(pTower);
			}
		}
		query->Close();
	}
	UNDEFINE_QUERY()

	DEFINE_QUERY(&m_db, CTBLGodBall)
	if(query->Open())
	{
		while(query->Fetch())
		{
			CTMap * pMap = FindTMap(DEFAULT_CHANNEL, 0, query->m_wMapID);
			if(pMap)
			{
				LPTGODBALL pBall = new TGODBALL();
				pBall->m_wID = query->m_wID;
				pBall->m_bCamp = query->m_bCamp;
				pBall->m_dwGuildID = 0;
				pBall->m_dwKEY = 0;
				pBall->m_fPosX = query->m_fPosX;
				pBall->m_fPosY = query->m_fPosY;
				pBall->m_fPosZ = query->m_fPosZ;
				pBall->m_pMAP = pMap;
				pBall->m_bMove = TRUE;
				pMap->m_mapTTEMPGODBALL.insert(MAPTGODBALL::value_type(pBall->m_wID, pBall));
			}
		}
		query->Close();
	}
	UNDEFINE_QUERY()

	DEFINE_QUERY(&m_db, CTBLLocalTable)
	if(query->Open())
	{
		while(query->Fetch())
		{
			MAPTBATTLEZONE::iterator itBz = m_mapTBATTLEZONE.find(query->m_wLocal);
			if(itBz == m_mapTBATTLEZONE.end())
				continue;

			LPTLOCAL pLocal = new TLOCAL();
			pLocal->m_bType = LOCAL_OCCUPATION;
			pLocal->m_wLocal = query->m_wLocal;
			pLocal->m_bCountry = query->m_bCountry;
			pLocal->m_dwGuild = query->m_dwGuild;
			pLocal->m_mapTPoint.clear();
			pLocal->m_mapKills.clear();

			if(query->IsNull(3))
				pLocal->m_strGuild = NAME_NULL;
			else
				pLocal->m_strGuild = query->m_szGuildName;

			pLocal->m_pZone = (*itBz).second;

			MAPTMONSPAWNTEMP::iterator itSp = m_mapTMONSPAWN.find(pLocal->m_pZone->m_wBossSpawnID);
			if(itSp==m_mapTMONSPAWN.end())
				pLocal->m_bValid = FALSE;
			else
				pLocal->m_bValid = TRUE;

			pLocal->m_bLastOccType = OCCUPY_DEFEND;
			pLocal->m_bOccupied = FALSE;
			pLocal->m_bGateOpened = FALSE;
			pLocal->m_bCanBattle = TRUE;
			pLocal->m_bStatus = BS_NORMAL;

			pLocal->m_timeOccupied = __DBTOTIME(query->m_dateOccupy);
			pLocal->m_timeNextDefend = __DBTOTIME(query->m_dateDefend);

			pLocal->m_strHero = query->m_szHero;  
			pLocal->m_timeHero = __DBTOTIME(query->m_dateHero);

			memset(pLocal->m_occupyGuild, 0, sizeof(pLocal->m_occupyGuild));
			memset(pLocal->m_occupyType, 0, sizeof(pLocal->m_occupyType));

			m_vOccupation.push_back(pLocal);
			m_mapTLOCAL.insert(MAPTLOCAL::value_type(pLocal->m_wLocal, pLocal));
			LPTLOCAL pCastle = FindCastle(pLocal->m_pZone->m_wCastle);
			if(pCastle)
				pCastle->m_mapLocal.insert(MAPTLOCAL::value_type(pLocal->m_wLocal, pLocal));
		}
		query->Close();
	}
	UNDEFINE_QUERY()

	DEFINE_QUERY(&m_db, CTBLLocalOccupy)
	if(query->Open())
	{
		while(query->Fetch())
		{
			LPTLOCAL pLocal = FindLocal(query->m_wLocalID);
			if(pLocal && query->m_bDay > 0)
			{
				pLocal->m_occupyGuild[query->m_bDay-1] = query->m_dwGuildID;
				pLocal->m_occupyType[query->m_bDay-1] = query->m_bType;
			}
		}
		query->Close();
	}
	UNDEFINE_QUERY()

	MAPTMONSPAWNTEMP::iterator itSPAWN;
	for( itSPAWN = m_mapTMONSPAWN.begin(); itSPAWN != m_mapTMONSPAWN.end(); itSPAWN++)
	{
		LPTMONSPAWN pSPAWN = (*itSPAWN).second;

		MAPTBATTLEZONE::iterator itBz = m_mapTBATTLEZONE.find(pSPAWN->m_wLocalID);
		if(itBz != m_mapTBATTLEZONE.end())
			(*itBz).second->m_vSpawnID.push_back(pSPAWN->m_wID);

		if(pSPAWN->m_wPartyID)
		{
			DEFINE_QUERY( &m_db, CTBLMonParty)
			query->m_wPartyID = pSPAWN->m_wPartyID;
			if(query->Open())
			{
				while(query->Fetch())
					pSPAWN->m_vPARTY.push_back(query->m_wSpawnID);
				query->Close();
			}
			UNDEFINE_QUERY();
		}

		DEFINE_QUERY( &m_db, CTBLMapMon)
		query->m_wSpawnID = pSPAWN->m_wID;
		if(query->Open())
		{
			while(query->Fetch())
			{
				LPTMAPMON pMON = new TMAPMON();

				pMON->m_wSpawnID = query->m_wSpawnID;
				pMON->m_wMonID = query->m_wMonID;
				pMON->m_bLeader = query->m_bLeader;

				pMON->m_bEssential = query->m_bEssential;
				pMON->m_bProb = query->m_bProb;
				pSPAWN->m_vMAPMON.push_back(pMON);
			}

			query->Close();
		}
		UNDEFINE_QUERY()

		if(pSPAWN->m_bEvent == SE_DEFAULT)
			for( itCH = m_mapTCHANNEL.begin(); itCH != m_mapTCHANNEL.end(); itCH++)
				(*itCH).second->AddMonSpawn( pSPAWN, pSPAWN->m_bCountry);
	}

#include "LoadQuestRoots.inc"

	DEFINE_QUERY(&m_db, CTBLSpawnPos);
	if(query->Open())
	{
		MAPTCHANNEL::iterator itCH;

		while(query->Fetch())
		{
			LPTSPAWNPOS pPOS = new TSPAWNPOS();

			pPOS->m_wMapID = query->m_wMapID;
			pPOS->m_fPosX = query->m_fPosX;
			pPOS->m_fPosY = query->m_fPosY;
			pPOS->m_fPosZ = query->m_fPosZ;
			pPOS->m_bType = query->m_bType;
			pPOS->m_wID = query->m_wID;

			for( itCH = m_mapTCHANNEL.begin(); itCH != m_mapTCHANNEL.end(); itCH++)
			{
				MAPTMAP::iterator itMAP = (*itCH).second->m_mapTMAP.find(pPOS->m_wMapID);

				if( itMAP != (*itCH).second->m_mapTMAP.end() )
					(*itMAP).second->m_mapTSPAWNPOS.insert( MAPTSPAWNPOS::value_type( pPOS->m_wMapID, pPOS));
			}

			m_mapTSPAWNPOS.insert( MAPTSPAWNPOS::value_type( pPOS->m_wID, pPOS));
		}

		query->Close();
	}
	UNDEFINE_QUERY();

	DEFINE_QUERY(&m_db, CTBLArena)
	if(query->Open())
	{
		while(query->Fetch())
		{
			MAPTSPAWNPOS::iterator itPos = m_mapTSPAWNPOS.find(query->m_wInPos);
			if(itPos!=m_mapTSPAWNPOS.end())
			{
				if(!IsMainCell(DEFAULT_CHANNEL, itPos->second->m_wMapID, itPos->second->m_fPosX, itPos->second->m_fPosZ))
					continue;
			}
			else
				continue;

			LPTARENA pArena = new TARENA();
			pArena->m_wID = query->m_wID;
			pArena->m_bType = query->m_bType;
			pArena->m_dwFee = query->m_dwFee;
			pArena->m_bStatus = BS_NORMAL;
			pArena->m_dwTick = 0;
			pArena->m_bWinCount = 0;
			pArena->m_bDrawCount = 0;
			pArena->m_wWinPartyID = 0;
			pArena->m_MaxWinCount = 0;
			pArena->m_TopPlayer.clear();

			for(BYTE i=0; i<2; i++)
				pArena->m_mapFighter[i].clear();

			pArena->m_pInPos = itPos->second;
			itPos = m_mapTSPAWNPOS.find(query->m_wOutPos);
			if(itPos != m_mapTSPAWNPOS.end())
				pArena->m_pOutPos = itPos->second;

			m_mapArena.insert(MAPARENA::value_type(pArena->m_wID, pArena));
		}
		query->Close();
	}
	UNDEFINE_QUERY()

	VWORD vAuctionNpc;  




	vAuctionNpc.clear();

	DEFINE_QUERY( &m_db, CTBLNpc)
	if(query->Open())
	{
		while(query->Fetch())
		{
			CTNpc *pNPC = new CTNpc();

			pNPC->m_wID		 = query->m_wID;
			pNPC->m_bType	 = query->m_bType;
			pNPC->m_bCountry = query->m_bCountry;
			pNPC->m_pLocal   = query->m_wLocalID ? FindOccupationZone(query->m_wLocalID) : NULL;
			pNPC->m_bDiscountCondition = query->m_bDiscountCondition;
			pNPC->m_bDiscountRate = query->m_bDiscountRate;  
			pNPC->m_bAddProb      = query->m_bAddProb;			
			pNPC->m_wItemID = query->m_wItemID;
			pNPC->m_wMapID = query->m_wMapID;  
			pNPC->m_fPosX = query->m_fPosX;
			pNPC->m_fPosY = query->m_fPosY;
			pNPC->m_fPosZ = query->m_fPosZ;

			m_mapTNpc.insert( MAPTNPC::value_type( pNPC->m_wID, pNPC));























			if(pNPC->m_bType == TNPC_AUCTION)
			{				
				WORD wUnitID =  MAKEWORD( BYTE(INT(pNPC->m_fPosX) / UNIT_SIZE), BYTE(INT(pNPC->m_fPosZ) / UNIT_SIZE));
				CTMap * pAuMap = FindTMap(DEFAULT_CHANNEL, 0, pNPC->m_wMapID);
				if(pAuMap)
				{
					MAPWORD::iterator itUT = pAuMap->m_mapTUNIT.find(wUnitID);
					if(itUT != pAuMap->m_mapTUNIT.end())
						vAuctionNpc.push_back(pNPC->m_wID);
				}
			}
		}

		query->Close();
	}

	UNDEFINE_QUERY()
	DEFINE_QUERY(&m_db, CTBLPortalChart)
	if(query->Open())
	{
		while(query->Fetch())
		{
			LPTPORTAL pPortal = new TPORTAL();
			pPortal->m_wPortalID = query->m_wPortalID;
			pPortal->m_wLocalID = query->m_wLocalID;
			pPortal->m_wSpawnID = query->m_wSpawnID;
			pPortal->m_bCondition = query->m_bCondition;
			pPortal->m_dwConditionID = 0;
			pPortal->m_bCountry = query->m_bCountry;
			MAPTLOCAL::iterator it = m_mapTLOCAL.find(query->m_wLocalID);
			if(it!=m_mapTLOCAL.end())
				pPortal->m_pLocal = (*it).second;
			else
				pPortal->m_pLocal = NULL;

			m_mapTPortal.insert(MAPTPORTAL::value_type(pPortal->m_wPortalID, pPortal));
		}
		query->Close();
	}
	UNDEFINE_QUERY();

	MAPTPORTAL::iterator itPt, itDest;
	for(itPt=m_mapTPortal.begin(); itPt!=m_mapTPortal.end(); itPt++)
	{
		DEFINE_QUERY(&m_db, CTBLDestinationChart)
		query->m_wPortalID = (*itPt).second->m_wPortalID;
		if(query->Open())
		{
			while(query->Fetch())
			{
				itDest = m_mapTPortal.find(query->m_wDestID);
				if(itDest != m_mapTPortal.end())
				{
					LPTDESTINATION pDes = new TDESTINATION();
					pDes->m_wDestID = query->m_wDestID;					
					pDes->m_dwPrice = query->m_dwPrice;
					pDes->m_bEnable = query->m_bEnable;					
					memcpy(pDes->m_bCondition, query->m_bCondition, sizeof(BYTE)*PORTALCONDITION_COUNT);
					memcpy(pDes->m_dwConditionID, query->m_dwConditionID, sizeof(DWORD)*PORTALCONDITION_COUNT);
					pDes->m_pPortal = (*itDest).second;
					(*itPt).second->m_mapDestination.insert(MAPTDESTINATION::value_type(pDes->m_wDestID, pDes));
				}
			}
			query->Close();
		}
		UNDEFINE_QUERY()
	}

	MAPTNPC::iterator itNpc;
	for(itNpc=m_mapTNpc.begin(); itNpc!=m_mapTNpc.end(); itNpc++)
	{
		DEFINE_QUERY(&m_db, CTBLNpcItem)
		query->m_wNpcID = (*itNpc).second->m_wID;
		if(query->Open())
		{
			while(query->Fetch())
			{
				CTNpc * pNpc = (*itNpc).second;
				switch(pNpc->m_bType)
				{
				case TNPC_SKILL_RENT:
				case TNPC_SKILL_MASTER:
					{
						MAPTSKILLTEMP::iterator itSkillTemp = m_mapTSKILL.find((WORD)query->m_dwItemID);
						if(itSkillTemp != m_mapTSKILL.end())
							pNpc->m_mapSkill.insert(MAPTSKILLTEMP::value_type((WORD)query->m_dwItemID, (*itSkillTemp).second));
					}
					break;
				case TNPC_PVPOINT:
				case TNPC_ITEM:
					{
						MAPTITEMTEMP::iterator itItemTemp = m_mapTITEM.find((WORD)query->m_dwItemID);
						if(itItemTemp != m_mapTITEM.end())
							pNpc->m_mapItem.insert(MAPTITEMTEMP::value_type((WORD)query->m_dwItemID, (*itItemTemp).second));
					}
					break;
				case TNPC_PORTAL:
					{
						MAPTPORTAL::iterator itPt = m_mapTPortal.find(WORD(query->m_dwItemID));
						if(itPt != m_mapTPortal.end())
							pNpc->m_pPortal = (*itPt).second;
					}
					break;
				case TNPC_RETURN:
					{
						pNpc->m_wSpawnPosID = WORD(query->m_dwItemID);
					}
					break;
				case TNPC_ARENA:
					{
						WORD wUnitID =  MAKEWORD( BYTE(INT(pNpc->m_fPosX) / UNIT_SIZE), BYTE(INT(pNpc->m_fPosZ) / UNIT_SIZE));
						CTMap * pArenaMap = FindTMap(DEFAULT_CHANNEL, 0, pNpc->m_wMapID);
						if(pArenaMap)
						{
							MAPWORD::iterator itUT = pArenaMap->m_mapTUNIT.find(wUnitID);
							if(itUT != pArenaMap->m_mapTUNIT.end())
							{
								MAPARENA::iterator itArn = m_mapArena.find(WORD(query->m_dwItemID));
								if(itArn != m_mapArena.end())
									pNpc->m_pArena = itArn->second;
							}
						}
					}
					break;
				case TNPC_MAGICITEM:
					{
						MAPTSTORAGEITEM::iterator itItemTemp = m_mapTQuestItem.find(query->m_dwItemID);
						if(itItemTemp != m_mapTQuestItem.end())
						{
							CTItem* pMagicItem = (*itItemTemp).second;
							pNpc->m_mapMagicItem.insert(MAPTSTORAGEITEM::value_type(query->m_dwItemID, pMagicItem));
						}
					}
					break;
				}
			}
			query->Close();
		}
		UNDEFINE_QUERY();
	}

	DEFINE_QUERY(&m_db, CTBLMonsterShop)
	if(query->Open())
	{
		while(query->Fetch())
		{
			CTNpc * pNpc = FindTNpc(query->m_wNpcID);
			if(pNpc)
			{
				MAPTMONSTERSHOP::iterator itMS = pNpc->m_mapMon.find(query->m_wID);
				if(itMS == pNpc->m_mapMon.end())
				{
					MAPTMONSPAWNTEMP::iterator itSp = m_mapTMONSPAWN.find(query->m_wSpawnID);
					if(itSp!=m_mapTMONSPAWN.end())
					{
						BYTE bMonLevel = 0;
						if(!(*itSp).second->m_vMAPMON.empty())
						{
							LPTMONSTER pMapMon = FindTMonster((*itSp).second->m_vMAPMON[0]->m_wMonID);
							if(pMapMon)
								bMonLevel = pMapMon->m_bLevel;
						}

						if(query->m_wTowerID && bMonLevel > m_bMaxLevel)
							continue;

						LPTMONSTERSHOP pMon = new TMONSTERSHOP();
						pMon->m_wID = query->m_wID;
						pMon->m_dwPrice = query->m_dwPrice;
						pMon->m_wTowerID = query->m_wTowerID;
						pMon->m_pSpawn = (*itSp).second;
						pNpc->m_mapMon.insert(MAPTMONSTERSHOP::value_type(pMon->m_wID, pMon));

						if(pMon->m_wTowerID)
						{
							CTMap * pMap = FindTMap(DEFAULT_CHANNEL, 0, pMon->m_pSpawn->m_wMapID);
							if(pMap)
							{
								LPTGODTOWER pTower = pMap->FindGodTower(pMon->m_wTowerID);
								if(pTower)
									pTower->m_vSpawnID.push_back(query->m_wSpawnID);
							}
						}
					}
				}
			}
		}
		query->Close();
	}
	UNDEFINE_QUERY()

	MAPQUESTTEMP::iterator itQu;
	for(itQu=m_mapQUESTTEMP.begin(); itQu!=m_mapQUESTTEMP.end(); itQu++)
	{
		LPQUESTTEMP pQuest = (*itQu).second;
		if(pQuest->m_bTriggerType == TT_TALKNPC)
		{
			MAPTNPC::iterator itNpc = m_mapTNpc.find((WORD)pQuest->m_dwTriggerID);
			if(itNpc != m_mapTNpc.end())
				(*itNpc).second->m_mapQuest.insert(MAPQUESTTEMP::value_type(pQuest->m_dwQuestID, pQuest));
		}
	}

	DEFINE_QUERY( &m_db, CTBLOperatorChart)
	if(query->Open())
	{
		while(query->Fetch())
            m_mapTOPERATOR.insert(MAPDWORD::value_type(query->m_dwOperator, query->m_dwOperator));




















		query->Close();
	}
	UNDEFINE_QUERY()

	DEFINE_QUERY( &m_db, CTBLPvPointChart)
	if(query->Open())
	{
		while(query->Fetch())
		{
			LPTPVPOINT pPvP = new TPVPOINT();
			pPvP->m_wID = MAKEWORD(query->m_bStatus, query->m_bEvent);
			pPvP->m_bTarget = query->m_bTarget;
			pPvP->m_dwIncPoint = query->m_dwIncPoint;
			pPvP->m_dwDecPoint = query->m_dwDecPoint;
			WORD wLocalID = query->m_wLocalID;

			if(wLocalID)
			{
				LPTBATTLEZONE pBattle = FindBattleZone(query->m_wLocalID);
				if(pBattle)
					pBattle->m_mapTPvPoint.insert(MAPTPVPOINT::value_type(pPvP->m_wID, pPvP));
				else
					delete pPvP;
			}
			else
				m_mapTPvPointKill.insert(MAPTPVPOINT::value_type(pPvP->m_wID, pPvP));
		}

		query->Close();
	}
	UNDEFINE_QUERY()

	DEFINE_QUERY( &m_db, CTBLCompanionBonusChart)
	if(query->Open())
	{
		while(query->Fetch())
		{
			LPTCOMPBONUS pBONUS = new TCOMPBONUS();

			pBONUS->m_bBonusID = query->m_bBonusID;
			pBONUS->m_fBase = query->m_fBase;
			pBONUS->m_fMultiplier = query->m_fMultiplier;

			m_mapCompBonus.insert(MAPTCOMPBONUS::value_type(query->m_bBonusID, pBONUS));
		}			

		query->Close();

	}
	UNDEFINE_QUERY()
	
	MAPTAUCTIONBIDDER mapTAUCTIONBIDDER;
	mapTAUCTIONBIDDER.clear();

	DEFINE_QUERY(&m_db, CTBLALLAuctionBidder)
	if(query->Open())
	{
		while(query->Fetch())
		{
			TAUCTIONBIDDER stTAUCTIONBIDDER;
			stTAUCTIONBIDDER.m_dwAuctionID = query->m_dwAuctionID;
			stTAUCTIONBIDDER.m_dwCharID = query->m_dwCharID;
			stTAUCTIONBIDDER.m_ldwBidPrice = query->m_ldwBidPrice;
			stTAUCTIONBIDDER.m_dBidDate = __DBTOTIME(query->m_dBid);

			mapTAUCTIONBIDDER.insert(MAPTAUCTIONBIDDER::value_type(stTAUCTIONBIDDER.m_dwAuctionID, stTAUCTIONBIDDER));			
		}
		query->Close();
	}
	UNDEFINE_QUERY()






	for(WORD w = 0; w < (WORD)vAuctionNpc.size(); w++)  
	{	
		__time64_t CurTime = CTime::GetCurrentTime().GetTime();
		DEFINE_QUERY(&m_db, CTBLAuctionTable)
		query->m_wNpcID = vAuctionNpc[w];
		if(query->Open())



		{



















































			while(query->Fetch())
			{			
				CTNpc * pNpc = FindTNpc(query->m_wNpcID);
				if(pNpc)
				{





					LPTAUCTION pAuction = new TAUCTION();
					pAuction->m_wNpcID = query->m_wNpcID;
					pAuction->m_bBidCount = query->m_bBidCount;
					pAuction->m_dwAuctionID = query->m_dwAuctionID;
					pAuction->m_dwSeller = query->m_dwSeller;
					pAuction->m_strSeller = query->m_szSeller;
					pAuction->m_ldwDirectPrice = query->m_ldwDirectPrice;
					pAuction->m_ldwStartPrice =	query->m_ldwStartPrice;






					pAuction->m_ldwBidPrice = 0;









					pAuction->m_dStart = __DBTOTIME(query->m_dStart);
					pAuction->m_dEnd = __DBTOTIME(query->m_dEnd);				

					if( pAuction->m_dEnd > CurTime)
						pAuction->m_dwRemainTime = (DWORD)(pAuction->m_dEnd - CurTime);
					else
						pAuction->m_dwRemainTime = 0;

					MAPTAUCTIONBIDDER::iterator itB = mapTAUCTIONBIDDER.find(query->m_dwAuctionID);
					if(itB != mapTAUCTIONBIDDER.end())
					{
						pAuction->m_ldwBidPrice = (*itB).second.m_ldwBidPrice;
						pAuction->m_dBid = (*itB).second.m_dBidDate;
						pAuction->m_dwBidder = (*itB).second.m_dwCharID;
					}
					else
					{
						pAuction->m_ldwBidPrice = pAuction->m_ldwStartPrice;
						pAuction->m_dwBidder = 0;
					}



					CTItem *pTITEM = CreateItem(query->m_item);				
					if(!pTITEM)
					{
						delete pAuction;
						LogEvent("Auction Item Error: %d",pAuction->m_dwAuctionID);
						continue;
					}

					pAuction->m_dwCode = pTITEM->m_pTITEM->m_dwCode;

					pTITEM->m_dlID = query->m_item.m_dlID;
					pAuction->m_pItem = pTITEM;

					if(pNpc->AddAuction(pAuction->m_dwAuctionID,pAuction))
						SendSM_AUCTIONCMD_REQ(pAuction->m_wNpcID,pAuction->m_dwAuctionID,pAuction->m_dEnd);

				}














				else			
					LogEvent(_T("Invalid Auction NPC"));			
			}




			query->Close();
		}



		UNDEFINE_QUERY()
	}




	vAuctionNpc.clear();

	BYTE bMonthRank = TOTALMONTHRANKCOUNT;
	DEFINE_QUERY(&m_db,CTBLMonthRankTable)	
	if(query->Open())
	{
		while(query->Fetch())
		{
			if(query->m_bMonthRank >= FAMERANKCOUNT || query->m_bMonthRank == bMonthRank)
				continue;

			m_arFameRank[FRT_HERO][query->m_bMonth][query->m_bMonthRank].m_dwCharID = query->m_dwCharID;
			m_arFameRank[FRT_HERO][query->m_bMonth][query->m_bMonthRank].m_dwMonthRank = query->m_bMonthRank;
			m_arFameRank[FRT_HERO][query->m_bMonth][query->m_bMonthRank].m_dwTotalRank = query->m_dwTotalRank;
			m_arFameRank[FRT_HERO][query->m_bMonth][query->m_bMonthRank].m_dwTotalPoint = query->m_dwTotalPoint;
			m_arFameRank[FRT_HERO][query->m_bMonth][query->m_bMonthRank].m_dwMonthPoint = query->m_dwMonthPoint;
			m_arFameRank[FRT_HERO][query->m_bMonth][query->m_bMonthRank].m_wMonthWin = query->m_wMonthWin;
			m_arFameRank[FRT_HERO][query->m_bMonth][query->m_bMonthRank].m_wMonthLose = query->m_wMonthLose;
			m_arFameRank[FRT_HERO][query->m_bMonth][query->m_bMonthRank].m_dwTotalWin = query->m_dwTotalWin;
			m_arFameRank[FRT_HERO][query->m_bMonth][query->m_bMonthRank].m_dwTotalLose = query->m_dwTotalLose;
			m_arFameRank[FRT_HERO][query->m_bMonth][query->m_bMonthRank].m_bCountry = query->m_bCountry;
			m_arFameRank[FRT_HERO][query->m_bMonth][query->m_bMonthRank].m_bLevel = query->m_bLevel;
			m_arFameRank[FRT_HERO][query->m_bMonth][query->m_bMonthRank].m_bClass = query->m_bClass;
			m_arFameRank[FRT_HERO][query->m_bMonth][query->m_bMonthRank].m_bRace = query->m_bRace;
			m_arFameRank[FRT_HERO][query->m_bMonth][query->m_bMonthRank].m_bSex = query->m_bSex;
			m_arFameRank[FRT_HERO][query->m_bMonth][query->m_bMonthRank].m_bFace = query->m_bFace;
			m_arFameRank[FRT_HERO][query->m_bMonth][query->m_bMonthRank].m_bHair = query->m_bHair;
			m_arFameRank[FRT_HERO][query->m_bMonth][query->m_bMonthRank].m_strName = query->m_szName;
			m_arFameRank[FRT_HERO][query->m_bMonth][query->m_bMonthRank].m_strSay = query->m_szSay;
			m_arFameRank[FRT_HERO][query->m_bMonth][query->m_bMonthRank].m_strGuild = query->m_szGuild;

			bMonthRank = query->m_bMonthRank;
		}
		query->Close();
	}
	UNDEFINE_QUERY()

	DEFINE_QUERY(&m_db,CTBLHeroTable)	
	if(query->Open())
	{
		while(query->Fetch())
		{
			if(query->m_bType >= FRT_COUNT || query->m_bMonth >= MONTHCOUNT || query->m_bOrder >= FAMERANKCOUNT)
				continue;

			m_arFameRank[query->m_bType][query->m_bMonth][query->m_bOrder].m_dwCharID = query->m_dwCharID;
			m_arFameRank[query->m_bType][query->m_bMonth][query->m_bOrder].m_dwMonthRank = query->m_dwMonthRank;
			m_arFameRank[query->m_bType][query->m_bMonth][query->m_bOrder].m_dwTotalRank = query->m_dwTotalRank;
			m_arFameRank[query->m_bType][query->m_bMonth][query->m_bOrder].m_dwTotalPoint = query->m_dwTotalPoint;
			m_arFameRank[query->m_bType][query->m_bMonth][query->m_bOrder].m_dwMonthPoint = query->m_dwMonthPoint;
			m_arFameRank[query->m_bType][query->m_bMonth][query->m_bOrder].m_wMonthWin = query->m_wMonthWin;
			m_arFameRank[query->m_bType][query->m_bMonth][query->m_bOrder].m_wMonthLose = query->m_wMonthLose;
			m_arFameRank[query->m_bType][query->m_bMonth][query->m_bOrder].m_dwTotalWin = query->m_dwTotalWin;
			m_arFameRank[query->m_bType][query->m_bMonth][query->m_bOrder].m_dwTotalLose = query->m_dwTotalLose;
			m_arFameRank[query->m_bType][query->m_bMonth][query->m_bOrder].m_bCountry = query->m_bCountry;
			m_arFameRank[query->m_bType][query->m_bMonth][query->m_bOrder].m_bLevel = query->m_bLevel;
			m_arFameRank[query->m_bType][query->m_bMonth][query->m_bOrder].m_bClass = query->m_bClass;
			m_arFameRank[query->m_bType][query->m_bMonth][query->m_bOrder].m_bRace = query->m_bRace;
			m_arFameRank[query->m_bType][query->m_bMonth][query->m_bOrder].m_bSex = query->m_bSex;
			m_arFameRank[query->m_bType][query->m_bMonth][query->m_bOrder].m_bFace = query->m_bFace;
			m_arFameRank[query->m_bType][query->m_bMonth][query->m_bOrder].m_bHair = query->m_bHair;
			m_arFameRank[query->m_bType][query->m_bMonth][query->m_bOrder].m_strName = query->m_szName;
			m_arFameRank[query->m_bType][query->m_bMonth][query->m_bOrder].m_strSay = query->m_szSay;
			m_arFameRank[query->m_bType][query->m_bMonth][query->m_bOrder].m_strGuild = query->m_szGuild;
		}
		query->Close();
	}
	UNDEFINE_QUERY()

	DEFINE_QUERY(&m_db,CTBLFirstGradeGroup)
	CTime t = CTime::GetCurrentTime().GetTime();
	BYTE bRankMonth = t.GetMonth();
	bRankMonth--;
	if(bRankMonth == 0)
		bRankMonth = 12;

	query->m_bMonth = bRankMonth;
	
	if(query->Open())
	{
		while(query->Fetch())
		{
			if(query->m_bCountry >= COUNTRY_COUNT || query->m_bRank >= FIRSTGRADEGROUPCOUNT)
				continue;

			m_arFirstGradeGroup[query->m_bCountry][query->m_bRank].m_dwMonthRank = query->m_bRank;
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

DEFINE_QUERY( &m_db, CTBLInvenTournametChart)
	if(query->Open())
	{
		while(query->Fetch())
		{
			LPTTNMTINVEN pTINVEN = new TTNMTINVEN();

			pTINVEN->m_bInven = query->m_bInvenID;
			pTINVEN->m_wItemID = query->m_wItemID;
			pTINVEN->m_bELD = query->m_bELD;
			pTINVEN->m_dEndTime = NULL;
			
			m_mapTTNMTINVEN.insert( MAPTTNMTINVEN::value_type( query->m_bInvenID, pTINVEN));
		}

		query->Close();
	}
	UNDEFINE_QUERY()

	DEFINE_QUERY( &m_db, CTBLItemTournamentChart)
	if(query->Open())
	{
		while(query->Fetch())
		{
			LPTTNMTITEM pTITEM = new TTNMTITEM();
			pTITEM->m_pItem = CreateItem(query->m_item);

			if(!pTITEM->m_pItem)
				continue;

			pTITEM->m_bInvenID = query->m_bInvenID;
			pTITEM->m_pItem->m_bItemID = query->m_item.m_bItemID;
			pTITEM->m_pItem->m_dEndTime = NULL;
			pTITEM->m_pItem->m_dwDuraCur = query->m_item.m_dwDuraMax;
			pTITEM->m_pItem->m_bRefineCur = NULL;

			m_vTTNMTITEM[query->m_bClass].push_back(pTITEM);
		}

		query->Close();
	}
UNDEFINE_QUERY();

	DEFINE_QUERY( &m_db, CTBLPremiumSkillChart)
	if(query->Open())
	{
		while(query->Fetch())
		{
#ifdef BR_COMPILE_MODE
			if (query->m_wSkillID == 1507)
				continue;
#endif
			LPPREMIUMSKILL pSKILL = new PREMIUMSKILL();
			pSKILL->m_wSkillID = query->m_wSkillID;
			pSKILL->m_wMedals = query->m_wMedals;
			pSKILL->m_bType = query->m_bType;

#ifdef BOW_COMPILE_MODE
			m_vPREMIUMSKILL[query->m_bType].push_back(pSKILL);
#else
			//if (query->m_bType == PREMIUMSKILL_BOW)
			//{
				MAPTSKILLTEMP::iterator itSkill = m_mapTSKILL.find(query->m_wSkillID);
				if (itSkill != m_mapTSKILL.end())
				{
					delete (*itSkill).second;
					m_mapTSKILL.erase(itSkill);
				}

				continue;
			//}

			//m_vPREMIUMSKILL[query->m_bType].push_back(pSKILL);
#endif
		}

#ifdef BR_COMPILE_MODE
			LPPREMIUMSKILL pSKILL = new PREMIUMSKILL();
			pSKILL->m_wSkillID = 1599;
			pSKILL->m_wMedals = 0;
			pSKILL->m_bType = PREMIUMSKILL_BOW;

			m_vPREMIUMSKILL[PREMIUMSKILL_BOW].push_back(pSKILL);
#endif
		query->Close();
	}
	UNDEFINE_QUERY();

	DEFINE_QUERY( &m_db, CTBLBattleRankChart);
	if(query->Open())
	{
		while(query->Fetch())
		{
			LPBATTLERANK pRANK = new BATTLERANK();
			pRANK->m_bRank = query->m_bRank;
			pRANK->m_dwPoints = query->m_dwPoints;

			m_vBATTLERANK.push_back(pRANK);
		}

		query->Close();
	}
	UNDEFINE_QUERY();

	DEFINE_QUERY( &m_db, CTBLSpecialBoxChart);
	if(query->Open())
	{
		while(query->Fetch())
		{
			LPSPECIALBOX pBOX = new SPECIALBOX();
			pBOX->m_bGroup = (BYTE) query->m_wID;
			pBOX->m_wID = query->m_wGroup;
			pBOX->m_pITEM = CreateItem(query->m_item);
			pBOX->m_wUseTime = query->m_wUseTime;
			pBOX->m_bClass = query->m_bClass;
			if (!pBOX->m_pITEM) {
				delete pBOX;
				continue;
			}

			MAPSPECIALBOX::iterator it = m_mapSpecialBox.find(pBOX->m_wID);
			if (it == m_mapSpecialBox.end())
			{
				VSBITEM vItems;
				vItems.push_back(pBOX);
				m_mapSpecialBox.insert(MAPSPECIALBOX::value_type(pBOX->m_wID, vItems));
			}
			else
				(*it).second.push_back(pBOX);
		}

		query->Close();
	}
	UNDEFINE_QUERY();
































































































































	DEFINE_QUERY(&m_db, CTBLGuildSkillChart);
	if (query->Open())
	{
		while (query->Fetch())
		{
			GUILDSKILL GuildSkill =
			{
				query->m_wSkillID,
				query->m_bType
			};

			m_mapGuildSkill.insert(std::make_pair(query->m_wSkillID, GuildSkill));
		}

		query->Close();
	}
	UNDEFINE_QUERY()

	DEFINE_QUERY(&m_db, CTBLAccessoryMagic);
	if (query->Open())
	{
		while (query->Fetch())
		{
			ACCESSORYMAGIC Magic =
			{
				query->m_ID,
				query->m_MagicID,
				query->m_MinValue,
				query->m_MaxValue
			};

			m_vAccessoryMagic.push_back(Magic);
		}

		query->Close();
	}
	UNDEFINE_QUERY()
#ifdef BOW_COMPILE_MODE
	LoadBOWData();
#endif
#ifdef BR_COMPILE_MODE
	LoadBRData();
#endif

	return EC_NOERROR;
