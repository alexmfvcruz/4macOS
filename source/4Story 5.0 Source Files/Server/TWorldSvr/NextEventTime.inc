	if(!bWeek || 7 < bWeek || 24 < bHour || 60 < bMin)
		return -1;

#ifdef FOURSTORY_NATIVE_SERVER
	CTime CurTime(current_time);
#else
	CTime CurTime = CTime::GetCurrentTime();		
#endif
	CTime EventTime = CTime(CurTime.GetYear(), CurTime.GetMonth(), CurTime.GetDay(), (int)bHour, (int)bMin, 0);
	BYTE bCurrentWeek = (BYTE)CurTime.GetDayOfWeek();
	int nDay = 0;

	if(bCurrentWeek > bWeek)
		bWeek+=7;

	nDay = bWeek - bCurrentWeek;

	if(nDay)
		EventTime = EventTime + CTimeSpan(nDay, 0, 0, 0);

	if(EventTime.GetTime() < CurTime.GetTime())
		EventTime = EventTime + CTimeSpan(7, 0, 0, 0);

	return EventTime.GetTime();
