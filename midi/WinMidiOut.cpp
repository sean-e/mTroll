/*
 * mTroll MIDI Controller
 * Copyright (C) 2007-2008,2010,2013 Sean Echevarria
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

#include "..\midi\WinMidiOut.h"
#include "..\Engine\ITraceDisplay.h"
#include "..\Engine\ISwitchDisplay.h"
#include <atlstr.h>

#pragma comment(lib, "winmm.lib")

static CString GetMidiErrorText(MMRESULT resultCode);
static WinMidiOut * sOutOnTimer = NULL;


WinMidiOut::WinMidiOut(ITraceDisplay * trace) : 
	mTrace(trace), 
	mMidiOut(NULL), 
	mMidiOutError(false),
	mCurMidiHdrIdx(0),
	mActivityIndicator(NULL),
	mActivityIndicatorIndex(0),
	mEnableActivityIndicator(false),
	mTimerEventCount(0),
	mTimerId(0),
	mDeviceIdx(0)
{
	for (int idx = 0; idx < MIDIHDR_CNT; ++idx)
		ZeroMemory(&mMidiHdrs[idx], sizeof(MIDIHDR));

	mTimerId = ::SetTimer(NULL, mTimerId, 150, TimerProc);
}

WinMidiOut::~WinMidiOut()
{
	::KillTimer(NULL, mTimerId);

	if (sOutOnTimer == this)
	{
		sOutOnTimer = NULL;
		TurnOffIndicator();
	}

	CloseMidiOut();
}

// IMidiOut
unsigned int
WinMidiOut::GetMidiOutDeviceCount() const
{
	return ::midiOutGetNumDevs();
}

std::string
WinMidiOut::GetMidiOutDeviceName(unsigned int deviceIdx) const
{
	std::string devName;
	MIDIOUTCAPSA outCaps;
	MMRESULT res = ::midiOutGetDevCapsA(deviceIdx, &outCaps, sizeof(MIDIOUTCAPSA));
	if (res == MMSYSERR_NOERROR)
		devName = outCaps.szPname;
	else
	{
		CString errMsg(::GetMidiErrorText(res));
		CString msg;
		msg.Format(_T("Error getting name of out device %d: %s"), deviceIdx, errMsg);
		devName = CStringA(msg);
	}

	return devName;
}

void
WinMidiOut::SetActivityIndicator(ISwitchDisplay * activityIndicator, 
								 int activityIndicatorIdx)
{
	mActivityIndicator = activityIndicator;
	mActivityIndicatorIndex = activityIndicatorIdx;
	mEnableActivityIndicator = mActivityIndicator > 0 && mActivityIndicator != NULL;
}

void
WinMidiOut::EnableActivityIndicator(bool enable)
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
WinMidiOut::OpenMidiOut(unsigned int deviceIdx)
{
	_ASSERTE(!mMidiOut);
	mDeviceIdx = deviceIdx;
	MMRESULT res = ::midiOutOpen(&mMidiOut, deviceIdx, (DWORD_PTR)MidiOutCallbackProc, (DWORD_PTR)this, CALLBACK_FUNCTION);
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

bool
WinMidiOut::MidiOut(const Bytes & bytes)
{
	if (!mMidiOut)
	{
		if (0 && mTrace)
			mTrace->Trace("midiout is not open.\n");
		return false;
	}

	const size_t kDataSize = bytes.size();
	if (!kDataSize)
		return false;

	IndicateActivity();

	// number of data bytes for each status
	static const int kMsgDataBytesLen = 23;
	static const int kMsgDataBytes[23] = { 2, 2, 2, 2, 1, 1, 2, 0, 1, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

	const byte * dataPtr = &bytes[0];
	size_t idx = 0;
	MMRESULT res;
	mMidiOutError = false;
	int sysexDepth = 0;

	const int kBufferSize = 256;
	const int kDelayTime = 60;

	for ( ; idx < kDataSize && !mMidiOutError; )
	{
		int curMsgLen;
		int statusByteIdx = dataPtr[idx];

		if (sysexDepth || SYSEX == statusByteIdx)
		{
			curMsgLen = 0;

			do
			{
				if (SYSEX == statusByteIdx)
					sysexDepth++;	// nested sysex
				else if (EOX == statusByteIdx)
					sysexDepth--;

				if (!((idx + ++curMsgLen) % kBufferSize))
					break;
			}
			while (sysexDepth && (idx + curMsgLen) < kDataSize);

			LPMIDIHDR curHdr = &mMidiHdrs[mCurMidiHdrIdx++];
			if (mCurMidiHdrIdx == MIDIHDR_CNT)
				mCurMidiHdrIdx = 0;

			curHdr->dwBufferLength = curMsgLen;
			curHdr->lpData = (LPSTR)(byte*)&bytes[idx];

			res = ::midiOutPrepareHeader(mMidiOut, curHdr, sizeof(MIDIHDR));
			if (MMSYSERR_NOERROR == res)
				res = ::midiOutLongMsg(mMidiOut, curHdr, sizeof(MIDIHDR));
		}
		else
		{
			if ((statusByteIdx & 0xF0) < SYSEX)		// is it a channel message?
				statusByteIdx = ((statusByteIdx & 0xF0) >> 4) - 8;
			else		// or system message
				statusByteIdx = (statusByteIdx & 0x0F) + 7;

			if (statusByteIdx < 0)
			{
				ReportError(_T("Status byte handling error at byte %d (%x).\n"), idx+1, dataPtr[idx]);
				break;
			}

			curMsgLen = statusByteIdx >= kMsgDataBytesLen ? 1 : kMsgDataBytes[statusByteIdx] + 1;
			if ((idx + curMsgLen) > kDataSize)
			{
				ReportError(_T("Data string consistency error at byte %d.  Missing data byte.\n"), idx+1);
				break;
			}

			DWORD shortMsg;
			switch (curMsgLen)
			{
			case 1:			// no data bytes
				shortMsg = dataPtr[idx];
				break;
			case 2:			// one data byte
				shortMsg = dataPtr[idx] | (dataPtr[idx+1] << 8);
				break;
			case 3:			// two data bytes
				shortMsg = dataPtr[idx] | (dataPtr[idx+1] << 8) | (dataPtr[idx+2] << 16);
				break;
			default:
				_ASSERTE(!"impossible");
				shortMsg = 0;
			}

			res = ::midiOutShortMsg(mMidiOut, shortMsg);
		}

		if (MMSYSERR_NOERROR != res)
		{
			if (MIDIERR_NOTREADY == res) 
			{
				Sleep(10);
				continue;
			}
			else
			{
				ReportMidiError(res, __LINE__);
				break;
			}
		}

		idx += curMsgLen;
		if (!(idx % kBufferSize))
			::Sleep(kDelayTime);
	}

	if (0 && mTrace && !mMidiOutError)
		mTrace->Trace("Transmission complete.\n");

	return true;
}

void
WinMidiOut::MidiOut(byte singleByte,
					bool useIndicator /*= true*/)
{
	if (!mMidiOut)
		return;

	MidiOut(singleByte, useIndicator);
}

