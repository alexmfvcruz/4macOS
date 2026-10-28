// Original Login module startup data body.
	DEFINE_QUERY(&m_db, CSPLoadService)
	query->m_bWorld = SVRGRP_NULL;
	query->m_bServiceGroup = SVRGRP_CTLSVR;
	if(!query->Call())
		return EC_INITSERVICE_DBOPENFAILED;

	m_addrCtrl.sin_addr.s_addr = inet_addr(query->m_szIP);
	m_addrCtrl.sin_port = query->m_wPort;
	UNDEFINE_QUERY();

	DEFINE_QUERY(&m_db, CSPGetNation)
	if(!query->Call())
		return EC_INITSERVICE_NATION;
	m_bNation = query->m_bNation;
	if(!m_bNation)
		return EC_INITSERVICE_NATION;
	UNDEFINE_QUERY()

	DEFINE_QUERY( &m_db, CTBLUserCount)
	if(query->Open())
	{
		while(query->Fetch())
			m_mapCurrentUser.insert(MAPDWORD::value_type(query->m_bGroup, query->m_dwCount));
		query->Close();
	}
	UNDEFINE_QUERY()

	DEFINE_QUERY( &m_db, CTBLVeteranChart)
	if(query->Open())
	{
		while(query->Fetch())
		{
			TVETERAN Veteran;

			Veteran.m_bOption = query->m_bID;
			Veteran.m_bLevel = query->m_bLevel;

			m_vVETERAN.push_back(Veteran);
		}

		query->Close();
	}
	UNDEFINE_QUERY()

#include "ClientReference.inc"

	return EC_NOERROR;
