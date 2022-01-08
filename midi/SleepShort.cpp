// Code from 2015 answer: 
//		https://stackoverflow.com/a/31411628/103912
// by user Oskar Dahlberg: 
//		https://stackoverflow.com/users/4858189/oskar-dahlberg
// to question: 
//		https://stackoverflow.com/questions/85122/how-to-make-thread-sleep-less-than-a-millisecond-on-windows

#include <windows.h>
#include "SleepShort.h"


static NTSTATUS(__stdcall* NtDelayExecution)(BOOL Alertable, PLARGE_INTEGER DelayInterval) = (NTSTATUS(__stdcall*)(BOOL, PLARGE_INTEGER)) GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtDelayExecution");
static NTSTATUS(__stdcall* ZwSetTimerResolution)(IN ULONG RequestedResolution, IN BOOLEAN Set, OUT PULONG ActualResolution) = (NTSTATUS(__stdcall*)(ULONG, BOOLEAN, PULONG)) GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "ZwSetTimerResolution");


void SleepShort(float milliseconds) 
{
	static bool once = true;
	if (once) 
	{
		ULONG actualResolution;
		ZwSetTimerResolution(1, true, &actualResolution);
		once = false;
	}

	LARGE_INTEGER interval;
	interval.QuadPart = -1 * (int)(milliseconds * 10000.0f);
	NtDelayExecution(false, &interval);
}
