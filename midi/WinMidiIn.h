/*
 * mTroll MIDI Controller
 * Copyright (C) 2010,2013,2018,2022 Sean Echevarria
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

#ifndef WinMidiIn_h__
#define WinMidiIn_h__

#include "..\Engine\IMidiIn.h"
#include <Windows.h>
#include <MMSystem.h>
#include <tchar.h>
#include <vector>

class ITraceDisplay;


class WinMidiIn : public IMidiIn
{
public:
	WinMidiIn(ITraceDisplay * trace);
	virtual ~WinMidiIn();

	// IMidiIn
	virtual unsigned int GetMidiInDeviceCount() const override;
	virtual std::string GetMidiInDeviceName(unsigned int deviceIdx) const override;
	virtual bool OpenMidiIn(unsigned int deviceIdx) override;
	virtual bool IsMidiInOpen() const override {return mMidiIn != nullptr;}
	virtual bool Subscribe(IMidiInSubscriberPtr sub) override;
	virtual void Unsubscribe(IMidiInSubscriberPtr sub) override;
	virtual bool SuspendMidiIn() override;
	virtual bool ResumeMidiIn() override;
	virtual void CloseMidiIn() override;

private:
	static unsigned int __stdcall ServiceThread(void * _this);
	void ServiceThread();
	void ReleaseMidiIn();
	void ReportMidiError(MMRESULT resultCode, unsigned int lineNumber);
	void ReportError(LPCTSTR msg);
	void ReportError(LPCTSTR msg, int param1);
	void ReportError(LPCTSTR msg, int param1, int param2);

	static void CALLBACK MidiInCallbackProc(HMIDIIN hmi, UINT wMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2);

	enum ThreadState { tsNotStarted, tsStarting, tsRunning, tsEnding };

	ITraceDisplay				* mTrace;
	HMIDIIN						mMidiIn;
	enum { MIDIHDR_CNT = 512 };
	MIDIHDR						mMidiHdrs[MIDIHDR_CNT];
	int							mCurMidiHdrIdx;
	unsigned int				mDeviceIdx;
	bool						mMidiInError;
	HANDLE						mDoneEvent;
	HANDLE						mThread;
	ThreadState					mThreadState;
	DWORD						mThreadId;
	using MidiInSubscribers = std::vector<IMidiInSubscriberPtr>;
	MidiInSubscribers			mInputSubscribers;
};

#endif // WinMidiIn_h__
