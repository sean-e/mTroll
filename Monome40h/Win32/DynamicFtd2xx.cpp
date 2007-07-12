#include "DynamicFtd2xx.h"


DynamicFtd2xx::DynamicFtd2xx()
{
	Init();
}

void
DynamicFtd2xx::Init()
{
	Load("FTD2xx.dll");
	if (!IsLoaded())
		return;
}

FT_STATUS
DynamicFtd2xx::ListDevices(PVOID pArg1, PVOID pArg2, DWORD Flags)
{
	if (!IsLoaded())
		return FT_OTHER_ERROR;

	return FT_OTHER_ERROR;
}

BOOL
DynamicFtd2xx::SetCommTimeouts(FT_HANDLE ftHandle, FTTIMEOUTS *pTimeouts)
{
	if (!IsLoaded())
		return false;

	return false;
}

FT_HANDLE
DynamicFtd2xx::CreateFile(LPCSTR lpszName, DWORD dwAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreate, DWORD dwAttrsAndFlags, HANDLE hTemplate)
{
	if (!IsLoaded())
		return INVALID_HANDLE_VALUE;

	return INVALID_HANDLE_VALUE;
}

BOOL
DynamicFtd2xx::ReadFile(FT_HANDLE ftHandle, LPVOID lpBuffer, DWORD nBufferSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped)
{
	_ASSERTE(IsLoaded());

	return FALSE;
}

BOOL
DynamicFtd2xx::WriteFile(FT_HANDLE ftHandle, LPVOID lpBuffer, DWORD nBufferSize, LPDWORD lpBytesWritten, LPOVERLAPPED lpOverlapped)
{
	_ASSERTE(IsLoaded());

	return FALSE;
}

BOOL
DynamicFtd2xx::CloseHandle(FT_HANDLE ftHandle)
{
	_ASSERTE(IsLoaded());

	return FALSE;
}
