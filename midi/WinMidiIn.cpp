/*
 * mTroll MIDI Controller
 * Copyright (C) 2010 Sean Echevarria
 *
 * This file is part of mTroll.
 *
 * mTroll is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * mTroll is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Let me know if you modify, extend or use mTroll.
 * Original project site: http://www.creepingfog.com/mTroll/
 * Contact Sean: "fester" at the domain of the original project site
 */

#include "..\midi\WinMidiIn.h"
#include "..\Engine\ITraceDisplay.h"
#include "..\Engine\ISwitchDisplay.h"
#include "../WinUtil/AutoLockCs.h"
#include <atlstr.h>

#pragma comment(lib, "winmm.lib")

static CString GetMidiErrorText(MMRESULT resultCode);

// http://home.roadrunner.com/~jgglatt/tech/lowmidi.htm
// http://msdn.microsoft.com/en-us/library/dd798458%28v=VS.85%29.aspx

WinMidiIn::WinMidiIn(ITraceDisplay * trace) : 
	mTrace(trace), 
	mMidiIn(NULL), 
	mMidiInError(false),
	mThread(NULL),
	mThreadId(0),
	mDeviceIdx(0),
	mThreadState(tsNotStarted),
	mCurMidiHdrIdx(0)
{
	for (int idx = 0; idx < MIDIHDR_CNT; ++idx)
		ZeroMemory(&mMidiHdrs[idx], sizeof(MIDIHDR));
	::InitializeCriticalSection(&mCs);
	mDoneEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
}

WinMidiIn::~WinMidiIn()
{
	CloseMidiIn();
	if (mDoneEvent && mDoneEvent != INVALID_HANDLE_VALUE)
		::CloseHandle(mDoneEvent);
	::DeleteCriticalSection(&mCs);
}

// IMidiIn
unsigned int
WinMidiIn::GetMidiInDeviceCount() const
{
	return ::midiInGetNumDevs();
}

std::string
WinMidiIn::GetMidiInDeviceName(unsigned int deviceIdx) const
{
	std::string devName;
	MIDIINCAPSA inCaps;
	MMRESULT res = ::midiInGetDevCapsA(deviceIdx, &inCaps, sizeof(MIDIINCAPSA));
	if (res == MMSYSERR_NOERROR)
		devName = inCaps.szPname;
	else
	{
		CString errMsg(::GetMidiErrorText(res));
		CString msg;
		msg.Format(_T("Error getting name of in device %d: %s"), deviceIdx, errMsg);
		devName = CStringA(msg);
	}

	return devName;
}

bool
WinMidiIn::OpenMidiIn(unsigned int deviceIdx)
{
	_ASSERTE(!mMidiIn);
	mDeviceIdx = deviceIdx;
	mThreadState = tsStarting;
	mThread = (HANDLE)_beginthreadex(NULL, 0, ServiceThread, this, 0, (unsigned int*)&mThreadId);
	if (!mThread)
		return false;

	while (mThread && mThreadState == tsStarting)
		::Sleep(500);

	return mThread && mThreadState == tsRunning;
}

void
WinMidiIn::ServiceThread()
{
	MMRESULT res = ::midiInOpen(&mMidiIn, mDeviceIdx, (DWORD_PTR)MidiInCallbackProc, (DWORD_PTR)this, CALLBACK_FUNCTION | MIDI_IO_STATUS);
	if (MMSYSERR_NOERROR != res)
	{
		mThreadState = tsEnding;
		ReportMidiError(res, __LINE__);
		return;
	}

	mThreadState = tsRunning;
	_ASSERTE(mDoneEvent && mDoneEvent != INVALID_HANDLE_VALUE);
    for (;;)
    {
		DWORD result; 
		MSG msg; 

		// Read all of the messages in this next loop, 
		// removing each message as we read it.
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) 
		{ 
			// If it is a quit message, exit.
			if (msg.message == WM_QUIT)  
				break; 

			// Otherwise, dispatch the message.
			DispatchMessage(&msg); 
		}

		// Wait for any message sent or posted to this queue 
		// or for one of the passed handles be set to signaled.
		result = MsgWaitForMultipleObjects(1, &mDoneEvent, FALSE, INFINITE, QS_ALLEVENTS);

		// The result tells us the type of event we have.
		if (result == (WAIT_OBJECT_0 + 1))
		{
			// New messages have arrived. 
			// Continue to the top of the always while loop to 
			// dispatch them and resume waiting.
			continue;
		} 
		else 
		{ 
			break;
		}
    }

	mThreadState = tsEnding;
	res = ::midiInReset(mMidiIn);
	_ASSERTE(res == MMSYSERR_NOERROR);
	res = ::midiInClose(mMidiIn);
	if (res == MMSYSERR_NOERROR)
		mMidiIn = NULL;
	else
		ReportMidiError(res, __LINE__);

	mThreadState = tsNotStarted;
	mThread = NULL;
	mThreadId = 0;
}

