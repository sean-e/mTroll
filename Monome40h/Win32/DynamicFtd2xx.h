#ifndef DynamicFtd2xx_h__
#define DynamicFtd2xx_h__

#include "..\..\mControlUI\Library.h"
#include "..\FTD2XX.H"


class DynamicFtd2xx : protected Library
{
public:
	DynamicFtd2xx();

	FT_STATUS ListDevices(PVOID pArg1, PVOID pArg2, DWORD Flags);
	BOOL SetCommTimeouts(FT_HANDLE ftHandle, FTTIMEOUTS *pTimeouts);
	FT_HANDLE CreateFile(LPCSTR lpszName, DWORD dwAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreate, DWORD dwAttrsAndFlags, HANDLE hTemplate);
	BOOL ReadFile(FT_HANDLE ftHandle, LPVOID lpBuffer, DWORD nBufferSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped);
	BOOL WriteFile(FT_HANDLE ftHandle, LPVOID lpBuffer, DWORD nBufferSize, LPDWORD lpBytesWritten, LPOVERLAPPED lpOverlapped);
	BOOL CloseHandle(FT_HANDLE ftHandle);

private:
	void		Init();
};

#endif // DynamicFtd2xx_h__
