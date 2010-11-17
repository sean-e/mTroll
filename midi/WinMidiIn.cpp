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
#include "../Engine/HexStringUtils.h"
//#include "../WinUtil/AutoLockCs.h"
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
	MMRESULT res = ::midiInOpen(&mMidiIn, mDeviceIdx, (DWORD_PTR)MidiInCallbackProc, (DWORD_PTR)this, 
		CALLBACK_FUNCTION | MIDI_IO_STATUS);
	if (MMSYSERR_NOERROR != res)
	{
		mThreadState = tsEnding;
		ReportMidiError(res, __LINE__);
		mThreadState = tsNotStarted;
		return;
	}

	const int kDataBufLen = 512;
	int idx;
	for (idx = 0; idx < MIDIHDR_CNT; ++idx)
	{
		mMidiHdrs[idx].lpData = (LPSTR) ::malloc(kDataBufLen);
		mMidiHdrs[idx].dwBufferLength = kDataBufLen;
		
		res = ::midiInPrepareHeader(mMidiIn, &mMidiHdrs[idx], (UINT)sizeof(MIDIHDR));
		_ASSERTE(res == MMSYSERR_NOERROR);

		res = ::midiInAddBuffer(mMidiIn, &mMidiHdrs[idx], sizeof(MIDIHDR));
		_ASSERTE(res == MMSYSERR_NOERROR);
	}

	mThreadState = tsRunning;
	res = ::midiInStart(mMidiIn);
	_ASSERTE(res == MMSYSERR_NOERROR);

	_ASSERTE(mDoneEvent && mDoneEvent != INVALID_HANDLE_VALUE);
    for (;;)
    {
		DWORD result;
		MSG msg;

		// Read all of the messages in this next loop, 
		// removing each message as we read it.
		while (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			// If it is a quit message, exit.
			if (msg.message == WM_QUIT)
				break;

			// Otherwise, dispatch the message.
			::DispatchMessage(&msg);
		}

		// Wait for any message sent or posted to this queue 
		// or for one of the passed handles be set to signaled.
		result = ::MsgWaitForMultipleObjects(1, &mDoneEvent, FALSE, INFINITE, QS_ALLINPUT);

		// The result tells us the type of event we have.
		if (result == (WAIT_OBJECT_0 + 1))
		{
			// New messages have arrived. 
			// Continue to the top of the always while loop to 
			// dispatch them and resume waiting.
			continue;
		}
		else if (WAIT_TIMEOUT == result)
			continue;
		else if (WAIT_OBJECT_0 == result)
			break; // done event fired
		else
			break; // ??
    }

	mThreadState = tsEnding;
	res = ::midiInReset(mMidiIn);
	_ASSERTE(res == MMSYSERR_NOERROR);

	for (idx = 0; idx < MIDIHDR_CNT; ++idx)
	{
		res = ::midiInUnprepareHeader(mMidiIn, &mMidiHdrs[idx], (UINT)sizeof(MIDIHDR));
		_ASSERTE(res == MMSYSERR_NOERROR);
		::free(mMidiHdrs[idx].lpData);
		mMidiHdrs[idx].lpData = NULL;
	}

	res = ::midiInClose(mMidiIn);
	if (res == MMSYSERR_NOERROR)
		mMidiIn = NULL;
	else
		ReportMidiError(res, __LINE__);

	mThreadState = tsNotStarted;
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
	if (_this->mThreadState != tsRunning)
		return;

	switch (wMsg)
	{
	case MIM_DATA:
		// 	dwParam1 is the midi event with status in the low byte of the low word
		// 	dwParam2 is the event time in ms since the start of midi in
// 		if (_this->mTrace)
// 		{
// 			const std::string msg(::GetAsciiHexStr((byte*)&dwParam1, 4, true) + "\n");
// 			_this->mTrace->Trace(msg);
// 		}
		break;
	case MIM_LONGDATA:
		// 	dwParam1 is the lpMidiHdr
		// 	dwParam2 is the event time in ms since the start of midi in
		{
			LPMIDIHDR hdr = (LPMIDIHDR) dwParam1;
			if (_this->mTrace)
			{
				const std::string msg(::GetAsciiHexStr((byte*)hdr->lpData, hdr->dwBytesRecorded, true) + "\n");
				_this->mTrace->Trace(msg);
			}
			res = ::midiInAddBuffer(_this->mMidiIn, hdr, sizeof(MIDIHDR));
			_ASSERTE(res == MMSYSERR_NOERROR);
		}
		break;
	case MIM_ERROR:
	case MIM_LONGERROR:
		_asm nop;
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
		::WaitForSingleObjectEx(mThread, 30000, FALSE);
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
