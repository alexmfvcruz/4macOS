	BYTE bGap = WARCOUNTRY_MAXGAP;
	if(bLevel < BROA_BASELEVEL)
		return bGap;

	return (bLevel - BROA_BASELEVEL) / 10;