unsigned int __stdcall
WinMidiIn::ServiceThread(void * _thisParam)
{
	WinMidiIn * _this = static_cast<WinMidiIn*>(_thisParam);
	_this->ServiceThread();
	_endthreadex(0);
	return 0;
}

#define NOTEOFF			0x80
#define	NOTEON			0x90
#define SYSEX			0xF0
#define EOX				0xF7
#define	MIDI_CLOCK		0xF8
#define	MIDI_START		0xFA
#define	MIDI_CONTINUE	0xFB
#define	MIDI_STOP		0xFC

void CALLBACK 
WinMidiIn::MidiInCallbackProc(HMIDIIN hmi, 
							  UINT wMsg, 
							  DWORD dwInstance, 
							  DWORD dwParam1, 
							  DWORD dwParam2)
{
	MMRESULT res;
	WinMidiIn * _this = (WinMidiIn *) dwInstance;
	LPMIDIHDR hdr = (LPMIDIHDR) dwParam1;
// 	std::strstream traceMsg;

	switch (wMsg)
	{
	case MIM_DATA:
		break;
	case MIM_LONGDATA:
		break;
	case MIM_ERROR:
	case MIM_LONGERROR:
		break;
	case MOM_DONE:
		res = ::midiInUnprepareHeader(_this->mMidiIn, hdr, sizeof(MIDIHDR));
		hdr->dwFlags = 0;
		_ASSERTE(MMSYSERR_NOERROR == res);
		break;
	}
}

void
WinMidiIn::CloseMidiIn()
{
	if (mThread)
	{
		_ASSERTE(mDoneEvent);
		::SetEvent(mDoneEvent);
		::WaitForSingleObjectEx(mThread, 10000, FALSE);
		_ASSERTE(mThreadState == tsNotStarted);
		CloseHandle(mThread);
		mThread = NULL;
		mThreadId = 0;
	}

	_ASSERTE(!mMidiIn);
}

void
WinMidiIn::ReportMidiError(MMRESULT resultCode, 
						   unsigned int lineNumber)
{
	CString errMsg(::GetMidiErrorText(resultCode));
	CString msg;

	mMidiInError = true;
	msg.Format(_T("Error: %s\nError location: %s (%d)\n"), errMsg, CString(__FILE__), lineNumber);
	if (mTrace)
		mTrace->Trace(std::string(CStringA(msg)));
}

void
WinMidiIn::ReportError(LPCTSTR msg)
{
	mMidiInError = true;
	if (mTrace)
		mTrace->Trace(std::string(CStringA(CString(msg))));
}

void
WinMidiIn::ReportError(LPCTSTR msg, 
					   int param1)
{
	CString errMsg;
	errMsg.Format(msg, param1);
	ReportError(errMsg);
}

void
WinMidiIn::ReportError(LPCTSTR msg, 
					   int param1, 
					   int param2)
{
	CString errMsg;
	errMsg.Format(msg, param1, param2);
	ReportError(errMsg);
}

CString
GetMidiErrorText(MMRESULT resultCode)
{
	TCHAR	errMsg[MAXERRORLENGTH];
	::midiInGetErrorText(resultCode, errMsg, sizeof(TCHAR)*MAXERRORLENGTH);
	return errMsg;
}
