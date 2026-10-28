// Native replacement of Client/TClient/HwidManager.cpp (C4).
// The Windows unit queries WMI (Win32_Processor, Win32_BaseBoard,
// Win32_OperatingSystem) through __CpuInfo, __MotherboardInfo and
// __WindowsInfo, which stop with #error on other systems. macOS has no WMI:
// the query results stay empty, and GetHwid hashes the empty strings exactly
// as the original ComputeHash does. No client code calls GetHwid (only
// HwidManager::Initialize, TClient.cpp:205), so no packet carries these values.
#include "StdAfx.h"
#include "HwidManager.h"
#include "__Sha256.h"

std::string HwidManager::m_WinUserQueryResult;
std::string HwidManager::m_WinDeviceQueryResult;
std::string HwidManager::m_WinSerialQueryResult;
std::string HwidManager::m_CpuQueryResult;
std::string HwidManager::m_MbQueryResult;

void HwidManager::Initialize()
{
}

void HwidManager::GetHwid(HwidDataObject& hwidObject)
{
	hwidObject = HwidManager::ComputeHash();
}

HwidDataObject HwidManager::ComputeHash()
{
	HwidDataObject hwidData;

	hwidData.CpuSegment = sha256(m_CpuQueryResult);
	hwidData.MotherboardSegment = sha256(m_MbQueryResult);
	hwidData.WinDeviceSegment = sha256(m_WinDeviceQueryResult);
	hwidData.WinSerialSegment = sha256(m_WinSerialQueryResult);

	return hwidData;
}
