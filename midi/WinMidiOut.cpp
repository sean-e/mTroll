/*
 * mTroll MIDI Controller
 * Copyright (C) 2007-2008,2010,2013,2015,2018,2020-2022,2025 Sean Echevarria
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
 *
 */

/*
 * The following notice applies to WinMidiOut::SetTempo and WinMidiOut::ClockThread:
 * 
Copyright (c) 2016 Pete Brown

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 **/

#include <atomic>
#include <cmath>
#include "WinMidiOut.h"
#include "../Engine/ITraceDisplay.h"
#include "../Engine/ISwitchDisplay.h"
#include <atlstr.h>
#include "SleepShort.h"

#pragma comment(lib, "winmm.lib")

static CString GetMidiErrorText(MMRESULT resultCode);
static WinMidiOut * sOutOnTimer = nullptr;
#ifdef ITEM_COUNTING
std::atomic<int> gWinMidiOutCnt = 0;
#endif


WinMidiOut::WinMidiOut(ITraceDisplay * trace) : 
	mTrace(trace), 
	mMidiOut(nullptr),
	mMidiOutError(false),
	mCurMidiHdrIdx(0),
	mActivityIndicator(nullptr),
	mActivityIndicatorIndex(0),
	mEnableActivityIndicator(false),
	mTimerEventCount(0),
	mTimerId(0),
	mDeviceIdx(0)
{
#ifdef ITEM_COUNTING
	++gWinMidiOutCnt;
#endif

	for (auto & midiHdr : mMidiHdrs)
		ZeroMemory(&midiHdr, sizeof(MIDIHDR));

	mTimerId = ::SetTimer(nullptr, mTimerId, 150, TimerProc);
	::QueryPerformanceFrequency(&mPerfFreq);
}

