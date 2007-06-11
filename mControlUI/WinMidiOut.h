#pragma once

#include "..\Engine\IMidiOut.h"
#include <Windows.h>
#include <MMSystem.h>

#define NOTEOFF			0x80
#define	NOTEON			0x90
#define SYSEX			0xF0
#define	MTC_QFRAME		0xF1
#define EOX				0xF7
#define	MIDI_CLOCK		0xF8
#define	MIDI_START		0xFA
#define	MIDI_CONTINUE	0xFB
#define	MIDI_STOP		0xFC

class ITraceDisplay;

class WinMidiOut : public IMidiOut
{
public:
	WinMidiOut(ITraceDisplay * trace);
	~WinMidiOut();

	// IMidiOut
	virtual unsigned int GetMidiOutDeviceCount() const;
	virtual std::string GetMidiOutDeviceName(unsigned int deviceIdx) const;
	virtual bool OpenMidiOut(unsigned int deviceIdx);
	virtual bool IsMidiOutOpen() const {return mMidiOut != NULL;}
	virtual bool MidiOut(const Bytes & bytes);
	virtual void CloseMidiOut();

private:
	MMRESULT SendSysex();
	void ReportMidiError(MMRESULT resultCode, unsigned int lineNumber) const;
	void ReportError(LPCSTR msg) const;
	void ReportError(LPCSTR msg, int param1) const;
	void ReportError(LPCSTR msg, int param1, int param2) const;

	static void CALLBACK MidiOutCallbackProc(HMIDIOUT hmo, UINT wMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2);

	ITraceDisplay				* mTrace;
	HMIDIOUT					mMidiOut;
	enum {MIDIHDR_CNT = 256};
	MIDIHDR						mMidiHdrs[MIDIHDR_CNT];
	int							mCurMidiHdrIdx;
	mutable bool				mMidiOutError;
};
