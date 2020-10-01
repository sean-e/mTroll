/*
 * mTroll MIDI Controller
 * Copyright (C) 2007-2008,2013,2018,2020 Sean Echevarria
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

#ifndef WinMidiOut_h__
#define WinMidiOut_h__

#include "..\Engine\IMidiOut.h"
#include <Windows.h>
#include <MMSystem.h>
#include <tchar.h>
#include "..\Engine\EngineLoader.h"

class ITraceDisplay;


class WinMidiOut : public IMidiOut
{
public:
	WinMidiOut(ITraceDisplay * trace);
	virtual ~WinMidiOut();

	// IMidiOut
	virtual unsigned int GetMidiOutDeviceCount() const override;
	virtual std::string GetMidiOutDeviceName(unsigned int deviceIdx) const override;
	virtual void SetActivityIndicator(ISwitchDisplay * activityIndicator, int activityIndicatorIdx, unsigned int ledColor) override;
	virtual void EnableActivityIndicator(bool enable) override;
	virtual bool OpenMidiOut(unsigned int deviceIdx) override;
	virtual bool IsMidiOutOpen() const override {return mMidiOut != nullptr;}
	virtual bool MidiOut(const Bytes & bytes, bool useIndicator = true) override;
	virtual void MidiOut(byte singleByte, bool useIndicator = true) override;
	virtual void MidiOut(byte byte1, byte byte2, bool useIndicator = true) override;
	virtual void MidiOut(byte byte1, byte byte2, byte byte3, bool useIndicator = true) override;
	virtual bool SuspendMidiOut() override;
	virtual bool ResumeMidiOut() override;
	virtual void CloseMidiOut() override;

private:
	void ReportMidiError(MMRESULT resultCode, unsigned int lineNumber);
	void ReportError(LPCTSTR msg);
	void ReportError(LPCTSTR msg, int param1);
	void ReportError(LPCTSTR msg, int param1, int param2);

	void MidiOut(DWORD shortMsg, bool useIndicator = true);
	void IndicateActivity();
	void TurnOffIndicator();
	void ReleaseMidiOut();
	static void CALLBACK TimerProc(HWND, UINT, UINT_PTR id, DWORD);
	static void CALLBACK MidiOutCallbackProc(HMIDIOUT hmo, UINT wMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2);

	ITraceDisplay				* mTrace;
	ISwitchDisplay				* mActivityIndicator;
	volatile bool				mEnableActivityIndicator;
	int							mActivityIndicatorIndex;
	HMIDIOUT					mMidiOut;
	enum {MIDIHDR_CNT = 128};
	MIDIHDR						mMidiHdrs[MIDIHDR_CNT];
	int							mCurMidiHdrIdx;
	bool						mMidiOutError;
	UINT_PTR					mTimerId;
	LONG						mTimerEventCount;
	unsigned int				mDeviceIdx;
	unsigned int				mLedColor = kFirstColorPreset;
};

#endif // WinMidiOut_h__
