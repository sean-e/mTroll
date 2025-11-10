/*
 * mTroll MIDI Controller
 * Copyright (C) 2010,2013,2015,2018,2020,2022,2025 Sean Echevarria
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

#include <atomic>
#include "WinMidiIn.h"
#include "../Engine/IMidiInSubscriber.h"
#include "../Engine/ITraceDisplay.h"
#include "../Engine/ISwitchDisplay.h"
#include "../Engine/HexStringUtils.h"
#include <atlstr.h>

#pragma comment(lib, "winmm.lib")

static CString GetMidiErrorText(MMRESULT resultCode);
#ifdef ITEM_COUNTING
std::atomic<int> gWinMidiInCnt = 0;
#endif

// http://home.roadrunner.com/~jgglatt/tech/lowmidi.htm
// http://msdn.microsoft.com/en-us/library/dd798458%28v=VS.85%29.aspx

WinMidiIn::WinMidiIn(ITraceDisplay * trace) : 
	mTrace(trace), 
	mMidiIn(nullptr),
	mMidiInError(false),
	mThread(nullptr),
	mThreadId(0),
	mDeviceIdx(0),
	mThreadState(tsNotStarted),
	mCurMidiHdrIdx(0)
{
#ifdef ITEM_COUNTING
	++gWinMidiInCnt;
#endif

	for (auto & midiHdr : mMidiHdrs)
		ZeroMemory(&midiHdr, sizeof(MIDIHDR));
	mDoneEvent = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
}

WinMidiIn::~WinMidiIn()
{
	CloseMidiIn();
	if (mDoneEvent && mDoneEvent != INVALID_HANDLE_VALUE)
		::CloseHandle(mDoneEvent);

#ifdef ITEM_COUNTING
	--gWinMidiInCnt;
#endif
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
		msg.Format(_T("Error getting name of in device %d: %s"), deviceIdx, (LPCWSTR)errMsg);
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
	mThread = (HANDLE)_beginthreadex(nullptr, 0, ServiceThread, this, 0, (unsigned int*)&mThreadId);
	if (!mThread)
	{
		mThreadState = tsNotStarted;
		return false;
	}

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
		while (::PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
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
		mMidiHdrs[idx].lpData = nullptr;
	}

	res = ::midiInClose(mMidiIn);
	if (res == MMSYSERR_NOERROR)
		mMidiIn = nullptr;
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
							  DWORD_PTR dwInstance, 
							  DWORD_PTR dwParam1, 
							  DWORD_PTR dwParam2)
{
	MMRESULT res;
	LPMIDIHDR hdr;
	WinMidiIn * _this = (WinMidiIn *) dwInstance;
	if (_this->mThreadState == tsEnding)
		return;

	switch (wMsg)
	{
	case MIM_DATA:
		// 	dwParam1 is the midi event with status in the low byte of the low word
		// 	dwParam2 is the event time in ms since the start of midi in
		if (_this->mThreadState != tsRunning)
			return;

		for (MidiInSubscribers::const_iterator it = _this->mInputSubscribers.begin();
			it != _this->mInputSubscribers.end(); ++it)
		{
			if (*it)
				(*it)->ReceivedData(LOBYTE(dwParam1), HIBYTE(dwParam1), LOBYTE(HIWORD(dwParam1)));
		}
		break;
	case MIM_ERROR:
		break;
	case MIM_LONGDATA:
		// 	dwParam1 is the lpMidiHdr
		// 	dwParam2 is the event time in ms since the start of midi in
		hdr = (LPMIDIHDR) dwParam1;
		if (_this->mThreadState == tsRunning)
		{
			for (MidiInSubscribers::const_iterator it = _this->mInputSubscribers.begin();
				it != _this->mInputSubscribers.end(); ++it)
			{
				if (*it)
					(*it)->ReceivedSysex((byte*)hdr->lpData, (int)hdr->dwBytesRecorded);
			}
		}

		res = ::midiInAddBuffer(_this->mMidiIn, hdr, sizeof(MIDIHDR));
		_ASSERTE(res == MMSYSERR_NOERROR);
		break;
	case MIM_LONGERROR:
		hdr = (LPMIDIHDR) dwParam1;
		res = ::midiInAddBuffer(_this->mMidiIn, hdr, sizeof(MIDIHDR));
		_ASSERTE(res == MMSYSERR_NOERROR);
		break;
	}
}

void
WinMidiIn::CloseMidiIn()
{
	ReleaseMidiIn();
	for (auto & curItem : mInputSubscribers)
	{
		try
		{
			auto _this = shared_from_this();
			if (_this)
				curItem->Closed(_this);
		}
		catch (const std::exception &)
		{
		}
	}
	mInputSubscribers.clear();
}

void
WinMidiIn::ReportMidiError(MMRESULT resultCode, 
						   unsigned int lineNumber)
{
	CString errMsg(::GetMidiErrorText(resultCode));
	CString msg;

	mMidiInError = true;
	msg.Format(_T("Error: %s\nError location: %s (%d)\n"), (LPCWSTR)errMsg, (LPCWSTR)CString(__FILE__), lineNumber);
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

bool
WinMidiIn::Subscribe(IMidiInSubscriberPtr sub)
{
	if (mThreadState != tsRunning)
	{
		for (auto & inputSubscriber : mInputSubscribers)
		{
			if (inputSubscriber == sub)
				return false;
		}

		mInputSubscribers.push_back(sub);
		return true;
	}
	else
	{
		_ASSERTE(!"don't subscribe to midi in events after thread has started");
		return false;
	}
}

void
WinMidiIn::Unsubscribe(IMidiInSubscriberPtr sub)
{
	if (mThreadState != tsRunning)
	{
		for (auto & inputSubscriber : mInputSubscribers)
		{
			if (inputSubscriber == sub)
				inputSubscriber = nullptr;
		}
	}
	else
	{
		_ASSERTE(!"don't unsubscribe to midi in events until thread has stopped");
	}
}

bool
WinMidiIn::SuspendMidiIn()
{
	if (mMidiIn)
	{
		ReleaseMidiIn();
		return true;
	}

	return false;
}

bool
WinMidiIn::ResumeMidiIn()
{
	return OpenMidiIn(mDeviceIdx);
}

void
WinMidiIn::ReleaseMidiIn()
{
	if (mThread)
	{
		_ASSERTE(mDoneEvent);
		if (mDoneEvent)
			::SetEvent(mDoneEvent);
		::WaitForSingleObjectEx(mThread, 30000, FALSE);
		_ASSERTE(mThreadState == tsNotStarted);
		::CloseHandle(mThread);
		mThread = nullptr;
		mThreadId = 0;
	}

	_ASSERTE(!mMidiIn);
}



CString
GetMidiErrorText(MMRESULT resultCode)
{
	TCHAR	errMsg[MAXERRORLENGTH];
	::midiInGetErrorText(resultCode, errMsg, MAXERRORLENGTH);
	return errMsg;
}
