	CString strSIZE;

	strSIZE.Format( _T("%04X%04X"), strHeader.GetLength(), strBody.GetLength());
	return strSIZE + strHeader + strBody;