void
WinMidiOut::MidiOut(byte byte1, 
					byte byte2,
					bool useIndicator /*= true*/)
{
	if (!mMidiOut)
		return;

	const DWORD shortMsg = byte1 | (byte2 << 8);
	MidiOut(shortMsg, useIndicator);
}

void
WinMidiOut::MidiOut(byte byte1, 
					byte byte2, 
					byte byte3,
					bool useIndicator /*= true*/)
{
	if (!mMidiOut)
		return;

	const DWORD shortMsg = byte1 | (byte2 << 8) | (byte3 << 16);
	MidiOut(shortMsg, useIndicator);
}

void
WinMidiOut::MidiOut(DWORD shortMsg, 
					bool useIndicator /*= true*/)
{
	for (;;)
	{
		const MMRESULT res = ::midiOutShortMsg(mMidiOut, shortMsg);
		if (MMSYSERR_NOERROR != res)
		{
			if (MIDIERR_NOTREADY == res) 
			{
				Sleep(10);
				continue;
			}
			else
			{
				ReportMidiError(res, __LINE__);
			}
		}
		else if (useIndicator)
			IndicateActivity();
		break;
	}
}

void CALLBACK 
WinMidiOut::MidiOutCallbackProc(HMIDIOUT hmo, 
								UINT wMsg, 
								DWORD dwInstance, 
								DWORD dwParam1, 
								DWORD dwParam2)
{
	if (MOM_DONE == wMsg)
	{
		WinMidiOut * _this = (WinMidiOut *) dwInstance;
		LPMIDIHDR hdr = (LPMIDIHDR) dwParam1;
		MMRESULT res = ::midiOutUnprepareHeader(_this->mMidiOut, hdr, sizeof(MIDIHDR));
		hdr->dwFlags = 0;
		_ASSERTE(MMSYSERR_NOERROR == res);
	}
}

