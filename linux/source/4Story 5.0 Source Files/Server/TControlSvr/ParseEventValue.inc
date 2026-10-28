	int nPos = 0;
	int nPos2 = 0;
	int nV1,nV2,nLength;

	BYTE bCount = 0;

	CString strTok;  
	CString strTok2;
	CString str1;
	CString str2;
	CString str3;

	if(bEventID == EVENT_CASHSALE)
	{
		strTok = str.Tokenize(_T(";"),nPos);
		while(strTok != _T(""))
		{
			nLength = strTok.GetLength();
			nV1 = strTok.Find(_T("-"));
			str1 = strTok.Left(nV1);
			str2 = strTok.Right(nLength - nV1-1);

			TCASHITEMSALE stSALE;
			stSALE.m_wID = atoi(str1);
			stSALE.m_bSaleValue = atoi(str2);

			stEVENTINFO.m_vCashItem.push_back(stSALE);

			strTok = str.Tokenize(_T(";"),nPos);
		}
	}
	else if( bEventID == EVENT_MONSPAWN)
	{
		strTok = str.Tokenize(_T(";"),nPos);
		WORD wSpawnID;
		while(strTok != _T(""))
		{
			wSpawnID = atoi(strTok);

			stEVENTINFO.m_stMONEVENT.m_vSpawnID.push_back(wSpawnID);

			strTok = str.Tokenize(_T(";"),nPos);			
		}
	}
	else if( bEventID == EVENT_MONREGEN)  
	{		
		strTok = str.Tokenize(_T(";"),nPos);
		while(strTok != _T("") )
		{
			bCount = 0;
			nPos2 = 0;
			MONREGEN stMONREGEN;

			strTok2 = strTok.Tokenize(_T("-"),nPos2);
			while(strTok2 != _T(""))
			{
				switch(bCount)
				{
				case 0:	stMONREGEN.m_wMonID = atoi(strTok2);	break;
				case 1: stMONREGEN.m_dwDelay = atoi(strTok2);	break;
				case 2: stMONREGEN.m_wMapID = atoi(strTok2);	break;
				case 3:	stMONREGEN.m_fPosX = (FLOAT)atof(strTok2);		break;
				case 4: stMONREGEN.m_fPosY = (FLOAT)atof(strTok2);		break;
				case 5: stMONREGEN.m_fPosZ = (FLOAT)atof(strTok2);		break;
				}

				bCount++;
				strTok2 = strTok.Tokenize(_T("-"),nPos2);
			}
			stEVENTINFO.m_vMONREGEN.push_back(stMONREGEN);

			strTok = str.Tokenize(_T(";"),nPos);
		}
	}
	else if( bEventID == EVENT_LOTTERY)
	{
		//	reword(IDxNUM-win;)'|'title'|'message
		//	ex) 1203x1-10;55x1-1;|당첨|축하드립니다!

		//	보상
		nPos2 = 0;
		strTok = str.Tokenize(_T("|"), nPos);
		strTok2 = strTok.Tokenize(_T(";"), nPos2);
		while(strTok2 != _T(""))
		{
			TLOTTERY tLot;
			nV1 = 0;
			nV2 = 0;

			nLength = strTok2.GetLength();
			nV1 = strTok.Find(_T("x"));
			str1 = strTok.Left(nV1);
			nV2 = strTok.Find(_T("-"));
			str2 = strTok.Mid(nV1+1, nV2-nV1-1);
			str3 = strTok.Mid(nV2+1, nLength-nV2-1);
			
			tLot.m_wItemID	= atoi(str1);
			tLot.m_bNum		= atoi(str2);
			tLot.m_wWinner	= atoi(str3);

			stEVENTINFO.m_vLOTTERY.push_back(tLot);

			strTok2 = strTok.Tokenize(_T(";"), nPos2);
		}
		stEVENTINFO.m_strLotMsg = str.Right(str.GetLength() - nPos);
	}
