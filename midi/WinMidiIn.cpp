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
#include <atlstr.h>

#pragma comment(lib, "winmm.lib")

static CString GetMidiErrorText(MMRESULT resultCode);
static WinMidiIn * sInOnTimer = NULL;


WinMidiIn::WinMidiIn(ITraceDisplay * trace) : 
	mTrace(trace), 
	mMidiIn(NULL), 
	mMidiInError(false),
	mCurMidiHdrIdx(0),
	mActivityIndicator(NULL),
	mActivityIndicatorIndex(0),
	mEnableActivityIndicator(false),
	mTimerEventCount(0),
	mTimerId(0)
{
	for (int idx = 0; idx < MIDIHDR_CNT; ++idx)
		ZeroMemory(&mMidiHdrs[idx], sizeof(MIDIHDR));

	mTimerId = ::SetTimer(NULL, mTimerId, 150, TimerProc);
}

WinMidiIn::~WinMidiIn()
{
	::KillTimer(NULL, mTimerId);

	if (sInOnTimer == this)
	{
		sInOnTimer = NULL;
		TurnOffIndicator();
	}

	CloseMidiIn();
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

void
WinMidiIn::SetActivityIndicator(ISwitchDisplay * activityIndicator, 
								int activityIndicatorIdx)
{
	mActivityIndicator = activityIndicator;
	mActivityIndicatorIndex = activityIndicatorIdx;
	mEnableActivityIndicator = mActivityIndicator > 0 && mActivityIndicator != NULL;
}

void
WinMidiIn::EnableActivityIndicator(bool enable)
{
	if (enable)
	{
		mEnableActivityIndicator = mActivityIndicator > 0 && mActivityIndicator != NULL;
		mTimerEventCount = 0;
	}
	else
	{
		mEnableActivityIndicator = false;
		TurnOffIndicator();
	}
}

bool
WinMidiIn::OpenMidiIn(unsigned int deviceIdx)
{
	_ASSERTE(!mMidiIn);
	MMRESULT res = ::midiInOpen(&mMidiIn, deviceIdx, (DWORD_PTR)MidiInCallbackProc, (DWORD_PTR)this, CALLBACK_FUNCTION);
	if (MMSYSERR_NOERROR != res)
		ReportMidiError(res, __LINE__);
	return res == MMSYSERR_NOERROR;
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
	if (MOM_DONE == wMsg)
	{
		WinMidiIn * _this = (WinMidiIn *) dwInstance;
		LPMIDIHDR hdr = (LPMIDIHDR) dwParam1;
		MMRESULT res = ::midiInUnprepareHeader(_this->mMidiIn, hdr, sizeof(MIDIHDR));
		hdr->dwFlags = 0;
		_ASSERTE(MMSYSERR_NOERROR == res);
	}
}

void
WinMidiIn::IndicateActivity()
{
	if (!mEnableActivityIndicator)
	{
		if (sInOnTimer == this)
		{
			sInOnTimer = NULL;
			TurnOffIndicator();
		}

		return;
	}

	LONG prevCount = ::InterlockedIncrement(&mTimerEventCount);
	if (sInOnTimer)
	{
		sInOnTimer->TurnOffIndicator();
		if (prevCount > 1)
		{
			if (!::InterlockedDecrement(&mTimerEventCount))
			{
				// whoops - don't go to 0 else the timer will go negative
				::InterlockedIncrement(&mTimerEventCount);
			}
		}
	}

	mActivityIndicator->SetSwitchDisplay(mActivityIndicatorIndex, true);
	mTimerId = ::SetTimer(NULL, mTimerId, 150, TimerProc);
	sInOnTimer = this;
}

void
WinMidiIn::TurnOffIndicator()
{
	if (mActivityIndicator)
		mActivityIndicator->SetSwitchDisplay(mActivityIndicatorIndex, false);
}

void CALLBACK
WinMidiIn::TimerProc(HWND, 
					 UINT, 
					 UINT_PTR id, 
					 DWORD)
{
	WinMidiIn * _this = sInOnTimer;
	if (_this)
	{
		if (!::InterlockedDecrement(&_this->mTimerEventCount))
			sInOnTimer = NULL;

		_this->TurnOffIndicator();
	}
}

void
WinMidiIn::CloseMidiIn()
{
	mEnableActivityIndicator = false;
	mActivityIndicator = NULL;
	mTimerEventCount = 0;
	if (mMidiIn)
	{
		MMRESULT res = ::midiInClose(mMidiIn);
		if (res == MMSYSERR_NOERROR)
			mMidiIn = NULL;
		else
			ReportMidiError(res, __LINE__);
	}
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