void
WinMidiOut::IndicateActivity()
{
	if (!mEnableActivityIndicator)
	{
		if (sOutOnTimer == this)
		{
			sOutOnTimer = NULL;
			TurnOffIndicator();
		}

		return;
	}

	LONG prevCount = ::InterlockedIncrement(&mTimerEventCount);
	if (sOutOnTimer)
	{
		sOutOnTimer->TurnOffIndicator();
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
	sOutOnTimer = this;
}

void
WinMidiOut::TurnOffIndicator()
{
	if (mActivityIndicator)
		mActivityIndicator->SetSwitchDisplay(mActivityIndicatorIndex, false);
}

void CALLBACK
WinMidiOut::TimerProc(HWND, 
					  UINT, 
					  UINT_PTR id, 
					  DWORD)
{
	WinMidiOut * _this = sOutOnTimer;
	if (_this)
	{
		if (!::InterlockedDecrement(&_this->mTimerEventCount))
			sOutOnTimer = NULL;

		_this->TurnOffIndicator();
	}
}

bool
WinMidiOut::SuspendMidiOut()
{
	if (mMidiOut)
	{
		ReleaseMidiOut();
		return true;
	}

	return false;
}

bool
WinMidiOut::ResumeMidiOut()
{
	return OpenMidiOut(mDeviceIdx);
}

void
WinMidiOut::CloseMidiOut()
{
	mEnableActivityIndicator = false;
	mActivityIndicator = NULL;
	mTimerEventCount = 0;
	ReleaseMidiOut();
}

void
WinMidiOut::ReleaseMidiOut()
{
	if (mMidiOut)
	{
		MMRESULT res = ::midiOutReset(mMidiOut);
		res = ::midiOutClose(mMidiOut);
		if (res == MMSYSERR_NOERROR)
			mMidiOut = NULL;
		else
			ReportMidiError(res, __LINE__);
	}
}

void
WinMidiOut::ReportMidiError(MMRESULT resultCode, 
							unsigned int lineNumber)
{
	CString errMsg(::GetMidiErrorText(resultCode));
	CString msg;

	mMidiOutError = true;
	msg.Format(_T("Error: %s\nError location: %s (%d)\n"), errMsg, CString(__FILE__), lineNumber);
	if (mTrace)
		mTrace->Trace(std::string(CStringA(msg)));
}

void
WinMidiOut::ReportError(LPCTSTR msg)
{
	mMidiOutError = true;
	if (mTrace)
		mTrace->Trace(std::string(CStringA(CString(msg))));
}

void
WinMidiOut::ReportError(LPCTSTR msg, 
						int param1)
{
	CString errMsg;
	errMsg.Format(msg, param1);
	ReportError(errMsg);
}

void
WinMidiOut::ReportError(LPCTSTR msg, 
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
	::midiOutGetErrorText(resultCode, errMsg, sizeof(TCHAR)*MAXERRORLENGTH);
	return errMsg;
}
