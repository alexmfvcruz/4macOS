// Native replacement for <Security.h> (FOURSTORY_CLIENT_NATIVE): only
// GetComputerNameEx, which CryptographyExt.cpp uses. The host name stands for
// every name format.
#pragma once
#include "windows.h"
enum COMPUTER_NAME_FORMAT {
    ComputerNameNetBIOS, ComputerNameDnsHostname, ComputerNameDnsDomain, ComputerNameDnsFullyQualified,
    ComputerNamePhysicalNetBIOS, ComputerNamePhysicalDnsHostname, ComputerNamePhysicalDnsDomain,
    ComputerNamePhysicalDnsFullyQualified, ComputerNameMax
};
inline BOOL GetComputerNameEx(COMPUTER_NAME_FORMAT, LPSTR buffer, LPDWORD size) { return GetComputerName(buffer, size); }
#define GetComputerNameExA GetComputerNameEx
