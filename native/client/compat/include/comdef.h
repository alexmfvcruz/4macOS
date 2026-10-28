// Windows SDK header name (C4). The COM support classes (_bstr_t, _variant_t,
// _com_error) are not provided; the only client user (CTClientGame::
// GetWMIValue) has a native branch. TClientWnd.cpp includes the header only.
#pragma once
#include <windows.h>