WinMidiOut::~WinMidiOut()
{
	::KillTimer(nullptr, mTimerId);

	if (sOutOnTimer == this)
	{
		sOutOnTimer = nullptr;
		TurnOffIndicator();
	}

	CloseMidiOut();

#ifdef ITEM_COUNTING
	--gWinMidiOutCnt;
#endif
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

std::string
WinMidiOut::GetMidiOutDeviceName() const
{
	return mName;
}

void
WinMidiOut::SetActivityIndicator(ISwitchDisplay * activityIndicator, 
								 int activityIndicatorIdx, 
								 unsigned int ledColor)
{
	mActivityIndicator = activityIndicator;
	mActivityIndicatorIndex = activityIndicatorIdx;
	mEnableActivityIndicator = mActivityIndicatorIndex > 0 && mActivityIndicator != nullptr;
	mLedColor = ledColor;
}

void
WinMidiOut::EnableActivityIndicator(bool enable)
{
	if (enable)
	{
		mEnableActivityIndicator = mActivityIndicatorIndex > 0 && mActivityIndicator != nullptr;
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
	else
		mName = GetMidiOutDeviceName(deviceIdx);

	if (mClockEnabled)
	{
		SetTempo(mTempo);
		EnableMidiClock(true);
	}

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
WinMidiOut::MidiOut(const Bytes & bytes, bool useIndicator /*= true*/)
{
	if (!mMidiOut)
	{
		if (false && mTrace)
			mTrace->Trace("midiout is not open.\n");
		return false;
	}

	const size_t kDataSize = bytes.size();
	if (!kDataSize)
		return false;

	if (useIndicator)
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

	if (false && mTrace && !mMidiOutError)
		mTrace->Trace("Transmission complete.\n");

	return true;
}

void
WinMidiOut::MidiOut(byte singleByte,
					bool useIndicator /*= true*/)
{
	if (!mMidiOut)
		return;

	MidiOut((DWORD)singleByte, useIndicator);
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
	MMRESULT res;
	for (;;)
	{
		res = ::midiOutShortMsg(mMidiOut, shortMsg);
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

unsigned int __stdcall
WinMidiOut::ClockThread(void* _thisParam)
{
	WinMidiOut* _this = static_cast<WinMidiOut*>(_thisParam);
	_this->ClockThread();
	_endthreadex(0);
	return 0;
}

// if mPerfFreq is 10,000,000 counts per second, then:
// 240bpm: (10000000 / (240 / 60)) / 24 = 104167 -> 96 pulses per second, ~10ms per clock pulse
// 120bpm: (10000000 / (120 / 60)) / 24 = 208333 -> 48 pulses per second, ~21ms per clock pulse
//  60bpm: (10000000 /  (60 / 60)) / 24 = 416666 -> 24 pulses per second, ~42ms per clock pulse

// WinMidiOut::ClockThread is from the implementation of MidiClockGenerator::ThreadWorker by Pete Brown
// https://github.com/Psychlist1972/Windows-10-MIDI-Library/blob/master/PeteBrown.Devices.Midi/PeteBrown.Devices.Midi/MidiClockGenerator.cpp
// Copyright (c) 2016 Pete Brown
// MIT License https://github.com/Psychlist1972/Windows-10-MIDI-Library/blob/master/LICENSE.md
// https://github.com/Psychlist1972
void
WinMidiOut::ClockThread()
{
	MMRESULT res;
	LARGE_INTEGER lastTime;
	LONGLONG nextTime;
	LARGE_INTEGER currentTime;

	currentTime.QuadPart = 0;

	double errorAccumulator = 0.0;
	// not exact 5ms boundaries for breathing room...
	const LONGLONG k40msInCycles = (LONGLONG)((mPerfFreq.QuadPart / 1000) * 40.2f);
	const LONGLONG k35msInCycles = (LONGLONG)((mPerfFreq.QuadPart / 1000) * 35.2f);
	const LONGLONG k30msInCycles = (LONGLONG)((mPerfFreq.QuadPart / 1000) * 30.2f);
	const LONGLONG k25msInCycles = (LONGLONG)((mPerfFreq.QuadPart / 1000) * 25.2f);
	const LONGLONG k20msInCycles = (LONGLONG)((mPerfFreq.QuadPart / 1000) * 20.2f);
	const LONGLONG k15msInCycles = (LONGLONG)((mPerfFreq.QuadPart / 1000) * 15.2f);
	const LONGLONG k10msInCycles = (LONGLONG)((mPerfFreq.QuadPart / 1000) * 10.4f);
	const LONGLONG k5msInCycles = (LONGLONG)((mPerfFreq.QuadPart / 1000) * 5.5f);

	while (mRunClockThread)
	{
		::QueryPerformanceCounter(&lastTime);

		// send the message
		res = ::midiOutShortMsg(mMidiOut, (DWORD)MIDI_CLOCK);

		nextTime = lastTime.QuadPart + mTickInterval;

		if (errorAccumulator >= 1.0)
		{
			// get the whole number part of the error and then remove it from
			// the accumulator (which is a double)
			nextTime += (long)(std::trunc(errorAccumulator));
			errorAccumulator -= (long)(std::trunc(errorAccumulator));
		}

		::QueryPerformanceCounter(&currentTime);
		if (nextTime > currentTime.QuadPart)
		{
			const LONGLONG waitCycles = nextTime - currentTime.QuadPart;
			// if nextTime is more than XXms away, use SleepShort for at least XXms of the time
			if (waitCycles > k40msInCycles)
				::SleepShort(40.0); // 60bpm
			else if (waitCycles > k35msInCycles)
				::SleepShort(35.0);
			else if (waitCycles > k30msInCycles)
				::SleepShort(30.0);
			else if (waitCycles > k25msInCycles)
				::SleepShort(25.0); // 90bpm
			else if (waitCycles > k20msInCycles)
				::SleepShort(20.0); // 120bpm
			else if (waitCycles > k15msInCycles)
				::SleepShort(15.0); // 120bpm
			else if (waitCycles > k10msInCycles)
				::SleepShort(10.0); // 180bpm, 240bpm
			else if (waitCycles > k5msInCycles)
				::SleepShort(5.0);// 240bpm
			else
				; // 240bpm
		}

		// spin until this cycle is done
		// This is the one part I really dislike here.
		// sleeping the thread is no good due to durations required
		while (currentTime.QuadPart < nextTime)
			::QueryPerformanceCounter(&currentTime);

		// accumulate some error :)
		errorAccumulator += mTickTruncationError;
	}

	mClockThreadId = 0;
}

void
WinMidiOut::EnableMidiClock(bool enable)
{
	if (enable)
	{
		if (mClockThread)
		{
			// end the thread if already running
			EnableMidiClock(false);
		}

		// start the thread
		mClockEnabled = mRunClockThread = true;
		mClockThread = (HANDLE)_beginthreadex(nullptr, 0, ClockThread, this, 0, (unsigned int*)&mClockThreadId);
	}
	else 
	{
		mClockEnabled = false;
		if (mRunClockThread)
		{
			// end the thread
			if (mClockThread)
			{
				mRunClockThread = false;
				::WaitForSingleObjectEx(mClockThread, 30000, FALSE);
				::CloseHandle(mClockThread);
				mClockThread = nullptr;
			}
		}
	}
}

// WinMidiOut::SetTempo is from the implementation of MidiClockGenerator::Tempo::set by Pete Brown
// https://github.com/Psychlist1972/Windows-10-MIDI-Library/blob/master/PeteBrown.Devices.Midi/PeteBrown.Devices.Midi/MidiClockGenerator.cpp
// Copyright (c) 2016 Pete Brown
// MIT License https://github.com/Psychlist1972/Windows-10-MIDI-Library/blob/master/LICENSE.md
// https://github.com/Psychlist1972
void
WinMidiOut::SetTempo(int bpm)
{
	constexpr int ppqn = 24;

	// The Fractal Audio Axe-FX III tempo range is 24-250, which seems reasonable to enforce
	if (bpm < 24)
		mTempo = 24;
	else if (bpm > 250)
		mTempo = 250;
	else
		mTempo = bpm;

	// this is going to have rounding errors
	// we account for this in the clock worker thread function
	mTickInterval = (LONGLONG)(std::trunc(((double)mPerfFreq.QuadPart / (mTempo / 60.0F)) / ppqn));

	mTickTruncationError = (double)(((double)mPerfFreq.QuadPart / (mTempo / 60.0F)) / ppqn) - mTickInterval;
}

int
WinMidiOut::GetTempo() const
{
	return mTempo;
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
			sOutOnTimer = nullptr;
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
				// whoops - don't go to 0 else the timer will make the count go negative
				::InterlockedIncrement(&mTimerEventCount);
			}
		}
	}

	mActivityIndicator->SetSwitchDisplay(mActivityIndicatorIndex, mLedColor);
	// this seems to sometimes cause indicator to remain on
	// mTimerId = ::SetTimer(nullptr, mTimerId, 150, TimerProc);
	sOutOnTimer = this;
}

void
WinMidiOut::TurnOffIndicator()
{
	if (mActivityIndicator)
		mActivityIndicator->TurnOffSwitchDisplay(mActivityIndicatorIndex);
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
			sOutOnTimer = nullptr;

		_this->TurnOffIndicator();
	}
}

bool
WinMidiOut::SuspendMidiOut()
{
	if (mMidiOut)
	{
		const auto prevVal = mClockEnabled;
		ReleaseMidiOut();
		mClockEnabled = prevVal;

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
	mActivityIndicator = nullptr;
	mTimerEventCount = 0;
	ReleaseMidiOut();
}

void
WinMidiOut::ReleaseMidiOut()
{
	EnableMidiClock(false);

	if (mMidiOut)
	{
		MMRESULT res = ::midiOutReset(mMidiOut);
		res = ::midiOutClose(mMidiOut);
		if (res == MMSYSERR_NOERROR)
			mMidiOut = nullptr;
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
	::midiOutGetErrorText(resultCode, errMsg, MAXERRORLENGTH);
	return errMsg;
}
